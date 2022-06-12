
#include "shadow_path.h"
#include "funcs.h"
using namespace FishHook;

static int myutimes(const char *filename, const struct timeval times[2])
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(filename, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_utimes>(mypath, times);
    }
    else
    {
        return CallOld<Name_utimes>(filename, times);
    }
}
rl_hook(utimes);

static int myutime(const char *filename, const void *v)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(filename, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_utime>(mypath, v);
    }
    else
    {
        return CallOld<Name_utime>(filename, v);
    }
}
rl_hook(utime);

static int myutimensat(int dirfd, const char *pathname,
                     const struct timespec times[2], int flags)
{
    fprintf(stderr, "Rootless: utimensat Not implemented\n");
    return 0;
}
rl_hook(utimensat);

static int myfutimens(int fd, const struct timespec times[2]) {
    fprintf(stderr, "Rootless: futimens Not implemented\n");
    return 0;
}
rl_hook(futimens);

static int mylutimes(const char *filename, const struct timeval tv[2]) {
    fprintf(stderr, "Rootless: lutimens Not implemented\n");
    return 0;
}
rl_hook(lutimes);