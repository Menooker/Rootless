#include "shadow_path.h"
#include "funcs.h"
#include <unistd.h>
#include <string.h>
using namespace FishHook;

static gid_t fake_groups[1024] = {0};
static size_t group_num = 1;

static int mysetgroups(size_t size, const gid_t *list)
{
    assert(size < 1024);
    memcpy(fake_groups, list, size * sizeof(gid_t));
    group_num = size;
    return 0;
}
rl_hook(setgroups)


static int mygetgroups(size_t size, gid_t *list)
{
    memcpy(list, fake_groups, std::min(size, group_num) * sizeof(gid_t));
    return group_num;
}
rl_hook(getgroups)

static gid_t mygid = 0;
static gid_t myegid = 0;
static gid_t mysgid = 0;

static int mysetegid(gid_t v)
{
    myegid = v;
    return 0;
}
rl_hook(setegid)


static int mysetgid(gid_t v)
{
    mygid = v;
    return 0;
}
rl_hook(setgid)


static gid_t mygetgid()
{
    return mygid;
}
rl_hook(getgid)

static gid_t mygetegid()
{
    return myegid;
}
rl_hook(getegid)

static int mysetresgid(gid_t r, gid_t e, gid_t s)
{
    mygid = r;
    myegid = e;
    mysgid = s;
    return 0;
}
rl_hook(setresgid)

static int mygetresgid(gid_t *r, gid_t *e, gid_t *s)
{
    *r = mygid;
    *e = myegid;
    *s = mysgid;
    return 0;
}
rl_hook(getresgid)

static uid_t myuid = 0;
static uid_t myeuid = 0;
static uid_t mysuid = 0;
static int mysetresuid(uid_t r, uid_t e, uid_t s)
{
    myuid = r;
    myeuid = e;
    mysuid = s;
    return 0;
}
rl_hook(setresuid)

static int mygetresuid(uid_t *r, uid_t *e, uid_t *s)
{
    *r = myuid;
    *e = myeuid;
    *s = mysuid;
    return 0;
}
rl_hook(getresuid)

static int myseteuid(uid_t v)
{
    myeuid = v;
    return 0;
}
rl_hook(seteuid)

static uid_t mygeteuid()
{
    return myeuid;
}
rl_hook(geteuid)

static int mysetuid(uid_t v)
{
    myuid = v;
    return 0;
}
rl_hook(setuid)

static uid_t mygetuid()
{
    return myuid;
}
rl_hook(getuid)
