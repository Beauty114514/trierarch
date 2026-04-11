/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_instance.h"

#include "venus-protocol/vn_protocol_renderer_instance.h"

#include "vkr_context.h"
#include "vkr_physical_device.h"

static void
vkr_dispatch_vkEnumerateInstanceVersion(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkEnumerateInstanceVersion *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vn_global_proc_table *vk = &ctx->proc_table;

   vn_replace_vkEnumerateInstanceVersion_args_handle(args);

   uint32_t version = 0;
   args->ret = vk->EnumerateInstanceVersion(&version);
   if (args->ret == VK_SUCCESS)
      version = vkr_api_version_cap_minor(version, VKR_MAX_API_VERSION);

   *args->pApiVersion = version;
}

static void
vkr_dispatch_vkEnumerateInstanceExtensionProperties(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkEnumerateInstanceExtensionProperties *args)
{
   VkExtensionProperties private_extensions[] = {
      {
         .extensionName = "VK_EXT_command_serialization",
      },
      {
         .extensionName = "VK_MESA_venus_protocol",
      },
   };

   if (!args->pProperties) {
      *args->pPropertyCount = ARRAY_SIZE(private_extensions);
      args->ret = VK_SUCCESS;
      return;
   }

   for (uint32_t i = 0; i < ARRAY_SIZE(private_extensions); i++) {
      VkExtensionProperties *props = &private_extensions[i];
      props->specVersion = vkr_extension_get_spec_version(props->extensionName);
   }

   const uint32_t count = MIN2(*args->pPropertyCount, ARRAY_SIZE(private_extensions));
   memcpy(args->pProperties, private_extensions, sizeof(*args->pProperties) * count);
   *args->pPropertyCount = count;
   args->ret = count == ARRAY_SIZE(private_extensions) ? VK_SUCCESS : VK_INCOMPLETE;
}

static VkBool32
vkr_validation_callback(UNUSED VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                        UNUSED VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                        void *pUserData)
{
   struct vkr_context *ctx = pUserData;

   vkr_log(pCallbackData->pMessage);

   if (!ctx->validate_fatal)
      return false;

   vkr_context_set_fatal(ctx);

   /* The spec says we "should" return false, because the meaning of true is
    * layer-defined and is reserved for layer development.  And we know that,
    * for VK_LAYER_KHRONOS_validation, the return value indicates whether the
    * call should be skipped.  Let's do it for now and seek advices.
    */
   return true;
}

static inline void
vkr_instance_init_proc_table(struct vkr_instance *instance, struct vkr_context *ctx)
{
   instance->get_proc_addr = ctx->get_proc_addr;
   vn_util_init_instance_proc_table(instance->base.handle.instance, ctx->get_proc_addr,
                                    &instance->proc_table);
}

static void
vkr_dispatch_vkCreateInstance(struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCreateInstance *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vn_global_proc_table *vk = &ctx->proc_table;

   if (ctx->instance) {
      vkr_context_set_fatal(ctx);
      return;
   }

   if (args->pCreateInfo->enabledLayerCount) {
      args->ret = VK_ERROR_LAYER_NOT_PRESENT;
      return;
   }

   if (args->pCreateInfo->enabledExtensionCount) {
      args->ret = VK_ERROR_EXTENSION_NOT_PRESENT;
      return;
   }

   uint32_t instance_version;
   args->ret = vk->EnumerateInstanceVersion(&instance_version);
   if (args->ret != VK_SUCCESS)
      return;

   /* require Vulkan 1.1 */
   if (instance_version < VK_API_VERSION_1_1) {
      args->ret = VK_ERROR_INITIALIZATION_FAILED;
      return;
   }

   VkInstanceCreateInfo *create_info = (VkInstanceCreateInfo *)args->pCreateInfo;
   const char *layer_names[8];
   const char *ext_names[8];
   uint32_t layer_count = 0;
   uint32_t ext_count = 0;

   /* TODO enable more validation features */
   const VkValidationFeatureDisableEXT validation_feature_disables_on[] = {
      VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT,
      VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT,
      VK_VALIDATION_FEATURE_DISABLE_OBJECT_LIFETIMES_EXT,
      VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT,
      VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT,
   };
   /* we are single-threaded */
   const VkValidationFeatureDisableEXT validation_feature_disables_full[] = {
      VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT,
   };
   VkValidationFeaturesEXT validation_features;
   VkDebugUtilsMessengerCreateInfoEXT messenger_create_info;
   if (ctx->validate_level != VKR_CONTEXT_VALIDATE_NONE) {
      /* let vkCreateInstance return VK_ERROR_LAYER_NOT_PRESENT or
       * VK_ERROR_EXTENSION_NOT_PRESENT when the layer or extensions are
       * missing
       */
      layer_names[layer_count++] = "VK_LAYER_KHRONOS_validation";
      ext_names[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
      ext_names[ext_count++] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;

      validation_features = (const VkValidationFeaturesEXT){
         .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
         .pNext = create_info->pNext,
      };
      if (ctx->validate_level == VKR_CONTEXT_VALIDATE_ON) {
         validation_features.disabledValidationFeatureCount =
            ARRAY_SIZE(validation_feature_disables_on);
         validation_features.pDisabledValidationFeatures = validation_feature_disables_on;
      } else {
         validation_features.disabledValidationFeatureCount =
            ARRAY_SIZE(validation_feature_disables_full);
         validation_features.pDisabledValidationFeatures =
            validation_feature_disables_full;
      }
      messenger_create_info = (VkDebugUtilsMessengerCreateInfoEXT){
         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
         .pNext = &validation_features,
         .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
         .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
         .pfnUserCallback = vkr_validation_callback,
         .pUserData = ctx,
      };

      create_info->pNext = &messenger_create_info;
   }

   assert(layer_count <= ARRAY_SIZE(layer_names));
   create_info->enabledLayerCount = layer_count;
   create_info->ppEnabledLayerNames = layer_names;

   assert(ext_count <= ARRAY_SIZE(ext_names));
   create_info->enabledExtensionCount = ext_count;
   create_info->ppEnabledExtensionNames = ext_names;

   /* patch apiVersion */
   VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion = VK_API_VERSION_1_1,
   };
   if (create_info->pApplicationInfo) {
      app_info = *create_info->pApplicationInfo;
      if (app_info.apiVersion < VK_API_VERSION_1_1)
         app_info.apiVersion = VK_API_VERSION_1_1;
   }
   create_info->pApplicationInfo = &app_info;

   struct vkr_instance *instance = vkr_context_alloc_object(
      ctx, sizeof(*instance), VK_OBJECT_TYPE_INSTANCE, args->pInstance);
   if (!instance) {
      args->ret = VK_ERROR_OUT_OF_HOST_MEMORY;
      return;
   }

   instance->api_version = app_info.apiVersion;

   vn_replace_vkCreateInstance_args_handle(args);
   args->ret = vk->CreateInstance(create_info, NULL, &instance->base.handle.instance);
   if (args->ret != VK_SUCCESS) {
      free(instance);
      return;
   }

   vkr_instance_init_proc_table(instance, ctx);

   if (ctx->validate_level != VKR_CONTEXT_VALIDATE_NONE) {
      instance->create_debug_utils_messenger =
         (PFN_vkCreateDebugUtilsMessengerEXT)ctx->get_proc_addr(
            instance->base.handle.instance, "vkCreateDebugUtilsMessengerEXT");
      instance->destroy_debug_utils_messenger =
         (PFN_vkDestroyDebugUtilsMessengerEXT)ctx->get_proc_addr(
            instance->base.handle.instance, "vkDestroyDebugUtilsMessengerEXT");

      messenger_create_info.pNext = NULL;
      args->ret = instance->create_debug_utils_messenger(instance->base.handle.instance,
                                                         &messenger_create_info, NULL,
                                                         &instance->validation_messenger);
      if (args->ret != VK_SUCCESS) {
         struct vn_instance_proc_table *vk = &instance->proc_table;
         vk->DestroyInstance(instance->base.handle.instance, NULL);
         free(instance);
         return;
      }
   }

   vkr_context_add_instance(ctx, instance, app_info.pApplicationName);
}

void
vkr_instance_destroy(struct vkr_context *ctx,
                     struct vkr_instance *instance,
                     bool destroy_vk)
{
   struct vn_instance_proc_table *vk = &instance->proc_table;

   for (uint32_t i = 0; i < instance->physical_device_count; i++) {
      struct vkr_physical_device *physical_dev = instance->physical_devices[i];
      if (!physical_dev)
         break;

      vkr_physical_device_destroy(ctx, physical_dev);
   }

   if (ctx->validate_level != VKR_CONTEXT_VALIDATE_NONE) {
      instance->destroy_debug_utils_messenger(instance->base.handle.instance,
                                              instance->validation_messenger, NULL);
   }

   if (destroy_vk || ctx->on_worker_thread)
      vk->DestroyInstance(instance->base.handle.instance, NULL);

   free(instance->physical_device_handles);
   free(instance->physical_devices);

   vkr_context_remove_instance(ctx, instance);
}

static void
vkr_dispatch_vkDestroyInstance(struct vn_dispatch_context *dispatch,
                               struct vn_command_vkDestroyInstance *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vkr_instance *instance = vkr_instance_from_handle(args->instance);

   if (ctx->instance != instance) {
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_instance_destroy(ctx, instance, true);
}

void
vkr_context_init_instance_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkEnumerateInstanceVersion =
      vkr_dispatch_vkEnumerateInstanceVersion;
   dispatch->dispatch_vkEnumerateInstanceExtensionProperties =
      vkr_dispatch_vkEnumerateInstanceExtensionProperties;
   /* we don't advertise layers (and should never) */
   dispatch->dispatch_vkEnumerateInstanceLayerProperties = NULL;
   dispatch->dispatch_vkCreateInstance = vkr_dispatch_vkCreateInstance;
   dispatch->dispatch_vkDestroyInstance = vkr_dispatch_vkDestroyInstance;
   dispatch->dispatch_vkGetInstanceProcAddr = NULL;
}
