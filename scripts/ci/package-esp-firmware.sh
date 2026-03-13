#!/bin/bash

set -euo pipefail

if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    echo "Usage: $0 <platform-dir> <chip-family> <output-dir> [name]" >&2
    exit 1
fi

platform_dir="$1"
chip_family="$2"
output_dir="$3"
name="${4:-RT-Claw}"

build_dir="${platform_dir}/build"
flash_args="${build_dir}/flash_args"
version="${RTCLAW_FIRMWARE_VERSION:-dev}"
parts_file="$(mktemp)"
first=1

if [ ! -f "$flash_args" ]; then
    echo "Missing flash_args: $flash_args" >&2
    exit 1
fi

mkdir -p "$output_dir"

while read -r offset path; do
    if [ -z "${offset:-}" ]; then
        continue
    fi

    case "$offset" in
        --*)
            continue
            ;;
    esac

    src="${build_dir}/${path}"
    base="$(basename "$path")"

    cp "$src" "${output_dir}/${base}"

    if [ $first -eq 0 ]; then
        printf ',\n' >> "$parts_file"
    fi
    printf '                    { "path": "%s", "offset": %d }' \
        "$base" "$((offset))" >> "$parts_file"
    first=0
done < "$flash_args"

cat > "${output_dir}/manifest.json" <<EOF
{
    "name": "${name}",
    "version": "${version}",
    "new_install_prompt_erase": true,
    "builds": [
        {
            "chipFamily": "${chip_family}",
            "parts": [
$(cat "$parts_file")
            ]
        }
    ]
}
EOF

rm -f "$parts_file"
