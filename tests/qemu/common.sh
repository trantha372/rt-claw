#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ARTIFACT_DIR="${RTCLAW_CI_ARTIFACT_DIR:-${PROJECT_ROOT}/ci-artifacts}"

backup_dir=""
profile_platform_dir=""
log_file=""

init_log()
{
    local platform="$1"
    local suite="$2"

    mkdir -p "$ARTIFACT_DIR"
    log_file="${ARTIFACT_DIR}/${platform}-${suite}.log"
    : > "$log_file"
}

restore_profile()
{
    if [ -z "$backup_dir" ] || [ -z "$profile_platform_dir" ]; then
        return
    fi

    if [ -f "${backup_dir}/sdkconfig.defaults" ]; then
        cp "${backup_dir}/sdkconfig.defaults" \
            "${profile_platform_dir}/sdkconfig.defaults"
    else
        rm -f "${profile_platform_dir}/sdkconfig.defaults"
    fi

    if [ -f "${backup_dir}/sdkconfig" ]; then
        cp "${backup_dir}/sdkconfig" "${profile_platform_dir}/sdkconfig"
    else
        rm -f "${profile_platform_dir}/sdkconfig"
    fi

    rm -rf "$backup_dir"
}

prepare_profile()
{
    local target="$1"
    local profile="$2"
    local defaults
    local profile_file

    profile_platform_dir="${PROJECT_ROOT}/platform/${target}"
    defaults="${profile_platform_dir}/sdkconfig.defaults"
    profile_file="${defaults}.${profile}"

    if [ ! -f "$profile_file" ]; then
        echo "Missing profile: $profile_file" >&2
        exit 1
    fi

    backup_dir="$(mktemp -d)"

    if [ -f "$defaults" ]; then
        cp "$defaults" "${backup_dir}/sdkconfig.defaults"
    fi

    if [ -f "${profile_platform_dir}/sdkconfig" ]; then
        cp "${profile_platform_dir}/sdkconfig" "${backup_dir}/sdkconfig"
    fi

    cp "$profile_file" "$defaults"
    rm -f "${profile_platform_dir}/sdkconfig"
}

run_and_log()
{
    local command="$1"
    local rc=0

    set +e
    bash -lc "$command" >> "$log_file" 2>&1
    rc=$?
    set -e

    if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ]; then
        cat "$log_file"
        exit "$rc"
    fi
}

command_timed_out()
{
    local rc="$1"

    if [ "$rc" -eq 124 ] || [ "$rc" -eq 143 ]; then
        return 0
    fi

    if [ "$rc" -eq 1 ] &&
       grep -Fq "terminating on signal 15 from pid" "$log_file"; then
        return 0
    fi

    return 1
}

build_target()
{
    local target="$1"
    local rc=0

    set +e
    bash -lc "make ${target}" >> "$log_file" 2>&1
    rc=$?
    set -e

    if [ "$rc" -ne 0 ]; then
        cat "$log_file"
        exit "$rc"
    fi
}

setup_esp_idf()
{
    if [ ! -f "$HOME/esp/esp-idf/export.sh" ]; then
        echo "ESP-IDF environment not found at ~/esp/esp-idf/export.sh" >&2
        exit 1
    fi

    # shellcheck source=/dev/null
    source "$HOME/esp/esp-idf/export.sh"
}

run_esp_qemu()
{
    local target="$1"
    local timeout_secs="$2"
    local input_delay="$3"
    local input_text="${4:-}"
    local build_dir="${PROJECT_ROOT}/platform/${target}/build"
    local qemu_bin
    local merge_cmd
    local run_cmd

    case "$target" in
        esp32c3-qemu)
            merge_cmd="cd ${build_dir} && esptool.py --chip esp32c3 merge_bin --fill-flash-size 4MB -o flash_image.bin @flash_args"
            qemu_bin="qemu-system-riscv32"
            run_cmd="${qemu_bin} -nographic -icount 1 -machine esp32c3 -drive file=${build_dir}/flash_image.bin,if=mtd,format=raw -global driver=timer.esp32c3.timg,property=wdt_disable,value=true -nic user,model=open_eth"
            ;;
        esp32s3-qemu)
            merge_cmd="cd ${build_dir} && esptool.py --chip esp32s3 merge_bin --fill-flash-size 4MB -o flash_image.bin @flash_args"
            qemu_bin="${HOME}/.espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa"
            run_cmd="${qemu_bin} -nographic -icount 1 -machine esp32s3 -drive file=${build_dir}/flash_image.bin,if=mtd,format=raw -nic user,model=open_eth"
            ;;
        *)
            echo "Unsupported ESP QEMU target: ${target}" >&2
            exit 1
            ;;
    esac

    run_and_log "$merge_cmd"

    if [ -n "$input_text" ]; then
        run_and_log "( sleep ${input_delay}; printf '%b' '${input_text}'; ) | timeout ${timeout_secs}s ${run_cmd}"
        return
    fi

    run_and_log "timeout ${timeout_secs}s ${run_cmd}"
}

run_esp_qemu_until_log()
{
    local target="$1"
    local timeout_secs="$2"
    local success_pattern="$3"
    local failure_pattern="${4:-}"
    local build_dir="${PROJECT_ROOT}/platform/${target}/build"
    local qemu_bin
    local merge_cmd
    local run_cmd
    local rc=0
    local timeout_pid=0
    local start_ts

    case "$target" in
        esp32c3-qemu)
            merge_cmd="cd ${build_dir} && esptool.py --chip esp32c3 merge_bin --fill-flash-size 4MB -o flash_image.bin @flash_args"
            qemu_bin="qemu-system-riscv32"
            run_cmd="${qemu_bin} -nographic -icount 1 -machine esp32c3 -drive file=${build_dir}/flash_image.bin,if=mtd,format=raw -global driver=timer.esp32c3.timg,property=wdt_disable,value=true -nic user,model=open_eth"
            ;;
        esp32s3-qemu)
            merge_cmd="cd ${build_dir} && esptool.py --chip esp32s3 merge_bin --fill-flash-size 4MB -o flash_image.bin @flash_args"
            qemu_bin="${HOME}/.espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa"
            run_cmd="${qemu_bin} -nographic -icount 1 -machine esp32s3 -drive file=${build_dir}/flash_image.bin,if=mtd,format=raw -nic user,model=open_eth"
            ;;
        *)
            echo "Unsupported ESP QEMU target: ${target}" >&2
            exit 1
            ;;
    esac

    run_and_log "$merge_cmd"

    set +e
    timeout "${timeout_secs}s" bash -lc "$run_cmd" >> "$log_file" 2>&1 &
    timeout_pid=$!
    set -e
    start_ts=$SECONDS

    while true; do
        if grep -Fq "$success_pattern" "$log_file"; then
            kill "$timeout_pid" >/dev/null 2>&1 || true
            wait "$timeout_pid" >/dev/null 2>&1 || true
            return 0
        fi

        if [ -n "$failure_pattern" ] &&
           grep -Fq "$failure_pattern" "$log_file"; then
            kill "$timeout_pid" >/dev/null 2>&1 || true
            wait "$timeout_pid" >/dev/null 2>&1 || true
            return 0
        fi

        if ! kill -0 "$timeout_pid" >/dev/null 2>&1; then
            set +e
            wait "$timeout_pid"
            rc=$?
            set -e

            if command_timed_out "$rc"; then
                return 0
            fi

            cat "$log_file"
            exit "$rc"
        fi

        if [ $((SECONDS - start_ts)) -ge "$timeout_secs" ]; then
            kill "$timeout_pid" >/dev/null 2>&1 || true
            wait "$timeout_pid" >/dev/null 2>&1 || true
            return 0
        fi

        sleep 1
    done
}

run_vexpress_qemu()
{
    local timeout_secs="$1"
    local a9_platform="${PROJECT_ROOT}/platform/vexpress-a9-qemu"
    local a9_build_dir="${PROJECT_ROOT}/build/vexpress-a9-qemu"

    if [ ! -f "${a9_platform}/sd.bin" ]; then
        dd if=/dev/zero of="${a9_platform}/sd.bin" bs=1024 count=65536 >> "$log_file" 2>&1
    fi

    run_and_log "timeout ${timeout_secs}s qemu-system-arm --version"
    run_and_log "timeout ${timeout_secs}s qemu-system-arm -M vexpress-a9 -smp cpus=1 -kernel ${a9_build_dir}/rtthread.bin -nographic -sd ${a9_platform}/sd.bin -nic user,model=lan9118"
}
