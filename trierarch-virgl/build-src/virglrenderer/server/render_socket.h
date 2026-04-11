/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_SOCKET_H
#define RENDER_SOCKET_H

#include "render_common.h"

struct render_socket {
   int fd;
};

bool
render_socket_pair(int out_fds[static 2]);

bool
render_socket_is_seqpacket(int fd);

void
render_socket_init(struct render_socket *socket, int fd);

void
render_socket_fini(struct render_socket *socket);

bool
render_socket_receive_request(struct render_socket *socket,
                              void *data,
                              size_t max_size,
                              size_t *out_size);

bool
render_socket_receive_request_with_fds(struct render_socket *socket,
                                       void *data,
                                       size_t max_size,
                                       size_t *out_size,
                                       int *fds,
                                       int max_fd_count,
                                       int *out_fd_count);

bool
render_socket_receive_data(struct render_socket *socket, void *data, size_t size);

bool
render_socket_send_reply(struct render_socket *socket, const void *data, size_t size);

bool
render_socket_send_reply_with_fds(struct render_socket *socket,
                                  const void *data,
                                  size_t size,
                                  const int *fds,
                                  int fd_count);

#endif /* RENDER_SOCKET_H */
