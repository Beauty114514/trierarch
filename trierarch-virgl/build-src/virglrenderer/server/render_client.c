/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "render_client.h"

#include <unistd.h>

#include "render_context.h"
#include "render_server.h"
#include "render_worker.h"
#include "vkr_library.h"

/* There is a render_context_record for each worker.
 *
 * When the client process destroys a context, it closes the connection to the
 * worker, which leads to worker termination.  It also sends a
 * RENDER_CLIENT_OP_DESTROY_CONTEXT to us to remove the record.  Because we
 * are responsible for cleaning up the worker, we don't care if the worker has
 * terminated or not.  We always kill, reap, and remove the record.
 */
struct render_context_record {
   uint32_t ctx_id;
   struct render_worker *worker;

   struct list_head head;
};

static struct render_context_record *
render_client_find_record(struct render_client *client, uint32_t ctx_id)
{
   list_for_each_entry (struct render_context_record, rec, &client->context_records,
                        head) {
      if (rec->ctx_id == ctx_id)
         return rec;
   }
   return NULL;
}

static void
render_client_detach_all_records(struct render_client *client)
{
   struct render_server *srv = client->server;

   /* free all render_workers without killing nor reaping */
   render_worker_jail_detach_workers(srv->worker_jail);

   list_for_each_entry_safe (struct render_context_record, rec, &client->context_records,
                             head)
      free(rec);
   list_inithead(&client->context_records);
}

static void
render_client_remove_record(struct render_client *client,
                            struct render_context_record *rec)
{
   struct render_server *srv = client->server;

   render_worker_destroy(srv->worker_jail, rec->worker);

   list_del(&rec->head);
   free(rec);
}

static void
render_client_clear_records(struct render_client *client)
{
   list_for_each_entry_safe (struct render_context_record, rec, &client->context_records,
                             head)
      render_client_remove_record(client, rec);
}

static void
init_context_args(struct render_context_args *ctx_args,
                  uint32_t init_flags,
                  const struct render_client_op_create_context_request *req,
                  int ctx_fd)
{
   *ctx_args = (struct render_context_args){
      .valid = true,
      .init_flags = init_flags,
      .ctx_id = req->ctx_id,
      .ctx_fd = ctx_fd,
   };

   static_assert(sizeof(ctx_args->ctx_name) == sizeof(req->ctx_name), "");
   memcpy(ctx_args->ctx_name, req->ctx_name, sizeof(req->ctx_name) - 1);
}

#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD

static int
render_client_worker_thread(void *thread_data)
{
   const struct render_context_args *ctx_args = thread_data;
   return render_context_main(ctx_args) ? 0 : -1;
}

#endif /* ENABLE_RENDER_SERVER_WORKER_THREAD */

static bool
render_client_create_context(struct render_client *client,
                             const struct render_client_op_create_context_request *req,
                             int *out_remote_fd)
{
   struct render_server *srv = client->server;

   struct render_context_record *rec = calloc(1, sizeof(*rec));
   if (!rec) {
      *out_remote_fd = -1;
      return false;
   }

   int socket_fds[2];
   if (!render_socket_pair(socket_fds)) {
      free(rec);
      *out_remote_fd = -1;
      return false;
   }
   int ctx_fd = socket_fds[0];
   int remote_fd = socket_fds[1];

   struct render_context_args ctx_args;
   init_context_args(&ctx_args, client->init_flags, req, ctx_fd);

#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   rec->worker = render_worker_create(srv->worker_jail, render_client_worker_thread,
                                      &ctx_args, sizeof(ctx_args));
   if (rec->worker)
      ctx_fd = -1; /* ownership transferred */
#else
   rec->worker = render_worker_create(srv->worker_jail, NULL, NULL, 0);
#endif
   if (!rec->worker) {
      render_log("failed to create a context worker");
      close(ctx_fd);
      close(remote_fd);
      free(rec);
      *out_remote_fd = -1;
      return false;
   }

   rec->ctx_id = req->ctx_id;
   list_addtail(&rec->head, &client->context_records);

   if (!render_worker_is_record(rec->worker)) {
      /* this is the child process */
      srv->state = RENDER_SERVER_STATE_SUBPROCESS;
      *srv->context_args = ctx_args;

      render_client_detach_all_records(client);

      /* ctx_fd ownership transferred */
      assert(srv->context_args->ctx_fd == ctx_fd);

      close(remote_fd);
      *out_remote_fd = -1;

      return true;
   }

   /* this is the parent process */
   if (ctx_fd >= 0)
      close(ctx_fd);
   *out_remote_fd = remote_fd;

   return true;
}

static bool
render_client_dispatch_destroy_context(struct render_client *client,
                                       const union render_client_op_request *req)
{
   const uint32_t ctx_id = req->destroy_context.ctx_id;
   struct render_context_record *rec = render_client_find_record(client, ctx_id);
   if (rec)
      render_client_remove_record(client, rec);

   return true;
}

static bool
render_client_dispatch_create_context(struct render_client *client,
                                      const union render_client_op_request *req)
{
   struct render_server *srv = client->server;

   int remote_fd;
   bool ok = render_client_create_context(client, &req->create_context, &remote_fd);

   if (srv->state == RENDER_SERVER_STATE_SUBPROCESS) {
      assert(remote_fd < 0);
      return true;
   }

   const struct render_client_op_create_context_reply reply = {
      .ok = ok,
   };
   if (!ok) {
      render_socket_send_reply(&client->socket, &reply, sizeof(reply));
      return false;
   }

   ok = render_socket_send_reply_with_fds(&client->socket, &reply, sizeof(reply),
                                          &remote_fd, 1);
   close(remote_fd);

   return ok;
}

static bool
render_client_dispatch_reset(struct render_client *client,
                             UNUSED const union render_client_op_request *req)
{
   render_client_clear_records(client);
   return true;
}

static bool
render_client_dispatch_init(struct render_client *client,
                            const union render_client_op_request *req)
{
   client->init_flags = req->init.flags;
   vkr_library_preload_icd();
   return true;
}

static bool
render_client_dispatch_nop(UNUSED struct render_client *client,
                           UNUSED const union render_client_op_request *req)
{
   return true;
}

struct render_client_dispatch_entry {
   size_t expect_size;
   bool (*dispatch)(struct render_client *client,
                    const union render_client_op_request *req);
};

static const struct render_client_dispatch_entry
   render_client_dispatch_table[RENDER_CLIENT_OP_COUNT] = {
#define RENDER_CLIENT_DISPATCH(NAME, name)                                               \
   [RENDER_CLIENT_OP_##                                                                  \
      NAME] = { .expect_size = sizeof(struct render_client_op_##name##_request),         \
                .dispatch = render_client_dispatch_##name }
      RENDER_CLIENT_DISPATCH(NOP, nop),
      RENDER_CLIENT_DISPATCH(INIT, init),
      RENDER_CLIENT_DISPATCH(RESET, reset),
      RENDER_CLIENT_DISPATCH(CREATE_CONTEXT, create_context),
      RENDER_CLIENT_DISPATCH(DESTROY_CONTEXT, destroy_context),
#undef RENDER_CLIENT_DISPATCH
   };

bool
render_client_dispatch(struct render_client *client)
{
   union render_client_op_request req;
   size_t req_size;
   if (!render_socket_receive_request(&client->socket, &req, sizeof(req), &req_size))
      return false;

   if (req.header.op >= RENDER_CLIENT_OP_COUNT) {
      render_log("invalid client op %d", req.header.op);
      return false;
   }

   const struct render_client_dispatch_entry *entry =
      &render_client_dispatch_table[req.header.op];
   if (entry->expect_size != req_size) {
      render_log("invalid request size %zu for client op %d", req_size, req.header.op);
      return false;
   }

   if (!entry->dispatch(client, &req))
      render_log("failed to dispatch client op %d", req.header.op);

   return true;
}

void
render_client_destroy(struct render_client *client)
{
   struct render_server *srv = client->server;

   if (srv->state == RENDER_SERVER_STATE_SUBPROCESS) {
      assert(list_is_empty(&client->context_records));
   } else {
      render_client_clear_records(client);
   }

   render_socket_fini(&client->socket);
   free(client);
}

struct render_client *
render_client_create(struct render_server *srv, int client_fd)
{
   struct render_client *client = calloc(1, sizeof(*client));

   if (!client)
      return NULL;

   client->server = srv;
   render_socket_init(&client->socket, client_fd);

   list_inithead(&client->context_records);

   return client;
}
