#include <PFishHook.h>
#include <HookFunc.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "hash_map.h"
#include <sys/types.h>
#include <sys/xattr.h>

concurrent_hash_map_owned opendir_map;
using namespace FishHook;

static std::string get_fixed_path(const char *pathname)
{
    return std::string("/home/menooker/fish/") + pathname;
}

def_name(open, int, const char *, int, mode_t);
static int myopen(const char *pathname, int flags, mode_t mode)
{
    std::string mypath = get_fixed_path(pathname);
    int myret = CallOld<Name_open>(mypath.c_str(), flags, mode);
    if (myret > 0)
    {
        return myret;
    }
    return CallOld<Name_open>(pathname, flags, mode);
}

def_name(lgetxattr, ssize_t, const char *, const char *, void *, size_t);
static ssize_t mylgetxattr(const char *__path, const char *__name,
                           void *__value, size_t __size)
{
    std::string mypath = get_fixed_path(__path);
    ssize_t myret = CallOld<Name_lgetxattr>(mypath.c_str(), __name, __value, __size);
    if (myret > 0 || errno == ENODATA)
    {
        return myret;
    }
    return CallOld<Name_lgetxattr>(__path, __name, __value, __size);
}

def_name(getxattr, ssize_t, const char *, const char *, void *, size_t);
static ssize_t mygetxattr(const char *__path, const char *__name,
                          void *__value, size_t __size)
{
    std::string mypath = get_fixed_path(__path);
    ssize_t myret = CallOld<Name_getxattr>(mypath.c_str(), __name, __value, __size);
    if (myret > 0 || errno == ENODATA)
    {
        return myret;
    }
    return CallOld<Name_getxattr>(__path, __name, __value, __size);
}

def_name(__xstat, int, int, const char *, struct stat *);
static int myxstat(int ver, const char *pathname, struct stat *statbuf)
{
    std::string mypath = get_fixed_path(pathname);
    int myret = CallOld<Name___xstat>(ver, mypath.c_str(), statbuf);
    if (!myret)
    {
        return myret;
    }
    return CallOld<Name___xstat>(ver, pathname, statbuf);
}

def_name(__lxstat, int, int, const char *, struct stat *);
static int mylxstat(int ver, const char *pathname, struct stat *statbuf)
{
    std::string mypath = std::string("/home/menooker/fish/") + pathname;
    int myret = CallOld<Name___lxstat>(ver, mypath.c_str(), statbuf);
    if (!myret)
    {
        return myret;
    }
    return CallOld<Name___lxstat>(ver, pathname, statbuf);
}

def_name(opendir, DIR *, const char *);
def_name(closedir, int, DIR *);
struct dir_data : public entry_base
{
    std::string name;
    DIR *underlying = nullptr;
    dir_data(const char *name) : name(name) {}

    void release_dir()
    {
        if (underlying)
        {
            CallOld<Name_closedir>(underlying);
            underlying = nullptr;
        }
    }
    ~dir_data()
    {
        release_dir();
    }
};

static DIR *myopendir(const char *name)
{
    std::string mypath = get_fixed_path(name);
    DIR *ret = CallOld<Name_opendir>(mypath.c_str());
    auto data = std::make_unique<dir_data>(name);
    if (ret)
    {
        int olderr = errno;
        data->underlying = CallOld<Name_opendir>(name);
        errno = olderr;
    }
    else if (errno == ENOENT)
    {
        ret = CallOld<Name_opendir>(name);
    }
    if (ret)
        opendir_map.set(ret, std::move(data));
    return ret;
}

static int myclosedir(DIR *name)
{
    int ret = CallOld<Name_closedir>(name);
    opendir_map.remove(name);
    return ret;
}

def_name(readdir, dirent *, DIR *);
dirent *myreaddir(DIR *dirp)
{
    auto data = (dir_data *)opendir_map.get(dirp);
    if (data->underlying)
    {
        dirent *ret = CallOld<Name_readdir>(data->underlying);
        if (ret)
        {
            return ret;
        }
        data->release_dir();
    }
    return CallOld<Name_readdir>(dirp);
}

__attribute__((constructor)) static void HookMe()
{
    void *handle = dlopen("libpthread.so.0", RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        exit(1);
    }
    void *handlec = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        exit(1);
    }
    DoHookInLibAndLibC<Name_opendir>(handlec, handle, myopendir);
    DoHookInLibAndLibC<Name_closedir>(handlec, handle, myclosedir);
    DoHookInLibAndLibC<Name_readdir>(handlec, handle, myreaddir);
    DoHookInLibAndLibC<Name___xstat>(handlec, handle, myxstat);
    DoHookInLibAndLibC<Name___lxstat>(handlec, handle, mylxstat);
    DoHookInLibAndLibC<Name_getxattr>(handlec, handle, mygetxattr);
    DoHookInLibAndLibC<Name_lgetxattr>(handlec, handle, mylgetxattr);
    DoHookInLibAndLibC<Name_open>(handlec, handle, myopen);
}
