#!/usr/bin/env python3
"""[U-485][U-486] A League of Legends champion mod (.fantome or an unpacked folder) → skins of a champion here.

    python3 tools/art/lol_skin_make.py <mod.fantome | unpacked folder> tools/art/lol_maps/<champion>.json [--only skin,skin]

1. unpacks the .fantome (a zip) into the scratch folder
2. per skin of the map: DDS texture → PNG, Blender renders every animation of the map (tools/art/lol_skin.py:
   the League mesh + skeleton + animations, our camera, 8 directions)
3. packs resourcepacks/default/champions/<champion>/skins/<skin>/model/sheet.png + sheet.txt (wrapped blocks of
   `wrap` frames, one `anim <key> …` line per animation key — client/anim.hpp), splash.png + icon.png from the
   loading-screen art, CREDITS.txt (the mod's author + licence)

Everything it writes from a League mod is LOCAL (the champion folder is in .gitignore). Nothing is downloaded.
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORK = Path(os.environ.get("OCC_BOX_SCRATCH", "/tmp/em-lol")) / "lol_skins"


def unpack(src: Path) -> Path:
    if src.is_dir():
        return src
    out = WORK / src.stem.replace(" ", "_")
    if not (out / "done").exists():
        out.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(src) as z:
            z.extractall(out)
        (out / "done").write_text("ok")
    return out


def find_assets(root: Path, rel: str) -> Path:
    for p in [root / rel, *root.glob("WAD/*/" + rel)]:
        if p.exists():
            return p
    raise SystemExit(f"{rel} not found in {root}")


def to_png(dds: Path, png: Path):
    png.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["magick", str(dds), str(png)], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mod")
    ap.add_argument("map")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    mp = json.loads(Path(a.map).read_text())
    base = find_assets(unpack(Path(a.mod).expanduser()), mp["dir"])
    only = set(filter(None, a.only.split(",")))
    for skin, sk in mp["skins"].items():
        if only and skin not in only:
            continue
        work = WORK / mp["champion"] / skin
        work.mkdir(parents=True, exist_ok=True)
        tex = work / "texture.png"
        to_png(base / sk["texture"], tex)
        anims = {}
        for key, name in {**mp["anims"], **sk.get("anims", {})}.items():
            # the skin's own animation folder first, then the base champion's
            for folder in [Path(sk["mesh"]).parent / "Animations", Path("Base/Animations")]:
                f = base / folder / f"{name}.anm"
                if f.exists():
                    anims[key] = str(f)
                    break
            else:
                print(f"  ({skin}: no animation {name} for {key} — skipped)")
        job = {"skn": str(base / (sk["mesh"] + ".skn")), "skl": str(base / (sk["mesh"] + ".skl")), "texture": str(tex),
               "out": str(work / "frames"), "ortho": mp["ortho"], "px": mp["px"], "anchor": mp["anchor"], "height": mp["height"],
               "max_frames": mp.get("max_frames", 5), "anims": anims}
        (work / "job.json").write_text(json.dumps(job))
        if not (work / "frames" / "done").exists():
            print(f"render {skin}: {len(anims)} animations", flush=True)
            subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools/art/lol_skin.py"), "--", str(work / "job.json")],
                           check=True, stdout=subprocess.DEVNULL)
        pack(mp, skin, sk, work, base)


def pack(mp, skin, sk, work: Path, base: Path):
    frames = json.loads((work / "frames" / "frames.json").read_text())
    wrap, px = mp.get("wrap", 16), mp["px"]
    dst = ROOT / "resourcepacks" / "default" / "champions" / mp["champion"] / "skins" / skin
    model = dst / "model"
    model.mkdir(parents=True, exist_ok=True)
    order, lines, col = [], [], 0
    for key, (n, fps) in frames.items():
        lines.append(f"anim {key} {col} {n} {fps}")
        order += [(key, f) for f in range(n)]
        col += n
    blocks = math.ceil(len(order) / wrap)
    tiles = []  # block by block: each block = 8 direction rows × `wrap` frames
    for b in range(blocks):
        chunk = order[b * wrap:(b + 1) * wrap]
        for d in range(8):
            for i in range(wrap):
                tiles.append(str(work / "frames" / f"{chunk[i][0]}_{d}_{chunk[i][1]}.png") if i < len(chunk) else "null:")
    subprocess.run(["magick", "montage", *tiles, "-tile", f"{wrap}x{blocks * 8}", "-geometry", f"{px}x{px}+0+0",
                    "-background", "none", str(model / "sheet.png")], check=True)
    (model / "sheet.txt").write_text(
        f"# [U-485][U-486] {sk['name']} — converted from a League of Legends mod by tools/art/lol_skin_make.py\n"
        f"# {mp['credit']}\n"
        f"frame_px {px}\ndirs 8\northo_m {mp['ortho']}\nanchor 0.5 {mp['anchor']}\nwrap {wrap}\n" + "\n".join(lines) + "\n")
    if sk.get("splash"):  # the loading-screen art: splash + a square icon from its top
        to_png(base / sk["splash"], dst / "splash.png")
        subprocess.run(["magick", str(dst / "splash.png"), "-gravity", "north", "-crop", "100%x55%+0+0", "+repage", "-resize", "128x128^",
                        "-gravity", "center", "-extent", "128x128", str(dst / "icon.png")], check=True)
    (dst / "CREDITS.txt").write_text(f"{sk['name']}\n{mp['credit']}\n")
    print(f"packed {skin}: {len(order)} frames, {len(frames)} animations → {model.relative_to(ROOT)}", flush=True)


if __name__ == "__main__":
    main()
