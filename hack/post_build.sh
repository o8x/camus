#!/bin/bash

set -e

TARGET="camus"
DATA_DIR="$1"

function get_file_size() {
    if [[ `uname -s` == "Darwin" ]]; then
        stat -f%z "$1"
    else
        stat -c%s "$1"
    fi
}

CAMUS_SIZE=$(get_file_size "$TARGET")
COPYFILE_DISABLE=1 tar -c -b 1 -f - --no-xattrs -C "$DATA_DIR/example" --exclude=html . | gzip -c >> "$TARGET"
TEMPLATE_SIZE=$(($(get_file_size "$TARGET") - $CAMUS_SIZE))

# 固定 8 位的十六进制字符串，实际是 4 字节
printf '%08x' "$CAMUS_SIZE" | xxd -r -p >> "$TARGET"
printf '%08x' "$TEMPLATE_SIZE" | xxd -r -p >> "$TARGET"

echo -e "command for extract: \n\tdd if=camus bs=1 skip=$CAMUS_SIZE count=$TEMPLATE_SIZE 2>/dev/null | tar xzf -"
