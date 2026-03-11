#!/bin/bash
# Launch rt-claw on QEMU vexpress-a9 (RT-Thread)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM_DIR="$(dirname "$SCRIPT_DIR")/platform/qemu-a9-rtthread"

cd "$PLATFORM_DIR" || exit 1

if [ ! -f "sd.bin" ]; then
    echo "Creating SD card image..."
    dd if=/dev/zero of=sd.bin bs=1024 count=65536
fi

if [ ! -f "rtthread.bin" ]; then
    echo "Error: rtthread.bin not found. Build first:"
    echo "  cd platform/qemu-a9-rtthread && scons -j\$(nproc)"
    exit 1
fi

qemu-system-arm --version
exec qemu-system-arm \
    -M vexpress-a9 \
    -smp cpus=2 \
    -kernel rtthread.bin \
    -serial stdio \
    -sd sd.bin \
    -show-cursor
