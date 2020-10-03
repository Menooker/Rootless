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
#include <fcntl.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <libgen.h>
#include <grp.h>

bool isDirExist(const std::string &path);

static bool domakePath(const std::string &path)
{
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
    if (ret == 0)
        return true;

    switch (errno)
    {
    case ENOENT:
        // parent didn't exist, try to create it
        {
            int pos = path.find_last_of('/');
            if (pos == std::string::npos)
                return false;
            if (!domakePath(path.substr(0, pos)))
                return false;
        }
        // now, try to create again
        return 0 == mkdir(path.c_str(), mode);
    case EEXIST:
        // done!
        return isDirExist(path);
    default:
        return false;
    }
}

bool makePath(const char *path)
{
    return domakePath(path);
}

bool makeParentPath(const char *path)
{
    std::string path2 = path;
    char *dir = dirname((char *)path2.c_str());
    return domakePath(dir);
}

int OSCopyFile(const char *source, const char *destination);

concurrent_hash_map_owned opendir_map;
using namespace FishHook;

enum class ShadowFileState
{
    NotExists,
    Deleted,
    Exists,
};

static char path_prefix[] = "/home/menooker/fish/";
static const int path_prefix_len = sizeof(path_prefix) - 1;
static char del_path_postfix[] = ".del_file";
static const int del_path_postfix_len = sizeof(del_path_postfix) - 1;
static ShadowFileState get_fixed_path(const char *pathname, char *outpath)
{
    char resolved_path[PATH_MAX];
    auto len1 = strlen(pathname);
    if (len1 > 0 && pathname[len1 - 1] == '/')
    {
        len1--;
    }
    memcpy(outpath, path_prefix, path_prefix_len);
    size_t currentlen = path_prefix_len;
    // resolve relative
    if (pathname[0] != '/')
    {
        char *ret = getcwd(outpath + currentlen, PATH_MAX - currentlen);
        assert(ret);
        currentlen += strlen(ret);
    }
    if (len1 + currentlen + del_path_postfix_len >= PATH_MAX)
    {
        fprintf(stderr, "Path length overflow\n");
        std::abort();
    }
    memcpy(outpath + currentlen, pathname, len1);
    currentlen += len1;
    memcpy(outpath + currentlen, del_path_postfix, del_path_postfix_len + 1);
    if (access(outpath, F_OK) == 0)
    {
        return ShadowFileState::Deleted;
    }
    outpath[currentlen] = 0;

    if (access(outpath, F_OK) == 0)
    {
        return ShadowFileState::Exists;
    }
    return ShadowFileState::NotExists;
}

static std::string get_fd_path(int fd)
{
    char filePath[PATH_MAX];
    auto fdpath = std::string("/proc/self/fd/") + std::to_string(fd);
    if (readlink(fdpath.c_str(), filePath, PATH_MAX))
    {
        return filePath;
    }
    std::abort();
    return "";
}

void mark_del(const char *path)
{
    std::string ret = path;
    ret += ".del_file";
    creat(ret.c_str(), 0660);
}

def_name_no_arg(geteuid, uid_t);
static uid_t mygeteuid()
{
    return 0;
}

def_name(setuid,int, uid_t);
static int mysetuid(uid_t)
{
    return 0;
}

def_name_no_arg(getuid, uid_t);
static uid_t mygetuid()
{
    return 0;
}

def_name(setgroups, int, size_t, const gid_t *);
static int mysetgroups(size_t size, const gid_t *list)
{
    return 0;
}

def_name(seteuid,int, uid_t);
static int myseteuid(uid_t)
{
    return 0;
}

def_name(setegid,int, gid_t);
static int mysetegid(gid_t)
{
    return 0;
}

def_name(unlink, int, const char *);
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
        if (access(name, F_OK) == 0)
        {
            mark_del(mypath);
            errno = 0;
            return 0;
        }
        errno = ENOENT;
        return -1;
    }
}

void undo_del(const char *path)
{
    std::string ret = path;
    ret += ".del_file";
    unlink(ret.c_str());
}

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
        if (statusnew == ShadowFileState::Deleted)
        {
            undo_del(mynewpath);
        }
        errno = errnoback;
        return ret;
    }
}

def_name(chmod, int, const char *, mode_t);
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

def_name(fchmodat, int, int, const char *, mode_t);
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
    if (!strncmp(pathname, "/dev", 4))
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
            if (access(pathname, F_OK) == 0)
            {
                // if file exists in underlying FS, open as usual
                if (flags & O_WRONLY || flags & O_RDWR)
                {
                    OSCopyFile(pathname, mypath);
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
                OSCopyFile(pathname, mypath);
                return CallOld<Name_open>(mypath, flags, mode);
            }
            else
            {
                return CallOld<Name_open>(pathname, flags, mode);
            }
        }
    }
}

def_name(lgetxattr, ssize_t, const char *, const char *, void *, size_t);
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

def_name(getxattr, ssize_t, const char *, const char *, void *, size_t);
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

def_name(__xstat, int, int, const char *, struct stat *);
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

bool isDirExist(const std::string &path)
{
    struct stat info;
    if (CallOld<Name___xstat>(1, path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

def_name(__lxstat, int, int, const char *, struct stat *);
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
    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    auto data = std::make_unique<dir_data>(name);
    DIR *ret;
    if (status == ShadowFileState::Deleted)
    {
        errno = ENOENT;
        return nullptr;
    }
    else if (status == ShadowFileState::Exists)
    {
        ret = CallOld<Name_opendir>(mypath);
        if (ret)
        {
            int olderr = errno;
            data->underlying = CallOld<Name_opendir>(name);
            errno = olderr;
        }
    }
    else
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
    DoHookInLibAndLibC<Name_execve>(handlec, handle, myexecve);
    DoHookInLibAndLibC<Name_chmod>(handlec, handle, mychmod);
    DoHookInLibAndLibC<Name_fchmodat>(handlec, handle, myfchmodat);
    DoHookInLibAndLibC<Name_rename>(handlec, handle, myrename);
    DoHookInLibAndLibC<Name_unlink>(handlec, handle, myunlink);
    DoHookInLibAndLibC<Name_geteuid>(handlec, handle, mygeteuid);
    DoHookInLibAndLibC<Name_getuid>(handlec, handle, mygetuid);
    DoHookInLibAndLibC<Name_setuid>(handlec, handle, mysetuid);
    DoHookInLibAndLibC<Name_seteuid>(handlec, handle, myseteuid);
    DoHookInLibAndLibC<Name_setegid>(handlec, handle, mysetegid);
    DoHookInLibAndLibC<Name_setgroups>(handlec, handle, mysetgroups);
}

int OSCopyFile(const char *source, const char *destination)
{
    int input, output;
    if ((input = CallOld<Name_open>(source, O_RDONLY, 0)) == -1)
    {
        return -1;
    }
    struct stat info;
    int stat_ret = CallOld<Name___xstat>(1, source, &info);
    if (!makeParentPath(destination))
    {
        return -1;
    }
    if ((output = creat(destination,
                        info.st_mode)) == -1)
    {
        close(input);
        return -1;
    }

    //Here we use kernel-space copying for performance reasons
#if defined(__APPLE__) || defined(__FreeBSD__)
    //fcopyfile works on FreeBSD and OS X 10.5+
    int result = fcopyfile(input, output, 0, COPYFILE_ALL);
#else
    //sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
    off_t bytesCopied = 0;
    struct stat fileinfo = {0};
    fstat(input, &fileinfo);
    int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
#endif

    close(input);
    close(output);

    return result;
}