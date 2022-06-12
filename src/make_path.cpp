#include "shadow_path.h"
#include "funcs.h"
#include <libgen.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

using namespace FishHook;

bool isDirExist(const std::string &path)
{
    struct stat info;
    if (CallOld<Name___xstat>(1, path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

static bool domakePath(const std::string &path);

bool makePath(const char *path)
{
    return domakePath(path);
}

bool makeParentPath(const char *path)
{
    auto old_mask = umask(0);
    std::string path2 = path;
    char *dir = dirname((char *)path2.c_str());
    auto ret = domakePath(dir);
    umask(old_mask);
    return ret;
}


static bool domakePath(const std::string &path)
{
    constexpr mode_t mode = 0755;
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
        fprintf(stderr, "Rootless mkdir failed at %s\n", path.c_str());
        return false;
    }
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
    if (S_ISDIR(info.st_mode)) {
        mode_t mode = 0755;
        return CallOld<Name_mkdir>(destination, mode);
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
