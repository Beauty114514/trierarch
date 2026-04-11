/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_common.h"

#include <stdarg.h>
#include <stdio.h>

#include "util/u_debug.h"
#include "venus-protocol/vn_protocol_renderer_info.h"

#include "vkr_context.h"
#include "vkr_cs.h"

static const struct vn_info_extension_table vkr_extension_table = {
   /* Venus extensions */
   .EXT_command_serialization = true,
   .MESA_venus_protocol = true,
   /* promoted to VK_VERSION_1_1 */
   .KHR_16bit_storage = true,
   .KHR_bind_memory2 = true,
   .KHR_dedicated_allocation = true,
   .KHR_descriptor_update_template = true,
   .KHR_device_group = true,
   .KHR_device_group_creation = true,
   .KHR_external_fence = true,
   .KHR_external_fence_capabilities = true,
   .KHR_external_memory = true,
   .KHR_external_memory_capabilities = true,
   .KHR_external_semaphore = true,
   .KHR_external_semaphore_capabilities = true,
   .KHR_get_memory_requirements2 = true,
   .KHR_get_physical_device_properties2 = true,
   .KHR_maintenance1 = true,
   .KHR_maintenance2 = true,
   .KHR_maintenance3 = true,
   .KHR_multiview = true,
   .KHR_relaxed_block_layout = true,
   .KHR_sampler_ycbcr_conversion = true,
   .KHR_shader_draw_parameters = true,
   .KHR_storage_buffer_storage_class = true,
   .KHR_variable_pointers = true,
   /* promoted to VK_VERSION_1_2 */
   .KHR_8bit_storage = true,
   .KHR_buffer_device_address = true,
   .KHR_create_renderpass2 = true,
   .KHR_depth_stencil_resolve = true,
   .KHR_draw_indirect_count = true,
   .KHR_driver_properties = true,
   .KHR_image_format_list = true,
   .KHR_imageless_framebuffer = true,
   .KHR_sampler_mirror_clamp_to_edge = true,
   .KHR_separate_depth_stencil_layouts = true,
   .KHR_shader_atomic_int64 = true,
   .KHR_shader_float16_int8 = true,
   .KHR_shader_float_controls = true,
   .KHR_shader_subgroup_extended_types = true,
   .KHR_spirv_1_4 = true,
   .KHR_timeline_semaphore = true,
   .KHR_uniform_buffer_standard_layout = true,
   .KHR_vulkan_memory_model = true,
   .EXT_descriptor_indexing = true,
   .EXT_host_query_reset = true,
   .EXT_sampler_filter_minmax = true,
   .EXT_scalar_block_layout = true,
   .EXT_separate_stencil_usage = true,
   .EXT_shader_viewport_index_layer = true,
   /* promoted to VK_VERSION_1_3 */
   .KHR_copy_commands2 = true,
   .KHR_dynamic_rendering = true,
   .KHR_format_feature_flags2 = true,
   .KHR_maintenance4 = true,
   .KHR_shader_integer_dot_product = true,
   .KHR_shader_non_semantic_info = true,
   .KHR_shader_terminate_invocation = true,
   .KHR_synchronization2 = true,
   .KHR_zero_initialize_workgroup_memory = true,
   .EXT_4444_formats = true,
   .EXT_extended_dynamic_state = true,
   .EXT_extended_dynamic_state2 = true,
   .EXT_image_robustness = true,
   .EXT_inline_uniform_block = true,
   .EXT_pipeline_creation_cache_control = true,
   .EXT_pipeline_creation_feedback = true,
   /* TODO(VK_EXT_private_data): Support natively in the guest */
   .EXT_private_data = true,
   .EXT_shader_demote_to_helper_invocation = true,
   .EXT_subgroup_size_control = true,
   .EXT_texel_buffer_alignment = true,
   .EXT_texture_compression_astc_hdr = true,
   .EXT_tooling_info = false, /* implementation in driver */
   .EXT_ycbcr_2plane_444_formats = true,
   /* promoted to VK_VERSION_1_4 */
   .KHR_dynamic_rendering_local_read = true,
   .KHR_global_priority = true,
   .KHR_index_type_uint8 = true,
   .KHR_line_rasterization = true,
   .KHR_load_store_op_none = true,
   .KHR_maintenance5 = true,
   .KHR_maintenance6 = true,
   .KHR_map_memory2 = false, /* implementation in driver */
   .KHR_push_descriptor = true,
   .KHR_shader_expect_assume = true,
   .KHR_shader_float_controls2 = true,
   .KHR_shader_subgroup_rotate = true,
   .KHR_vertex_attribute_divisor = true,
   .EXT_host_image_copy = true,
   .EXT_pipeline_protected_access = true,
   .EXT_pipeline_robustness = true,
   /* KHR extensions */
   .KHR_acceleration_structure = true,
   .KHR_calibrated_timestamps = true,
   .KHR_compute_shader_derivatives = true,
   .KHR_cooperative_matrix = true,
   .KHR_deferred_host_operations = false, /* implementation in driver */
   .KHR_depth_clamp_zero_one = true,
   .KHR_external_fence_fd = true,
   .KHR_external_memory_fd = true,
   .KHR_external_semaphore_fd = true,
   .KHR_fragment_shader_barycentric = true,
   .KHR_fragment_shading_rate = true,
   .KHR_maintenance7 = true,
   .KHR_pipeline_library = true,
   .KHR_ray_query = true,
   .KHR_ray_tracing_maintenance1 = true,
   .KHR_ray_tracing_pipeline = true,
   .KHR_ray_tracing_position_fetch = true,
   .KHR_robustness2 = true,
   .KHR_shader_bfloat16 = true,
   .KHR_shader_clock = true,
   .KHR_shader_maximal_reconvergence = true,
   .KHR_shader_quad_control = true,
   .KHR_shader_relaxed_extended_instruction = true,
   .KHR_shader_subgroup_uniform_control_flow = true,
   .KHR_shader_untyped_pointers = true,
   .KHR_workgroup_memory_explicit_layout = true,
   /* EXT extensions */
   .EXT_attachment_feedback_loop_dynamic_state = true,
   .EXT_attachment_feedback_loop_layout = true,
   .EXT_blend_operation_advanced = true,
   .EXT_border_color_swizzle = true,
   .EXT_buffer_device_address = true,
   .EXT_calibrated_timestamps = true,
   .EXT_color_write_enable = true,
   .EXT_conservative_rasterization = true,
   .EXT_conditional_rendering = true,
   .EXT_custom_border_color = true,
   .EXT_depth_bias_control = true,
   .EXT_depth_clamp_control = true,
   .EXT_depth_clamp_zero_one = true,
   .EXT_depth_clip_control = true,
   .EXT_depth_clip_enable = true,
   .EXT_depth_range_unrestricted = true,
   .EXT_dynamic_rendering_unused_attachments = true,
   .EXT_extended_dynamic_state3 = true,
   .EXT_external_memory_acquire_unmodified = true,
   .EXT_external_memory_dma_buf = true,
   .EXT_filter_cubic = true,
   .EXT_fragment_shader_interlock = true,
   .EXT_global_priority = true,
   .EXT_global_priority_query = true,
   .EXT_graphics_pipeline_library = true,
   .EXT_image_2d_view_of_3d = true,
   .EXT_image_drm_format_modifier = true,
   .EXT_image_sliced_view_of_3d = true,
   .EXT_image_view_min_lod = true,
   .EXT_index_type_uint8 = true,
   .EXT_legacy_dithering = true,
   .EXT_legacy_vertex_attributes = true,
   .EXT_line_rasterization = true,
   .EXT_load_store_op_none = true,
   .EXT_memory_budget = true,
   .EXT_mesh_shader = true,
   .EXT_multi_draw = true,
   .EXT_multisampled_render_to_single_sampled = true,
   .EXT_mutable_descriptor_type = true,
   .EXT_nested_command_buffer = true,
   .EXT_non_seamless_cube_map = true,
   .EXT_pci_bus_info = true,
   .EXT_pipeline_library_group_handles = true,
   .EXT_post_depth_coverage = true,
   .EXT_primitive_topology_list_restart = true,
   .EXT_primitives_generated_query = true,
   .EXT_provoking_vertex = true,
   .EXT_queue_family_foreign = true,
   .EXT_rasterization_order_attachment_access = true,
   .EXT_robustness2 = true,
   .EXT_sample_locations = true,
   .EXT_shader_atomic_float = true,
   .EXT_shader_atomic_float2 = true,
   .EXT_shader_float8 = true,
   .EXT_shader_image_atomic_int64 = true,
   .EXT_shader_replicated_composites = true,
   .EXT_shader_stencil_export = true,
   .EXT_shader_subgroup_ballot = true,
   .EXT_shader_subgroup_vote = true,
   .EXT_shader_uniform_buffer_unsized_array = true,
   .EXT_transform_feedback = true,
   .EXT_vertex_attribute_divisor = true,
   .EXT_vertex_input_dynamic_state = true,
   .EXT_ycbcr_image_arrays = true,
   /* vendor extensions */
   .ARM_rasterization_order_attachment_access = true,
   .GOOGLE_decorate_string = true,
   .GOOGLE_hlsl_functionality1 = true,
   .GOOGLE_user_type = true,
   .IMG_filter_cubic = true,
   .NV_compute_shader_derivatives = true,
   .VALVE_mutable_descriptor_type = true,
};

static const struct debug_named_value vkr_debug_options[] = {
   { "validate", VKR_DEBUG_VALIDATE, "Force enabling the validation layer" },
   { "udmabuf", VKR_DEBUG_UDMABUF, "Force udmabuf for host visible memory" },
   DEBUG_NAMED_VALUE_END
};

uint32_t vkr_debug_flags;

DEBUG_GET_ONCE_FLAGS_OPTION(vkr_debug_flags, "VKR_DEBUG", vkr_debug_options, 0)

void
vkr_debug_init(void)
{
   vkr_debug_flags = debug_get_option_vkr_debug_flags();
}

void
vkr_log(const char *fmt, ...)
{
   va_list va;

   va_start(va, fmt);
   virgl_prefixed_logv("vkr", VIRGL_LOG_LEVEL_INFO, fmt, va);
   va_end(va);
}

void
vkr_extension_table_init(struct vn_info_extension_table *table,
                         const char *const *exts,
                         uint32_t count)
{
   memset(table, 0, sizeof(*table));
   for (uint32_t i = 0; i < count; i++) {
      const int32_t index = vn_info_extension_index(exts[i]);
      if (index >= 0)
         table->enabled[index] = true;
   }
}

uint32_t
vkr_extension_get_spec_version(const char *name)
{
   const int32_t index = vn_info_extension_index(name);
   if (index < 0 || !vkr_extension_table.enabled[index])
      return 0;

   const struct vn_info_extension *ext = vn_info_extension_get(index);
   return ext->spec_version;
}

void
object_array_fini(struct object_array *arr)
{
   if (!arr->objects_stolen) {
      for (uint32_t i = 0; i < arr->count; i++)
         free(arr->objects[i]);
   }

   free(arr->objects);
   free(arr->handle_storage);
}

bool
object_array_init(struct vkr_context *ctx,
                  struct object_array *arr,
                  uint32_t count,
                  VkObjectType obj_type,
                  size_t obj_size,
                  size_t handle_size,
                  const void *obj_id_handles)
{
   arr->count = count;

   arr->objects = malloc(sizeof(*arr->objects) * count);
   if (!arr->objects)
      return false;

   arr->handle_storage = malloc(handle_size * count);
   if (!arr->handle_storage) {
      free(arr->objects);
      return false;
   }

   arr->objects_stolen = false;
   for (uint32_t i = 0; i < count; i++) {
      const void *obj_id_handle = (const char *)obj_id_handles + handle_size * i;
      struct vkr_object *obj =
         vkr_context_alloc_object(ctx, obj_size, obj_type, obj_id_handle);
      if (!obj) {
         arr->count = i;
         object_array_fini(arr);
         return false;
      }

      arr->objects[i] = obj;
   }

   return true;
}
