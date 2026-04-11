/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "render_server.h"

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <unistd.h>

#include "render_client.h"
#include "render_worker.h"

#define RENDER_SERVER_MAX_WORKER_COUNT 256

enum render_server_poll_type {
   RENDER_SERVER_POLL_SOCKET = 0,
   RENDER_SERVER_POLL_SIGCHLD /* optional */,
   RENDER_SERVER_POLL_COUNT,
};

static int
render_server_init_poll_fds(struct render_server *srv,
                            struct pollfd poll_fds[static RENDER_SERVER_POLL_COUNT])
{
   const int socket_fd = srv->client->socket.fd;
   const int sigchld_fd = render_worker_jail_get_sigchld_fd(srv->worker_jail);

   poll_fds[RENDER_SERVER_POLL_SOCKET] = (const struct pollfd){
      .fd = socket_fd,
      .events = POLLIN,
   };
   poll_fds[RENDER_SERVER_POLL_SIGCHLD] = (const struct pollfd){
      .fd = sigchld_fd,
      .events = POLLIN,
   };

   return sigchld_fd >= 0 ? 2 : 1;
}

static bool
render_server_poll(UNUSED struct render_server *srv,
                   struct pollfd *poll_fds,
                   int poll_fd_count)
{
   int ret;
   do {
      ret = poll(poll_fds, poll_fd_count, -1);
   } while (ret < 0 && (errno == EINTR || errno == EAGAIN));

   if (ret <= 0) {
      render_log("failed to poll in the main loop");
      return false;
   }

   return true;
}

static bool
render_server_run(struct render_server *srv)
{
   struct render_client *client = srv->client;

   struct pollfd poll_fds[RENDER_SERVER_POLL_COUNT];
   const int poll_fd_count = render_server_init_poll_fds(srv, poll_fds);

   while (srv->state == RENDER_SERVER_STATE_RUN) {
      if (!render_server_poll(srv, poll_fds, poll_fd_count))
         return false;

      if (poll_fds[RENDER_SERVER_POLL_SOCKET].revents) {
         if (!render_client_dispatch(client))
            return false;
      }

      if (poll_fds[RENDER_SERVER_POLL_SIGCHLD].revents) {
         if (!render_worker_jail_reap_workers(srv->worker_jail))
            return false;
      }
   }

   return true;
}

static void
render_server_fini(struct render_server *srv)
{
   if (srv->client)
      render_client_destroy(srv->client);

   if (srv->worker_jail)
      render_worker_jail_destroy(srv->worker_jail);

   if (srv->client_fd >= 0)
      close(srv->client_fd);
}

static bool
render_server_parse_options(struct render_server *srv, int argc, char **argv)
{
   enum {
      OPT_SOCKET_FD = 'a',
      OPT_WORKER_SECCOMP_BPF,
      OPT_WORKER_SECCOMP_MINIJAIL_POLICY,
      OPT_WORKER_SECCOMP_MINIJAIL_LOG,
      OPT_COUNT,
   };
   static const struct option options[] = {
      { "socket-fd", required_argument, NULL, OPT_SOCKET_FD },
      { "worker-seccomp-bpf", required_argument, NULL, OPT_WORKER_SECCOMP_BPF },
      { "worker-seccomp-minijail-policy", required_argument, NULL,
        OPT_WORKER_SECCOMP_MINIJAIL_POLICY },
      { "worker-seccomp-minijail-log", no_argument, NULL,
        OPT_WORKER_SECCOMP_MINIJAIL_LOG },
      { NULL, 0, NULL, 0 }
   };
   static_assert(OPT_COUNT <= 'z', "");

   while (true) {
      const int ret = getopt_long(argc, argv, "", options, NULL);
      if (ret == -1)
         break;

      switch (ret) {
      case OPT_SOCKET_FD:
         srv->client_fd = atoi(optarg);
         break;
      case OPT_WORKER_SECCOMP_BPF:
         srv->worker_seccomp_bpf = optarg;
         break;
      case OPT_WORKER_SECCOMP_MINIJAIL_POLICY:
         srv->worker_seccomp_minijail_policy = optarg;
         break;
      case OPT_WORKER_SECCOMP_MINIJAIL_LOG:
         srv->worker_seccomp_minijail_log = true;
         break;
      default:
         render_log("unknown option specified");
         return false;
         break;
      }
   }

   if (optind < argc) {
      render_log("non-option arguments specified");
      return false;
   }

   if (srv->client_fd < 0 || !render_socket_is_seqpacket(srv->client_fd)) {
      render_log("no valid client fd specified");
      return false;
   }

   return true;
}

static bool
render_server_init(struct render_server *srv,
                   int argc,
                   char **argv,
                   struct render_context_args *ctx_args)
{
   memset(srv, 0, sizeof(*srv));
   srv->state = RENDER_SERVER_STATE_RUN;
   srv->context_args = ctx_args;
   srv->client_fd = -1;

   if (!render_server_parse_options(srv, argc, argv))
      return false;

   enum render_worker_jail_seccomp_filter seccomp_filter =
      RENDER_WORKER_JAIL_SECCOMP_NONE;
   const char *seccomp_path = NULL;
   if (srv->worker_seccomp_minijail_log && srv->worker_seccomp_minijail_policy) {
      seccomp_filter = RENDER_WORKER_JAIL_SECCOMP_MINIJAIL_POLICY_LOG;
      seccomp_path = srv->worker_seccomp_minijail_policy;
   } else if (srv->worker_seccomp_bpf) {
      seccomp_filter = RENDER_WORKER_JAIL_SECCOMP_BPF;
      seccomp_path = srv->worker_seccomp_bpf;
   } else if (srv->worker_seccomp_minijail_policy) {
      seccomp_filter = RENDER_WORKER_JAIL_SECCOMP_MINIJAIL_POLICY;
      seccomp_path = srv->worker_seccomp_minijail_policy;
   }

   srv->worker_jail = render_worker_jail_create(RENDER_SERVER_MAX_WORKER_COUNT,
                                                seccomp_filter, seccomp_path);
   if (!srv->worker_jail) {
      render_log("failed to create worker jail");
      goto fail;
   }

   srv->client = render_client_create(srv, srv->client_fd);
   if (!srv->client) {
      render_log("failed to create client");
      goto fail;
   }
   /* ownership transferred */
   srv->client_fd = -1;

   return true;

fail:
   render_server_fini(srv);
   return false;
}

bool
render_server_main(int argc, char **argv, struct render_context_args *ctx_args)
{
   struct render_server srv;
   if (!render_server_init(&srv, argc, argv, ctx_args))
      return false;

   const bool ok = render_server_run(&srv);
   render_server_fini(&srv);

   return ok;
}
