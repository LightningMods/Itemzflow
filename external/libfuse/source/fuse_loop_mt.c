/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "fuse_lowlevel.h"
#include "fuse_misc.h"
#include "fuse_kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fuse.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#define FUSE_DEBUG 0
#define FUSE_WORKER_THREAD_MAX    4

struct fuse_worker {
	struct fuse_worker *prev;
	struct fuse_worker *next;
	pthread_t thread_id;
	size_t bufsize;
	char *buf;
	struct fuse_mt *mt;
};

struct fuse_mt {
	pthread_mutex_t lock;
	int numworker;
	int numavail;
	struct fuse_session *se;
	struct fuse_chan *prevch;
	struct fuse_worker main;
	sem_t finish;
	int exit;
	int error;
};

static void list_add_worker(struct fuse_worker *w, struct fuse_worker *next)
{
	struct fuse_worker *prev = next->prev;
	w->next = next;
	w->prev = prev;
	prev->next = w;
	next->prev = w;
}

static void list_del_worker(struct fuse_worker *w)
{
	struct fuse_worker *prev = w->prev;
	struct fuse_worker *next = w->next;
	prev->next = next;
	next->prev = prev;
}

#define FUSE_PRIO_WORK 903
int sceKernelSetBesteffort(int besteffort, int priority);
static int fuse_start_thread(struct fuse_mt *mt);

static void *fuse_do_work(void *data)
{
	struct fuse_worker *w = (struct fuse_worker *) data;
	struct fuse_mt *mt = w->mt;

	sceKernelSetBesteffort(FUSE_PRIO_WORK, 255);

	while (!fuse_session_exited(mt->se)) {
		int isforget = 0;
		struct fuse_chan *ch = mt->prevch;
		int res;
#if FUSE_DEBUG==1
		libfuse_print("fuse_do_work");
#endif

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		res = fuse_chan_recv(&ch, w->buf, w->bufsize);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (res == -EINTR)
			continue;
		if (res <= 0) {
			if (res < 0) {
				fuse_session_exit(mt->se);
				libfuse_print("fuse error exit");
				mt->error = -1;
			}
			break;
		}

		pthread_mutex_lock(&mt->lock);
		if (mt->exit) {
			libfuse_print("exiting...");
			pthread_mutex_unlock(&mt->lock);
			return NULL;
		}

		/*
		 * This disgusting hack is needed so that zillions of threads
		 * are not created on a burst of FORGET messages
		 */
		if (((struct fuse_in_header *) w->buf)->opcode == FUSE_FORGET)
			isforget = 1;


		if (!isforget)
			mt->numavail--;
		if (mt->numavail == 0 && mt->numworker < FUSE_WORKER_THREAD_MAX)
			fuse_start_thread(mt);
		pthread_mutex_unlock(&mt->lock);

		fuse_session_process(mt->se, w->buf, res, ch);
#if FUSE_DEBUG==1
		libfuse_print("processing...");
#endif

		pthread_mutex_lock(&mt->lock);
		if (!isforget)
			mt->numavail++;
		if (mt->numavail > 3) {
			if (mt->exit) {
				libfuse_print("exiting...");
				pthread_mutex_unlock(&mt->lock);
				return NULL;
			}
			list_del_worker(w);
			mt->numavail--;
			mt->numworker--;
			pthread_mutex_unlock(&mt->lock);

			pthread_detach(w->thread_id);
			free(w->buf);
			free(w);
			libfuse_print("exiting...");
			return NULL;
		}
		pthread_mutex_unlock(&mt->lock);
	}

	sem_post(&mt->finish);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pause();
	libfuse_print("exiting thr...");

	return NULL;
}

static int fuse_start_thread(struct fuse_mt *mt)
{

	sigset_t oldset;
	sigset_t newset;
	int res;
	struct fuse_worker *w = malloc(sizeof(struct fuse_worker));
	if (!w) {
		libfuse_print("fuse: failed to allocate worker structure");
		return -1;
	}
	memset(w, 0, sizeof(struct fuse_worker));
	w->bufsize = fuse_chan_bufsize(mt->prevch);
	w->buf = malloc(w->bufsize);
	w->mt = mt;
	if (!w->buf) {
		libfuse_print("fuse: failed to allocate read buffer");
		free(w);
		return -1;
	}
	
	sigemptyset(&newset);
	sigaddset(&newset, SIGTERM);
	sigaddset(&newset, SIGINT);
	sigaddset(&newset, SIGHUP);
	sigaddset(&newset, SIGQUIT);
	pthread_sigmask(SIG_BLOCK, &newset, &oldset);

	char Thrname[THREAD_NAME_MAXLEN];
	snprintf(Thrname, THREAD_NAME_MAXLEN, "FUSE_Daemon_Thread_%i", mt->numworker);
#if FUSE_DEBUG==1
    libfuse_print("fuse: create thread: %s", Thrname);
#endif
	res = scePthreadCreate((ScePthread*)&w->thread_id, NULL, fuse_do_work, w, Thrname);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	if (res != 0) {
		libfuse_print("fuse: error creating thread: %s",
			strerror(res));
		free(w->buf);
		free(w);
		return -1;
	}
	list_add_worker(w, &mt->main);
	mt->numavail ++;
	mt->numworker ++;

	return 0;
}

static void fuse_join_worker(struct fuse_mt *mt, struct fuse_worker *w)
{
//	pthread_join(w->thread_id, NULL);
	scePthreadJoin(w->thread_id, NULL);
	pthread_mutex_lock(&mt->lock);
	list_del_worker(w);
	pthread_mutex_unlock(&mt->lock);
	free(w->buf);
	free(w);
}

int fuse_session_loop_mt(struct fuse_session *se)
{
	int err;
	struct fuse_mt mt;
	struct fuse_worker *w;

	memset(&mt, 0, sizeof(struct fuse_mt));
	mt.se = se;
	mt.prevch = fuse_session_next_chan(se, NULL);
	mt.error = 0;
	mt.numworker = 0;
	mt.numavail = 0;
	mt.main.thread_id = pthread_self();
	mt.main.prev = mt.main.next = &mt.main;
	sem_init(&mt.finish, 0, 0);
	fuse_mutex_init(&mt.lock);

	pthread_mutex_lock(&mt.lock);
	err = fuse_start_thread(&mt);
	pthread_mutex_unlock(&mt.lock);
	if (!err) {
		/* sem_wait() is interruptible */
		while (!fuse_session_exited(se))
			sem_wait(&mt.finish);

		for (w = mt.main.next; w != &mt.main; w = w->next)
			scePthreadCancel(w->thread_id);

		mt.exit = 1;
		pthread_mutex_unlock(&mt.lock);

		while (mt.main.next != &mt.main)
			fuse_join_worker(&mt, mt.main.next);

		err = mt.error;
	}

	pthread_mutex_destroy(&mt.lock);
	sem_destroy(&mt.finish);
	fuse_session_reset(se);
	return err;
}
