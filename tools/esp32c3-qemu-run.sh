#!/bin/bash
# Launch rt-claw on ESP32-C3 QEMU (Espressif fork)
#
# Prerequisites:
#   1. ESP-IDF installed and sourced (. $IDF_PATH/export.sh)
#   2. Espressif QEMU installed:
#      python $IDF_PATH/tools/idf_tools.py install qemu-riscv32
#   3. Project built:
#      cd platform/esp32c3 && idf.py set-target esp32c3 && idf.py build

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM_DIR="$(dirname "$SCRIPT_DIR")/platform/esp32c3"

cd "$PLATFORM_DIR" || exit 1

if [ ! -d "build" ]; then
    echo "Error: build/ not found. Build first:"
    echo "  cd platform/esp32c3"
    echo "  idf.py set-target esp32c3"
    echo "  idf.py build"
    exit 1
fi

exec idf.py qemu monitor
