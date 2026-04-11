#!/bin/bash

WAYLAND_PARAMS=''
X_PARAMS=''

if [[ -v XDG_RUNTIME_DIR  && -v WAYLAND_DISPLAY ]]; then
     WAYLAND_PARAMS="--volume $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY:$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY --env XDG_RUNTIME_DIR --env WAYLAND_DISPLAY"
fi

if [[ -d /tmp/.X11-unix/ && -v XAUTHORITY && -v DISPLAY ]]; then
     X_PARAMS="--env XAUTHORITY=/tmp/xauth --volume $XAUTHORITY:/tmp/xauth --volume /tmp/.X11-unix/:/tmp/.X11-unix/ --env DISPLAY"
fi

podman run \
    --ipc=host \
    --user root \
    $WAYLAND_PARAMS \
    $X_PARAMS \
    --env LIBGL_ALWAYS_SOFTWARE=1 \
    --env GALLIUM_DRIVER=virpipe \
    --env VN_DEBUG=vtest \
    --volume /tmp/.virgl_test:/tmp/.virgl_test \
    --annotation virgl=enabled \
    --annotation venus=enabled \
    --rm -it $1
