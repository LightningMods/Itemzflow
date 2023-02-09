/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "config.h"
#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse_opt.h"
#include "fuse_lowlevel.h"
#include "fuse_common_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/param.h>
#include <stdarg.h>

enum  {
	KEY_HELP,
	KEY_HELP_NOHEADER,
	KEY_VERSION,
};

struct helper_opts {
	int singlethread;
	int foreground;
	int nodefault_subtype;
	char *mountpoint;
};

#define DGB_CHANNEL_TTYL 0
int sceKernelDebugOutText(int DBG_CHANNEL, const char *text);

void libfuse_print(char* format, ...)
{
	char buff[1024];
	memset(&buff[0], 0, 1024);

	va_list args;
	va_start(args, format);
	vsprintf(&buff[0], format, args);
	va_end(args);

	snprintf(&buff[0], 1023, "%s\n", &buff[0]);
	sceKernelDebugOutText(DGB_CHANNEL_TTYL, &buff[0]);

	int fd = sceKernelOpen("/data/itemzflow_daemon/libfuse.log", O_WRONLY | O_CREAT | O_APPEND, 0777);
	if (fd >= 0)
	{
		sceKernelWrite(fd, &buff[0], strlen(&buff[0]));
		sceKernelClose(fd);
	}
}

#define FUSE_HELPER_OPT(t, p) { t, offsetof(struct helper_opts, p), 1 }

static const struct fuse_opt fuse_helper_opts[] = {
	FUSE_HELPER_OPT("-d",		foreground),
	FUSE_HELPER_OPT("debug",	foreground),
	FUSE_HELPER_OPT("-f",		foreground),
	FUSE_HELPER_OPT("-s",		singlethread),
	FUSE_HELPER_OPT("fsname=",	nodefault_subtype),
	FUSE_HELPER_OPT("subtype=",	nodefault_subtype),

	FUSE_OPT_KEY("-h",		KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("-ho",		KEY_HELP_NOHEADER),
	FUSE_OPT_KEY("-V",		KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("-d",		FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("debug",		FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("fsname=",		FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("subtype=",	FUSE_OPT_KEY_KEEP),
	FUSE_OPT_END
};

static void usage(const char *progname)
{
	fprintf(stderr,
		"usage: %s mountpoint [options]\n", progname);
	fprintf(stderr,
		"general options:"
		"    -o opt,[opt...]        mount options"
		"    -h   --help            print help"
		"    -V   --version         print version"
		"");
}

static void helper_help(void)
{
	fprintf(stderr,
		"FUSE options:"
		"    -d   -o debug          enable debug output (implies -f)"
		"    -f                     foreground operation"
		"    -s                     disable multi-threaded operation"
		""
		);
}

static void helper_version(void)
{
	libfuse_print("FUSE library version: %s", PACKAGE_VERSION);
}

static int fuse_helper_opt_proc(void *data, const char *arg, int key,
				struct fuse_args *outargs)
{
	struct helper_opts *hopts = data;

	switch (key) {
	case KEY_HELP:

	case KEY_HELP_NOHEADER:
		return fuse_opt_add_arg(outargs, "-h");

	case KEY_VERSION:
		helper_version();
		return 1;

	case FUSE_OPT_KEY_NONOPT:
		if (!hopts->mountpoint) {
			char mountpoint[PATH_MAX];
			strncpy(mountpoint, arg, PATH_MAX); 
			libfuse_print("fuse: mountpoint is %s", mountpoint);
			return fuse_opt_add_opt(&hopts->mountpoint, mountpoint);
		} else {
			libfuse_print("fuse: invalid argument `%s'", arg);
			return -1;
		}

	default:
		return 1;
	}
}

static int add_default_subtype(const char *progname, struct fuse_args *args)
{
	int res;
	char *subtype_opt;
	const char *basename = strrchr(progname, '/');
	if (basename == NULL)
		basename = progname;
	else if (basename[1] != '\0')
		basename++;

	size_t subtype_opt_size = strlen(basename) + 64;
	subtype_opt = (char *) malloc(subtype_opt_size);
	if (subtype_opt == NULL) {
		libfuse_print("fuse: memory allocation failed");
		return -1;
	}

	snprintf(subtype_opt, subtype_opt_size, "-osubtype=%s", basename);
	libfuse_print(subtype_opt);
	res = fuse_opt_add_arg(args, subtype_opt);
	free(subtype_opt);
	return res;
}

int fuse_parse_cmdline(struct fuse_args *args, char **mountpoint,
		       int *multithreaded, int *foreground)
{
	int res;
	struct helper_opts hopts;

	memset(&hopts, 0, sizeof(hopts));
	res = fuse_opt_parse(args, &hopts, fuse_helper_opts,
			     fuse_helper_opt_proc);
	if (res == -1)
		return -1;

	if (!hopts.nodefault_subtype) {
		res = add_default_subtype(args->argv[0], args);
		if (res == -1)
			goto err;
	}
	if (mountpoint)
		*mountpoint = hopts.mountpoint;
	else
		free(hopts.mountpoint);

	if (multithreaded)
		*multithreaded = !hopts.singlethread;
	if (foreground)
		*foreground = hopts.foreground;
	return 0;

err:
	free(hopts.mountpoint);
	return -1;
}

int fuse_daemonize(int foreground)
{
	int res;

	if (!foreground) {
		res = daemon(0, 0);
		if (res == -1) {
			perror("fuse: failed to daemonize program");
			return -1;
		}
	}
	return 0;
}


static struct fuse_chan *fuse_mount_common(const char *mountpoint,
					   struct fuse_args *args, const struct fuse_operations *op)
{
	struct fuse_chan *ch;
	int fd;

	/*
	 * Make sure file descriptors 0, 1 and 2 are open, otherwise chaos
	 * would ensue.
	 */
	do {
		fd = open("/dev/null", O_RDWR);
		if (fd > 2)
			close(fd);
	} while (fd >= 0 && fd <= 2);

	fd = fuse_mount_compat25(mountpoint, args, op);
	if (fd == -1)
		return NULL;

	ch = fuse_kern_chan_new(fd);

	return ch;
}

static void fuse_unmount_common(const char *mountpoint, struct fuse_chan *ch)
{
	//ch ? fuse_chan_fd(ch) : -1;
	//fuse_kern_unmount(mountpoint, fd);
	fuse_chan_destroy(ch);
}

void fuse_unmount(const char *mountpoint, struct fuse_chan *ch)
{
	fuse_unmount_common(mountpoint, ch);
}

struct fuse *fuse_setup_common(int argc, char *argv[],
			       const struct fuse_operations *op,
			       size_t op_size,
			       char **mountpoint,
			       int *multithreaded,
			       int *fd,
			       void *user_data,
			       int compat)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	struct fuse *fuse;
	int foreground;
	int res;

	libfuse_print("libfuse: STARTING %s", __func__);

	res = fuse_parse_cmdline(&args, mountpoint, multithreaded, &foreground);
	if (res == -1){
		libfuse_print("libfuse: Failed to parse cmd args");
		return NULL;
	}

	ch = fuse_mount_common(*mountpoint, &args, op);
	if (!ch) {
		fuse_opt_free_args(&args);
		libfuse_print("libfuse: fuse_mount_common failed");
		goto err_free;
	}

	fuse = fuse_new_common(ch, &args, op, op_size, user_data, compat);
	fuse_opt_free_args(&args);
	if (fuse == NULL){
		libfuse_print("libfuse: fuse_new_common failed");
		goto err_unmount;
	}


	res = fuse_set_signal_handlers(fuse_get_session(fuse));
	if (res == -1){
		libfuse_print("libfuse: fuse_set_signal_handlers failed");
		goto err_unmount;
	}

	if (fd)
		*fd = fuse_chan_fd(ch);

	libfuse_print("[libfuse] fuse: %p: fuse_setup_common() done", fuse);

	return fuse;

err_unmount:
	fuse_unmount_common(*mountpoint, ch);
	libfuse_print("[ERROR] Libfuse unmouting...");
	if (fuse)
		fuse_destroy(fuse);
err_free:
	free(*mountpoint);
	libfuse_print("[ERROR] Libfuse: %s failed", __func__);
	return NULL;
}

struct fuse *fuse_setup(int argc, char *argv[],
			const struct fuse_operations *op, size_t op_size,
			char **mountpoint, int *multithreaded, void *user_data)
{
	return fuse_setup_common(argc, argv, op, op_size, mountpoint,
				 multithreaded, NULL, user_data, 0);
}

static void fuse_teardown_common(struct fuse *fuse, char *mountpoint)
{
	struct fuse_session *se = fuse_get_session(fuse);
	struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
	fuse_remove_signal_handlers(se);
	fuse_unmount_common(mountpoint, ch);
	fuse_destroy(fuse);
	free(mountpoint);
}

void fuse_teardown(struct fuse *fuse, char *mountpoint)
{
	fuse_teardown_common(fuse, mountpoint);
}

static int fuse_main_common(int argc, char *argv[],
			    const struct fuse_operations *op, size_t op_size,
			    void *user_data, int compat)
{
	struct fuse *fuse;
	char *mountpoint;
	int multithreaded;
	int res;

	fuse = fuse_setup_common(argc, argv, op, op_size, &mountpoint,
				 &multithreaded, NULL, user_data, compat);
	if (fuse == NULL)
		return 1;

	res = fuse_loop_mt(fuse);

	fuse_teardown_common(fuse, mountpoint);
	if (res == -1)
		return 1;

	return res;
}

int fuse_main_real(int argc, char *argv[], const struct fuse_operations *op,
		   size_t op_size, void *user_data)
{
	libfuse_print("libfuse: # of args %d", argc);
	return fuse_main_common(argc, argv, op, op_size, user_data, 0);
}

#undef fuse_main
int fuse_main(void);
int fuse_main(void)
{
	libfuse_print("fuse_main(): This function does not exist");
	return -1;
}

int fuse_version(void)
{
	return FUSE_VERSION;
}

#include "fuse_compat.h"

#ifndef __FreeBSD__

struct fuse *fuse_setup_compat22(int argc, char *argv[],
				 const struct fuse_operations_compat22 *op,
				 size_t op_size, char **mountpoint,
				 int *multithreaded, int *fd)
{
	return fuse_setup_common(argc, argv, (struct fuse_operations *) op,
				 op_size, mountpoint, multithreaded, fd, NULL,
				 22);
}

struct fuse *fuse_setup_compat2(int argc, char *argv[],
				const struct fuse_operations_compat2 *op,
				char **mountpoint, int *multithreaded,
				int *fd)
{
	return fuse_setup_common(argc, argv, (struct fuse_operations *) op,
				 sizeof(struct fuse_operations_compat2),
				 mountpoint, multithreaded, fd, NULL, 21);
}

int fuse_main_real_compat22(int argc, char *argv[],
			    const struct fuse_operations_compat22 *op,
			    size_t op_size)
{
	return fuse_main_common(argc, argv, (struct fuse_operations *) op,
				op_size, NULL, 22);
}

void fuse_main_compat1(int argc, char *argv[],
		       const struct fuse_operations_compat1 *op)
{
	fuse_main_common(argc, argv, (struct fuse_operations *) op,
			 sizeof(struct fuse_operations_compat1), NULL, 11);
}

int fuse_main_compat2(int argc, char *argv[],
		      const struct fuse_operations_compat2 *op)
{
	return fuse_main_common(argc, argv, (struct fuse_operations *) op,
				sizeof(struct fuse_operations_compat2), NULL,
				21);
}

int fuse_mount_compat1(const char *mountpoint, const char *args[])
{
	/* just ignore mount args for now */
	(void) args;
	return fuse_mount_compat22(mountpoint, NULL);
}

FUSE_SYMVER(".symver fuse_setup_compat2,__fuse_setup@");
FUSE_SYMVER(".symver fuse_setup_compat22,fuse_setup@FUSE_2.2");
FUSE_SYMVER(".symver fuse_teardown,__fuse_teardown@");
FUSE_SYMVER(".symver fuse_main_compat2,fuse_main@");
FUSE_SYMVER(".symver fuse_main_real_compat22,fuse_main_real@FUSE_2.2");

#endif /* __FreeBSD__ */


struct fuse *fuse_setup_compat25(int argc, char *argv[],
				 const struct fuse_operations_compat25 *op,
				 size_t op_size, char **mountpoint,
				 int *multithreaded, int *fd)
{
	return fuse_setup_common(argc, argv, (struct fuse_operations *) op,
				 op_size, mountpoint, multithreaded, fd, NULL,
				 25);
}

int fuse_main_real_compat25(int argc, char *argv[],
			    const struct fuse_operations_compat25 *op,
			    size_t op_size)
{
	return fuse_main_common(argc, argv, (struct fuse_operations *) op,
				op_size, NULL, 25);
}

void fuse_teardown_compat22(struct fuse *fuse, int fd, char *mountpoint)
{
	(void) fd;
	fuse_teardown_common(fuse, mountpoint);
}


int fuse_mount_compat25(const char *mountpoint, struct fuse_args *args, const struct fuse_operations *op)
{
	return fuse_kern_mount(mountpoint, args, op);
}

FUSE_SYMVER(".symver fuse_setup_compat25,fuse_setup@FUSE_2.5");
FUSE_SYMVER(".symver fuse_teardown_compat22,fuse_teardown@FUSE_2.2");
FUSE_SYMVER(".symver fuse_main_real_compat25,fuse_main_real@FUSE_2.5");
FUSE_SYMVER(".symver fuse_mount_compat25,fuse_mount@FUSE_2.5");
