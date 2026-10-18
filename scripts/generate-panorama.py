#!/usr/bin/env python3
"""Generate the EntWatch Panorama HUD layout and stylesheet.

Reads the hand-edited templates in assets_source/panorama/templates and writes
  assets_source/panorama/layout/custom_game/hud.xml
  assets_source/panorama/styles/entwatch/hud.css

Limits (row count, position cap, size range) are read from the C++ sources so
the generated classes always match what the server will actually send.

Usage:
  python scripts/generate-panorama.py           # (re)generate
  python scripts/generate-panorama.py --check   # exit 1 if generated files are stale

After regenerating, recompile with scripts/compile-panorama.py and commit the
compiled resources in assets/panorama.
"""

import argparse
import re
import sys
from collections import namedtuple
from decimal import Decimal
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PANORAMA = REPO / "assets_source" / "panorama"
TEMPLATES = PANORAMA / "templates"
OUTPUTS = {
    "xml": PANORAMA / "layout" / "custom_game" / "hud.xml",
    "css": PANORAMA / "styles" / "entwatch" / "hud.css",
}

Limits = namedtuple("Limits", "pos_max size_min size_max size_default max_rows")

# Geometry at 100% size. Every other size class is these values scaled by N/100.
# (selector, [(property, (px values...)), ...])
SIZE_RULES = [
    (".Item", [("width", (480,)), ("padding", (8, 12)), ("margin-bottom", (5,)), ("border-left-width", (3,))]),
    (".ItemName", [("font-size", (18,))]),
    (".Owner", [("font-size", (16,)), ("margin-left", (8,))]),
    (".Status", [("margin-left", (12,))]),
    (".Status Label", [("font-size", (16,))]),
    (".Status .Icon", [("font-size", (19,)), ("margin-right", (5,))]),
    (".Status .ReloadIcon", [("width", (22,)), ("height", (22,)), ("margin-left", (5,)), ("margin-right", (0,))]),
    (".Ammo", [("margin-right", (12,))]),
    (".Overflow", [("font-size", (14,))]),
]


def read_constexpr(path, name):
    match = re.search(r"constexpr\s+(?:int|size_t)\s+%s\s*=\s*(\d+)\s*;" % re.escape(name), path.read_text(encoding="utf-8"))
    if not match:
        sys.exit("Could not find constexpr %s in %s" % (name, path))
    return int(match.group(1))


def read_limits():
    header = REPO / "src" / "entwatch_hud.h"
    source = REPO / "src" / "entwatch_hud.cpp"
    return Limits(
        pos_max=read_constexpr(header, "EW_PANORAMA_POS_MAX"),
        size_min=read_constexpr(header, "EW_PANORAMA_SIZE_MIN"),
        size_max=read_constexpr(header, "EW_PANORAMA_SIZE_MAX"),
        size_default=read_constexpr(header, "EW_PANORAMA_SIZE_DEFAULT"),
        max_rows=read_constexpr(source, "MaxRows"),
    )


def scale(value, percent):
    """value * percent / 100 as a short decimal string (no float rounding noise)."""
    return format((Decimal(value * percent) / 100).normalize(), "f")


def size_section(limits):
    lines = []
    for percent in range(limits.size_min, limits.size_max + 1):
        for selector, props in SIZE_RULES:
            decls = "; ".join(
                "%s: %s" % (prop, " ".join(scale(v, percent) + "px" for v in values)) for prop, values in props
            )
            lines.append(".EntWatch.Size%d %s { %s; }" % (percent, selector, decls))
    return "\n".join(lines) + "\n"


def position_section(limits):
    lines = []
    for n in range(limits.pos_max + 1):
        lines.append(".EntWatch.PosX%d { margin-left: %d%%; }" % (n, n))
        lines.append(".EntWatch.PosY%d { margin-top: %d%%; }" % (n, n))
    return "\n".join(lines) + "\n"


def fill(template, marker, content):
    lines = template.split("\n")
    hits = [i for i, line in enumerate(lines) if marker in line]
    if len(hits) != 1:
        sys.exit("Template must contain exactly one '%s' marker (found %d)" % (marker, len(hits)))
    lines[hits[0]] = content.rstrip("\n")
    return "\n".join(lines)


def generate(limits):
    """Return {"xml": text, "css": text}."""
    if limits.size_default != 100:
        sys.exit("The base geometry in SIZE_RULES is the 100%% size; EW_PANORAMA_SIZE_DEFAULT is %d." % limits.size_default)
    read = lambda name: (TEMPLATES / name).read_text(encoding="utf-8")

    item = read("item.xml").rstrip("\n")
    items = "\n".join(item.replace("@INDEX@", str(i)) for i in range(limits.max_rows))
    xml = fill(read("hud.base.xml"), "@generate: items", items)

    css = fill(read("hud.base.css"), "@generate: positions", position_section(limits))
    css = fill(css, "@generate: sizes", size_section(limits))
    return {"xml": xml, "css": css}


def normalized(path):
    return path.read_bytes().replace(b"\r\n", b"\n").decode("utf-8") if path.exists() else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="fail if the generated files on disk are out of date")
    args = parser.parse_args(argv)

    generated = generate(read_limits())
    stale = [key for key, text in generated.items() if normalized(OUTPUTS[key]) != text]
    if args.check:
        if stale:
            print("Out of date, run scripts/generate-panorama.py: " + ", ".join(str(OUTPUTS[k].relative_to(REPO)) for k in stale))
            return 1
        print("Generated Panorama sources are up to date.")
        return 0
    for key, text in generated.items():
        if key in stale:
            with open(OUTPUTS[key], "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            print("Wrote " + str(OUTPUTS[key].relative_to(REPO)))
    if not stale:
        print("Already up to date.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
