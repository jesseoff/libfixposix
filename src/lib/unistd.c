/*******************************************************************************/
/* Permission is hereby granted, free of charge, to any person or organization */
/* obtaining a copy of the software and accompanying documentation covered by  */
/* this license (the "Software") to use, reproduce, display, distribute,       */
/* execute, and transmit the Software, and to prepare derivative works of the  */
/* Software, and to permit third-parties to whom the Software is furnished to  */
/* do so, all subject to the following:                                        */
/*                                                                             */
/* The copyright notices in the Software and this entire statement, including  */
/* the above license grant, this restriction and the following disclaimer,     */
/* must be included in all copies of the Software, in whole or in part, and    */
/* all derivative works of the Software, unless such copies or derivative      */
/* works are solely in the form of machine-executable object code generated by */
/* a source language processor.                                                */
/*                                                                             */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    */
/* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT   */
/* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE   */
/* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, */
/* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER */
/* DEALINGS IN THE SOFTWARE.                                                   */
/*******************************************************************************/

#include <lfp/unistd.h>
#include <lfp/stdlib.h>
#include <lfp/string.h>
#include <lfp/errno.h>
#include <lfp/fcntl.h>

#if !defined(HAVE_GETPEEREID)
# if defined(HAVE_UCRED_H)
#  include <ucred.h>
# else
#  include <sys/socket.h>
# endif
#endif

#if defined(__APPLE__)
# include <crt_externs.h>
#else
extern char** environ;
#endif

#include <limits.h>
#include <stdio.h>

DSO_PUBLIC char**
lfp_get_environ(void)
{
#if defined(__APPLE__)
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

DSO_PUBLIC int
lfp_set_environ(char **newenv)
{
    if (lfp_clearenv() < 0) {
        return -1;
    } else if (newenv != NULL) {
        for(char **var = newenv; *var != NULL; var++) {
            putenv(*var);
        }
    }
    return 0;
}

#if !defined(HAVE_CLEARENV)
static void
_lfp_reset_environ(void)
{
# if defined(__APPLE__)
    char ***envptr = _NSGetEnviron();
    *envptr = NULL;
# else
    environ = NULL;
# endif
}
#endif

DSO_PUBLIC int
lfp_clearenv(void)
{
#if defined(HAVE_CLEARENV)
    return clearenv();
#else
    char **env = lfp_get_environ();
    if (env == NULL) return 0;

    for(char **var = env; *var != NULL; var++) {
        char *tmp = strdup(*var);
        char *eql = strchr(tmp, '=');
        if (eql == NULL) {
            free(tmp);
            return -1;
        } else {
            eql = '\0';
            unsetenv(eql);
            free(tmp);
        }
    }

    _lfp_reset_environ();
    return 0;
#endif
}


DSO_PUBLIC off_t
lfp_lseek(int fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}



DSO_PUBLIC int
lfp_pipe (int pipefd[2], uint64_t flags)
{
#if defined(HAVE_PIPE2)
    // We assume that if pipe2() is defined then O_CLOEXEC too
    // exists, which means that it's in the lower part of "flags"
    return pipe2(pipefd, (int)flags & 0xFFFFFFFF);
#else
    if (pipe(pipefd) < 0) {
        goto error_return;
    }

    if ((flags & O_CLOEXEC) &&
        (lfp_set_fd_cloexec(pipefd[0], true) < 0 ||
         lfp_set_fd_cloexec(pipefd[1], true) < 0)) {
        goto error_close;
    }
    if ((flags & O_NONBLOCK) &&
        (lfp_set_fd_nonblock(pipefd[0], true) < 0 ||
         lfp_set_fd_nonblock(pipefd[1], true) < 0)) {
        goto error_close;
    }

    return 0;

  error_close:
    close(pipefd[0]);
    close(pipefd[1]);
  error_return:
    return -1;
#endif // HAVE_PIPE2
}



DSO_PUBLIC ssize_t
lfp_pread(int fd, void *buf, size_t count, off_t offset)
{
    return pread(fd, buf, count, offset);
}

DSO_PUBLIC ssize_t
lfp_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    return pwrite(fd, buf, count, offset);
}



DSO_PUBLIC int
lfp_truncate(const char *path, off_t length)
{
    return truncate(path, length);
}

DSO_PUBLIC int
lfp_ftruncate(int fd, off_t length)
{
    return ftruncate(fd, length);
}



DSO_PUBLIC int
lfp_execve(const char *path, char *const argv[], char *const envp[])
{
    SYSCHECK(EINVAL, path == NULL);
    SYSCHECK(ENOENT, path[0] == '\0');

    return execve(path, argv, envp);
}

DSO_PUBLIC int
lfp_execvpe(const char *file, char *const argv[], char *const envp[])
{
    SYSCHECK(EINVAL, file == NULL);
    SYSCHECK(ENOENT, file[0] == '\0');

    if (strchr(file, '/'))
        return execve(file, argv, envp);

    size_t filelen = strlen(file);

    char path[PATH_MAX];
    char *searchpath=NULL, *tmpath=NULL, *saveptr=NULL, *bindir=NULL;

    tmpath = searchpath = lfp_getpath(envp);

    while ((bindir = strtok_r(tmpath, ":", &saveptr)) != NULL) {
        tmpath = NULL;
        if ( bindir[0] != '\0' ) {
            size_t dirlen = lfp_strnlen(bindir, PATH_MAX);
            // directory + '/' + file
            size_t pathlen = dirlen + 1 + filelen;
            // if pathlen == PATH_MAX there's no room for the final \0
            SYSCHECK(ENAMETOOLONG, pathlen >= PATH_MAX);
            snprintf(path, PATH_MAX, "%s/%s", bindir, file);
            path[pathlen] = '\0';
            execve(path, argv, envp);
            if ( errno == E2BIG  || errno == ENOEXEC ||
                 errno == ENOMEM || errno == ETXTBSY )
                break;
        }
    }

    free(searchpath);

    return -1;
}
