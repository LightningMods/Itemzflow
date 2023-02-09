/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) by Ronnie Sahlberg <ronniesahlberg@gmail.com> 2013
   ------------------------------------------------------------------
   Edited by LM for PS4 (2023)
   ------------------------------------------------------------------
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
/* A FUSE filesystem based on libnfs. */

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64


#include <fuse/fuse.h>
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "utils.h"
#include <sys/types.h>
#include <orbisNfs.h>

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Whether sockaddr struct has sa_len */
#define HAVE_SOCKADDR_LEN 1

/* Whether we have sockaddr_Storage */
#define HAVE_SOCKADDR_STORAGE 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if `st_mtim.tv_nsec' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif



#ifndef FUSE_STAT
#define FUSE_STAT stat
#endif
bool fuse_debug_flag = false;
/* ============================= END OF CONFIG.H ========================================= */

/* Only one thread at a time can enter libnfs */
static pthread_mutex_t nfs_mutex = PTHREAD_MUTEX_INITIALIZER;

#define discard_const(ptr) ((void *)((intptr_t)(ptr)))

struct nfs_context *nfs = NULL;
int custom_uid = -1;
int custom_gid = -1;

uid_t mount_user_uid;
gid_t mount_user_gid;

int fusenfs_allow_other_own_ids=0;
int fuse_default_permissions=1;
int fuse_multithreads=1;

static int map_uid(int possible_uid) {
    if (custom_uid != -1 && possible_uid == custom_uid){
        return fuse_get_context()->uid;
    }
    return possible_uid;
}

static int map_gid(int possible_gid) {
    if (custom_gid != -1 && possible_gid == custom_gid){
        return fuse_get_context()->gid;
    }
    return possible_gid;
}

static int map_reverse_uid(int possible_uid) {
    if (custom_uid != -1 && possible_uid == getuid()) {
        return custom_uid;
    }
    return possible_uid;
}

static int map_reverse_gid(int possible_gid) {
    if (custom_gid != -1 && possible_gid == getgid()){
        return custom_gid;
    }
    return possible_gid;
}

struct sync_cb_data {
	int is_finished;
	int status;

	void *return_data;
	size_t max_size;
};

static void
wait_for_nfs_reply(struct nfs_context *nfs, struct sync_cb_data *cb_data)
{
	struct pollfd pfd;
	int revents;
	int ret;
	static pthread_mutex_t reply_mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&reply_mutex);
	while (!cb_data->is_finished) {

		pfd.fd = nfs_get_fd(nfs);
		pfd.events = nfs_which_events(nfs);
		pfd.revents = 0;

		ret = poll(&pfd, 1, 100);
		if (ret < 0) {
			revents = -1;
		} else {
			revents = pfd.revents;
		}

		pthread_mutex_lock(&nfs_mutex);
		ret = nfs_service(nfs, revents);
		pthread_mutex_unlock(&nfs_mutex);
		if (ret < 0) {
			cb_data->status = -EIO;
			log_error("nfs_service failed with %d", ret);
			break;
		}
	}
	pthread_mutex_unlock(&reply_mutex);
}

static void
generic_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;
}

/* Update the rpc credentials to the current user unless
 * have are overriding the credentials via url arguments.
 */
static void update_rpc_credentials(void) {
	if (custom_uid == -1  && !fusenfs_allow_other_own_ids) {
		nfs_set_uid(nfs, fuse_get_context()->uid);
	} else if ((custom_uid == -1 ||
                    fuse_get_context()->uid != mount_user_uid)
                   && fusenfs_allow_other_own_ids) {
		nfs_set_uid(nfs, fuse_get_context()->uid);
	} else {
		nfs_set_uid(nfs, custom_uid);
	}
	if (custom_gid == -1 && !fusenfs_allow_other_own_ids) {
		nfs_set_gid(nfs, fuse_get_context()->gid);
        } else if ((custom_gid == -1 ||
                    fuse_get_context()->gid != mount_user_gid)
                   && fusenfs_allow_other_own_ids) {
		nfs_set_gid(nfs, fuse_get_context()->gid);
	} else {
		nfs_set_gid(nfs, custom_gid);
	}
}

static void
stat64_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

    if(fuse_debug_flag)
	   log_debug("stat64_cb status:%d", status);

	if (status < 0) {
		log_error("stat64_cb failed with %d", status);
		return;
	}
	memcpy(cb_data->return_data, data, sizeof(struct nfs_stat_64));
}

static int
fuse_nfs_getattr(const char *path, struct FUSE_STAT *stbuf)
{
	struct nfs_stat_64 st;
	struct sync_cb_data cb_data;
	int ret;

    if(fuse_debug_flag)
    	log_debug("fuse_nfs_getattr entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	cb_data.return_data = &st;

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_lstat64_async(nfs, path, stat64_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_lstat64_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	stbuf->st_dev          = st.nfs_dev;
	stbuf->st_ino          = st.nfs_ino;
	stbuf->st_mode         = st.nfs_mode;
	stbuf->st_nlink        = st.nfs_nlink;
	stbuf->st_uid          = map_uid(st.nfs_uid);
	stbuf->st_gid          = map_gid(st.nfs_gid);
	stbuf->st_rdev         = st.nfs_rdev;
	stbuf->st_size         = st.nfs_size;
	stbuf->st_blksize      = st.nfs_blksize;
	stbuf->st_blocks       = st.nfs_blocks;

	stbuf->st_atim.tv_sec  = st.nfs_atime;
	stbuf->st_atim.tv_nsec = st.nfs_atime_nsec;
	stbuf->st_mtim.tv_sec  = st.nfs_mtime;
	stbuf->st_mtim.tv_nsec = st.nfs_mtime_nsec;
	stbuf->st_ctim.tv_sec  = st.nfs_ctime;
	stbuf->st_ctim.tv_nsec = st.nfs_ctime_nsec;

	return cb_data.status;
}

static void
readdir_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		log_error("readdir_cb status:%d", status);
		return;
	}
	cb_data->return_data = data;
}

static int
fuse_nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi)
{
	struct nfsdir *nfsdir;
	struct nfsdirent *nfsdirent;
	struct sync_cb_data cb_data;
	int ret;

	 if(fuse_debug_flag)
	     log_debug("fuse_nfs_readdir entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
    update_rpc_credentials();
	ret = nfs_opendir_async(nfs, path, readdir_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_opendir_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
	if (cb_data.status){
		log_error("nfs_opendir_async failed with %d", cb_data.status);
		return cb_data.status;
	}

	nfsdir = cb_data.return_data;
	while ((nfsdirent = nfs_readdir(nfs, nfsdir)) != NULL) {
		struct stat st;
		fuse_nfs_getattr(nfsdirent->name, &st);
		filler(buf, nfsdirent->name, &st, 0);
	}

	nfs_closedir(nfs, nfsdir);

	return cb_data.status;
}

static void
readlink_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		log_error("readlink_cb failed with %d", status);
		return;
	}
	strncat(cb_data->return_data, data, cb_data->max_size);
}

static int
fuse_nfs_readlink(const char *path, char *buf, size_t size)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	    log_debug("fuse_nfs_readlink entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	*buf = 0;
	cb_data.return_data = buf;
	cb_data.max_size = size;

	pthread_mutex_lock(&nfs_mutex);
        update_rpc_credentials();
	ret = nfs_readlink_async(nfs, path, readlink_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static void
open_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if(fuse_debug_flag)
	   log_debug("open_cb status:%d", status);

	if (status < 0) {
		log_error("open_cb status:%d", status);
		return;
	}
	cb_data->return_data = data;
}

static int
fuse_nfs_open(const char *path, struct fuse_file_info *fi)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_open entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
        update_rpc_credentials();

	fi->fh = 0;
        ret = nfs_open_async(nfs, path, fi->flags, open_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_info("nfs_open_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	fi->fh = (uint64_t)cb_data.return_data;

	return cb_data.status;
}

static int fuse_nfs_release(const char *path, struct fuse_file_info *fi)
{
	struct sync_cb_data cb_data;
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;

	if(fuse_debug_flag)
	   log_debug("fuse_nfs_release entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	nfs_close_async(nfs, nfsfh, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	wait_for_nfs_reply(nfs, &cb_data);

	return 0;
}

static void
read_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		log_error("read_cb status:%d", status);
		return;
	}
	memcpy(cb_data->return_data, data, status);
}

static int
fuse_nfs_read(const char *path, char *buf, size_t size,
	      off_t offset, struct fuse_file_info *fi)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;
	struct sync_cb_data cb_data;
	int ret;

	if(fuse_debug_flag)
	   log_debug("fuse_nfs_read entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	cb_data.return_data = buf;

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_pread_async(nfs, nfsfh, offset, size, read_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_info("nfs_pread_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_write(const char *path, const char *buf, size_t size,
       off_t offset, struct fuse_file_info *fi)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;
	struct sync_cb_data cb_data;
	int ret;

	if(fuse_debug_flag)
	   log_debug("fuse_nfs_write entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
        update_rpc_credentials();
	ret = nfs_pwrite_async(nfs, nfsfh, offset, size, discard_const(buf),
			       generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_info("nfs_pwrite_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	struct sync_cb_data cb_data;
	int ret = 0;
   if(fuse_debug_flag)
	   log_debug("fuse_nfs_create entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_creat_async(nfs, path, mode, open_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_info("nfs_creat_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	fi->fh = (uint64_t)cb_data.return_data;
	
	return cb_data.status;
}

static int fuse_nfs_utime(const char *path, struct utimbuf *times)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)     
    	log_debug("fuse_nfs_utime entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_utime_async(nfs, path, times, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
                log_error("fuse_nfs_utime returned %d. %s", ret,
                    nfs_get_error(nfs));
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_unlink(const char *path)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_unlink entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
        ret = nfs_unlink_async(nfs, path, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_unlink_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_rmdir(const char *path)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_mknod entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_rmdir_async(nfs, path, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_rmdir_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int
fuse_nfs_mkdir(const char *path, mode_t mode)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	    log_debug("fuse_nfs_mkdir entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_mkdir_async(nfs, path, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_mkdir_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	cb_data.is_finished = 0;

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_chmod_async(nfs, path, mode, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_mkdir_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	    log_debug("fuse_nfs_mknod entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_mknod_async(nfs, path, mode, rdev, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_mknod_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_symlink(const char *from, const char *to)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_symlink entered [%s -> %s]", from, to);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_symlink_async(nfs, from, to, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_symlink_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int fuse_nfs_rename(const char *from, const char *to)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_rename entered [%s -> %s]", from, to);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_rename_async(nfs, from, to, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_rename_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int
fuse_nfs_link(const char *from, const char *to)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	    log_debug("fuse_nfs_link entered [%s -> %s]", from, to);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_link_async(nfs, from, to, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_link_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
	
	return cb_data.status;
}

static int
fuse_nfs_chmod(const char *path, mode_t mode)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	   log_debug("fuse_nfs_chmod entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_chmod_async(nfs, path, mode, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_chmod_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
	
	return cb_data.status;
}

static int fuse_nfs_chown(const char *path, uid_t uid, gid_t gid)
{
	struct sync_cb_data cb_data;
	int ret;
    if(fuse_debug_flag)
	    log_debug("fuse_nfs_chown entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_chown_async(nfs, path,
			      map_reverse_uid(uid), map_reverse_gid(gid),
			      generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_chown_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
	
	return cb_data.status;
}

static int fuse_nfs_truncate(const char *path, off_t size)
{
	struct sync_cb_data cb_data;
	int ret;

	if(fuse_debug_flag)
	   log_debug("fuse_nfs_truncate entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
	ret = nfs_truncate_async(nfs, path, size, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_truncate_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);

	return cb_data.status;
}

static int
fuse_nfs_fsync(const char *path, int isdatasync,
	       struct fuse_file_info *fi)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;
	struct sync_cb_data cb_data;
	int ret;

    //log_info("fuse_nfs_fsync entered [%s]", path);
    memset(&cb_data, 0, sizeof(struct sync_cb_data));

	pthread_mutex_lock(&nfs_mutex);
	update_rpc_credentials();
        ret = nfs_fsync_async(nfs, nfsfh, generic_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_fsync_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
	
	return cb_data.status;
}

static void
statvfs_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
        log_error("statvfs_cb failed with %d", status);
		return;
	}
	memcpy(cb_data->return_data, data, sizeof(struct statvfs));
}

static int
fuse_nfs_statfs(const char *path, struct statvfs* stbuf)
{
        int ret;
        struct statvfs svfs;

	struct sync_cb_data cb_data;

    if(fuse_debug_flag)
	   log_debug("fuse_nfs_statfs entered [%s]", path);

    memset(&cb_data, 0, sizeof(struct sync_cb_data));
	cb_data.return_data = &svfs;

	pthread_mutex_lock(&nfs_mutex);
    ret = nfs_statvfs_async(nfs, path, statvfs_cb, &cb_data);
	pthread_mutex_unlock(&nfs_mutex);
	if (ret < 0) {
		log_error("nfs_statvfs_async failed with %d", ret);
		return ret;
	}
	wait_for_nfs_reply(nfs, &cb_data);
  
        stbuf->f_bsize      = svfs.f_bsize;
        stbuf->f_frsize     = svfs.f_frsize;
        stbuf->f_fsid       = svfs.f_fsid;
        stbuf->f_flag       = svfs.f_flag;
        stbuf->f_namemax    = svfs.f_namemax;
        stbuf->f_blocks     = svfs.f_blocks;
        stbuf->f_bfree      = svfs.f_bfree;
        stbuf->f_bavail     = svfs.f_bavail;
        stbuf->f_files      = svfs.f_files;
        stbuf->f_ffree      = svfs.f_ffree;
        stbuf->f_favail     = svfs.f_favail;



	return cb_data.status;
}

static struct fuse_operations nfs_oper = {
	.chmod		= fuse_nfs_chmod,
	.chown		= fuse_nfs_chown,
	.create		= fuse_nfs_create,
	.fsync		= fuse_nfs_fsync,
	.getattr	= fuse_nfs_getattr,
	.link		= fuse_nfs_link,
	.mkdir		= fuse_nfs_mkdir,
	.mknod		= fuse_nfs_mknod,
	.open		= fuse_nfs_open,
	.read		= fuse_nfs_read,
	.readdir	= fuse_nfs_readdir,
	.readlink	= fuse_nfs_readlink,
	.release	= fuse_nfs_release,
	.rmdir		= fuse_nfs_rmdir,
	.unlink		= fuse_nfs_unlink,
	.utime		= fuse_nfs_utime,
	.rename		= fuse_nfs_rename,
	.symlink	= fuse_nfs_symlink,
	.truncate	= fuse_nfs_truncate,
	.write		= fuse_nfs_write,
    .statfs 	= fuse_nfs_statfs,
	//.access	= fuse_nfs_access,
};

void *fuse_main_init(void *args)
{
    struct fuse_actions *fuse_args = (struct fuse_actions *)args;
    int ret;

   if(fuse_args->is_debug_mode)
       log_debug("fuse_main_init DEBUG entered %d", fuse_args->argc);
	else
	   fuse_args->argc--;

    char* argv[] = { 
		"dummy", 
		 fuse_args->path,
		"-o", 
		"allow_other",
		"-o", "rw",
		"-o", "use_ino",
		"-o", "direct_io", 
		"-o", "attr_timeout=0.0", 
		"-o", "max_read=262144", 
		"-f", "-d", NULL };

    ret = fuse_main(fuse_args->argc, argv, &nfs_oper, NULL);
    if (ret < 0) {
        log_error("fuse_main failed");
    }

    free(fuse_args);
    log_info("fuse_main_init exited with status %d, err %s", ret, strerror(errno));

    return NULL;
}

int fuse_nfs_main(struct fuse_actions args)
{

    int ret = 0;
    struct nfs_url *urls = NULL;
    //fuse_thread
    ScePthread fuse_thread;

	mount_user_uid=getuid();
	mount_user_gid=getgid();

    char nfs_ip[0x100];
	if (strncmp(args.ip, "nfs://", strlen("nfs://")) == 0) {
       log_debug("String is in NFS format.");
	   snprintf(nfs_ip, sizeof(nfs_ip)-1, "%s", args.ip);
    } else {
       log_debug("String is not in NFS format.");
	   snprintf(nfs_ip, sizeof(nfs_ip)-1, "nfs://%s/", args.ip);
    }

	log_info("NFS [%s]", nfs_ip);

	nfs = nfs_init_context();
	if (nfs == NULL) {
		log_error("Failed to init NFS context");
		ret = 10;
		goto finished;
	}

	urls = nfs_parse_url_dir(nfs, nfs_ip);
	if (urls == NULL) {
        log_error("Failed to parse url : %s", nfs_get_error(nfs));
		ret = 10;
		goto finished;
	}

	ret = nfs_mount(nfs, urls->server, urls->path);
	if (ret != 0) {
        log_error("Failed to mount nfs share : %s", nfs_get_error(nfs));
		goto finished;
	}

    struct fuse_actions *fa = malloc(sizeof(struct fuse_actions));
    fa->argc = args.argc;
	fa->is_debug_mode = args.is_debug_mode;
	snprintf(fa->path, 40, "%s", args.path);

    if(fa->is_debug_mode)
		log_debug("Starting fuse_main in debug mode");

	scePthreadCreate(&fuse_thread, NULL, fuse_main_init, fa, "FUSE_Main_Thread");
    return 0;

finished:
	nfs_destroy_url(urls);
	if (nfs != NULL) {
		nfs_destroy_context(nfs);
	}
    log_error("Failed to mount nfs share : %i", ret);
	return ret;
}