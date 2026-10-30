#!/usr/bin/env python3
"""[U-257] Copy the ComfyUI pictures (art/raw/...) into the default resource pack at game sizes (PNG).

    python3 tools/art/install_art.py

art/raw/champion_icon/<id>.png   → resourcepacks/default/champions/<id>/base/icon.png        128×128
art/raw/champion_splash/<id>.png → resourcepacks/default/champions/<id>/base/splash.png      400×585
                                   + skins/shadow/{icon,splash}.png (darker copy, the example skin)
art/raw/skill/<id>.png           → resourcepacks/default/icons/skills/<id>.png               96×96
art/raw/item/<id>.png            → resourcepacks/default/icons/items/<id>.png                96×96
art/raw/texture/<id>.png         → resourcepacks/default/textures/<id>.png                   256×256
art/raw/screen/<id>.png          → resourcepacks/default/screens/<id>.png                    1216×832
Needs ImageMagick (`magick`). Existing files are replaced.
"""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art" / "raw"
PACK = ROOT / "resourcepacks" / "default"


def magick(src, dst, *ops):
    dst.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["magick", str(src), *ops, "-strip", str(dst)], check=True)


def main():
    n = 0
    for src in sorted((RAW / "champion_icon").glob("*.png")):
        cid = src.stem
        magick(src, PACK / "champions" / cid / "base" / "icon.png", "-resize", "128x128")
        magick(src, PACK / "champions" / cid / "skins" / "shadow" / "icon.png", "-resize", "128x128", "-modulate", "55,70")
        n += 2
    for src in sorted((RAW / "champion_splash").glob("*.png")):
        cid = src.stem
        magick(src, PACK / "champions" / cid / "base" / "splash.png", "-resize", "400x585")
        # the example skin: darker + colder; must stay under 512 KB (sent to other players [U-200])
        magick(src, PACK / "champions" / cid / "skins" / "shadow" / "splash.png", "-resize", "300x438", "-modulate", "55,70,90",
               "-colors", "200")
        n += 2
    for cat, sub, size in (("skill", "icons/skills", "96x96"), ("item", "icons/items", "96x96"), ("source", "icons/sources", "96x96"),
                           ("texture", "textures", "256x256"), ("screen", "screens", "1216x832")):
        for src in sorted((RAW / cat).glob("*.png")):
            magick(src, PACK / sub / f"{src.stem}.png", "-resize", size)
            n += 1
    for src in sorted((RAW / "emote").glob("*.png")):  # [U-414]
        magick(src, PACK / "emotes" / src.stem / "emote.png", "-resize", "96x96")
        magick(src, PACK / "emotes" / src.stem / "icon.png", "-resize", "48x48")
        n += 2
    for src in sorted((RAW / "ping").glob("*.png")):
        magick(src, PACK / "icons" / "pings" / f"{src.stem}.png", "-resize", "48x48")
        n += 1
    # the old generated BMPs are replaced by the PNGs (the game tries .png first)
    print(f"{n} pictures installed into {PACK}")


if __name__ == "__main__":
    main()
