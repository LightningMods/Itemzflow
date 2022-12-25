#ifndef FW_DEFINES_H
#define FW_DEFINES_H

#include <sys/types.h>
#include <stdint.h>
#include <fcntl.h>

#define AF_UNIX 1
#define SOCK_STREAM 1
#define MAX_PATH 1024

#define SYSCALL(nr, fn) __attribute__((naked)) fn\
{\
    asm volatile("mov $" #nr ", %rax\nmov %rcx, %r10\nsyscall\nret");\
}

struct mount_in_sandbox_param
{
    const char* path;
    const char* name;
    int ans;
};

typedef enum KmemKind { USERSPACE, KERNEL_HEAP, KERNEL_TEXT } KmemKind;
enum { CWD_KEEP, CWD_ROOT, CWD_RESET };

struct jbc_cred
{
    uid_t uid;
    uid_t ruid;
    uid_t svuid;
    gid_t rgid;
    gid_t svgid;
    uintptr_t prison;
    uintptr_t cdir;
    uintptr_t rdir;
    uintptr_t jdir;
    uint64_t sceProcType;
    uint64_t sonyCred;
    uint64_t sceProcCap;
};


int socketpair(int domain, int type, int protocol, int* out);
ssize_t read(int fd, void* dst, size_t sz);
ssize_t write(int fd, const void* dst, size_t sz);
int close(int fd);
uint64_t jbc_krw_kcall(uint64_t fn, ...);
uintptr_t jbc_krw_get_td(void);
int jbc_krw_memcpy(uintptr_t dst, uintptr_t src, size_t sz, KmemKind kind);
uint64_t jbc_krw_read64(uintptr_t p, KmemKind kind);
int jbc_krw_write64(uintptr_t p, KmemKind kind, uintptr_t val);
uintptr_t jbc_get_prison0(void);
uintptr_t jbc_get_rootvnode(void);
int jbc_get_cred(struct jbc_cred*);
int jbc_jailbreak_cred(struct jbc_cred*);
int jbc_set_cred(const struct jbc_cred*);
void jbc_run_as_root(void(*fn)(void* arg), void* arg, int cwd_mode);
int jbc_mount_in_sandbox(const char* system_path, const char* mnt_name);
int jbc_unmount_in_sandbox(const char* mnt_name);
int jailbreak_multi();



#endif
