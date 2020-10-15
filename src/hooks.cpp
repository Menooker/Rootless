#include <errno.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

static void *loadLib(const char *n)
{
    void *handle = dlopen(n, RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        exit(1);
    }
    return handle;
}
namespace FishHook
{

    void *GetLibCHandle()
    {
        static void *handle = loadLib("libc.so.6");
        return handle;
    }
    void *GetOtherHandle()
    {
        static void *handle = loadLib("libpthread.so.0");
        return handle;
    }
} // namespace FishHook

