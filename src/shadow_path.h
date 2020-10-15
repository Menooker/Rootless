#pragma once
#include <linux/limits.h>
#include <assert.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
enum class ShadowFileState
{
    NotExists,
    Deleted,
    Exists,
};

ShadowFileState get_fixed_path(const char *pathname, char *outpath);
std::string get_fd_path(int fd);
void mark_del(const char *path);
void undo_del(const char *path);

bool isDirExist(const std::string &path);
bool makePath(const char *path);
bool makeParentPath(const char *path);
int OSCopyFile(const char *source, const char *destination);

bool prepareDirIfIsEmpty(const char *name);