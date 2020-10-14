
#include "shadow_path.h"
#include "dir.h"
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