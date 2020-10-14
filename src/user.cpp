#include "shadow_path.h"
#include "dir.h"
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


static int mygetgroups(size_t size, gid_t *list)
{
    memcpy(list, fake_groups, std::min(size, group_num) * sizeof(gid_t));
    return group_num;
}

static gid_t mygid = 0;
static gid_t myegid = 0;
static gid_t mysgid = 0;

static int mysetegid(gid_t v)
{
    myegid = v;
    return 0;
}


static int mysetgid(gid_t v)
{
    mygid = v;
    return 0;
}


static gid_t mygetgid()
{
    return mygid;
}


static gid_t mygetegid()
{
    return myegid;
}

static int mysetresgid(gid_t r, gid_t e, gid_t s)
{
    mygid = r;
    myegid = e;
    mysgid = s;
    return 0;
}

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
static int mysetresuid(uid_t r, uid_t e, uid_t s)
{
    myuid = r;
    myeuid = e;
    mysuid = s;
    return 0;
}

static int mygetresuid(uid_t *r, uid_t *e, uid_t *s)
{
    *r = myuid;
    *e = myeuid;
    *s = mysuid;
    return 0;
}

static int myseteuid(uid_t v)
{
    myeuid = v;
    return 0;
}

static uid_t mygeteuid()
{
    return myeuid;
}

static int mysetuid(uid_t v)
{
    myuid = v;
    return 0;
}

static uid_t mygetuid()
{
    return myuid;
}
