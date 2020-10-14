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
#include <grp.h>
#include <sys/time.h>
#include <vector>

using namespace FishHook;

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
    DoHookInLibAndLibC<Name_access>(handlec, handle, myaccess);
}
