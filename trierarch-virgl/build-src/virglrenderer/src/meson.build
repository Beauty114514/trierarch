#############################################################################
#
# Copyright (C) 2019 Collabora Ltd
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

subdir('mesa')
subdir('gallium')

virgl_sources = [
   'virgl_context.c',
   'virgl_fence.c',
   'virgl_resource.c',
   'virgl_util.c',
]

vrend_sources = [
   'vrend/iov.c',
   'vrend/vrend_blitter.c',
   'vrend/vrend_debug.c',
   'vrend/vrend_decode.c',
   'vrend/vrend_formats.c',
   'vrend/vrend_object.c',
   'vrend/vrend_renderer.c',
   'vrend/vrend_shader.c',
   'vrend/vrend_tweaks.c',
   'vrend/vrend_winsys.c',
   'gl4es-decompress.c',
]

virglrenderer_sources = [
   'virglrenderer.c',
]

vrend_winsys_gbm_sources = [
   'vrend/vrend_winsys_gbm.c',
]

vrend_winsys_egl_sources = [
   'vrend/vrend_winsys_egl.c',
]

vrend_winsys_glx_sources = [
   'vrend/vrend_winsys_glx.c',
]

venus_sources = [
   'venus/vkr_acceleration_structure.c',
   'venus/vkr_allocator.c',
   'venus/vkr_buffer.c',
   'venus/vkr_command_buffer.c',
   'venus/vkr_common.c',
   'venus/vkr_context.c',
   'venus/vkr_cs.c',
   'venus/vkr_descriptor_set.c',
   'venus/vkr_device.c',
   'venus/vkr_device_memory.c',
   'venus/vkr_host_copy.c',
   'venus/vkr_image.c',
   'venus/vkr_instance.c',
   'venus/vkr_library.c',
   'venus/vkr_physical_device.c',
   'venus/vkr_pipeline.c',
   'venus/vkr_query_pool.c',
   'venus/vkr_queue.c',
   'venus/vkr_render_pass.c',
   'venus/vkr_renderer.c',
   'venus/vkr_ring.c',
   'venus/vkr_transport.c',
]

venus_codegen = custom_target(
   'venus_codegen',
   input : ['venus/vkr_device_object.py', 'venus/vkr_device_object.json'],
   output : [
      'vkr_buffer_gen.h',
      'vkr_command_buffer_gen.h',
      'vkr_descriptor_set_gen.h',
      'vkr_device_memory_gen.h',
      'vkr_image_gen.h',
      'vkr_pipeline_gen.h',
      'vkr_query_pool_gen.h',
      'vkr_queue_gen.h',
      'vkr_render_pass_gen.h',
   ],
   command : [prog_python, '@INPUT0@', '-o', '@OUTDIR@', '@INPUT1@'],
)

drm_sources = [
   'drm/drm_context.c',
   'drm/drm_fence.c',
   'drm/drm_renderer.c',
   'drm/drm_util.c',
]

drm_msm_sources = [
   'drm/msm/msm_renderer.c',
]

drm_amdgpu_sources = [
   'drm/amdgpu/amdgpu_renderer.c',
]

drm_asahi_sources = [
   'drm/drm-uapi/asahi_drm.h',
   'drm/asahi/asahi_proto.h',
   'drm/asahi/asahi_renderer.c',
   'drm/asahi/asahi_renderer.h',
]

drm_panfrost_sources = [
   'drm/panfrost/panfrost_renderer.c',
   'drm/panfrost/panfrost_resource.c',
   'drm/panfrost/panfrost_object.c',
   'drm/panfrost/panfrost_ccmd.c',
]

drm_i915_sources = [
   'drm/drm-uapi/i915_drm.h',
   'drm/i915/i915_ccmd.c',
   'drm/i915/i915_ccmd.h',
   'drm/i915/i915_proto.h',
   'drm/i915/i915_object.c',
   'drm/i915/i915_object.h',
   'drm/i915/i915_resource.c',
   'drm/i915/i915_resource.h',
   'drm/i915/i915_renderer.c',
   'drm/i915/i915_renderer.h',
]

proxy_sources = [
   'proxy/proxy_client.c',
   'proxy/proxy_common.c',
   'proxy/proxy_context.c',
   'proxy/proxy_renderer.c',
   'proxy/proxy_server.c',
   'proxy/proxy_socket.c',
]

video_sources = [
   'vrend/virgl_video.c',
   'vrend/vrend_video.c',
]

virgl_depends = [
   gallium_dep,
   epoxy_dep,
   libdrm_dep,
   thread_dep,
   m_dep,
   dl_dep,
]

if with_tracing == 'perfetto'
   virgl_depends += [vperfetto_min_dep]
endif

if with_tracing == 'percetto'
   virgl_depends += [percetto_dep]
endif

if with_tracing == 'sysprof'
   virgl_depends += [sysprof_dep]
endif

virgl_sources += vrend_sources

if gbm_dep.found()
   virgl_sources += vrend_winsys_gbm_sources
   virgl_depends += [gbm_dep]
endif

if have_egl
   virgl_sources += vrend_winsys_egl_sources
endif

if have_glx
   virgl_sources += vrend_winsys_glx_sources
   virgl_depends += [glx_dep]
endif

if with_venus
   virgl_sources += venus_sources
   virgl_sources += venus_codegen
   virgl_depends += [venus_dep]
endif

if with_drm_renderers
   virgl_sources += drm_sources
endif

if with_drm_msm
   virgl_sources += drm_msm_sources
endif

if with_drm_amdgpu
   virgl_sources += drm_amdgpu_sources
   virgl_depends += [libdrm_amdgpu_dep]
endif

if with_drm_asahi
   virgl_sources += drm_asahi_sources
endif

if with_drm_panfrost
   virgl_sources += drm_panfrost_sources
endif

if with_drm_i915
   virgl_sources += drm_i915_sources
endif

if with_render_server
   virgl_sources += proxy_sources
endif

if with_video
  virgl_sources += video_sources
  virgl_depends += [libva_dep, libvadrm_dep]
endif

if with_host_windows
  virgl_sources += ['mman_win32.c']
endif

libvirgl = static_library(
   'virgl',
   virgl_sources,
   include_directories: [inc_gallium, inc_configuration, 'venus', 'drm'],
   dependencies : [virgl_depends, drm_uapi_dep],
)

libvirgl_inc = [
   inc_gallium,
   inc_configuration,
   include_directories(['.', 'venus', 'drm'])
]

libvirgl_dep = declare_dependency(
   link_with: libvirgl,
   include_directories: libvirgl_inc
)

libvirglrenderer = library(
   'virglrenderer',
   virglrenderer_sources,
   include_directories: [inc_gallium, inc_configuration],
   dependencies : [virgl_depends, libvirgl_dep, drm_uapi_dep],
   version : binary_age.to_string() + '.'
             + interface_age.to_string() + '.'
             + revision.to_string(),
   install : true
)

pkg = import('pkgconfig')
pkg.generate(libvirglrenderer,
   description: 'virgl GL renderer',
   extra_cflags: pkg_cflags,
   subdirs: 'virgl'
)

libvirglrenderer_dep = declare_dependency(
   link_with: libvirglrenderer,
   include_directories: libvirgl_inc,
   dependencies : [libvirgl_dep, gallium_dep]
)

# Version header
version_conf = configuration_data()
version_conf.set('VIRGL_MAJOR_VERSION', virgl_major_version)
version_conf.set('VIRGL_MINOR_VERSION', virgl_minor_version)
version_conf.set('VIRGL_MICRO_VERSION', virgl_micro_version)

configure_file(
  input: 'virgl-version.h.meson',
  output: 'virgl-version.h',
  configuration: version_conf,
  install_dir: join_paths(get_option('includedir'), 'virgl'),
)

install_headers('virglrenderer.h', subdir : 'virgl')
