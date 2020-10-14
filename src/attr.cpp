#include "shadow_path.h"
#include "dir.h"
using namespace FishHook;
int myaccess(const char *name, int type)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_access>(mypath, type);
    }
    else
    {
        return CallOld<Name_access>(name, type);
    }
}

static ssize_t mylgetxattr(const char *__path, const char *__name,
                           void *__value, size_t __size)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(__path, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_lgetxattr>(mypath, __name, __value, __size);
    }
    else
    {
        return CallOld<Name_lgetxattr>(__path, __name, __value, __size);
    }
}

static ssize_t mygetxattr(const char *__path, const char *__name,
                          void *__value, size_t __size)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(__path, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name_getxattr>(mypath, __name, __value, __size);
    }
    else
    {
        return CallOld<Name_getxattr>(__path, __name, __value, __size);
    }
}

static int myxstat(int ver, const char *pathname, struct stat *statbuf)
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
        return CallOld<Name___xstat>(ver, mypath, statbuf);
    }
    else
    {
        return CallOld<Name___xstat>(ver, pathname, statbuf);
    }
}

static int myfxstatat(int ver, int dirp, const char *filename,
                      struct stat *stat_buf, int flag)
{
    if (dirp != AT_FDCWD && filename[0] != '/')
    {
        //if is relative
        std::string mypath = get_fd_path(dirp);
        if (mypath.back() != '/')
        {
            mypath.push_back('/');
        }
        mypath += filename;
        return myfxstatat(ver, AT_FDCWD, mypath.c_str(), stat_buf, flag);
    }
    char mypath[PATH_MAX];
    auto status = get_fixed_path(filename, mypath);
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        return CallOld<Name___fxstatat>(ver, AT_FDCWD, mypath, stat_buf, flag);
    }
    else
    {
        return CallOld<Name___fxstatat>(ver, dirp, filename, stat_buf, flag);
    }
}

static int mylxstat(int ver, const char *pathname, struct stat *statbuf)
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
        return CallOld<Name___lxstat>(ver, mypath, statbuf);
    }
    else
    {
        return CallOld<Name___lxstat>(ver, pathname, statbuf);
    }
}


int mychown(const char *name, __uid_t __owner, __gid_t __group)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    const char *thepath = name;
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return -1;
    }
    else if (status == ShadowFileState::Exists)
    {
        thepath = mypath;
    }
    auto ret = CallOld<Name_chown>(thepath, __owner, __group);
    if (ret < 0)
    {
        perror("chown error ignored: ");
        return 0;
    }
    return ret;
}

int myfchown(int __fd, __uid_t __owner, __gid_t __group)
{
    int ret = CallOld<Name_fchown>(__fd, __owner, __group);
    if (ret < 0)
    {
        perror("fchown error ignored: ");
        return 0;
    }
    return ret;
}

int myfchownat(int __fd, const char *__file, __uid_t __owner,
               __gid_t __group, int __flag)
{
    int ret = CallOld<Name_fchownat>(__fd, __file, __owner, __group, __flag);
    if (ret < 0)
    {
        perror("fchownat error ignored: ");
        return 0;
    }
    return ret;
}

static int mychmod(const char *pathname, mode_t mode)
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
        return CallOld<Name_chmod>(mypath, mode);
    }
    else
    {
        if (OSCopyFile(pathname, mypath) >= 0)
        {
            return CallOld<Name_chmod>(mypath, mode);
        }
        return -1;
    }
}

static int myfchmodat(int dirp, const char *pathname, mode_t mode)
{
    if (dirp == AT_FDCWD)
    {
        return mychmod(pathname, mode);
    }
    std::string mypath = get_fd_path(dirp);
    if (mypath.back() != '/')
    {
        mypath.push_back('/');
    }
    mypath += pathname;
    return mychmod(mypath.c_str(), mode);
}