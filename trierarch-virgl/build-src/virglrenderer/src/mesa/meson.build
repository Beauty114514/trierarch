# Copyright 2021 Google LLC
# SPDX-License-Identifier: MIT

inc_mesa = include_directories('.', 'compat', 'pipe', 'util')

files_mesa = files(
  'util/bitscan.c',
  'util/hash_table.c',
  'util/os_file.c',
  'util/os_misc.c',
  'util/ralloc.c',
  'util/u_cpu_detect.c',
  'util/u_debug.c',
  'util/u_math.c',
)

if not with_host_windows
   files_mesa += files('util/anon_file.c')
endif

deps_mesa = [
  m_dep,
  thread_dep,
]

libmesa = static_library(
  'mesa',
  files_mesa,
  include_directories: inc_mesa,
  dependencies: deps_mesa,
)

mesa_dep = declare_dependency(
  link_with: libmesa,
  include_directories: inc_mesa,
  dependencies: deps_mesa,
)
