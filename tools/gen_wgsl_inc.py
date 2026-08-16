#!/usr/bin/env python3
"""Regenerate shaders/wgpu/fill.wgsl.inc from shaders/wgpu/fill.wgsl.

WGSL is source-level (no bytecode step), so the .inc is just the shader text
as a C array-initializer of byte values, mirroring the SPIR-V .inc convention
used by shaders/vulkan/fill.{vert,frag}.inc. Included as:

    static const char wgslSource[] = {
    #include "fill.wgsl.inc"
    };
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "shaders" / "wgpu" / "fill.wgsl"
DST = ROOT / "shaders" / "wgpu" / "fill.wgsl.inc"


def main():
    data = SRC.read_bytes() + b"\x00"
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append(",".join(str(b) for b in chunk) + ",")
    DST.write_text("\n" + "\n".join(lines) + "\n", newline="\n")
    print(f"wrote {DST} ({len(data)} bytes)")


if __name__ == "__main__":
    sys.exit(main())
