/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "proxy_client.h"

#include <unistd.h>

#include "server/render_protocol.h"

#include "proxy_server.h"

bool
proxy_client_destroy_context(struct proxy_client *client, uint32_t ctx_id)
{
   const struct render_client_op_destroy_context_request req = {
      .header.op = RENDER_CLIENT_OP_DESTROY_CONTEXT,
      .ctx_id = ctx_id,
   };

   return proxy_socket_send_request(&client->socket, &req, sizeof(req));
}

bool
proxy_client_create_context(struct proxy_client *client,
                            uint32_t ctx_id,
                            size_t ctx_name_len,
                            const char *ctx_name,
                            int *out_ctx_fd)
{
   struct render_client_op_create_context_request req = {
      .header.op = RENDER_CLIENT_OP_CREATE_CONTEXT,
      .ctx_id = ctx_id,
   };

   const size_t len = MIN2(ctx_name_len, sizeof(req.ctx_name) - 1);
   memcpy(req.ctx_name, ctx_name, len);

   if (!proxy_socket_send_request(&client->socket, &req, sizeof(req)))
      return false;

   struct render_client_op_create_context_reply reply;
   int fd_count;
   int ctx_fd;
   if (!proxy_socket_receive_reply_with_fds(&client->socket, &reply, sizeof(reply),
                                            &ctx_fd, 1, &fd_count))
      return false;

   if (reply.ok != fd_count) {
      if (fd_count)
         close(ctx_fd);
      return false;
   } else if (!reply.ok) {
      return false;
   }

   if (!proxy_socket_is_seqpacket(ctx_fd)) {
      close(ctx_fd);
      return false;
   }

   *out_ctx_fd = ctx_fd;
   return true;
}

bool
proxy_client_reset(struct proxy_client *client)
{
   const struct render_client_op_reset_request req = {
      .header.op = RENDER_CLIENT_OP_RESET,
   };
   return proxy_socket_send_request(&client->socket, &req, sizeof(req));
}

void
proxy_client_destroy(struct proxy_client *client)
{
   proxy_socket_fini(&client->socket);
   free(client);
}

static bool
proxy_client_init(struct proxy_client *client, uint32_t flags)
{
   const struct render_client_op_init_request req = {
      .header.op = RENDER_CLIENT_OP_INIT,
      .flags = flags,
   };
   return proxy_socket_send_request(&client->socket, &req, sizeof(req));
}

struct proxy_client *
proxy_client_create(struct proxy_server *srv, uint32_t flags)
{
   struct proxy_client *client = calloc(1, sizeof(*client));
   if (!client)
      return NULL;

   const int client_fd = proxy_server_connect(srv);
   if (client_fd < 0) {
      free(client);
      return NULL;
   }

   proxy_socket_init(&client->socket, client_fd);

   if (!proxy_client_init(client, flags)) {
      proxy_socket_fini(&client->socket);
      free(client);
      return NULL;
   }

   return client;
}
