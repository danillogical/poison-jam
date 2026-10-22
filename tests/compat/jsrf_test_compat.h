/* Force-included into every game target on non-Windows hosts: the MSVC CRT spellings the
 * portable tests use, mapped onto POSIX.  Windows never sees this header. */
#ifndef JSRF_TEST_COMPAT_H
#define JSRF_TEST_COMPAT_H

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* _putenv_s: an empty value removes the variable, as the MSVC CRT does. */
static inline int _putenv_s(const char *name, const char *value)
{
    if (!name || !*name) return EINVAL;
    if (!value || !*value) return unsetenv(name);
    return setenv(name, value, 1);
}

/* _spawnv(_P_WAIT, ...): run a program to completion and return its exit status. */
#define _P_WAIT 0
static inline int _spawnv(int mode, const char *path, const char *const *argv)
{
    pid_t pid;
    int status;
    (void)mode;
    fflush(NULL);
    pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(path, (char *const *)argv);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* GetModuleFileNameA(NULL, ...): the running executable's path; 0 on failure. */
static inline uint32_t GetModuleFileNameA(void *module, char *buf, uint32_t size)
{
    (void)module;
#ifdef __APPLE__
    uint32_t n = size;
    if (_NSGetExecutablePath(buf, &n) != 0) return 0;
    return (uint32_t)strlen(buf);
#else
    ssize_t n = readlink("/proc/self/exe", buf, size ? size - 1 : 0);
    if (n < 0) return 0;
    buf[n] = '\0';
    return (uint32_t)n;
#endif
}

#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _write write
#define _strtoui64 strtoull
#define _stricmp strcasecmp
#define _strdup strdup
#define _dup dup
/* _dup2 returns 0 on success (POSIX dup2 returns the new descriptor). */
static inline int _dup2(int from, int to) { return dup2(from, to) < 0 ? -1 : 0; }
#define _close close
#define _getpid getpid
#define _open open
#define _unlink unlink
#define _access access
#define _chdir chdir
#define _mkdir(p) mkdir((p), 0777)
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_WRONLY O_WRONLY
#define _O_RDONLY O_RDONLY
#define _O_BINARY 0
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR

#endif /* !_WIN32 */
#endif /* JSRF_TEST_COMPAT_H */
