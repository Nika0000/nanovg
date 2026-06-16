#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHADER="$SCRIPT_DIR/include/nanovg/nanovg_mtl_shaders.metal"
OUTDIR="$SCRIPT_DIR/include/nanovg/mnvg_bitcode"
TMPDIR=$(mktemp -d)

generate() {
    local sdk=$1
    local name=$2
    local var_name="mnvg_bitcode_${name}"

    xcrun -sdk "$sdk" metal -c "$SHADER" -o "$TMPDIR/${name}.air"
    xcrun -sdk "$sdk" metallib "$TMPDIR/${name}.air" -o "$TMPDIR/${name}.metallib"

    echo "unsigned char ${var_name}[] = {" > "$OUTDIR/${name}.h"
    xxd -i < "$TMPDIR/${name}.metallib" >> "$OUTDIR/${name}.h"
    echo "};" >> "$OUTDIR/${name}.h"
    echo "unsigned int ${var_name}_len = sizeof(${var_name});" >> "$OUTDIR/${name}.h"

    echo "Generated $name ($(wc -c < "$TMPDIR/${name}.metallib") bytes)"
}

mkdir -p "$OUTDIR"

generate macosx macos
generate iphoneos ios
generate appletvos tvos
generate iphonesimulator simulator

rm -rf "$TMPDIR"
echo "Done. All bitcode headers regenerated."
