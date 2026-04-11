/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "render_worker.h"

/* One and only one of ENABLE_RENDER_SERVER_WORKER_* must be set.
 *
 * With ENABLE_RENDER_SERVER_WORKER_PROCESS, each worker is a subprocess
 * forked from the server process.
 *
 * With ENABLE_RENDER_SERVER_WORKER_THREAD, each worker is a thread of the
 * server process.
 *
 * With ENABLE_RENDER_SERVER_WORKER_MINIJAIL, each worker is a subprocess
 * forked from the server process, jailed with minijail.
 */
#if (ENABLE_RENDER_SERVER_WORKER_PROCESS + ENABLE_RENDER_SERVER_WORKER_THREAD +          \
     ENABLE_RENDER_SERVER_WORKER_MINIJAIL) != 1
#error "no worker defined"
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <unistd.h>

struct minijail;

struct render_worker_jail {
   int max_worker_count;

   int sigchld_fd;
   struct minijail *minijail;

   struct list_head workers;
   int worker_count;
};

struct render_worker {
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   thrd_t thread;
#else
   pid_t pid;
#endif
   bool destroyed;
   bool reaped;

   struct list_head head;

   char thread_data[];
};

#ifdef ENABLE_RENDER_SERVER_WORKER_MINIJAIL

#include <fcntl.h>
#include <libminijail.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stdio.h>
#include <sys/stat.h>

static bool
load_bpf_program(struct sock_fprog *prog, const char *path)
{
   int fd = -1;
   void *data = NULL;

   fd = open(path, O_RDONLY);
   if (fd < 0)
      goto fail;

   const off_t size = lseek(fd, 0, SEEK_END);
   if (size <= 0 || size % sizeof(struct sock_filter))
      goto fail;
   lseek(fd, 0, SEEK_SET);

   data = malloc(size);
   if (!data)
      goto fail;

   off_t cur = 0;
   while (cur < size) {
      const ssize_t r = read(fd, (char *)data + cur, size - cur);
      if (r <= 0)
         goto fail;
      cur += r;
   }

   close(fd);

   prog->len = size / sizeof(struct sock_filter);
   prog->filter = data;

   return true;

fail:
   if (data)
      free(data);
   if (fd >= 0)
      close(fd);
   return false;
}

static struct minijail *
create_minijail(enum render_worker_jail_seccomp_filter seccomp_filter,
                const char *seccomp_path)
{
   struct minijail *j = minijail_new();

   /* TODO namespaces and many more */
   minijail_no_new_privs(j);

   if (seccomp_filter != RENDER_WORKER_JAIL_SECCOMP_NONE) {
      if (seccomp_filter == RENDER_WORKER_JAIL_SECCOMP_BPF) {
         struct sock_fprog prog;
         if (!load_bpf_program(&prog, seccomp_path)) {
            minijail_destroy(j);
            return NULL;
         }

         minijail_set_seccomp_filters(j, &prog);
         free(prog.filter);
      } else {
         if (seccomp_filter == RENDER_WORKER_JAIL_SECCOMP_MINIJAIL_POLICY_LOG)
            minijail_log_seccomp_filter_failures(j);
         minijail_parse_seccomp_filters(j, seccomp_path);
      }

      minijail_use_seccomp_filter(j);
   }

   return j;
}

static pid_t
fork_minijail(const struct minijail *template)
{
   struct minijail *j = minijail_new();
   if (!j)
      return -1;

   /* is this faster? */
   if (minijail_copy_jail(template, j)) {
      minijail_destroy(j);
      return -1;
   }

   pid_t pid = minijail_fork(j);
   minijail_destroy(j);

   return pid;
}

#endif /* ENABLE_RENDER_SERVER_WORKER_MINIJAIL */

#ifndef ENABLE_RENDER_SERVER_WORKER_THREAD

static int
create_sigchld_fd(void)
{
   /* restore the ability of waitid() to catch SIGCHLD, in case parent disabled
    * it (e.g. virgl_test_server) */
   struct sigaction sa = { 0 };
   sa.sa_handler = SIG_DFL;
   int ret = sigaction(SIGCHLD, &sa, NULL);
   if (ret) {
      render_log("failed to set sigaction for SIGCHLD");
      return ret;
   }

   const int signum = SIGCHLD;

   sigset_t set;
   if (sigemptyset(&set) || sigaddset(&set, signum)) {
      render_log("failed to initialize sigset_t");
      return -1;
   }

   int fd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
   if (fd < 0) {
      render_log("failed to create signalfd");
      return -1;
   }

   if (sigprocmask(SIG_BLOCK, &set, NULL)) {
      render_log("failed to call sigprocmask");
      close(fd);
      return -1;
   }

   return fd;
}

#endif /* !ENABLE_RENDER_SERVER_WORKER_THREAD */

static void
render_worker_jail_add_worker(struct render_worker_jail *jail,
                              struct render_worker *worker)
{
   list_add(&worker->head, &jail->workers);
   jail->worker_count++;
}

static void
render_worker_jail_remove_worker(struct render_worker_jail *jail,
                                 struct render_worker *worker)
{
   list_del(&worker->head);
   jail->worker_count--;

   free(worker);
}

static struct render_worker *
render_worker_jail_reap_any_worker(struct render_worker_jail *jail, bool block)
{
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   (void)jail;
   (void)block;
   return NULL;
#else
   const int options = WEXITED | (block ? 0 : WNOHANG);
   siginfo_t siginfo = { 0 };
   const int ret = waitid(P_ALL, 0, &siginfo, options);
   const pid_t pid = ret ? 0 : siginfo.si_pid;
   if (!pid)
      return NULL;

   list_for_each_entry (struct render_worker, worker, &jail->workers, head) {
      if (worker->pid == pid) {
         worker->reaped = true;
         return worker;
      }
   }

   render_log("unknown child process %d", pid);
   return NULL;
#endif
}

struct render_worker_jail *
render_worker_jail_create(int max_worker_count,
                          enum render_worker_jail_seccomp_filter seccomp_filter,
                          const char *seccomp_path)
{
   struct render_worker_jail *jail = calloc(1, sizeof(*jail));
   if (!jail)
      return NULL;

   jail->max_worker_count = max_worker_count;
   jail->sigchld_fd = -1;
   list_inithead(&jail->workers);

#ifndef ENABLE_RENDER_SERVER_WORKER_THREAD
   jail->sigchld_fd = create_sigchld_fd();
   if (jail->sigchld_fd < 0)
      goto fail;
#endif

#if defined(ENABLE_RENDER_SERVER_WORKER_MINIJAIL)
   jail->minijail = create_minijail(seccomp_filter, seccomp_path);
   if (!jail->minijail)
      goto fail;
#else
   /* TODO RENDER_WORKER_JAIL_SECCOMP_BPF */
   if (seccomp_filter != RENDER_WORKER_JAIL_SECCOMP_NONE)
      goto fail;
   (void)seccomp_path;
#endif

   return jail;

fail:
   free(jail);
   return NULL;
}

static void
render_worker_jail_wait_workers(struct render_worker_jail *jail)
{
   while (jail->worker_count) {
      struct render_worker *worker =
         render_worker_jail_reap_any_worker(jail, true /* block */);
      if (worker) {
         assert(worker->destroyed && worker->reaped);
         render_worker_jail_remove_worker(jail, worker);
      }
   }
}

void
render_worker_jail_destroy(struct render_worker_jail *jail)
{
   render_worker_jail_wait_workers(jail);

#if defined(ENABLE_RENDER_SERVER_WORKER_MINIJAIL)
   minijail_destroy(jail->minijail);
#endif

   if (jail->sigchld_fd >= 0)
      close(jail->sigchld_fd);

   free(jail);
}

int
render_worker_jail_get_sigchld_fd(const struct render_worker_jail *jail)
{
   return jail->sigchld_fd;
}

static bool
render_worker_jail_drain_sigchld_fd(struct render_worker_jail *jail)
{
   if (jail->sigchld_fd < 0)
      return true;

   do {
      struct signalfd_siginfo siginfos[8];
      const ssize_t r = read(jail->sigchld_fd, siginfos, sizeof(siginfos));
      if (r == sizeof(siginfos))
         continue;
      if (r > 0 || (r < 0 && errno == EAGAIN))
         break;

      render_log("failed to read signalfd");
      return false;
   } while (true);

   return true;
}

bool
render_worker_jail_reap_workers(struct render_worker_jail *jail)
{
   if (!render_worker_jail_drain_sigchld_fd(jail))
      return false;

   do {
      struct render_worker *worker =
         render_worker_jail_reap_any_worker(jail, false /* block */);
      if (!worker)
         break;

      assert(worker->reaped);
      if (worker->destroyed)
         render_worker_jail_remove_worker(jail, worker);
   } while (true);

   return true;
}

void
render_worker_jail_detach_workers(struct render_worker_jail *jail)
{
   /* free workers without killing nor reaping */
   list_for_each_entry_safe (struct render_worker, worker, &jail->workers, head)
      render_worker_jail_remove_worker(jail, worker);
}

struct render_worker *
render_worker_create(struct render_worker_jail *jail,
                     int (*thread_func)(void *thread_data),
                     void *thread_data,
                     size_t thread_data_size)
{
   if (jail->worker_count >= jail->max_worker_count) {
      render_log("too many workers");
      return NULL;
   }

   struct render_worker *worker = calloc(1, sizeof(*worker) + thread_data_size);
   if (!worker)
      return NULL;

   memcpy(worker->thread_data, thread_data, thread_data_size);

   bool ok;
#if defined(ENABLE_RENDER_SERVER_WORKER_PROCESS)
   worker->pid = fork();
   ok = worker->pid >= 0;
   (void)thread_func;
#elif defined(ENABLE_RENDER_SERVER_WORKER_THREAD)
   ok = thrd_create(&worker->thread, thread_func, worker->thread_data) == thrd_success;
#elif defined(ENABLE_RENDER_SERVER_WORKER_MINIJAIL)
   worker->pid = fork_minijail(jail->minijail);
   ok = worker->pid >= 0;
   (void)thread_func;
#endif
   if (!ok) {
      free(worker);
      return NULL;
   }

   render_worker_jail_add_worker(jail, worker);

   return worker;
}

void
render_worker_destroy(struct render_worker_jail *jail, struct render_worker *worker)
{
   assert(render_worker_is_record(worker));

#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   /* we trust the thread to clean up and exit in finite time */
   thrd_join(worker->thread, NULL);
   worker->reaped = true;
#else
   /* kill to make sure the worker exits in finite time */
   if (!worker->reaped)
      kill(worker->pid, SIGKILL);
#endif

   worker->destroyed = true;

   if (worker->reaped)
      render_worker_jail_remove_worker(jail, worker);
}

bool
render_worker_is_record(const struct render_worker *worker)
{
   /* return false if called from the worker itself */
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   return !thrd_equal(worker->thread, thrd_current());
#else
   return worker->pid > 0;
#endif
}
