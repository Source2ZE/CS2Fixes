#!/usr/bin/env python3
"""Compile the EntWatch Panorama HUD with the CS2 Workshop Tools resource compiler.

Regenerates the HUD sources first (see generate-panorama.py), stages them in a
local addon under the CS2 install, compiles them and copies the results into
assets/panorama. Commit those compiled files; clients and servers need them.

Usage:
  python scripts/compile-panorama.py --cs2-root "<path to Counter-Strike Global Offensive>"
The root can also come from the CS2_ROOT environment variable.
"""

import argparse
import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
RESOURCES = [Path("panorama/layout/custom_game/hud.vxml_c"), Path("panorama/styles/entwatch/hud.vcss_c")]
ADDON_INFO = (
    "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} "
    "format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
    "{ IsPlayable = false }\n"
)


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def find_compiler(cs2_root):
    if sys.platform == "win32":
        candidates = [cs2_root / "game" / "bin" / "win64" / "resourcecompiler.exe"]
    else:
        candidates = [cs2_root / "game" / "bin" / "linuxsteamrt64" / "resourcecompiler"]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    sys.exit("resourcecompiler not found (looked for %s). Install the CS2 Workshop Tools." % ", ".join(map(str, candidates)))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cs2-root", default=os.environ.get("CS2_ROOT"), help="CS2 install directory (default: $CS2_ROOT)")
    parser.add_argument("--destination", default=str(REPO / "assets"), help="where to copy compiled resources (default: <repo>/assets)")
    args = parser.parse_args()
    if not args.cs2_root:
        parser.error("--cs2-root or CS2_ROOT is required")

    cs2_root = Path(args.cs2_root)
    destination = Path(args.destination)
    compiler = find_compiler(cs2_root)

    spec = importlib.util.spec_from_file_location("generate_panorama", HERE / "generate-panorama.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    if generator.main([]) != 0:
        sys.exit("Panorama source generation failed")

    content = cs2_root / "content" / "csgo_addons" / "cs2fixes_hud"
    output = cs2_root / "game" / "csgo_addons" / "cs2fixes_hud"
    layout = content / "panorama" / "layout" / "custom_game" / "hud.xml"
    style = content / "panorama" / "styles" / "entwatch" / "hud.css"
    for src, dst in ((generator.OUTPUTS["xml"], layout), (generator.OUTPUTS["css"], style)):
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    write_text(content / "panorama" / "preprocessor_config.txt", '"PanzipCfg" { "BlockDefs" {} }\n')
    write_text(content / "addoninfo.txt", ADDON_INFO)

    # Do not accept outputs left over from a previous compilation.
    for relative in RESOURCES:
        (output / relative).unlink(missing_ok=True)

    subprocess.run([str(compiler), "-game", str(cs2_root / "game" / "csgo"), "-i", str(style), "-i", str(layout), "-f", "-nop4"], check=True)

    for relative in RESOURCES:
        compiled = output / relative
        if not compiled.is_file() or compiled.stat().st_size == 0:
            sys.exit("Missing or empty compiled resource: %s" % compiled)
    for relative in RESOURCES:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output / relative, target)
    shutil.copyfile(content / "addoninfo.txt", output / "addoninfo.txt")
    print("Compiled Panorama resources copied into %s." % destination)


if __name__ == "__main__":
    main()
