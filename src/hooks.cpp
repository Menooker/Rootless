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
#include <sys/time.h>
#include <vector>

bool isDirExist(const std::string &path);

static bool domakePath(const std::string &path);

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

static char path_prefix[PATH_MAX];
static int path_prefix_len;
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
    // todo: check pathname ends with \0 or '/'
    if (!strncmp(pathname, path_prefix, path_prefix_len - 1))
    {
        // if path name starts with prefix
        assert(len1 + 1 < PATH_MAX);
        memcpy(outpath, pathname, len1);
        outpath[len1] = 0;
        return ShadowFileState::Exists;
    }
    size_t currentlen = path_prefix_len;
    // resolve relative
    if (pathname[0] != '/')
    {
        char *ret = getcwd(outpath + currentlen, PATH_MAX - currentlen - 1);
        assert(ret);

        // todo: check pathname ends with \0 or '/'
        if (!strncmp(outpath + currentlen, path_prefix, path_prefix_len - 1))
        {
            // if current dir is in prefix
            assert(len1 + 1 < PATH_MAX);
            memcpy(outpath, pathname, len1);
            outpath[len1] = 0;
            return ShadowFileState::Exists;
        }
        currentlen += strlen(ret);
        // append a /
        outpath[currentlen] = '/';
        currentlen += 1;
    }
    memcpy(outpath, path_prefix, path_prefix_len);
    if (len1 + currentlen + del_path_postfix_len >= PATH_MAX)
    {
        fprintf(stderr, "Path length overflow %s\n", pathname);
        std::abort();
    }
    memcpy(outpath + currentlen, pathname, len1);
    currentlen += len1;
    memcpy(outpath + currentlen, del_path_postfix, del_path_postfix_len + 1);
    if (access(outpath, F_OK) == 0)
    {
        outpath[currentlen] = 0;
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
    int retv = creat(ret.c_str(), 0660);
    close(retv);
    errno = 0;
}

def_name(chdir, int, const char *);
static int mychdir(const char *name)
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
        return CallOld<Name_chdir>(mypath);
    }
    else
    {
        return CallOld<Name_chdir>(name);
    }
}

def_name(utimes, int, const char *, const struct timeval *);
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

def_name(utime, int, const char *, const void *);
static int myutime(const char *filename, const void* v)
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

void undo_del(const char *path);

def_name(mkdir, int, const char *, mode_t);
static int mymkdir(const char *name, mode_t mode)
{
    char mypath[PATH_MAX];
    auto status = get_fixed_path(name, mypath);
    makeParentPath(mypath);
    int ret = CallOld<Name_mkdir>(mypath, mode);
    if (ret == 0)
    {
        undo_del(mypath);
    }
    return ret;
}

static gid_t fake_groups[1024] = {0};
static size_t group_num = 1;
def_name(setgroups, int, size_t, const gid_t *);
static int mysetgroups(size_t size, const gid_t *list)
{
    assert(size < 1024);
    memcpy(fake_groups, list, size * sizeof(gid_t));
    group_num = size;
    return 0;
}

def_name(getgroups, int, size_t, gid_t *);
static int mygetgroups(size_t size, gid_t *list)
{
    memcpy(list, fake_groups, std::min(size, group_num) * sizeof(gid_t));
    return group_num;
}

static gid_t mygid = 0;
static gid_t myegid = 0;
static gid_t mysgid = 0;
def_name(setegid, int, gid_t);
static int mysetegid(gid_t v)
{
    myegid = v;
    return 0;
}

def_name(setgid, int, gid_t);
static int mysetgid(gid_t v)
{
    mygid = v;
    return 0;
}

def_name_no_arg(getgid, gid_t);
static gid_t mygetgid()
{
    return mygid;
}

def_name_no_arg(getegid, gid_t);
static gid_t mygetegid()
{
    return myegid;
}

def_name(setresgid, int, gid_t, gid_t, gid_t);
static int mysetresgid(gid_t r, gid_t e, gid_t s)
{
    mygid = r;
    myegid = e;
    mysgid = s;
    return 0;
}

def_name(getresgid, int, gid_t *, gid_t *, gid_t *);
static int mygetresgid(gid_t *r, gid_t *e, gid_t *s)
{
    *r = mygid;
    *e = myegid;
    *s = mysgid;
    return 0;
}

static uid_t myuid = 0;
static uid_t myeuid = 0;
static uid_t mysuid = 0;
def_name(setresuid, int, uid_t, uid_t, uid_t);
static int mysetresuid(uid_t r, uid_t e, uid_t s)
{
    myuid = r;
    myeuid = e;
    mysuid = s;
    return 0;
}

def_name(getresuid, int, uid_t *, uid_t *, uid_t *);
static int mygetresuid(uid_t *r, uid_t *e, uid_t *s)
{
    *r = myuid;
    *e = myeuid;
    *s = mysuid;
    return 0;
}

def_name(seteuid, int, uid_t);
static int myseteuid(uid_t v)
{
    myeuid = v;
    return 0;
}

def_name_no_arg(geteuid, uid_t);
static uid_t mygeteuid()
{
    return myeuid;
}

def_name(setuid, int, uid_t);
static int mysetuid(uid_t v)
{
    myuid = v;
    return 0;
}

def_name_no_arg(getuid, uid_t);
static uid_t mygetuid()
{
    return myuid;
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

static bool prepareDirIfIsEmpty(const char *name);
def_name(unlinkat, int, int, const char *, int);
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

def_name(rmdir, int, const char *);
static int myrmdir(const char *name)
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
        ret = CallOld<Name_rmdir>(mypath);
        olderrno = errno;
        if (ret != 0 && olderrno == ENOTEMPTY)
        {
            if (prepareDirIfIsEmpty(mypath))
            {
                ret = CallOld<Name_rmdir>(mypath);
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
        // todo: check empty
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
    CallOld<Name_unlink>(ret.c_str());
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
        if (ret == 0 && statusnew == ShadowFileState::Deleted)
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

def_name(__fxstatat, int, int, int, const char *, struct stat *, int);
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

def_name(fdopendir, DIR *, int);
static DIR *myfdopendir(int fd)
{
    // todo: if fd is in shadow, open unshadowed files
    char mypath[PATH_MAX];
    std::string name = get_fd_path(fd);
    auto status = get_fixed_path(name.c_str(), mypath);
    auto data = std::make_unique<dir_data>(name.c_str());
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
            data->underlying = CallOld<Name_fdopendir>(fd);
            errno = olderr;
        }
    }
    else
    {
        ret = CallOld<Name_fdopendir>(fd);
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

int strEndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

def_name(readdir, dirent *, DIR *);
dirent *DoReaddir(DIR *dirp, dir_data *data)
{
    if (data->underlying)
    {
        dirent *ret = CallOld<Name_readdir>(data->underlying);
        if (ret)
        {
            if (strEndsWith(ret->d_name, ".del_file"))
            {
                return DoReaddir(dirp, data);
            }

            return ret;
        }
        data->release_dir();
    }
    auto ret = CallOld<Name_readdir>(dirp);
    if (ret && strEndsWith(ret->d_name, ".del_file"))
    {
        return DoReaddir(dirp, data);
    }
    return ret;
}

dirent *myreaddir(DIR *dirp)
{
    auto data = (dir_data *)opendir_map.get(dirp);
    return DoReaddir(dirp, data);
}

static bool prepareDirIfIsEmpty(const char *n)
{
    DIR *d = CallOld<Name_opendir>(n);
    if (!d)
    {
        return false;
    }
    bool isempty = true;
    std::string base_path = n;
    base_path += '/';
    std::vector<std::string> paths;
    for (;;)
    {
        auto dir = CallOld<Name_readdir>(d);
        if (!dir)
        {
            break;
        }
        bool isCurDirOrParent = !memcmp("..", dir->d_name, 3) || !memcmp(".", dir->d_name, 2);
        if (!isCurDirOrParent && !strEndsWith(dir->d_name, ".del_file"))
        {
            isempty = false;
            break;
        }
        paths.emplace_back(base_path + dir->d_name);
    }
    CallOld<Name_closedir>(d);
    if (isempty)
    {
        for (auto &p : paths)
        {
            CallOld<Name_unlink>(p.c_str());
        }
    }
    return isempty;
}

def_name(chown, int, const char *, __uid_t, __gid_t);
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

def_name(fchown, int, int, __uid_t, __gid_t);
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

def_name(fchownat, int, int, const char *, __uid_t, __gid_t, int);
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

def_name(link, int, const char *, const char *);
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

__attribute__((constructor)) static void HookMe()
{
    char *newroot = getenv("RL_ROOT");
    if (!newroot)
    {
        fprintf(stderr, "RL_ROOT not set\n");
        abort();
    }
    path_prefix_len = strlen(newroot);
    if (path_prefix_len >= PATH_MAX - 1)
    {
        fprintf(stderr, "RL_ROOT too long\n");
        abort();
    }
    strncpy(path_prefix, newroot, path_prefix_len + 1);
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
    DoHookInLibAndLibC<Name_fdopendir>(handlec, handle, myfdopendir);
    DoHookInLibAndLibC<Name_closedir>(handlec, handle, myclosedir);
    DoHookInLibAndLibC<Name_readdir>(handlec, handle, myreaddir);
    DoHookInLibAndLibC<Name___xstat>(handlec, handle, myxstat);
    DoHookInLibAndLibC<Name___lxstat>(handlec, handle, mylxstat);
    DoHookInLibAndLibC<Name___fxstatat>(handlec, handle, myfxstatat);
    DoHookInLibAndLibC<Name_getxattr>(handlec, handle, mygetxattr);
    DoHookInLibAndLibC<Name_lgetxattr>(handlec, handle, mylgetxattr);
    DoHookInLibAndLibC<Name_open>(handlec, handle, myopen);
    DoHookInLibAndLibC<Name_openat>(handlec, handle, myopenat);
    DoHookInLibAndLibC<Name_execve>(handlec, handle, myexecve);
    DoHookInLibAndLibC<Name_chmod>(handlec, handle, mychmod);
    DoHookInLibAndLibC<Name_fchmodat>(handlec, handle, myfchmodat);
    DoHookInLibAndLibC<Name_rename>(handlec, handle, myrename);
    DoHookInLibAndLibC<Name_unlink>(handlec, handle, myunlink);
    DoHookInLibAndLibC<Name_unlinkat>(handlec, handle, myunlinkat);
    DoHookInLibAndLibC<Name_link>(handlec, handle, mylink);
    DoHookInLibAndLibC<Name_geteuid>(handlec, handle, mygeteuid);
    DoHookInLibAndLibC<Name_getuid>(handlec, handle, mygetuid);
    DoHookInLibAndLibC<Name_setuid>(handlec, handle, mysetuid);
    DoHookInLibAndLibC<Name_seteuid>(handlec, handle, myseteuid);
    DoHookInLibAndLibC<Name_setegid>(handlec, handle, mysetegid);
    DoHookInLibAndLibC<Name_setgroups>(handlec, handle, mysetgroups);
    DoHookInLibAndLibC<Name_getgroups>(handlec, handle, mygetgroups);
    DoHookInLibAndLibC<Name_mkdir>(handlec, handle, mymkdir);
    DoHookInLibAndLibC<Name_chdir>(handlec, handle, mychdir);
    DoHookInLibAndLibC<Name_fchown>(handlec, handle, myfchown);
    DoHookInLibAndLibC<Name_fchownat>(handlec, handle, myfchownat);
    DoHookInLibAndLibC<Name_rmdir>(handlec, handle, myrmdir);
    DoHookInLibAndLibC<Name_setresgid>(handlec, handle, mysetresgid);
    DoHookInLibAndLibC<Name_setresuid>(handlec, handle, mysetresuid);
    DoHookInLibAndLibC<Name_getresgid>(handlec, handle, mygetresgid);
    DoHookInLibAndLibC<Name_getresuid>(handlec, handle, mygetresuid);
    DoHookInLibAndLibC<Name_utimes>(handlec, handle, myutimes);
    DoHookInLibAndLibC<Name_utime>(handlec, handle, myutime);
    DoHookInLibAndLibC<Name_chown>(handlec, handle, mychown);
    DoHookInLibAndLibC<Name_getgid>(handlec, handle, mygetgid);
    DoHookInLibAndLibC<Name_getegid>(handlec, handle, mygetegid);
    DoHookInLibAndLibC<Name_setgid>(handlec, handle, mysetgid);
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

static bool domakePath(const std::string &path)
{
    mode_t mode = 0755;
    int ret = CallOld<Name_mkdir>(path.c_str(), mode);
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
        return 0 == CallOld<Name_mkdir>(path.c_str(), mode);
    case EEXIST:
        // done!
        return isDirExist(path);
    default:
        return false;
    }
}
