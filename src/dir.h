#include <HookFunc.h>
#include <unistd.h>
struct DIR;
struct timeval;
struct stat;

namespace FishHook
{
    def_name(chdir, int, const char *);
    def_name(mkdir, int, const char *, mode_t);
    def_name(opendir, DIR *, const char *);
    def_name(closedir, int, DIR *);
    def_name(access, int, const char *, int);
    def_name(utimes, int, const char *, const struct timeval *);
    def_name(utime, int, const char *, const void *);
    def_name(unlinkat, int, int, const char *, int);
    def_name(unlink, int, const char *);
    def_name(rmdir, int, const char *);
    def_name(lgetxattr, ssize_t, const char *, const char *, void *, size_t);
    def_name(getxattr, ssize_t, const char *, const char *, void *, size_t);
    def_name(__xstat, int, int, const char *, struct stat *);
    def_name(__fxstatat, int, int, int, const char *, struct stat *, int);
    def_name(__lxstat, int, int, const char *, struct stat *);
    def_name(link, int, const char *, const char *);
    def_name(chown, int, const char *, __uid_t, __gid_t);
    def_name(fchown, int, int, __uid_t, __gid_t);
    def_name(fchownat, int, int, const char *, __uid_t, __gid_t, int);
    def_name(chmod, int, const char *, mode_t);
    def_name(fchmodat, int, int, const char *, mode_t);
    def_name(setgroups, int, size_t, const gid_t *);
    def_name(getgroups, int, size_t, gid_t *);
    def_name(setegid, int, gid_t);
    def_name(setgid, int, gid_t);
    def_name_no_arg(getgid, gid_t);
    def_name_no_arg(getegid, gid_t);
    def_name(setresgid, int, gid_t, gid_t, gid_t);
    def_name(getresgid, int, gid_t *, gid_t *, gid_t *);
    def_name(setresuid, int, uid_t, uid_t, uid_t);
    def_name(getresuid, int, uid_t *, uid_t *, uid_t *);
    def_name(seteuid, int, uid_t);
    def_name_no_arg(geteuid, uid_t);
    def_name(setuid, int, uid_t);
    def_name_no_arg(getuid, uid_t);
} // namespace FishHook