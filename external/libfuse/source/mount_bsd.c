/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2005-2008 Csaba Henk <csaba.henk@creo.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse_opt.h"
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define FUSERMOUNT_PROG		"/system/sys/mount_fusefs.elf"
#define FUSE_DEV_TRUNK		"/dev/fuse"

enum {
	KEY_ALLOW_ROOT,
	KEY_RO,
	KEY_HELP,
	KEY_VERSION,
	KEY_KERN
};

struct mount_opts {
	int allow_other;
	int allow_root;
	int ishelp;
	char *kernel_opts;
};


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

#define	MNT_FORCE	0x0000000000080000ULL  /* force unmount or readonly change */

int mount_fuse(const char* device, const char* mountpoint)
{
    struct iovec* iov = NULL;
    int iovlen = 0;

    // fuse mount flags
	build_iovec(&iov, &iovlen, "private", "", -1);
	build_iovec(&iov, &iovlen, "__private", "", -1);
	build_iovec(&iov, &iovlen, "sync_unmount", "", -1);
	build_iovec(&iov, &iovlen, "__sync_unmount", "", -1);
	build_iovec(&iov, &iovlen, "max_read=", "262144", -1);
	build_iovec(&iov, &iovlen, "subtype=", "dummy", -1);
	build_iovec(&iov, &iovlen, "allow_other", "", -1);
	build_iovec(&iov, &iovlen, "fstype", "fusefs", -1);
    build_iovec(&iov, &iovlen, "fspath", mountpoint, -1);
    build_iovec(&iov, &iovlen, "from", device, -1);


	libfuse_print("##^  [I] Mounting \"%s\" to \"%s\" ", device, mountpoint);
    int ret = nmount(iov, iovlen, MNT_FORCE);
    if (ret < 0) {
		libfuse_print("##^  [E] Failed: %d (errno: %d).", ret, errno);
        return ret;
    }
    else 
        libfuse_print("##^  [I] Success.");

    return 0;
}


#define FUSE_DUAL_OPT_KEY(templ, key) 				\
	FUSE_OPT_KEY(templ, key), FUSE_OPT_KEY("no" templ, key)

static const struct fuse_opt fuse_mount_opts[] = {
	{ "allow_other", offsetof(struct mount_opts, allow_other), 1 },
	{ "allow_root", offsetof(struct mount_opts, allow_root), 1 },
	FUSE_OPT_KEY("allow_root",		KEY_ALLOW_ROOT),
	FUSE_OPT_KEY("-r",			KEY_RO),
	FUSE_OPT_KEY("-h",			KEY_HELP),
	FUSE_OPT_KEY("--help",			KEY_HELP),
	FUSE_OPT_KEY("-V",			KEY_VERSION),
	FUSE_OPT_KEY("--version",		KEY_VERSION),
	/* standard FreeBSD mount options */
	FUSE_DUAL_OPT_KEY("dev",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("async",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("atime",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("dev",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("exec",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("suid",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("symfollow",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("rdonly",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("sync",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("union",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("userquota",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("groupquota",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("clusterr",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("clusterw",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("suiddir",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("snapshot",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("multilabel",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("acls",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("force",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("update",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("ro",			KEY_KERN),
	FUSE_DUAL_OPT_KEY("rw",			KEY_KERN),
	FUSE_DUAL_OPT_KEY("auto",		KEY_KERN),
	/* options supported under both Linux and FBSD */
	FUSE_DUAL_OPT_KEY("allow_other",	KEY_KERN),
	FUSE_DUAL_OPT_KEY("default_permissions",KEY_KERN),
	FUSE_OPT_KEY("max_read=",		KEY_KERN),
	FUSE_OPT_KEY("subtype=",		KEY_KERN),
	/* FBSD FUSE specific mount options */
	FUSE_DUAL_OPT_KEY("private",		KEY_KERN),
	FUSE_DUAL_OPT_KEY("neglect_shares",	KEY_KERN),
	FUSE_DUAL_OPT_KEY("push_symlinks_in",	KEY_KERN),
	FUSE_OPT_KEY("nosync_unmount",		KEY_KERN),
	/* stock FBSD mountopt parsing routine lets anything be negated... */
	/*
	 * Linux specific mount options, but let just the mount util
	 * handle them
	 */
	FUSE_OPT_KEY("fsname=",			KEY_KERN),
	FUSE_OPT_KEY("nonempty",		KEY_KERN),
	FUSE_OPT_KEY("large_read",		KEY_KERN),
	FUSE_OPT_END
};

static void mount_help(void)
{
	fprintf(stderr,
		"    -o allow_root          allow access to root"
		);
	system(FUSERMOUNT_PROG " --help");
	fputc('\n', stderr);
}

static void mount_version(void)
{
	system(FUSERMOUNT_PROG " --version");
}

static int fuse_mount_opt_proc(void *data, const char *arg, int key,
			       struct fuse_args *outargs)
{
	struct mount_opts *mo = data;

	switch (key) {
	case KEY_ALLOW_ROOT:
		if (fuse_opt_add_opt(&mo->kernel_opts, "allow_other") == -1 ||
		    fuse_opt_add_arg(outargs, "-oallow_root") == -1)
			return -1;
		return 0;

	case KEY_RO:
		arg = "ro";
		/* fall through */

	case KEY_KERN:
		return fuse_opt_add_opt(&mo->kernel_opts, arg);

	case KEY_HELP:
		mount_help();
		mo->ishelp = 1;
		break;

	case KEY_VERSION:
		mount_version();
		mo->ishelp = 1;
		break;
	}
	return 1;
}

static int fuse_mount_core(const char *mountpoint, const char *opts, const struct fuse_operations *op)
{
	int fd = -1;
    int dev_trunk = open("/dev/fuse", O_RDWR);
	if(dev_trunk == -1){
		libfuse_print("fuse: failed to open /dev/fuse: %s", strerror(errno));
		return dev_trunk;
	}
	else
	  libfuse_print("mount_fuse fd: %d, mountpoint: %s", dev_trunk, mountpoint);

	close(dev_trunk), dev_trunk = -1;


	if(strcmp(mountpoint, "/host") == 0 ) {
	    fd = open("/dev/fuse0", O_RDWR);
	    libfuse_print("Devpath: /dev/fuse0 fd: %d, mountpoint: %s", fd, mountpoint);
	    if (fd == -1) {
		   libfuse_print("fuse: failed to open /dev/fuse0: %s", strerror(errno));
		   return fd;
	    }

	   	if(mount_fuse("/dev/fuse0", mountpoint)){
			libfuse_print("fuse: failed to mount /dev/fuse0: %s", strerror(errno));
			close(fd);
			return -1;
		}
	}
	else if(strcmp(mountpoint, "/hostapp") == 0) {
	    fd = open("/dev/fuse1", O_RDWR);
	    libfuse_print("Devpath: /dev/fuse1 fd: %d, mountpoint: %s", fd, mountpoint);
	    if (fd == -1) {
		   libfuse_print("fuse: failed to open /dev/fuse1: %s", strerror(errno));
		   return fd;
	    }

	   	if(mount_fuse("/dev/fuse1", mountpoint)){
			libfuse_print("fuse: failed to mount /dev/fuse1: %s", strerror(errno));
			close(fd);
			return -1;
		}
	}
	else{
		libfuse_print("[FATAL ERROR] NO mount rules for mp: %s", mountpoint);
		return -1;
	}

	return fd;
}


int fuse_kern_mount(const char *mountpoint, struct fuse_args *args, const struct fuse_operations *op)
{
	struct mount_opts mo;
	int res = -1;

	libfuse_print("fuse_kern_mount start");

	memset(&mo, 0, sizeof(mo));
	/* mount util should not try to spawn the daemon */
	setenv("MOUNT_FUSEFS_SAFE", "1", 1);
	/* to notify the mount util it's called from lib */
	setenv("MOUNT_FUSEFS_CALL_BY_LIB", "1", 1);

	if (args &&
	    fuse_opt_parse(args, &mo, fuse_mount_opts, fuse_mount_opt_proc) == -1){
		libfuse_print("fuse: failed to parse options");
		return -1;
	}

	res = fuse_mount_core(mountpoint, mo.kernel_opts, op);
	libfuse_print("fuse_mount_core res = %d", res);

	free(mo.kernel_opts);
	return res;
}

FUSE_SYMVER(".symver fuse_unmount_compat22,fuse_unmount@FUSE_2.2");
