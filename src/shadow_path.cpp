#include "shadow_path.h"
#include "funcs.h"
#include <string.h>

using namespace FishHook;

static char path_prefix[PATH_MAX];
static int path_prefix_len;
static char del_path_postfix[] = ".del_file";
static const int del_path_postfix_len = sizeof(del_path_postfix) - 1;
ShadowFileState get_fixed_path(const char *pathname, char *outpath)
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

    if (CallOld<Name_access>(outpath, F_OK) == 0)
    {
        outpath[currentlen] = 0;
        return ShadowFileState::Deleted;
    }
    outpath[currentlen] = 0;
    struct stat s;
    if (CallOld<Name___lxstat>(0, outpath, &s) == 0)
    {
        return ShadowFileState::Exists;
    }
    return ShadowFileState::NotExists;
}

std::string get_fd_path(int fd)
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

void undo_del(const char *path)
{
    std::string ret = path;
    ret += ".del_file";
    CallOld<Name_unlink>(ret.c_str());
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
}