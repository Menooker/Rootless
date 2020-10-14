#include "shadow_path.h"
#include "dir.h"
#include <unistd.h>
#include <string.h>
using namespace FishHook;

def_name(rename, int, const char *, const char *);
static int myrename(const char *oldpath, const char *newpath)
{
    char mypath[PATH_MAX];
    char mynewpath[PATH_MAX];
    auto status = get_fixed_path(oldpath, mypath);
    auto statusnew = get_fixed_path(newpath, mynewpath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else
    {
        bool success = makeParentPath(mynewpath);
        assert(success);
        int ret;
        int errnoback;
        if (status == ShadowFileState::Exists)
        {
            ret = CallOld<Name_rename>(mypath, mynewpath);
            errnoback = errno;
        }
        else
        {
            bool success = makeParentPath(mypath);
            assert(success);
            if (OSCopyFile(oldpath, mynewpath) >= 0)
            {
                ret = 0;
            }
            else
            {
                ret = -1;
            }
            errnoback = errno;
        }

        mark_del(mypath);
        if (ret == 0 && statusnew == ShadowFileState::Deleted)
        {
            undo_del(mynewpath);
        }
        errno = errnoback;
        return ret;
    }
}


def_name(execve, int, const char *, char **, char **);
static int myexecve(const char *pathname, char **argv, char **envp)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(pathname, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_execve>(mypath, argv, envp);
    }
    else
    {
        return CallOld<Name_execve>(pathname, argv, envp);
    }
}

def_name(open, int, const char *, int, mode_t);
static int myopen(const char *pathname, int flags, mode_t mode)
{
    if (!strncmp(pathname, "/dev", 4) || !strncmp(pathname, "/tmp", 4))
    {
        return CallOld<Name_open>(pathname, flags, mode);
    }
    char mypath[PATH_MAX];
    auto status = get_fixed_path(pathname, mypath);
    if (flags & O_CREAT)
    {
        if (status == ShadowFileState::Deleted)
        {
            undo_del(mypath);
            return CallOld<Name_open>(mypath, flags, mode);
        }
        else if (status == ShadowFileState::Exists)
        {
            return CallOld<Name_open>(mypath, flags, mode);
        }
        else
        {
            if (CallOld<Name_access>(pathname, F_OK) == 0)
            {
                // if file exists in underlying FS, open as usual
                if (flags & O_WRONLY || flags & O_RDWR)
                {
                    if (OSCopyFile(pathname, mypath) < 0)
                    {
                        fprintf(stderr, "Rootless Error: Cannot copy file for write: %s\n", pathname);
                    }
                    return CallOld<Name_open>(mypath, flags, mode);
                }
                else
                {
                    return CallOld<Name_open>(pathname, flags, mode);
                }
            }
            else
            {
                makeParentPath(mypath);
                // not exist, create in mypath
                return CallOld<Name_open>(mypath, flags, mode);
            }
        }
    }
    else
    {
        // if is not create
        if (status == ShadowFileState::Deleted)
        {
            errno = ENOENT;
            return -1;
        }
        else if (status == ShadowFileState::Exists)
        {
            return CallOld<Name_open>(mypath, flags, mode);
        }
        else
        {
            if (flags & O_WRONLY || flags & O_RDWR)
            {
                if (OSCopyFile(pathname, mypath) < 0)
                {
                    fprintf(stderr, "Rootless Error: Cannot copy file for write: %s\n", pathname);
                }
                return CallOld<Name_open>(mypath, flags, mode);
            }
            else
            {
                return CallOld<Name_open>(pathname, flags, mode);
            }
        }
    }
}

def_name(openat, int, int, const char *, int, mode_t);
static int myopenat(int dirp, const char *pathname, int flags, mode_t mode)
{
    if (dirp == AT_FDCWD)
    {
        return myopen(pathname, flags, mode);
    }
    std::string mypath = get_fd_path(dirp);
    if (mypath.back() != '/')
    {
        mypath.push_back('/');
    }
    mypath += pathname;
    return myopen(mypath.c_str(), flags, mode);
}

