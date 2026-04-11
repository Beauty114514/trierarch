/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_CLIENT_H
#define RENDER_CLIENT_H

#include "render_common.h"

struct render_client {
   struct render_server *server;
   struct render_socket socket;

   uint32_t init_flags;

   struct list_head context_records;
};

struct render_client *
render_client_create(struct render_server *srv, int client_fd);

void
render_client_destroy(struct render_client *client);

bool
render_client_dispatch(struct render_client *client);

#endif /* RENDER_CLIENT_H */
