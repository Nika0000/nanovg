#!/usr/bin/env python3
"""Generate the playground's WASM bindings from include/nanovg/nanovg.h.

Emits three artifacts (all committed to the repo so contributors don't need
Python to build the playground):

  playground/src/nanovg_web_gen.inc   C shim, flat scalar ABI, one export per fn
  playground/www/nvg_gen.js           JS wrapper + enums + API docs index
  playground/www/nvg_api.json         same docs, for tooling

Functions whose signatures can't be expressed as flat scalars are listed in
SKIP or MANUAL below.  Anything else that fails classification raises, so
adding a function to nanovg.h that needs attention breaks the generator loudly
instead of silently dropping the binding.

Usage:  python playground/tools/gen_bindings.py
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
HEADER = os.path.join(ROOT, "include", "nanovg", "nanovg.h")

# Not exposed to the playground at all.
SKIP = {
    # Internal render API / backend plumbing.
    "nvgCreateInternal", "nvgDeleteInternal", "nvgInternalParams",
    "nvgDebugDumpPathCache",
    # Frame lifecycle is owned by the playground runtime, not user code.
    "nvgBeginFrame", "nvgEndFrame", "nvgCancelFrame",
    # Filesystem-backed loaders; the runtime fetches over HTTP instead.
    "nvgCreateImage", "nvgCreateFont", "nvgCreateFontAtIndex",
    # Free-standing float[6] matrix math: pure JS is friendlier than a
    # heap round-trip, and user code has nvgTransform()/nvgCurrentTransform().
    "nvgTransformIdentity", "nvgTransformTranslate", "nvgTransformScale",
    "nvgTransformRotate", "nvgTransformSkewX", "nvgTransformSkewY",
    "nvgTransformMultiply", "nvgTransformPremultiply", "nvgTransformInverse",
    "nvgTransformPoint",
    # Struct-array out params; not worth a binding for a playground.
    "nvgTextGlyphPositions", "nvgTextBreakLines",
}

# Hand-written in src/nanovg_web.c + www/nvg.js (out params, byte buffers).
MANUAL = {
    "nvgCurrentTransform", "nvgTextBounds", "nvgTextBoxBounds",
    "nvgTextMetrics", "nvgImageSize",
    "nvgCreateImageMem", "nvgCreateImageRGBA", "nvgUpdateImage",
    "nvgCreateFontMem", "nvgCreateFontMemAtIndex",
}

# nvgRGBA -> rgba rather than the naive rGBA.
JS_NAME_OVERRIDES = {
    "nvgRGB": "rgb", "nvgRGBf": "rgbf", "nvgRGBA": "rgba", "nvgRGBAf": "rgbaf",
    "nvgHSL": "hsl", "nvgHSLA": "hsla",
    "nvgLerpRGBA": "lerpRGBA", "nvgTransRGBA": "transRGBA",
    "nvgTransRGBAf": "transRGBAf",
}

DECL_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*\s*\*?)\s+(nvg[A-Za-z0-9_]+)\s*\((.*)\);\s*$")


class Unsupported(Exception):
    pass


def js_name(c_name):
    if c_name in JS_NAME_OVERRIDES:
        return JS_NAME_OVERRIDES[c_name]
    stem = c_name[3:]
    return stem[0].lower() + stem[1:]


def parse_params(raw):
    """Classify a C parameter list into binding kinds."""
    if raw.strip() in ("", "void"):
        return []
    out = []
    for part in raw.split(","):
        decl = part.strip()
        name = re.sub(r"[^A-Za-z0-9_]", "", decl.split()[-1])
        ty = decl[: len(decl) - len(decl.split()[-1])].strip()
        ty = re.sub(r"\bconst\b", "", ty).strip()
        ty = re.sub(r"\s+", " ", ty)
        if ty in ("NVGcontext*", "NVGcontext *"):
            kind = "ctx"
        elif ty == "float":
            kind = "float"
        elif ty in ("int", "unsigned int"):
            kind = "int"
        elif ty == "unsigned char":
            kind = "uchar"
        elif ty == "NVGcolor":
            kind = "color"
        elif ty == "NVGpaint":
            kind = "paint"
        elif ty in ("char*", "char *"):
            # `end` is nanovg's optional substring terminator: always NULL here.
            kind = "nullstr" if name == "end" else "str"
        else:
            raise Unsupported("parameter type %r" % decl)
        out.append({"kind": kind, "name": name, "ctype": ty})
    return out


def parse_header(path):
    with open(path, "r", encoding="utf-8") as fh:
        lines = fh.read().split("\n")

    fns, enums = [], {}
    doc, section = [], ""
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Section banner:  //\n// Title\n//
        if (stripped == "//" and i + 2 < len(lines)
                and lines[i + 1].strip().startswith("// ")
                and lines[i + 2].strip() == "//"):
            section = lines[i + 1].strip()[3:].strip()
            doc = []
            i += 3
            continue

        if stripped.startswith("//"):
            doc.append(stripped[2:].strip())
            i += 1
            continue

        m = re.match(r"^enum\s+(NVG[A-Za-z]+)\s*\{", stripped)
        if m:
            body, i = [], i + 1
            while i < len(lines) and "}" not in lines[i]:
                body.append(lines[i])
                i += 1
            i += 1
            value = 0
            for entry in body:
                entry = re.sub(r"//.*$", "", entry).strip().rstrip(",")
                if not entry:
                    continue
                em = re.match(r"^(NVG_[A-Za-z0-9_]+)\s*(?:=\s*(.+))?$", entry)
                if not em:
                    continue
                if em.group(2):
                    value = eval(em.group(2), {"__builtins__": {}}, {})  # noqa: S307
                enums[em.group(1)] = int(value)
                value = int(value) + 1
            doc = []
            continue

        m = DECL_RE.match(stripped)
        if m:
            ret, name, params = m.group(1).strip(), m.group(2), m.group(3)
            fns.append({
                "name": name,
                "ret": re.sub(r"\s+", "", ret),
                "raw_params": params,
                "doc": " ".join(d for d in doc if d).strip(),
                "section": section,
                "decl": stripped.rstrip(";"),
            })
        if stripped:
            doc = []
        i += 1
    return fns, enums


def classify(fn):
    if fn["ret"] == "void":
        ret = "void"
    elif fn["ret"] == "float":
        ret = "float"
    elif fn["ret"] == "int":
        ret = "int"
    elif fn["ret"] == "NVGcolor":
        ret = "color"
    elif fn["ret"] == "NVGpaint":
        ret = "paint"
    else:
        raise Unsupported("return type %r" % fn["ret"])
    return ret, parse_params(fn["raw_params"])


# --------------------------------------------------------------------------- C

def emit_c(bound):
    o = ["// Generated by playground/tools/gen_bindings.py -- do not edit.", ""]
    for fn in bound:
        ret, params = fn["ret_kind"], fn["params"]
        cret = {"void": "void", "float": "float", "int": "int",
                "color": "void", "paint": "int"}[ret]

        sig, call, ci = [], [], 0
        for p in params:
            if p["kind"] == "ctx":
                call.append("g_ctx")
            elif p["kind"] == "color":
                names = ["%s_r" % p["name"], "%s_g" % p["name"],
                         "%s_b" % p["name"], "%s_a" % p["name"]]
                sig += ["float %s" % n for n in names]
                call.append("nvgw__col(%s)" % ", ".join(names))
            elif p["kind"] == "paint":
                sig.append("int %s" % p["name"])
                call.append("nvgw__paint(%s)" % p["name"])
            elif p["kind"] == "nullstr":
                call.append("NULL")
            elif p["kind"] == "str":
                sig.append("const char* %s" % p["name"])
                call.append(p["name"])
            elif p["kind"] == "uchar":
                sig.append("int %s" % p["name"])
                call.append("(unsigned char) %s" % p["name"])
            else:
                sig.append("%s %s" % (p["kind"], p["name"]))
                call.append(p["name"])
            ci += 1

        inner = "%s(%s)" % (fn["name"], ", ".join(call))
        if ret == "void":
            body = "%s;" % inner
        elif ret == "color":
            body = "nvgw__retcol(%s);" % inner
        elif ret == "paint":
            body = "return nvgw__storepaint(%s);" % inner
        else:
            body = "return %s;" % inner

        o.append("EMSCRIPTEN_KEEPALIVE %s nvgw_%s(%s) { %s }"
                 % (cret, fn["jsName"], ", ".join(sig) or "void", body))
    o.append("")
    return "\n".join(o)


# -------------------------------------------------------------------------- JS

def emit_js(bound, enums):
    o = [
        "// Generated by playground/tools/gen_bindings.py -- do not edit.",
        "//",
        "// Thin JS mirror of the nanovg C API.  Bound to the wasm module by",
        "// bindGenerated() in nvg.js.",
        "",
        "export const ENUMS = {",
    ]
    for k in sorted(enums):
        o.append("  %s: %d, %s: %d," % (k, enums[k], k[4:], enums[k]))
    o += ["};", ""]

    # The helper object is `$h`, not `h`: nanovg parameter names include `h`
    # (nvgRect, nvgEllipse, ...) and would shadow it inside the arrow body.
    o.append("export function bindGenerated(nvg, m, $h) {")
    for fn in bound:
        args, call, strslot = [], [], 0
        for p in fn["params"]:
            if p["kind"] == "ctx" or p["kind"] == "nullstr":
                continue
            if p["kind"] == "color":
                args.append(p["name"])
                call.append("...$h.col(%s)" % p["name"])
            elif p["kind"] == "paint":
                args.append(p["name"])
                call.append("$h.paint(%s)" % p["name"])
            elif p["kind"] == "str":
                args.append(p["name"])
                call.append("$h.str(%s, %d)" % (p["name"], strslot))
                strslot += 1
            else:
                args.append(p["name"])
                call.append(p["name"])

        invoke = "m._nvgw_%s(%s)" % (fn["jsName"], ", ".join(call))
        if fn["ret_kind"] == "color":
            body = "%s; return $h.readColor();" % invoke
        elif fn["ret_kind"] == "paint":
            body = "return $h.wrapPaint(%s);" % invoke
        elif fn["ret_kind"] == "void":
            body = "%s;" % invoke
        else:
            body = "return %s;" % invoke
        o.append("  nvg.%s = (%s) => { %s };" % (fn["jsName"], ", ".join(args), body))
    o += ["}", ""]

    docs = [{
        "name": fn["name"], "js": fn["jsName"], "section": fn["section"],
        "signature": fn["decl"], "doc": fn["doc"],
        "args": [p["name"] for p in fn["params"]
                 if p["kind"] not in ("ctx", "nullstr")],
    } for fn in bound]
    o.append("export const DOCS = %s;" % json.dumps(docs, indent=0).replace("\n", ""))
    o.append("")
    return "\n".join(o), docs


def main():
    fns, enums = parse_header(HEADER)
    bound, skipped = [], []
    for fn in fns:
        if fn["name"] in SKIP or fn["name"] in MANUAL:
            skipped.append(fn["name"])
            continue
        try:
            ret, params = classify(fn)
        except Unsupported as exc:
            sys.exit("gen_bindings: %s: unsupported %s.\nAdd it to SKIP or "
                     "MANUAL in %s, or extend the classifier."
                     % (fn["name"], exc, os.path.basename(__file__)))
        fn["ret_kind"], fn["params"] = ret, params
        fn["jsName"] = js_name(fn["name"])
        bound.append(fn)

    inc = os.path.join(ROOT, "playground", "src", "nanovg_web_gen.inc")
    jsf = os.path.join(ROOT, "playground", "www", "nvg_gen.js")
    jsonf = os.path.join(ROOT, "playground", "www", "nvg_api.json")

    js, docs = emit_js(bound, enums)
    for path, data in ((inc, emit_c(bound)), (jsf, js),
                       (jsonf, json.dumps(docs, indent=1) + "\n")):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(data)

    print("gen_bindings: %d bound, %d manual/skipped, %d enum values"
          % (len(bound), len(skipped), len(enums)))


if __name__ == "__main__":
    main()
