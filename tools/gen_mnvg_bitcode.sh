#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHADER="$SCRIPT_DIR/../shaders/metal/nanovg_mtl_shaders.metal"
OUTDIR="$SCRIPT_DIR/../shaders/metal"
TMPDIR=$(mktemp -d)

generate() {
    local sdk=$1
    local name=$2
    local min_flag=$3
    local var_name="mnvg_bitcode_${name}"

    xcrun -sdk "$sdk" metal -c "$SHADER" $min_flag -o "$TMPDIR/${name}.air"
    xcrun -sdk "$sdk" metallib "$TMPDIR/${name}.air" -o "$TMPDIR/${name}.metallib"

    echo "unsigned char ${var_name}[] = {" > "$OUTDIR/${name}.h"
    xxd -i < "$TMPDIR/${name}.metallib" >> "$OUTDIR/${name}.h"
    echo "};" >> "$OUTDIR/${name}.h"
    echo "unsigned int ${var_name}_len = sizeof(${var_name});" >> "$OUTDIR/${name}.h"

    echo "Generated $name ($(wc -c < "$TMPDIR/${name}.metallib") bytes)"
}

mkdir -p "$OUTDIR"

generate macosx macos "-mmacosx-version-min=10.13"
generate iphoneos ios "-mios-version-min=13.0"
generate appletvos tvos "-mtvos-version-min=17.0"
generate iphonesimulator simulator "-mios-simulator-version-min=13.0"

rm -rf "$TMPDIR"
echo "Done. All bitcode headers regenerated."
