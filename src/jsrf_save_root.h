#ifndef JSRF_SAVE_ROOT_H
#define JSRF_SAVE_ROOT_H

#include <windows.h>
#include <stddef.h>

#define JSRF_SAVE_ROOT_UTF8_CAP (MAX_PATH * 4)
#define JSRF_PROBE_NAME_CAP 128

typedef struct JsrfLaunchOptions {
    WCHAR save_root[MAX_PATH];
    WCHAR probe[JSRF_PROBE_NAME_CAP];
    BOOL has_save_root;
    BOOL has_probe;
} JsrfLaunchOptions;

/* Parse only exact supported argv tokens. The caller owns the argv array. */
BOOL jsrf_parse_launch_args_w(int argc, wchar_t *const argv[],
                              JsrfLaunchOptions *options, DWORD *error);

/* Validate and canonicalize an existing, writable, absolute disposable root.
 * This function never calls the toolkit path layer. */
BOOL jsrf_preflight_save_root(const wchar_t *requested,
                              wchar_t resolved[MAX_PATH], DWORD *error);

BOOL jsrf_save_root_to_utf8(const wchar_t *root, char *out, size_t out_size,
                            DWORD *error);

/* Append one CRT argv value using the Windows CreateProcess quoting rules. */
BOOL jsrf_append_windows_arg(char *command, size_t command_size,
                             size_t *used, const char *argument);

/* Probe mode is enabled only by an exact, nonempty --probe=<name> token. */
BOOL jsrf_argv_has_probe(int argc, char *const argv[]);

#endif
