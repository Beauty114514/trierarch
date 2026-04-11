/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "proxy_server.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/render_protocol.h"

int
proxy_server_connect(struct proxy_server *srv)
{
   int client_fd = srv->client_fd;
   /* transfer ownership */
   srv->client_fd = -1;
   return client_fd;
}

void
proxy_server_destroy(struct proxy_server *srv)
{
   if (srv->pid >= 0) {
      kill(srv->pid, SIGKILL);

      siginfo_t siginfo = { 0 };
      waitid(P_PID, srv->pid, &siginfo, WEXITED);
   }

   if (srv->client_fd >= 0)
      close(srv->client_fd);

   free(srv);
}

static bool
proxy_server_fork(struct proxy_server *srv)
{
   int socket_fds[2];
   if (!proxy_socket_pair(socket_fds))
      return false;
   const int client_fd = socket_fds[0];
   const int remote_fd = socket_fds[1];

   pid_t pid = fork();
   if (pid < 0) {
      proxy_log("failed to fork proxy server");
      close(client_fd);
      close(remote_fd);
      return false;
   }

   if (pid > 0) {
      srv->pid = pid;
      srv->client_fd = client_fd;
      close(remote_fd);
   } else {
      close(client_fd);

      /* do not receive signals from terminal */
      setpgid(0, 0);

      char fd_str[16];
      snprintf(fd_str, sizeof(fd_str), "%d", remote_fd);

      /* for devenv without installing server */
      char *const server_path = getenv("RENDER_SERVER_EXEC_PATH");
      char *const binary = server_path ? server_path : RENDER_SERVER_EXEC_PATH;
#if defined(__ANDROID__)
      /* Android /data is mounted noexec; bootstrap through the system linker. */
      char *const linker = sizeof(void*) == 8
         ? "/system/bin/linker64" : "/system/bin/linker";
      char *const argv[] = {
         linker,
         binary,
         "--socket-fd",
         fd_str,
         NULL,
      };
      execv(linker, argv);
#else
      char *const argv[] = {
         binary,
         "--socket-fd",
         fd_str,
         NULL,
      };
      execv(argv[0], argv);
#endif

      proxy_log("failed to exec %s: %s", argv[0], strerror(errno));
      close(remote_fd);
      exit(-1);
   }

   return true;
}

static bool
proxy_server_init_fd(struct proxy_server *srv)
{
   /* the fd represents a connection to the server */
   srv->client_fd = proxy_renderer.cbs->get_server_fd(RENDER_SERVER_VERSION);
   if (srv->client_fd < 0)
      return false;

   return true;
}

struct proxy_server *
proxy_server_create(void)
{
   struct proxy_server *srv = calloc(1, sizeof(*srv));
   if (!srv)
      return NULL;

   srv->pid = -1;

   if (!proxy_server_init_fd(srv)) {
      /* start the render server on demand when the client does not provide a
       * server fd
       */
      if (!proxy_server_fork(srv)) {
         free(srv);
         return NULL;
      }
   }

   if (!proxy_socket_is_seqpacket(srv->client_fd)) {
      proxy_log("invalid client fd type");
      close(srv->client_fd);
      free(srv);
      return NULL;
   }

   proxy_log("proxy server with pid %d", srv->pid);

   return srv;
}
