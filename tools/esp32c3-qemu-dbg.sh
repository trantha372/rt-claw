#!/bin/bash
# Launch rt-claw on ESP32-C3 QEMU with GDB server
#
# Two modes:
#   ./esp32c3-qemu-dbg.sh           - use idf.py (recommended)
#   ./esp32c3-qemu-dbg.sh --raw     - use qemu-system-riscv32 directly
#
# Then connect GDB in another terminal:
#   riscv32-esp-elf-gdb build/rt-claw.elf -ex 'target remote :1234'

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM_DIR="$(dirname "$SCRIPT_DIR")/platform/esp32c3"

cd "$PLATFORM_DIR" || exit 1

if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH not set. Source ESP-IDF first:"
    echo "  source \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

if [ ! -d "build" ]; then
    echo "Error: build/ not found. Build first:"
    echo "  ./tools/esp32c3-build.sh"
    exit 1
fi

if [ "$1" = "--raw" ]; then
    FLASH_SIZE="4MB"
    FLASH_IMAGE="build/flash_image.bin"

    echo ">>> Generating merged flash image ..."
    esptool.py --chip esp32c3 merge_bin \
        --fill-flash-size "$FLASH_SIZE" \
        -o "$FLASH_IMAGE" \
        @build/flash_args

    echo ">>> Starting QEMU debug mode (GDB port 1234) ..."
    echo "Connect with: riscv32-esp-elf-gdb build/rt-claw.elf -ex 'target remote :1234'"
    exec qemu-system-riscv32 -nographic -s -S \
        -icount 3 \
        -machine esp32c3 \
        -drive "file=$FLASH_IMAGE,if=mtd,format=raw" \
        -global driver=timer.esp32c3.timg,property=wdt_disable,value=true
else
    echo ">>> Starting QEMU debug mode via idf.py ..."
    echo "Then run in another terminal: idf.py gdb"
    exec idf.py qemu --gdb monitor
fi
