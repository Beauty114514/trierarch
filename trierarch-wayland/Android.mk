# Trierarch Wayland compositor: SHM + xdg-shell fullscreen only.
# Put libwayland-server.so, libffi.so and headers in libs/ (see README).
LOCAL_PATH := $(call my-dir)
WAYLAND_LIBS := $(LOCAL_PATH)/libs
WAYLAND_H := $(WAYLAND_LIBS)/include/wayland-server.h

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  ifneq ($(wildcard $(WAYLAND_H)),)
    ifneq ($(wildcard $(WAYLAND_LIBS)/lib/libwayland-server.so),)
      include $(CLEAR_VARS)
      LOCAL_MODULE := wayland-server
      LOCAL_SRC_FILES := libs/lib/libwayland-server.so
      include $(PREBUILT_SHARED_LIBRARY)
      include $(CLEAR_VARS)
      LOCAL_MODULE := ffi
      LOCAL_SRC_FILES := libs/lib/libffi.so
      include $(PREBUILT_SHARED_LIBRARY)
    endif
  endif
endif

include $(CLEAR_VARS)
LOCAL_MODULE := wayland-compositor
LOCAL_SRC_FILES := \
    src/compositor.c src/buffer.c src/linux_dmabuf.c src/surface.c src/subcompositor.c \
    src/xdg_shell.c src/output.c src/seat.c src/pointer_input.c src/keyboard_input.c src/data_device.c \
    src/single_pixel_buffer.c src/viewporter.c src/xdg_decoration.c src/xdg_output.c \
    src/fractional_scale.c \
    src/pointer_constraints.c src/relative_pointer.c src/presentation.c \
    src/renderer.c src/jni_bridge.c src/keycode_map.c \
    protocol/xdg-shell-server-protocol.c \
    protocol/single-pixel-buffer-v1-server-protocol.c \
    protocol/viewporter-server-protocol.c \
    protocol/xdg-decoration-unstable-v1-server-protocol.c \
    protocol/xdg-output-unstable-v1-server-protocol.c \
    protocol/fractional-scale-v1-server-protocol.c \
    protocol/pointer-constraints-unstable-v1-server-protocol.c \
    protocol/relative-pointer-unstable-v1-server-protocol.c \
    protocol/presentation-time-server-protocol.c \
    protocol/linux-dmabuf-v1-server-protocol.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src $(LOCAL_PATH)/protocol
LOCAL_LDLIBS := -lEGL -lGLESv2 -llog -landroid
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  ifneq ($(wildcard $(WAYLAND_H)),)
    LOCAL_C_INCLUDES += $(WAYLAND_LIBS)/include
    LOCAL_SHARED_LIBRARIES := wayland-server ffi
  endif
endif
include $(BUILD_SHARED_LIBRARY)
