# Copyright 2021 Google LLC
# SPDX-License-Identifier: MIT

virgl_render_server_sources = [
   'main.c',
   'render_client.c',
   'render_common.c',
   'render_context.c',
   'render_server.c',
   'render_socket.c',
   'render_state.c',
   'render_worker.c',
]

virgl_render_server_depends = [libvirglrenderer_dep, epoll_dep]

if with_render_server_worker == 'thread'
   virgl_render_server_depends += [thread_dep]
elif with_render_server_worker == 'minijail'
   virgl_render_server_depends += [minijail_dep]
endif

if with_tracing == 'percetto'
   virgl_render_server_depends += [percetto_dep]
endif

virgl_render_server = executable(
   'virgl_render_server',
   virgl_render_server_sources,
   dependencies : virgl_render_server_depends,
   install : true,
   install_dir : render_server_install_dir,
)
