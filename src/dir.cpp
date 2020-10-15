#include "funcs.h"
#include "shadow_path.h"
#include <string.h>
#include <dirent.h>
#include "hash_map.h"
#include <vector>

using namespace FishHook;

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
rl_hook(chdir)

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
rl_hook(mkdir)

concurrent_hash_map_owned opendir_map;

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
rl_hook(opendir)

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
rl_hook(fdopendir)

static int myclosedir(DIR *name)
{
    int ret = CallOld<Name_closedir>(name);
    opendir_map.remove(name);
    return ret;
}
rl_hook(closedir)

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
rl_hook(readdir);

bool prepareDirIfIsEmpty(const char *n)
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
rl_hook(rmdir)

