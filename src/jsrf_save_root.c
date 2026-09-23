#include "jsrf_save_root.h"

#include <limits.h>
#include <shlobj.h>
#include <string.h>
#include <wchar.h>

static void set_error(DWORD *error, DWORD value)
{
    if (error)
        *error = value;
}

static BOOL is_separator(WCHAR ch)
{
    return ch == L'\\' || ch == L'/';
}

static void normalize_separators(WCHAR *path)
{
    for (WCHAR *p = path; *p; ++p) {
        if (*p == L'/')
            *p = L'\\';
    }
}

static size_t trim_trailing_separators(WCHAR *path)
{
    size_t length = wcslen(path);
    while (length > 3 && is_separator(path[length - 1]))
        path[--length] = L'\0';
    return length;
}

static BOOL is_absolute_path(const WCHAR *path)
{
    const WCHAR *p;

    if (!path || !path[0])
        return FALSE;
    if (((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && is_separator(path[2]))
        return TRUE;

    /* Accept UNC paths, but not device namespaces whose semantics are not
     * compatible with the toolkit's fixed MAX_PATH path buffers. */
    if (path[0] != L'\\' || path[1] != L'\\' ||
        path[2] == L'?' || path[2] == L'.')
        return FALSE;
    p = path + 2;
    if (!*p || is_separator(*p))
        return FALSE;
    while (*p && !is_separator(*p))
        ++p;
    if (!*p)
        return FALSE;
    while (is_separator(*p))
        ++p;
    if (!*p)
        return FALSE;
    while (*p && !is_separator(*p))
        ++p;
    return TRUE;
}

static BOOL path_is_prefix(const WCHAR *prefix, const WCHAR *path)
{
    size_t prefix_length = wcslen(prefix);
    if (_wcsnicmp(prefix, path, prefix_length) != 0)
        return FALSE;
    return path[prefix_length] == L'\0' || is_separator(path[prefix_length]);
}

static DWORD default_profile_overlap(const WCHAR *path, BOOL *overlaps)
{
    WCHAR local[MAX_PATH];
    WCHAR profile[MAX_PATH];
    WCHAR full[MAX_PATH];
    DWORD length;
    HRESULT result;

    result = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local);
    if (FAILED(result)) {
        *overlaps = FALSE;
        return HRESULT_FACILITY(result) == FACILITY_WIN32 ?
               HRESULT_CODE(result) : ERROR_NOT_READY;
    }
    if (swprintf_s(profile, MAX_PATH, L"%s\\xboxrecomp", local) < 0)
        return ERROR_FILENAME_EXCED_RANGE;
    length = GetFullPathNameW(profile, MAX_PATH, full, NULL);
    if (!length || length >= MAX_PATH)
        return length ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
    normalize_separators(full);
    trim_trailing_separators(full);

    /* Never permit a selected root to be the toolkit's real profile root,
     * live below it, or become its parent and receive new disk images. */
    *overlaps = path_is_prefix(path, full) || path_is_prefix(full, path);
    return ERROR_SUCCESS;
}

static BOOL strip_final_path_prefix(WCHAR *path)
{
    if (_wcsnicmp(path, L"\\\\?\\UNC\\", 8) == 0) {
        size_t rest = wcslen(path + 8);
        if (rest + 3 >= MAX_PATH)
            return FALSE;
        memmove(path + 2, path + 8, (rest + 1) * sizeof(WCHAR));
        path[0] = L'\\';
        path[1] = L'\\';
        return TRUE;
    }
    if (_wcsnicmp(path, L"\\\\?\\", 4) == 0) {
        memmove(path, path + 4, (wcslen(path + 4) + 1) * sizeof(WCHAR));
        return TRUE;
    }
    return FALSE;
}

BOOL jsrf_parse_launch_args_w(int argc, wchar_t *const argv[],
                              JsrfLaunchOptions *options, DWORD *error)
{
    int i;

    if (!options || argc < 0 || (argc && !argv)) {
        set_error(error, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    memset(options, 0, sizeof(*options));
    for (i = 1; i < argc; ++i) {
        const WCHAR *argument = argv[i];
        static const WCHAR save_prefix[] = L"--save-root=";
        static const WCHAR probe_prefix[] = L"--probe=";

        if (!argument) {
            set_error(error, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        if (wcsncmp(argument, save_prefix, ARRAYSIZE(save_prefix) - 1) == 0) {
            const WCHAR *value = argument + ARRAYSIZE(save_prefix) - 1;
            if (options->has_save_root || !value[0] ||
                wcslen(value) >= ARRAYSIZE(options->save_root)) {
                set_error(error, ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            wcscpy_s(options->save_root, ARRAYSIZE(options->save_root), value);
            options->has_save_root = TRUE;
        } else if (wcsncmp(argument, probe_prefix,
                           ARRAYSIZE(probe_prefix) - 1) == 0) {
            const WCHAR *value = argument + ARRAYSIZE(probe_prefix) - 1;
            if (options->has_probe || !value[0] ||
                wcslen(value) >= ARRAYSIZE(options->probe)) {
                set_error(error, ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            wcscpy_s(options->probe, ARRAYSIZE(options->probe), value);
            options->has_probe = TRUE;
        } else {
            set_error(error, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }
    if (!options->has_save_root) {
        set_error(error, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    set_error(error, ERROR_SUCCESS);
    return TRUE;
}

BOOL jsrf_preflight_save_root(const WCHAR *requested,
                              WCHAR resolved[MAX_PATH], DWORD *error)
{
    WCHAR canonical[MAX_PATH];
    WCHAR final_path[MAX_PATH];
    WCHAR volume[MAX_PATH];
    WCHAR probe[MAX_PATH];
    HANDLE directory = INVALID_HANDLE_VALUE;
    HANDLE test_file = INVALID_HANDLE_VALUE;
    DWORD attributes;
    DWORD length;
    DWORD status = ERROR_INVALID_PARAMETER;
    size_t root_length;
    BOOL ok = FALSE;
    BOOL profile_overlap;

    probe[0] = L'\0';

    if (!requested || !requested[0] || !resolved || !is_absolute_path(requested)) {
        status = ERROR_BAD_PATHNAME;
        goto done;
    }
    length = GetFullPathNameW(requested, MAX_PATH, canonical, NULL);
    if (!length || length >= MAX_PATH) {
        status = length ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
        goto done;
    }
    normalize_separators(canonical);
    root_length = trim_trailing_separators(canonical);

    /* The toolkit appends fixed names into MAX_PATH buffers during init. */
    if (root_length + wcslen(L"\\Partition5.img") >= MAX_PATH) {
        status = ERROR_FILENAME_EXCED_RANGE;
        goto done;
    }
    status = default_profile_overlap(canonical, &profile_overlap);
    if (status != ERROR_SUCCESS)
        goto done;
    if (profile_overlap) {
        status = ERROR_INVALID_NAME;
        goto done;
    }

    attributes = GetFileAttributesW(canonical);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        status = GetLastError();
        goto done;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        status = ERROR_DIRECTORY;
        goto done;
    }
    if (!GetVolumePathNameW(canonical, volume, ARRAYSIZE(volume))) {
        status = GetLastError();
        goto done;
    }
    normalize_separators(volume);
    trim_trailing_separators(volume);
    if (_wcsicmp(canonical, volume) == 0) {
        status = ERROR_INVALID_NAME;
        goto done;
    }

    directory = CreateFileW(canonical, FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (directory == INVALID_HANDLE_VALUE) {
        status = GetLastError();
        goto done;
    }
    length = GetFinalPathNameByHandleW(directory, final_path, ARRAYSIZE(final_path),
                                       FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!length || length >= ARRAYSIZE(final_path)) {
        status = length ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
        goto done;
    }
    if (strip_final_path_prefix(final_path)) {
        normalize_separators(final_path);
        trim_trailing_separators(final_path);
    }
    root_length = wcslen(final_path);
    if (root_length + wcslen(L"\\Partition5.img") >= MAX_PATH) {
        status = ERROR_FILENAME_EXCED_RANGE;
        goto done;
    }
    status = default_profile_overlap(final_path, &profile_overlap);
    if (status != ERROR_SUCCESS)
        goto done;
    if (profile_overlap) {
        status = ERROR_INVALID_NAME;
        goto done;
    }
    /* A junction or symbolic link could redirect a seemingly disposable path
     * into another directory. Require the selected and resolved names to
     * identify the same path before any toolkit write occurs. */
    if (_wcsicmp(canonical, final_path) != 0) {
        status = ERROR_CANT_RESOLVE_FILENAME;
        goto done;
    }

    if (!GetTempFileNameW(final_path, L"jrf", 0, probe)) {
        status = GetLastError();
        goto done;
    }
    test_file = CreateFileW(probe, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                            NULL);
    if (test_file == INVALID_HANDLE_VALUE) {
        status = GetLastError();
        DeleteFileW(probe);
        goto done;
    }
    {
        static const BYTE sentinel = 0xA5;
        DWORD written = 0;
        if (!WriteFile(test_file, &sentinel, sizeof(sentinel), &written, NULL)) {
            status = GetLastError();
            goto done;
        }
        if (written != sizeof(sentinel)) {
            status = ERROR_WRITE_FAULT;
            goto done;
        }
        if (!FlushFileBuffers(test_file)) {
            status = GetLastError();
            goto done;
        }
    }
    CloseHandle(test_file);
    test_file = INVALID_HANDLE_VALUE;
    if (GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES) {
        status = ERROR_ACCESS_DENIED;
        goto done;
    } else {
        DWORD delete_status = GetLastError();
        if (delete_status != ERROR_FILE_NOT_FOUND &&
            delete_status != ERROR_PATH_NOT_FOUND) {
            status = delete_status;
            goto done;
        }
    }
    status = ERROR_SUCCESS;
    wcscpy_s(resolved, MAX_PATH, final_path);
    ok = TRUE;

done:
    if (test_file != INVALID_HANDLE_VALUE)
        CloseHandle(test_file);
    if (directory != INVALID_HANDLE_VALUE)
        CloseHandle(directory);
    if (!ok && probe[0])
        DeleteFileW(probe);
    set_error(error, status);
    return ok;
}

BOOL jsrf_save_root_to_utf8(const WCHAR *root, char *out, size_t out_size,
                            DWORD *error)
{
    int written;
    if (!root || !out || !out_size || out_size > INT_MAX) {
        set_error(error, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, root, -1,
                                  out, (int)out_size, NULL, NULL);
    if (!written) {
        set_error(error, GetLastError());
        return FALSE;
    }
    set_error(error, ERROR_SUCCESS);
    return TRUE;
}

BOOL jsrf_append_windows_arg(char *command, size_t command_size,
                             size_t *used, const char *argument)
{
    size_t out;
    const unsigned char *p;

    if (!command || !command_size || !used || !argument || *used >= command_size)
        return FALSE;
    out = *used;
    if (out && command[out - 1] != ' ') {
        if (out + 1 >= command_size)
            return FALSE;
        command[out++] = ' ';
    }
    if (out + 1 >= command_size)
        return FALSE;
    command[out++] = '"';
    p = (const unsigned char *)argument;
    while (*p) {
        size_t slashes = 0;
        while (*p == '\\') {
            ++slashes;
            ++p;
        }
        if (*p == '"') {
            size_t i;
            for (i = 0; i < slashes * 2 + 1; ++i) {
                if (out + 1 >= command_size)
                    return FALSE;
                command[out++] = '\\';
            }
            if (out + 1 >= command_size)
                return FALSE;
            command[out++] = '"';
            ++p;
        } else if (*p == '\0') {
            size_t i;
            for (i = 0; i < slashes * 2; ++i) {
                if (out + 1 >= command_size)
                    return FALSE;
                command[out++] = '\\';
            }
        } else {
            size_t i;
            for (i = 0; i < slashes; ++i) {
                if (out + 1 >= command_size)
                    return FALSE;
                command[out++] = '\\';
            }
            if (out + 1 >= command_size)
                return FALSE;
            command[out++] = (char)*p++;
        }
    }
    if (out + 2 > command_size)
        return FALSE;
    command[out++] = '"';
    command[out] = '\0';
    *used = out;
    return TRUE;
}

BOOL jsrf_argv_has_probe(int argc, char *const argv[])
{
    int i;
    static const char prefix[] = "--probe=";
    if (argc <= 1 || !argv)
        return FALSE;
    /* Collector argv[1..3] are seconds, output directory and executable. */
    for (i = 4; i < argc; ++i) {
        if (argv[i] && strncmp(argv[i], prefix, sizeof(prefix) - 1) == 0 &&
            argv[i][sizeof(prefix) - 1] != '\0')
            return TRUE;
    }
    return FALSE;
}
