#include <windows.h>
#include <aclapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "jsrf_save_root.h"
#include "kernel.h"

static int failures;

/* kernel_path.c logs its initialization; the native fixture links that real
 * source directly and stubs only this unrelated diagnostic sink. */
void xbox_log(int level, const char *subsystem, const char *format, ...)
{
    (void)level;
    (void)subsystem;
    (void)format;
}

static void check(BOOL condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s (winerror=%lu)\n", message,
                (unsigned long)GetLastError());
        ++failures;
    }
}

static BOOL join_path(WCHAR *out, size_t capacity,
                      const WCHAR *base, const WCHAR *leaf)
{
    return swprintf_s(out, capacity, L"%s\\%s", base, leaf) >= 0;
}

static BOOL create_fixture_directory(WCHAR *parent, size_t parent_cap,
                                     WCHAR *selected, size_t selected_cap,
                                     WCHAR *denied, size_t denied_cap,
                                     WCHAR *sentinel_dir, size_t sentinel_cap)
{
    WCHAR temp[MAX_PATH];
    WCHAR unique[MAX_PATH];
    DWORD length = GetTempPathW(ARRAYSIZE(temp), temp);
    if (!length || length >= ARRAYSIZE(temp) ||
        !GetTempFileNameW(temp, L"jsr", 0, unique))
        return FALSE;
    if (!DeleteFileW(unique) || !CreateDirectoryW(unique, NULL))
        return FALSE;
    if (wcslen(unique) >= parent_cap)
        return FALSE;
    wcscpy_s(parent, parent_cap, unique);
    if (!join_path(selected, selected_cap, parent, L"selected save --probe=literal") ||
        !CreateDirectoryW(selected, NULL) ||
        !join_path(denied, denied_cap, parent, L"denied save") ||
        !CreateDirectoryW(denied, NULL) ||
        !join_path(sentinel_dir, sentinel_cap, parent, L"sentinel save") ||
        !CreateDirectoryW(sentinel_dir, NULL))
        return FALSE;
    return TRUE;
}

static BOOL delete_file_if_present(const WCHAR *path)
{
    DWORD error;
    if (DeleteFileW(path))
        return TRUE;
    error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static BOOL remove_directory_if_present(const WCHAR *path)
{
    DWORD error;
    if (RemoveDirectoryW(path))
        return TRUE;
    error = GetLastError();
    return error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND;
}

static BOOL remove_tree_contents(const WCHAR *root)
{
    WCHAR path[MAX_PATH];
    int i;
    BOOL ok = TRUE;
    static const WCHAR *dirs[] = {
        L"TitleData", L"UserData", L"Cache", L"SystemData"
    };
    for (i = 0; i <= 5; ++i) {
        if (swprintf_s(path, ARRAYSIZE(path), L"%s\\Partition%d.img", root, i) < 0 ||
            !delete_file_if_present(path))
            ok = FALSE;
    }
    for (i = 0; i < (int)ARRAYSIZE(dirs); ++i) {
        if (!join_path(path, ARRAYSIZE(path), root, dirs[i]) ||
            !remove_directory_if_present(path))
            ok = FALSE;
    }
    if (!remove_directory_if_present(root))
        ok = FALSE;
    return ok;
}

static BOOL write_sentinel(const WCHAR *path, const BYTE *bytes, DWORD length)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE)
        return FALSE;
    ok = WriteFile(file, bytes, length, &written, NULL) && written == length;
    CloseHandle(file);
    return ok;
}

static BOOL sentinel_matches(const WCHAR *path, const BYTE *expected, DWORD length)
{
    BYTE actual[32];
    DWORD read = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE || length > sizeof(actual))
        return FALSE;
    ok = ReadFile(file, actual, length, &read, NULL) && read == length &&
         memcmp(actual, expected, length) == 0;
    CloseHandle(file);
    return ok;
}

static BOOL deny_current_user_file_creation(const WCHAR *directory,
                                            PSECURITY_DESCRIPTOR *saved_sd)
{
    HANDLE token = NULL;
    DWORD needed = 0;
    TOKEN_USER *user = NULL;
    PACL old_acl = NULL;
    PACL new_acl = NULL;
    DWORD result;
    EXPLICIT_ACCESSW entry;

    *saved_sd = NULL;
    result = GetNamedSecurityInfoW((LPWSTR)directory, SE_FILE_OBJECT,
                                   DACL_SECURITY_INFORMATION, NULL, NULL,
                                   &old_acl, NULL, saved_sd);
    if (result != ERROR_SUCCESS)
        goto failed;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        result = GetLastError();
        goto failed;
    }
    GetTokenInformation(token, TokenUser, NULL, 0, &needed);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !needed) {
        result = GetLastError();
        goto failed;
    }
    user = (TOKEN_USER *)malloc(needed);
    if (!user) {
        result = ERROR_NOT_ENOUGH_MEMORY;
        goto failed;
    }
    if (!GetTokenInformation(token, TokenUser, user, needed, &needed)) {
        result = GetLastError();
        goto failed;
    }

    memset(&entry, 0, sizeof(entry));
    entry.grfAccessPermissions = FILE_ADD_FILE;
    entry.grfAccessMode = DENY_ACCESS;
    entry.grfInheritance = NO_INHERITANCE;
    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
    entry.Trustee.ptstrName = (LPWSTR)user->User.Sid;
    result = SetEntriesInAclW(1, &entry, old_acl, &new_acl);
    if (result != ERROR_SUCCESS)
        goto failed;
    result = SetNamedSecurityInfoW((LPWSTR)directory, SE_FILE_OBJECT,
                                   DACL_SECURITY_INFORMATION, NULL, NULL,
                                   new_acl, NULL);
    if (result != ERROR_SUCCESS)
        goto failed;

    free(user);
    CloseHandle(token);
    LocalFree(new_acl);
    return TRUE;

failed:
    if (user)
        free(user);
    if (token)
        CloseHandle(token);
    if (new_acl)
        LocalFree(new_acl);
    if (*saved_sd) {
        LocalFree(*saved_sd);
        *saved_sd = NULL;
    }
    SetLastError(result);
    return FALSE;
}

static BOOL restore_directory_acl(const WCHAR *directory,
                                  PSECURITY_DESCRIPTOR saved_sd)
{
    PACL old_acl = NULL;
    BOOL dacl_present = FALSE;
    BOOL dacl_defaulted = FALSE;
    BOOL restored = FALSE;
    if (!saved_sd)
        return FALSE;
    if (GetSecurityDescriptorDacl(saved_sd, &dacl_present, &old_acl,
                                  &dacl_defaulted)) {
        restored = SetNamedSecurityInfoW((LPWSTR)directory, SE_FILE_OBJECT,
                                         DACL_SECURITY_INFORMATION, NULL, NULL,
                                         old_acl, NULL) == ERROR_SUCCESS;
    }
    LocalFree(saved_sd);
    return restored;
}

static BOOL expect_translation(const char *xbox_path, const WCHAR *expected)
{
    WCHAR actual[MAX_PATH];
    if (!xbox_translate_path(xbox_path, actual, ARRAYSIZE(actual))) {
        fprintf(stderr, "FAIL: xbox_translate_path rejected %s\n", xbox_path);
        ++failures;
        return FALSE;
    }
    if (_wcsicmp(actual, expected) != 0) {
        fwprintf(stderr, L"FAIL: %S resolved to <%s>, expected <%s>\n",
                 xbox_path, actual, expected);
        ++failures;
        return FALSE;
    }
    return TRUE;
}

static void test_command_quoting(void)
{
    char command[2048] = "";
    size_t used = 0;
    const char *args[] = {
        "C:\\program files\\jsrf_recomp.exe",
        "--save-root=C:\\save roots\\ending\\",
        "--probe=healthy",
        "argument with \"quoted\" text"
    };
    WCHAR wide_command[2048];
    LPWSTR *parsed;
    int count = 0;
    int i;

    for (i = 0; i < (int)ARRAYSIZE(args); ++i)
        check(jsrf_append_windows_arg(command, sizeof(command), &used, args[i]),
              "append a Windows child argument");
    check(MultiByteToWideChar(CP_ACP, 0, command, -1, wide_command,
                              ARRAYSIZE(wide_command)) != 0,
          "convert generated command line for round-trip check");
    parsed = CommandLineToArgvW(wide_command, &count);
    check(parsed != NULL && count == ARRAYSIZE(args),
          "quoted child command has all arguments");
    if (parsed) {
        for (i = 0; i < count && i < (int)ARRAYSIZE(args); ++i) {
            WCHAR expected[512];
            MultiByteToWideChar(CP_ACP, 0, args[i], -1, expected, ARRAYSIZE(expected));
            check(wcscmp(parsed[i], expected) == 0,
                  "Windows argv quoting preserves exact argument");
        }
        LocalFree(parsed);
    }
}

int main(void)
{
    WCHAR parent[MAX_PATH], selected[MAX_PATH], denied[MAX_PATH], sentinel_dir[MAX_PATH];
    WCHAR sentinel_path[MAX_PATH], file_root[MAX_PATH], missing_root[MAX_PATH];
    WCHAR overlong[MAX_PATH * 2], default_root[MAX_PATH], local_root[MAX_PATH];
    WCHAR root_argument[MAX_PATH + 32];
    WCHAR *argv_ok[3];
    WCHAR *argv_missing[1];
    WCHAR *argv_empty[2];
    WCHAR *argv_bare[3];
    JsrfLaunchOptions options;
    WCHAR resolved[MAX_PATH];
    char root_utf8[JSRF_SAVE_ROOT_UTF8_CAP];
    DWORD error = ERROR_SUCCESS;
    PSECURITY_DESCRIPTOR saved_sd = NULL;
    BYTE sentinel_bytes[] = { 0x54, 0x45, 0x53, 0x54, 0xA5, 0x00, 0x7F };
    BOOL launch_valid = FALSE;
    BOOL fixture_ready = create_fixture_directory(parent, ARRAYSIZE(parent),
                                                  selected, ARRAYSIZE(selected),
                                                  denied, ARRAYSIZE(denied),
                                                  sentinel_dir, ARRAYSIZE(sentinel_dir));

    check(fixture_ready, "create disposable fixture roots with spaces");
    if (!fixture_ready)
        return 1;

    check(join_path(sentinel_path, ARRAYSIZE(sentinel_path), sentinel_dir,
                    L"save-sentinel.bin") &&
          write_sentinel(sentinel_path, sentinel_bytes, sizeof(sentinel_bytes)),
          "write disposable sentinel bytes");

    argv_missing[0] = L"jsrf_recomp.exe";
    check(!jsrf_parse_launch_args_w(1, argv_missing, &options, &error) &&
          error == ERROR_INVALID_PARAMETER,
          "missing save-root option is rejected");
    argv_empty[0] = L"jsrf_recomp.exe";
    argv_empty[1] = L"--save-root=";
    check(!jsrf_parse_launch_args_w(2, argv_empty, &options, &error) &&
          error == ERROR_INVALID_PARAMETER,
          "empty save-root value is rejected");
    argv_bare[0] = L"jsrf_recomp.exe";
    argv_bare[1] = L"--save-root";
    argv_bare[2] = selected;
    check(!jsrf_parse_launch_args_w(3, argv_bare, &options, &error) &&
          error == ERROR_INVALID_PARAMETER,
          "separate/missing save-root option value is rejected");
    check(!jsrf_preflight_save_root(L"relative-save-root", resolved, &error) &&
          error == ERROR_BAD_PATHNAME,
          "relative root is rejected before path initialization");

    check(join_path(missing_root, ARRAYSIZE(missing_root), parent, L"missing root") &&
          !jsrf_preflight_save_root(missing_root, resolved, &error) &&
          (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND),
          "nonexistent root is rejected before path initialization");
    check(join_path(file_root, ARRAYSIZE(file_root), parent, L"regular file") &&
          write_sentinel(file_root, sentinel_bytes, sizeof(sentinel_bytes)) &&
          !jsrf_preflight_save_root(file_root, resolved, &error) &&
          error == ERROR_DIRECTORY,
          "regular-file root is rejected before path initialization");

    swprintf_s(overlong, ARRAYSIZE(overlong), L"%s\\", parent);
    for (size_t i = wcslen(overlong); i < ARRAYSIZE(overlong) - 1; ++i)
        overlong[i] = L'a';
    overlong[ARRAYSIZE(overlong) - 1] = L'\0';
    check(!jsrf_preflight_save_root(overlong, resolved, &error) &&
          error == ERROR_FILENAME_EXCED_RANGE,
          "overlong root is rejected before toolkit MAX_PATH buffers");

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_root)) &&
        swprintf_s(default_root, ARRAYSIZE(default_root), L"%s\\xboxrecomp",
                   local_root) >= 0) {
        check(!jsrf_preflight_save_root(default_root, resolved, &error) &&
              error == ERROR_INVALID_NAME,
              "real toolkit profile root is rejected without initialization");
    }

    check(deny_current_user_file_creation(denied, &saved_sd),
          "install disposable access-denied control ACL");
    if (saved_sd) {
        BOOL accepted = jsrf_preflight_save_root(denied, resolved, &error);
        check(restore_directory_acl(denied, saved_sd),
              "restore disposable access-denied control ACL");
        saved_sd = NULL;
        check(!accepted && error == ERROR_ACCESS_DENIED,
              "actual write-denied root reports access denied");
    }

    check(sentinel_matches(sentinel_path, sentinel_bytes, sizeof(sentinel_bytes)),
          "invalid-root rejection leaves disposable sentinel bytes unchanged");

    swprintf_s(root_argument, ARRAYSIZE(root_argument), L"--save-root=%s", selected);
    argv_ok[0] = L"jsrf_recomp.exe";
    argv_ok[1] = root_argument;
    argv_ok[2] = L"--probe=healthy";
    launch_valid = jsrf_parse_launch_args_w(3, argv_ok, &options, &error) &&
                   options.has_save_root && options.has_probe &&
                   wcscmp(options.probe, L"healthy") == 0 &&
                   wcsstr(options.save_root, L"--probe=literal") != NULL;
    check(launch_valid,
          "save-root substring does not become probe detection");
    if (launch_valid) {
        launch_valid = jsrf_preflight_save_root(options.save_root, resolved, &error);
        check(launch_valid,
              "selected disposable root passes absolute/existing/writable preflight");
    }
    if (launch_valid) {
        launch_valid = jsrf_save_root_to_utf8(resolved, root_utf8,
                                             sizeof(root_utf8), &error);
        check(launch_valid, "validated root converts to toolkit UTF-8");
    }

    if (launch_valid) {
        xbox_path_init("game", root_utf8);
        for (int i = 0; i <= 5; ++i) {
            char guest_path[64];
            WCHAR expected[MAX_PATH];
            snprintf(guest_path, sizeof(guest_path),
                     "\\Device\\Harddisk0\\Partition%d", i);
            swprintf_s(expected, ARRAYSIZE(expected), L"%s\\Partition%d.img",
                       resolved, i);
            expect_translation(guest_path, expected);
            check(GetFileAttributesW(expected) != INVALID_FILE_ATTRIBUTES,
                  "toolkit created partition image under selected root");
        }
        {
            WCHAR expected[MAX_PATH];
            swprintf_s(expected, ARRAYSIZE(expected), L"%s\\TitleData\\slot.bin", resolved);
            expect_translation("T:\\slot.bin", expected);
            swprintf_s(expected, ARRAYSIZE(expected), L"%s\\UserData\\slot.bin", resolved);
            expect_translation("U:\\slot.bin", expected);
            swprintf_s(expected, ARRAYSIZE(expected), L"%s\\Cache\\slot.bin", resolved);
            expect_translation("Z:\\slot.bin", expected);
        }
        check(sentinel_matches(sentinel_path, sentinel_bytes, sizeof(sentinel_bytes)),
              "successful toolkit path initialization leaves separate sentinel unchanged");
    }

    test_command_quoting();
    if (saved_sd)
        check(restore_directory_acl(denied, saved_sd),
              "restore disposable access-denied control ACL during cleanup");
    check(remove_tree_contents(selected),
          "delete only the disposable toolkit partition fixture");
    check(delete_file_if_present(sentinel_path),
          "delete disposable sentinel file");
    check(delete_file_if_present(file_root),
          "delete disposable regular-file root control");
    check(remove_directory_if_present(sentinel_dir),
          "remove disposable sentinel directory");
    check(remove_directory_if_present(denied),
          "remove disposable denied-root directory");
    check(remove_directory_if_present(parent),
          "remove disposable fixture parent after its contents");

    if (failures) {
        fprintf(stderr, "FAIL: %d save-root contract checks\n", failures);
        return 1;
    }
    puts("PASS: save-root options, fail-closed preflight, real toolkit path mapping, sentinels and Windows argv quoting");
    return 0;
}
