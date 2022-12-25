#include "multi-jb.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
static uintptr_t prison0;
static uintptr_t rootvnode;
struct iovec {
	void	*iov_base;	/* Base address. */
	size_t	 iov_len;	/* Length. */
};

SYSCALL(12, static int chdir(char* path))
SYSCALL(22, static int unmount(const char* path, int flags))
SYSCALL(136, static int mkdir(const char* path, int mode))
SYSCALL(137, static int rmdir(const char* path))
SYSCALL(326, static int getcwd(char* buf, size_t sz))
SYSCALL(378, static int nmount(struct iovec* iov, unsigned int niov, int flags))


void jbc_run_as_root(void(*fn)(void* arg), void* arg, int cwd_mode)
{
    struct jbc_cred cred;
    jbc_get_cred(&cred);
    struct jbc_cred root_cred = cred;
    jbc_jailbreak_cred(&root_cred);
    switch (cwd_mode)
    {
    case CWD_KEEP:
    default:
        root_cred.cdir = cred.cdir;
        break;
    case CWD_ROOT:
        root_cred.cdir = cred.rdir;
        break;
    case CWD_RESET:
        root_cred.cdir = root_cred.rdir;
        break;
    }
    jbc_set_cred(&root_cred);
    fn(arg);
    jbc_set_cred(&cred);
}

void build_iovec(struct iovec** iov, int* iovlen, const char* name, const void* val, size_t len) {
    int i;

    if (*iovlen < 0)
        return;

    i = *iovlen;
    *iov = (struct iovec*)realloc((void*)(*iov), sizeof(struct iovec) * (i + 2));
    if (*iov == NULL) {
        *iovlen = -1;
        return;
    }

    (*iov)[i].iov_base = strdup(name);
    (*iov)[i].iov_len = strlen(name) + 1;
    ++i;

    (*iov)[i].iov_base = (void*)val;
    if (len == (size_t)-1) {
        if (val != NULL)
            len = strlen((const char*)val) + 1;
        else
            len = 0;
    }
    (*iov)[i].iov_len = (int)len;

    *iovlen = ++i;
}


int mount_NULL_fs(char* null_path, char* mountpoint, char* mode, unsigned int flags) {
	struct iovec* iov = NULL;
	int iovlen = 0;
	int ret;
	//

	mkdir(mountpoint, 0777);

	build_iovec(&iov, &iovlen, "fstype", "nullfs", (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", null_path, (size_t)-1); // Where i want to add
	build_iovec(&iov, &iovlen, "target", mountpoint, (size_t)-1); // What i want to add
	build_iovec(&iov, &iovlen, "allow_other", "", (size_t)-1); // What i want to add
	build_iovec(&iov, &iovlen, mode, "", (size_t)-1);


	printf("  [I] Mounting %s (%s) to %s...\n", null_path, "nullfs", mountpoint);
	ret = nmount(iov, iovlen, flags);
	if (ret < 0) {
		printf("  [E] Failed: %d (errno: %s).\n", ret, strerror(errno));
		goto error;
	}
	else {
		printf("  [I] Success.\n");
	}

error:
	return ret;
}

static void do_mount_in_sandbox(void* op)
{
    struct mount_in_sandbox_param* p = op;
    mount_NULL_fs(p->path, p->name, "rw", 0777);
}

int jbc_mount_in_sandbox(const char* system_path, const char* mnt_name)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "/mnt/sandbox/ITEM00001_000%s", mnt_name);
    struct mount_in_sandbox_param op = {
        .path = system_path,
        .name = buf,
        .ans = 0,
    };
    jbc_run_as_root(do_mount_in_sandbox, &op, CWD_ROOT);
    return op.ans;
}

int jbc_unmount_in_sandbox(const char* mnt_name)
{
    return jbc_mount_in_sandbox(0, mnt_name);
}

static int k_kcall(void* td, uint64_t** uap)
{
    uint64_t* args = uap[1];
    args[0] = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))args[0])(args[1], args[2], args[3], args[4], args[5], args[6]);
    return 0;
}

asm("kexec:\nmov $11, %rax\nmov %rcx, %r10\nsyscall\nret");
void kexec(void*, void*);

uint64_t jbc_krw_kcall(uint64_t fn, ...)
{
    va_list v;
    va_start(v, fn);
    uint64_t uap[7] = { fn };
    for (int i = 1; i <= 6; i++)
        uap[i] = va_arg(v, uint64_t);
    kexec(k_kcall, uap);
    return uap[0];
}

asm("k_get_td:\nmov %gs:0, %rax\nret");
extern char k_get_td[];

uintptr_t jbc_krw_get_td(void)
{
    return jbc_krw_kcall((uintptr_t)k_get_td);
}

static int have_mira = -1;
static int mira_socket[2];

static int do_check_mira(void)
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, mira_socket))
        return 0;
    if (write(mira_socket[1], (void*)jbc_krw_get_td(), 1) == 1)
    {
        char c;
        read(mira_socket[0], &c, 1);
        return 1;
    }
    return 0;
}

static inline bool check_mira(void)
{
    if (have_mira < 0)
        have_mira = do_check_mira();
    return (bool)have_mira;
}

static inline bool check_ptr(uintptr_t p, KmemKind kind)
{
    if (kind == USERSPACE)
        return p < 0x800000000000;
    else if (kind == KERNEL_HEAP)
        return p >= 0xffff800000000000 && p < 0xffffffff00000000;
    else if (kind == KERNEL_TEXT)
        return p >= 0xffffffff00000000 && p < 0xfffffffffffff000;
    else
        return false;
}

static int kcpy_mira(uintptr_t dst, uintptr_t src, size_t sz)
{
    while (sz > 0)
    {
        size_t chk = (sz > 64 ? 64 : sz);
        if (write(mira_socket[1], (void*)src, chk) != chk)
            return -1;
        if (read(mira_socket[0], (void*)dst, chk) != chk)
            return -1;
        dst += chk;
        src += chk;
        sz -= chk;
    }
    return 0;
}

asm("k_kcpy:\nmov %rdx, %rcx\nrep movsb\nret");
extern char k_kcpy[];

int jbc_krw_memcpy(uintptr_t dst, uintptr_t src, size_t sz, KmemKind kind)
{
    if (sz == 0)
        return 0;
    bool u1 = check_ptr(dst, USERSPACE) && check_ptr(dst + sz - 1, USERSPACE);
    bool ok1 = check_ptr(dst, kind) && check_ptr(dst + sz - 1, kind);
    bool u2 = check_ptr(src, USERSPACE) && check_ptr(src + sz - 1, USERSPACE);
    bool ok2 = check_ptr(src, kind) && check_ptr(src + sz - 1, kind);
    if (!((u1 || ok1) && (u2 || ok2)))
        return -1;
    if (u1 && u2)
        return -1;
    if (check_mira())
        return kcpy_mira(dst, src, sz);
    jbc_krw_kcall((uintptr_t)k_kcpy, dst, src, sz);
    return 0;
}

uint64_t jbc_krw_read64(uintptr_t p, KmemKind kind)
{
    uint64_t ans;
    if (jbc_krw_memcpy((uintptr_t)&ans, p, sizeof(ans), kind))
        return -1;
    return ans;
}

int jbc_krw_write64(uintptr_t p, KmemKind kind, uintptr_t val)
{
    return jbc_krw_memcpy(p, (uintptr_t)&val, sizeof(val), kind);
}


asm("open:\nmov $5, %rax\nmov %rcx, %r10\nsyscall\nret");
asm("close:\nmov $6, %rax\nmov %rcx, %r10\nsyscall\nret");

int open(const char*, int, ...);
int close(int);

static uintptr_t prison0;
static uintptr_t rootvnode;

static int resolve(void)
{
restart:;
    uintptr_t td = jbc_krw_get_td();
    uintptr_t proc = jbc_krw_read64(td+8, KERNEL_HEAP);
    for(;;)
    {
        int pid;
        if(jbc_krw_memcpy((uintptr_t)&pid, proc+0xb0, sizeof(pid), KERNEL_HEAP))
            goto restart;
        if(pid == 1)
            break;
        uintptr_t proc2 = jbc_krw_read64(proc, KERNEL_HEAP);
        uintptr_t proc1 = jbc_krw_read64(proc2+8, KERNEL_HEAP);
        if(proc1 != proc)
            goto restart;
        proc = proc2;
    }
    uintptr_t pid1_ucred = jbc_krw_read64(proc+0x40, KERNEL_HEAP);
    uintptr_t pid1_fd = jbc_krw_read64(proc+0x48, KERNEL_HEAP);
    if(jbc_krw_memcpy((uintptr_t)&prison0, pid1_ucred+0x30, sizeof(prison0), KERNEL_HEAP))
        return -1;
    if(jbc_krw_memcpy((uintptr_t)&rootvnode, pid1_fd+0x18, sizeof(rootvnode), KERNEL_HEAP))
    {
        prison0 = 0;
        return -1;
    }
    return 0;
}

uintptr_t jbc_get_prison0(void)
{
    if(!prison0)
        resolve();
    return prison0;
}

uintptr_t jbc_get_rootvnode(void)
{
    if(!rootvnode)
        resolve();
    return rootvnode;
}

static inline int ppcopyout(void* u1, void* u2, uintptr_t k)
{
    return jbc_krw_memcpy((uintptr_t)u1, k, (uintptr_t)u2-(uintptr_t)u1, KERNEL_HEAP);
}

static inline int ppcopyin(const void* u1, const void* u2, uintptr_t k)
{
    return jbc_krw_memcpy(k, (uintptr_t)u1, (uintptr_t)u2-(uintptr_t)u1, KERNEL_HEAP);
}

int jbc_get_cred(struct jbc_cred* ans)
{
    uintptr_t td = jbc_krw_get_td();
    uintptr_t proc = jbc_krw_read64(td + 8, KERNEL_HEAP);
    uintptr_t ucred = jbc_krw_read64(proc + 0x40, KERNEL_HEAP);
    uintptr_t fd = jbc_krw_read64(proc + 0x48, KERNEL_HEAP);

    if (ppcopyout(&ans->uid, 1 + &ans->svuid, ucred + 4)
        || ppcopyout(&ans->rgid, 1 + &ans->svgid, ucred + 20)
        || ppcopyout(&ans->prison, 1 + &ans->prison, ucred + 0x30)
        || ppcopyout(&ans->cdir, 1 + &ans->jdir, fd + 0x10)
        || ppcopyout(&ans->sceProcType, 1 + &ans->sceProcCap, ucred + 88))
        return -1;

    return 0;
}

static int jbc_set_cred_internal(const struct jbc_cred* ans)
{
    uintptr_t td = jbc_krw_get_td();
    uintptr_t proc = jbc_krw_read64(td + 8, KERNEL_HEAP);
    uintptr_t ucred = jbc_krw_read64(proc + 0x40, KERNEL_HEAP);
    uintptr_t fd = jbc_krw_read64(proc + 0x48, KERNEL_HEAP);

    if (ppcopyin(&ans->uid, 1 + &ans->svuid, ucred + 4)
        || ppcopyin(&ans->rgid, 1 + &ans->svgid, ucred + 20)
        || ppcopyin(&ans->prison, 1 + &ans->prison, ucred + 0x30)
        || ppcopyin(&ans->cdir, 1 + &ans->jdir, fd + 0x10) 
        || ppcopyin(&ans->sceProcType, 1 + &ans->sceProcCap, ucred + 88))
        return -1;
    return 0;
}

int jbc_jailbreak_cred(struct jbc_cred* ans)
{
    uintptr_t prison0 = jbc_get_prison0();
    if (!prison0)
        return -1;
    uintptr_t rootvnode = jbc_get_rootvnode();
    if (!rootvnode)
        return -1;

    //without some modules wont load like Apputils
    ans->sceProcCap = 0xffffffffffffffff;
    ans->sceProcType = 0x3801000000000013;
    ans->sonyCred = 0xffffffffffffffff;

    ans->uid = ans->ruid = ans->svuid = ans->rgid = ans->svgid = 0;
    ans->prison = prison0;
    ans->cdir = ans->rdir = ans->jdir = rootvnode;
    return 0;
}

static int jbc_open_this(const struct jbc_cred* cred0, uintptr_t vnode)
{
    if(!vnode)
        return -1;
    struct jbc_cred cred = *cred0;
    jbc_jailbreak_cred(&cred);
    cred.cdir = cred.rdir = cred.jdir = vnode;
    jbc_set_cred_internal(&cred);
    int ans = open("/", 0);
    jbc_set_cred_internal(cred0);
    return ans;
}

static int swap64(uintptr_t p1, KmemKind k1, uintptr_t p2, KmemKind k2)
{
    uintptr_t v1 = jbc_krw_read64(p1, k1);
    uintptr_t v2 = jbc_krw_read64(p2, k2);
    if(ppcopyin(&v2, 1+&v2, p1)
    || ppcopyin(&v1, 1+&v1, p2))
        return -1;
    return 0;
}

static int return0(void)
{
    return 0;
}

static void* fake_vtable[16] = {
    return0, return0, return0, return0,
    return0, return0, return0, return0,
    return0, return0, return0, return0,
    return0, return0, return0, return0,
};

int jbc_set_cred(const struct jbc_cred* newp)
{
    struct jbc_cred old, neww = *newp;
    if(jbc_get_cred(&old))
        return -1;
    if(old.cdir && !neww.cdir)
        neww.cdir = jbc_get_rootvnode();
    if(old.rdir && !neww.rdir)
        neww.rdir = jbc_get_rootvnode();
    if(old.jdir && !neww.jdir)
        neww.jdir = jbc_get_rootvnode();
    int cdir_fd = jbc_open_this(&old, neww.cdir);
    int rdir_fd = jbc_open_this(&old, neww.rdir);
    int jdir_fd = jbc_open_this(&old, neww.jdir);
    struct jbc_cred elevated = neww;
    elevated.cdir = old.cdir;
    elevated.jdir = old.jdir;
    elevated.rdir = old.rdir;
    //this offset is the same in 5.05-9.60, probably safe to use
    uint32_t rc;
    if(elevated.prison)
    {
        if(jbc_krw_memcpy((uintptr_t)&rc, elevated.prison+0x14, sizeof(rc), KERNEL_HEAP)
        && jbc_krw_memcpy((uintptr_t)&rc, elevated.prison+0x14, sizeof(rc), KERNEL_TEXT))
            return -1;
        rc++;
        if(jbc_krw_memcpy(elevated.prison+0x14, (uintptr_t)&rc, sizeof(rc), KERNEL_HEAP)
        && jbc_krw_memcpy((uintptr_t)&rc, elevated.prison+0x14, sizeof(rc), KERNEL_TEXT))
            return -1;
    }
    if(jbc_set_cred_internal(&elevated))
        return -1;
    if(old.prison)
    {
        if(jbc_krw_memcpy((uintptr_t)&rc, old.prison+0x14, sizeof(rc), KERNEL_HEAP)
        && jbc_krw_memcpy((uintptr_t)&rc, old.prison+0x14, sizeof(rc), KERNEL_TEXT))
            return -1;
        rc--;
        if(jbc_krw_memcpy(old.prison+0x14, (uintptr_t)&rc, sizeof(rc), KERNEL_HEAP)
        && jbc_krw_memcpy(old.prison+0x14, (uintptr_t)&rc, sizeof(rc), KERNEL_TEXT))
            return -1;
    }
    uintptr_t td = jbc_krw_get_td();
    uintptr_t proc = jbc_krw_read64(td + 8, KERNEL_HEAP);
    uintptr_t ucred = jbc_krw_read64(proc + 0x40, KERNEL_HEAP);
    uintptr_t fd = jbc_krw_read64(proc + 0x48, KERNEL_HEAP);
    uintptr_t ofiles = jbc_krw_read64(fd, KERNEL_HEAP);
    int fds[3] = {cdir_fd, rdir_fd, jdir_fd};
    for(int i = 0; i < 3; i++)
    {
        if(fds[i] < 0)
            continue;
        uintptr_t file = jbc_krw_read64(ofiles+8*fds[i], KERNEL_HEAP);
        if(swap64(file, KERNEL_HEAP, fd+0x10+8*i, KERNEL_HEAP))
            return -1;
        if(jbc_krw_read64(file, KERNEL_HEAP) == 0)
        {
            uintptr_t p = (uintptr_t)fake_vtable;
            if(ppcopyin(&p, 1+&p, file+8))
                return -1;
        }
        close(fds[i]);
    }
    return 0;
}

struct jbc_cred rejail;

int jailbreak_multi() {

  struct jbc_cred ans;
  jbc_get_cred(&rejail);
  jbc_get_cred(&ans);
  jbc_jailbreak_cred(&ans);
  return jbc_set_cred(&ans);
}

int rejail_multi() {
  return jbc_set_cred(&rejail);
}

int jailbreak_me(void)
{
    return jailbreak_multi();
}

int _fini(void)
{
    return 0;
}
int _init(void)
{
    return 0;
}