#include "shadow_path.h"
#include "dir.h"
using namespace FishHook;


static int myunlink(const char *name)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    int olderrno;
    int ret;
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        ret = CallOld<Name_unlink>(mypath);
        olderrno = errno;
        if (ret == 0)
            mark_del(mypath);
        errno = olderrno;
        return ret;
    }
    else
    {
        if (CallOld<Name_access>(name, F_OK) == 0)
        {
            mark_del(mypath);
            errno = 0;
            return 0;
        }
        errno = ENOENT;
        return -1;
    }
}

static int myunlinkat(int dirp, const char *name, int flag)
{
    if (dirp != AT_FDCWD && name[0] != '/')
    {
        //if is relative
        std::string mypath = get_fd_path(dirp);
        if (mypath.back() != '/')
        {
            mypath.push_back('/');
        }
        mypath += name;
        return myunlinkat(AT_FDCWD, mypath.c_str(), flag);
    }

    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    int olderrno;
    int ret;
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        ret = CallOld<Name_unlinkat>(dirp, mypath, flag);
        olderrno = errno;
        if (ret != 0 && olderrno == ENOTEMPTY)
        {
            if (prepareDirIfIsEmpty(mypath))
            {
                ret = CallOld<Name_unlinkat>(dirp, mypath, flag);
                olderrno = errno;
            }
        }
        if (ret == 0)
            mark_del(mypath);
        errno = olderrno;
        return ret;
    }
    else
    {
        if (CallOld<Name_access>(name, F_OK) == 0)
        {
            mark_del(mypath);
            errno = 0;
            return 0;
        }
        errno = ENOENT;
        return -1;
    }
}

int mylink(const char *from, const char *to)
{
    char from_path[PATH_MAX];
    char to_path[PATH_MAX];
    auto from_status = get_fixed_path(from, from_path);
    auto to_status = get_fixed_path(to, to_path);
    if (from_status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else
    {
        bool success = makeParentPath(to_path);
        assert(success);
        int ret;
        int errnoback;
        if (from_status == ShadowFileState::Exists)
        {
            ret = CallOld<Name_link>(from_path, to_path);
            errnoback = errno;
        }
        else
        {
            bool success = makeParentPath(from_path);
            assert(success);
            if (OSCopyFile(from, from_path) >= 0)
            {
                ret = CallOld<Name_link>(from_path, to_path);
                errnoback = errno;
            }
            else
            {
                return -1;
            }
        }
        if (ret == 0 && to_status == ShadowFileState::Deleted)
        {
            undo_del(to_path);
        }
        errno = errnoback;
        return ret;
    }
}