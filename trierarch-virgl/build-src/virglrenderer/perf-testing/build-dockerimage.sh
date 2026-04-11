#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
cd "${0%/*}"

export USER_ID=$(id -u)
export GROUP_ID=$(id -g)
src_root="$(realpath ..)"

print_help() {
  echo "Build docker image for performance profiling"
  echo "Usage build-dockerimage.sh [options]"
  echo ""
  echo "  --vtest          Use vtest backend"
  echo ""
  echo "  --help, -h       Print this help"
}

tag="mesa"
dockerfile="Dockerfile"

while [ -n "$1" ] ; do
    case "$1" in

        --vtest)
            tag="mesa-vtest"
            dockerfile="Dockerfile.vtest"
            ;;

        --help|-h)
            print_help
            exit
            ;;

        *)
            echo "Unknown option '$1' given, run with option --help to see supported options"
            exit
            ;;
    esac
    shift
done

docker build -t ${tag} \
    -f Docker/${dockerfile} \
    --build-arg USER_ID=${USER_ID} \
    --build-arg GROUP_ID=${GROUP_ID} \
    "$@" \
    "${src_root}"
