#include "log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <stddef.h>
#define FUSE_USE_VERSION 26
#include "fuse/fuse.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>

static int
fs_readdir(const char *path, void *data, fuse_fill_dir_t filler,
           off_t off, struct fuse_file_info *ffi)
{
	log_info("readdir: %s", path);

	filler(data, ".", NULL, 0);
	filler(data, "..", NULL, 0);
	filler(data, "file", NULL, 0);
	return 0;
}

static int
fs_read(const char *path, char *buf, size_t size, off_t off,
        struct fuse_file_info *ffi)
{
	size_t len;
	log_info("read: %s", path);
	const char *file_contents = "fuse filesystem example\n";

	len = strlen(file_contents);

	if (off < len) {
		if (off + size > len)
			size = len - off;
		memcpy(buf, file_contents + off, size);
	} else
		size = 0;

	return size;
}

static int
fs_open(const char *path, struct fuse_file_info *ffi)
{
	log_info("open: %s", path);
	if (strncmp(path, "/hostapp/file", 10) != 0)
		return -ENOENT;

	if ((ffi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int
fs_getattr(const char *path, struct stat *st)
{
	log_info("getattr: %s", path);

	st->st_blksize = 512;
	st->st_mode = 0755;
	st->st_nlink = 2;
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = st->st_mtime = st->st_ctime = time(NULL);
	st->st_ino = 1;

	if (strcmp(path, "/file") == 0) {
		st->st_mode = 0644;
		st->st_blksize = 512;
		st->st_nlink = 1;
		st->st_size = 5;
	}

	return 0;
}

struct fuse_operations fsops = {
	.readdir = fs_readdir,
	.read = fs_read,
	.open = fs_open,
	.getattr = fs_getattr,
};

//char* argv[] = { "fuse", "-f", "/host", "-o", "subtype=dummy" };

int fuse_ret  = -1;
int (*fuse_main_real_2)(int argc, char *argv[], const struct fuse_operations *op, size_t op_size, void *user_data);
int sceKernelSetBesteffort(int besteffort, int priority);

char* argv[] = { "dummy", "/hostapp", "-o", "allow_other", "-o", "rw", "-o", "use_ino", "-o", "direct_io", "-o", "attr_timeout=0.0", "-o", "max_read=65536", "-f", NULL };

int sceKernelLoadStartModule(
    const char *moduleFileName,
    size_t args,
    const void *argp,
    uint32_t flags,
    int*,
    int *pRes
);
int sceKernelDlsym(
    int handle,
    const char *symbol,
    void **addrp
);

void* kpayload(struct thread *td)
{
   
	void* kernel_base = &((uint8_t*)kernelRdmsr(0xC0000082))[-0x1C0];
	uint8_t* kernel_ptr = (uint8_t*)kernel_base;

    cpu_disable_wp();

int *ksuser_enabled=(int *)(kernel_base+0x2300B88);
		struct vfsconf *p=(struct vfsconf *)(kernel_base + 0x19FC380 );
		//suser_enabled in priv_check_cred
		*ksuser_enabled=1;
		//add jail friendly for fuse file system
		p->vfc_flags=0x00400000 | 0x00080000;
		//avoid enforce_dev_perms checks
		//*kfuse_enforce_dev_perms=0;
		//default prison_priv_check to 0
		kernel_ptr[0x3B219E]=0;

		//skip devkit/testkit/dipsw check in fuse_loader
		kernel_ptr[0x49DDDE] = 0xEB;
		kernel_ptr[0x49DDDF] = 0x1B;

		//skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
		kernel_ptr[0x4A28EE] = 0xEB;
		kernel_ptr[0x4A28EF] = 0x0;

		//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_close_device
		kernel_ptr[0x4A29E2] = 0xEB;

		//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_poll_device
		kernel_ptr[0x4A2F34] = 0xEB;
		
		// skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
		kernel_ptr[0x4A30F7] = 0xEB;
		kernel_ptr[0x4A30F8] = 0x04;
  
		// skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
		kernel_ptr[0x4A384C] = 0xEB;
		kernel_ptr[0x4A384D] = 0x00;
  
		// skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
		kernel_ptr[0x4A3BED] = 0xEB;
		kernel_ptr[0x4A3BEE] = 0x04;	

        kernel_ptr[0x4A27FC]=0xB6;

    // patch kernel

    cpu_enable_wp();

	int (*fuse_loader)(void* m, int op, void* arg)=(void *)(kernel_base + 0x49DDB0);
    fuse_loader(NULL, 0, NULL);


    return NULL;
}


void* initialize_userland_fuse()
{
	//syscall(11, &kpayload);
    log_info("Starting FUSE ...");
    sceKernelSetBesteffort(903, 255); // set best cpu effort to highest for IO
	
    fuse_ret = fuse_main(15, argv, &fsops, NULL);
	log_error("[ERROR] fuse: fuse_main returned %d", fuse_ret);
	notify("Fuse failed to Start (see logs)");
	return (void*)(uintptr_t)fuse_ret;
}