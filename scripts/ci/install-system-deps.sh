#!/bin/bash

set -euo pipefail

mode="${1:-build}"

packages=(
    git
    wget
    flex
    bison
    gperf
    python3
    python3-venv
    cmake
    ninja-build
    ccache
    libffi-dev
    libssl-dev
    dfu-util
    libusb-1.0-0
    meson
    scons
)

if [ "$mode" = "qemu" ]; then
    packages+=(qemu-system-arm)
fi

sudo apt-get update
sudo apt-get install -y "${packages[@]}"
