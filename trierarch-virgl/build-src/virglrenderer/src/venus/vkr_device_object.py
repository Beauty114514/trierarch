#Copyright 2021 Google LLC
#SPDX - License - Identifier : MIT

import argparse
import json
import os

SIMPLE_OBJECT_CREATE_DRIVER_HANDLE_TEMPL = '''
/* create a driver {vk_type} and update the vkr_{vkr_type} */
static inline VkResult
vkr_{create_func_name}_create_driver_handle(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct vkr_{vkr_type} *obj)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   /* handles in args are replaced */
   vn_replace_{create_cmd}_args_handle(args);
   args->ret = vk->{proc_create}(args->device, args->{create_info}, NULL,
      &obj->base.handle.{vkr_type});
   return args->ret;
}}
'''

POOL_OBJECT_CREATE_DRIVER_HANDLES_TEMPL = '''
/* create an array of driver {vk_type}s from a pool and update the
 * object_array
 */
static inline
VkResult vkr_{create_func_name}_create_driver_handles(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct object_array *arr)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   /* handles in args are replaced */
   vn_replace_{create_cmd}_args_handle(args);
   args->ret = vk->{proc_create}(args->device, args->{create_info},
      arr->handle_storage);
   return args->ret;
}}
'''

PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL = '''
/* create an array of driver {vk_type}s and update the object_array */
static inline VkResult
vkr_{create_func_name}_create_driver_handles(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct object_array *arr)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   /* handles in args are replaced */
   vn_replace_{create_cmd}_args_handle(args);
   args->ret = vk->{proc_create}(args->device, args->{create_cache},
      args->{create_count}, args->{create_info}, NULL,
      arr->handle_storage);
   return args->ret;
}}
'''

PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL2 = '''
/* create an array of driver {vk_type}s and update the object_array */
static inline VkResult
vkr_{create_func_name}_create_driver_handles(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct object_array *arr)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   /* handles in args are replaced */
   vn_replace_{create_cmd}_args_handle(args);
   args->ret = vk->{proc_create}(args->device, args->{create_hop},
      args->{create_cache}, args->{create_count}, args->{create_info}, NULL,
      arr->handle_storage);
   return args->ret;
}}
'''

SIMPLE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL = '''
/* destroy a driver {vk_type} */
static inline void
vkr_{destroy_func_name}_destroy_driver_handle(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{destroy_cmd} *args)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   /* handles in args are replaced */
   vn_replace_{destroy_cmd}_args_handle(args);
   vk->{proc_destroy}(args->device, args->{destroy_obj}, NULL);
}}
'''

POOL_OBJECT_DESTROY_DRIVER_HANDLES_TEMPL = '''
/* destroy an array of driver {vk_type}s from a pool, remove them from the
 * vkr_{pool_type}, and return the list of affected vkr_{vkr_type}s
 */
static inline void
vkr_{destroy_func_name}_destroy_driver_handles(
   UNUSED struct vkr_context *ctx,
   struct vn_command_{destroy_cmd} *args,
   struct list_head *free_list)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   list_inithead(free_list);
   for (uint32_t i = 0; i < args->{destroy_count}; i++) {{
      struct vkr_{vkr_type} *obj =
         vkr_{vkr_type}_from_handle(args->{destroy_objs}[i]);
      if (!obj)
         continue;

      list_del(&obj->base.track_head);
      list_addtail(&obj->base.track_head, free_list);
   }}

   /* handles in args are replaced */
   vn_replace_{destroy_cmd}_args_handle(args);
   vk->{proc_destroy}(args->device, args->{destroy_pool},
      args->{destroy_count}, args->{destroy_objs});
}}
'''

PIPELINE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL = SIMPLE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL

COMMON_OBJECT_INIT_ARRAY_TEMPL = '''
/* initialize an object_array for vkr_{vkr_type}s */
static inline VkResult
vkr_{create_func_name}_init_array(
   struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct object_array *arr)
{{
   args->ret = object_array_init(ctx, arr, args->{create_count},
                                 {vk_enum}, sizeof(struct vkr_{vkr_type}),
                                 sizeof(*args->{create_objs}),
                                 args->{create_objs})
                  ? VK_SUCCESS
                  : VK_ERROR_OUT_OF_HOST_MEMORY;
   return args->ret;
}}
'''

POOL_OBJECT_INIT_ARRAY_TEMPL = COMMON_OBJECT_INIT_ARRAY_TEMPL
PIPELINE_OBJECT_INIT_ARRAY_TEMPL = COMMON_OBJECT_INIT_ARRAY_TEMPL

SIMPLE_OBJECT_CREATE_TEMPL = '''
/* create a vkr_{vkr_type} */
static inline struct vkr_{vkr_type} *
vkr_{create_func_name}_create(
   struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args)
{{
   struct vkr_{vkr_type} *obj = vkr_context_alloc_object(ctx, sizeof(*obj),
      {vk_enum}, args->{create_obj});
   if (!obj) {{
      args->ret = VK_ERROR_OUT_OF_HOST_MEMORY;
      return NULL;
   }}

   /* handles in args are replaced */
   if (vkr_{create_func_name}_create_driver_handle(ctx, args, obj) != VK_SUCCESS) {{
      free(obj);
      return NULL;
   }}

   return obj;
}}
'''

COMMON_OBJECT_CREATE_ARRAY_TEMPL = '''
/* create an array of vkr_{vkr_type}s */
static inline VkResult
vkr_{create_func_name}_create_array(
   struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args,
   struct object_array *arr)
{{
   if (vkr_{create_func_name}_init_array(ctx, args, arr) != VK_SUCCESS)
      return args->ret;

   if (vkr_{create_func_name}_create_driver_handles(ctx, args, arr) < VK_SUCCESS) {{
      /* In case the client expects a reply, clear all returned handles to
       * VK_NULL_HANDLE.
       */
      memset(args->{create_objs}, 0,
             args->{create_count} * sizeof(args->{create_objs}[0]));
      object_array_fini(arr);
      return args->ret;
   }}

   return args->ret;
}}
'''

POOL_OBJECT_CREATE_ARRAY_TEMPL = COMMON_OBJECT_CREATE_ARRAY_TEMPL
PIPELINE_OBJECT_CREATE_ARRAY_TEMPL = COMMON_OBJECT_CREATE_ARRAY_TEMPL

SIMPLE_OBJECT_CREATE_AND_ADD_TEMPL = '''
/* create a vkr_{vkr_type} and add it to the vkr_device */
static inline struct vkr_{vkr_type} *
vkr_{create_func_name}_create_and_add(
   struct vkr_context *ctx,
   struct vn_command_{create_cmd} *args)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);

   struct vkr_{vkr_type} *obj = vkr_{create_func_name}_create(ctx, args);
   if (!obj)
      return NULL;

   vkr_device_add_object(ctx, dev, &obj->base);
   return obj;
}}
'''

POOL_OBJECT_ADD_ARRAY_TEMPL = '''
/* steal vkr_{vkr_type}s from an object_array and add them to the
 * vkr_{pool_type} and the context
 */
static inline
void vkr_{create_func_name}_add_array(
   struct vkr_context *ctx,
   struct vkr_device *dev,
   struct vkr_{pool_type} *pool,
   struct object_array *arr)
{{
   for (uint32_t i = 0; i < arr->count; i++) {{
      struct vkr_{vkr_type} *obj = arr->objects[i];

      obj->base.handle.{vkr_type} = (({vk_type} *)arr->handle_storage)[i];
      obj->device = dev;

      /* pool objects are tracked by the pool other than the device */
      list_add(&obj->base.track_head, &pool->{vkr_type}s);

      vkr_context_add_object(ctx, &obj->base);
   }}

   arr->objects_stolen = true;
   object_array_fini(arr);
}}
'''

PIPELINE_OBJECT_ADD_ARRAY_TEMPL = '''
/* steal vkr_{vkr_type}s from an object_array and add them to the device */
static inline void
vkr_{create_func_name}_add_array(
   struct vkr_context *ctx,
   struct vkr_device *dev,
   struct object_array *arr,
   {vk_type} *args_{create_objs})
{{
   for (uint32_t i = 0; i < arr->count; i++) {{
      struct vkr_{vkr_type} *obj = arr->objects[i];

      obj->base.handle.{vkr_type} = (({vk_type} *)arr->handle_storage)[i];

      /* Individual pipelines may fail creation. */
      if (obj->base.handle.{vkr_type} == VK_NULL_HANDLE) {{
         free(obj);
         arr->objects[i] = NULL;
         args_{create_objs}[i] = VK_NULL_HANDLE;
      }} else {{
         vkr_device_add_object(ctx, dev, &obj->base);
      }}
   }}

   arr->objects_stolen = true;
   object_array_fini(arr);
}}
'''

SIMPLE_OBJECT_DESTROY_AND_REMOVE_TEMPL = '''
/* remove a vkr_{vkr_type} from the device and destroy it */
static inline void
vkr_{destroy_func_name}_destroy_and_remove(
   struct vkr_context *ctx,
   struct vn_command_{destroy_cmd} *args)
{{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vkr_{vkr_type} *obj = vkr_{vkr_type}_from_handle(args->{destroy_obj});
   if (!obj)
      return;

   vkr_{destroy_func_name}_destroy_driver_handle(ctx, args);

   vkr_device_remove_object(ctx, dev, &obj->base);
}}
'''

PIPELINE_OBJECT_DESTROY_AND_REMOVE_TEMPL = SIMPLE_OBJECT_DESTROY_AND_REMOVE_TEMPL

def apply_variant(json_obj, json_variant):
    tmp_obj = json_obj.copy()
    for key, val in json_variant.items():
        tmp_obj[key] = val
    return tmp_obj

def simple_object_generator(json_obj):
    '''Generate functions for a simple object.

    For most device objects, object creation can be broken down into 3 steps

     (1) allocate and initialize the object
     (2) create the driver handle
     (3) add the object to the device and the object table

    SIMPLE_OBJECT_CREATE_DRIVER_HANDLE_TEMPL defines a function for (2).
    SIMPLE_OBJECT_CREATE_TEMPL defines a function for (1) and (2).
    SIMPLE_OBJECT_CREATE_AND_ADD_TEMPL defines a function for all steps.

    Object destruction can be broken down into 2 steps

     (1) destroy the driver handle
     (2) remove the object from the device and the object table

    SIMPLE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL defines a function for (1).
    SIMPLE_OBJECT_DESTROY_AND_REMOVE_TEMPL defines a function for both steps.
    '''
    contents = ''

    contents += SIMPLE_OBJECT_CREATE_DRIVER_HANDLE_TEMPL.format(**json_obj)
    contents += SIMPLE_OBJECT_CREATE_TEMPL.format(**json_obj)
    contents += SIMPLE_OBJECT_CREATE_AND_ADD_TEMPL.format(**json_obj)

    contents += SIMPLE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL.format(**json_obj)
    contents += SIMPLE_OBJECT_DESTROY_AND_REMOVE_TEMPL.format(**json_obj)

    for json_variant in json_obj['variants']:
        tmp_obj = apply_variant(json_obj, json_variant)
        contents += SIMPLE_OBJECT_CREATE_DRIVER_HANDLE_TEMPL.format(**tmp_obj)
        contents += SIMPLE_OBJECT_CREATE_TEMPL.format(**tmp_obj)
        contents += SIMPLE_OBJECT_CREATE_AND_ADD_TEMPL.format(**tmp_obj)

    return contents

def pool_object_generator(json_obj):
    '''Generate functions for a pool object.

    For VkCommandBuffer and VkDescriptorSet, object creation can be broken down
    into 3 steps

     (1) allocate and initialize the object array
     (2) create the driver handles
     (3) add the object array to the device and the object table

    POOL_OBJECT_INIT_ARRAY_TEMPL defines a function for (1).
    POOL_OBJECT_CREATE_DRIVER_HANDLES_TEMPL defines a function for (2).
    POOL_OBJECT_CREATE_ARRAY_TEMPL defines a function for (1) and (2).
    POOL_OBJECT_ADD_ARRAY_TEMPL defines a function for step (3).

    Object destruction can be broken down into 2 steps

     (1) destroy the driver handles
     (2) remove the objects from the pool and the object table

    POOL_OBJECT_DESTROY_DRIVER_HANDLES_TEMPL defines a function for (1) and
    the first half of (2).
    '''
    contents = ''

    contents += POOL_OBJECT_INIT_ARRAY_TEMPL.format(**json_obj)
    contents += POOL_OBJECT_CREATE_DRIVER_HANDLES_TEMPL.format(**json_obj)
    contents += POOL_OBJECT_CREATE_ARRAY_TEMPL.format(**json_obj)
    contents += POOL_OBJECT_ADD_ARRAY_TEMPL.format(**json_obj)

    contents += POOL_OBJECT_DESTROY_DRIVER_HANDLES_TEMPL.format(**json_obj)

    assert not json_obj['variants']

    return contents

def pipeline_object_generator(json_obj):
    '''Generate functions for a pipeline object.

    For VkPipeline, object creation can be broken down into 3 steps

     (1) allocate and initialize the object array
     (2) create the driver handles
     (3) add the object array to the device and the object table

    PIPELINE_OBJECT_INIT_ARRAY_TEMPL defines a function for (1).
    PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL defines a function for (2).
    PIPELINE_OBJECT_CREATE_ARRAY_TEMPL defines a function for (1) and (2).
    PIPELINE_OBJECT_ADD_ARRAY_TEMPL defines a function for step (3).

    Object destruction can be broken down into 2 steps

     (1) destroy the driver handle
     (2) remove the object from the device and the object table

    PIPELINE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL defines a function for (1).
    PIPELINE_OBJECT_DESTROY_AND_REMOVE_TEMPL defines a function for both steps.
    '''
    contents = ''

    contents += PIPELINE_OBJECT_INIT_ARRAY_TEMPL.format(**json_obj)
    contents += PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL.format(**json_obj)
    contents += PIPELINE_OBJECT_CREATE_ARRAY_TEMPL.format(**json_obj)

    # shared by both graphics and compute
    tmp_obj = json_obj.copy()
    tmp_obj['create_func_name'] = tmp_obj['vkr_type']
    contents += PIPELINE_OBJECT_ADD_ARRAY_TEMPL.format(**tmp_obj)

    contents += PIPELINE_OBJECT_DESTROY_DRIVER_HANDLE_TEMPL.format(**json_obj)
    contents += PIPELINE_OBJECT_DESTROY_AND_REMOVE_TEMPL.format(**json_obj)

    for json_variant in json_obj['variants']:
        tmp_obj = apply_variant(json_obj, json_variant)
        contents += PIPELINE_OBJECT_INIT_ARRAY_TEMPL.format(**tmp_obj)
        if tmp_obj['create_hop']:
            contents += PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL2.format(**tmp_obj)
        else:
            contents += PIPELINE_OBJECT_CREATE_DRIVER_HANDLES_TEMPL.format(**tmp_obj)
        contents += PIPELINE_OBJECT_CREATE_ARRAY_TEMPL.format(**tmp_obj)

    return contents

object_generators = {
    'simple-object': simple_object_generator,
    'pool-object': pool_object_generator,
    'pipeline-object': pipeline_object_generator,
}

FILE_HEADER_TEMPL = '''
/* This file is generated by {script}. */

#ifndef {guard}
#define {guard}

#include "vkr_common.h"

{protocol_includes}

#include "vkr_context.h"
#include "vkr_device.h"
'''

FILE_FOOTER_TEMPL = '''
#endif /* {guard} */
'''

def get_guard(filename):
    return filename.upper().translate(str.maketrans('.', '_'))

def file_header_generator(json_file):
    script = os.path.basename(__file__)
    guard = get_guard(json_file['filename'])

    include_filenames = []
    for json_obj in json_file['objects']:
        name = 'venus-protocol/vn_protocol_renderer_{}.h'.format(
            json_obj['vkr_type'])
        include_filenames.append(name)
    protocol_includes = '#include "' + '"\n#include "'.join(include_filenames) + '"'

    return FILE_HEADER_TEMPL.format(script=script, guard=guard,
            protocol_includes=protocol_includes).lstrip()

def file_footer_generator(json_file):
    guard = get_guard(json_file['filename'])
    return FILE_FOOTER_TEMPL.format(guard=guard)

def process_objects(json_objs):
    for json_obj in json_objs:
        json_obj.setdefault('create_func_name', json_obj['vkr_type'])
        json_obj.setdefault('destroy_func_name', json_obj['vkr_type'])
        json_obj.setdefault('variants', [])
        json_obj['proc_create'] = json_obj.get('create_cmd')[2:]
        json_obj['proc_destroy'] = json_obj.get('destroy_cmd')[2:]
        for variant in json_obj.get('variants'):
            if variant.get('create_cmd') != None:
                variant['proc_create'] = variant.get('create_cmd')[2:]
            if variant.get('destroy_cmd') != None:
                variant['proc_destroy'] = variant.get('create_cmd')[2:]

def file_generator(json_file):
    contents = file_header_generator(json_file)
    for json_obj in json_file['objects']:
        contents += object_generators[json_obj['generator']](json_obj)
    contents += file_footer_generator(json_file)

    return contents

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('json', help='specifies the input JSON file')
    parser.add_argument('-o', '--output-dir', required=True,
            help='specifies output directory')
    return parser.parse_args()

def main():
    args = parse_args()
    with open(args.json) as f:
        json_files = json.load(f)

    for json_file in json_files:
        process_objects(json_file['objects'])

        output = os.path.join(args.output_dir, json_file['filename'])
        with open(output, 'wb') as f:
            contents = file_generator(json_file)
            f.write(contents.encode())

if __name__ == '__main__':
    main()
