/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "config.h"
#include <pthread.h>

/* 
Modified by T.MOMOZONO,  08/05/2016

---------------
Copyright (c) 2016 Sony Interactive Entertainment Inc. All Rights Reserved. 
---------------

*/
#define THREAD_NAME_MAXLEN (32)
#define SCE_ULPMGR_OBJNAME_DECI4H_PREFIX			"SceDeci4h"
#define SCE_ULPMGR_OBJNAME_DECI4H_PREFIX_DRFP		SCE_ULPMGR_OBJNAME_DECI4H_PREFIX "Drfp"
#define SCE_DRFP_FUSEWORKTHREAD_NAME SCE_ULPMGR_OBJNAME_DECI4H_PREFIX_DRFP "Fus%d"
#define SCE_ULPMGR_THREAD_PRIO_BASE		(903)

/* Versioned symbols confuse the dynamic linker in uClibc */
#ifndef __UCLIBC__
#define FUSE_SYMVER(x) __asm__(x)
#else
#define FUSE_SYMVER(x)
#endif

#ifndef USE_UCLIBC
#define fuse_mutex_init(mut) pthread_mutex_init(mut, NULL)
#else
/* Is this hack still needed? */
static inline void fuse_mutex_init(pthread_mutex_t *mut)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
	pthread_mutex_init(mut, &attr);
	pthread_mutexattr_destroy(&attr);
}
#endif

#ifdef HAVE_STRUCT_STAT_ST_ATIM
/* Linux */
#define ST_ATIM_NSEC(stbuf) ((stbuf)->st_atim.tv_nsec)
#define ST_CTIM_NSEC(stbuf) ((stbuf)->st_ctim.tv_nsec)
#define ST_MTIM_NSEC(stbuf) ((stbuf)->st_mtim.tv_nsec)
#define ST_ATIM_NSEC_SET(stbuf, val) (stbuf)->st_atim.tv_nsec = (val)
#define ST_MTIM_NSEC_SET(stbuf, val) (stbuf)->st_mtim.tv_nsec = (val)
#elif defined(HAVE_STRUCT_STAT_ST_ATIMESPEC)
/* FreeBSD */
#define ST_ATIM_NSEC(stbuf) ((stbuf)->st_atimespec.tv_nsec)
#define ST_CTIM_NSEC(stbuf) ((stbuf)->st_ctimespec.tv_nsec)
#define ST_MTIM_NSEC(stbuf) ((stbuf)->st_mtimespec.tv_nsec)
#define ST_ATIM_NSEC_SET(stbuf, val) (stbuf)->st_atimespec.tv_nsec = (val)
#define ST_MTIM_NSEC_SET(stbuf, val) (stbuf)->st_mtimespec.tv_nsec = (val)
#else
#define ST_ATIM_NSEC(stbuf) 0
#define ST_CTIM_NSEC(stbuf) 0
#define ST_MTIM_NSEC(stbuf) 0
#define ST_ATIM_NSEC_SET(stbuf, val) do { } while (0)
#define ST_MTIM_NSEC_SET(stbuf, val) do { } while (0)
#endif
