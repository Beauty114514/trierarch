/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_SERVER_H
#define RENDER_SERVER_H

#include "render_common.h"

enum render_server_state {
   RENDER_SERVER_STATE_RUN,
   RENDER_SERVER_STATE_SUBPROCESS,
};

struct render_server {
   enum render_server_state state;

   /* only initialized in subprocesses */
   struct render_context_args *context_args;

   /* options */
   int client_fd;
   const char *worker_seccomp_bpf;
   const char *worker_seccomp_minijail_policy;
   bool worker_seccomp_minijail_log;

   struct render_worker_jail *worker_jail;

   /* only one client in the current design */
   struct render_client *client;
};

bool
render_server_main(int argc, char **argv, struct render_context_args *ctx_args);

#endif /* RENDER_SERVER_H */
