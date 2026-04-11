/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_WORKER_H
#define RENDER_WORKER_H

#include "render_common.h"

enum render_worker_jail_seccomp_filter {
   /* seccomp_path is ignored and seccomp is disabled */
   RENDER_WORKER_JAIL_SECCOMP_NONE,
   /* seccomp_path is a file containing a BPF program */
   RENDER_WORKER_JAIL_SECCOMP_BPF,
   /* seccomp_path is a file containing a minijail policy */
   RENDER_WORKER_JAIL_SECCOMP_MINIJAIL_POLICY,
   RENDER_WORKER_JAIL_SECCOMP_MINIJAIL_POLICY_LOG,
};

struct render_worker_jail *
render_worker_jail_create(int max_worker_count,
                          enum render_worker_jail_seccomp_filter seccomp_filter,
                          const char *seccomp_path);

void
render_worker_jail_destroy(struct render_worker_jail *jail);

int
render_worker_jail_get_sigchld_fd(const struct render_worker_jail *jail);

bool
render_worker_jail_reap_workers(struct render_worker_jail *jail);

void
render_worker_jail_detach_workers(struct render_worker_jail *jail);

struct render_worker *
render_worker_create(struct render_worker_jail *jail,
                     int (*thread_func)(void *thread_data),
                     void *thread_data,
                     size_t thread_data_size);

void
render_worker_destroy(struct render_worker_jail *jail, struct render_worker *worker);

bool
render_worker_is_record(const struct render_worker *worker);

#endif /* RENDER_WORKER_H */
