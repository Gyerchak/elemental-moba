#!/usr/bin/env python3
"""[U-258] Placeholder 3D models → sprite sheets for the game.

    python3 tools/art/make_models.py [work dir]     (default work dir: $OCC_BOX_SCRATCH/models or /tmp/em-models)

1. builds the model list (champions from ./build/dump_content + minions, monsters, structures, trees)
2. runs Blender headless (tools/art/blender_models.py): primitives → posed frames, 8 directions
3. packs every model's frames into resourcepacks/default/models/<id>/sheet.png + sheet.txt (+ model.glb)

sheet.txt (read by the game, see client/sprites.hpp):
    frame_px 128          one frame is 128 × 128 px
    dirs 8                rows: direction 0 = facing screen right, then 45° steps clockwise (2 = towards the camera)
    ortho_m 2.6           the picture is 2.6 m wide; the game uses 38.9 game units per metre (champion 1.8 m = 70 units)
    anchor 0.5 0.8        where the model's ground point is in a frame (fractions of width / height)
    anim walk 2 6 10      name, first column, frames, frames per second
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "resourcepacks" / "default" / "models"
WORK = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(os.environ.get("OCC_BOX_SCRATCH", "/tmp/em-models")) / "models"
FRAMES = {"idle": 2, "walk": 6, "cast": 3, "attack": 3, "death": 3}
FPS = {"idle": 2, "walk": 10, "cast": 8, "attack": 10, "death": 5}  # (structures collapse in 1 s)
HUMAN = ["idle", "walk", "cast", "attack", "death"]
MOB = ["idle", "walk", "attack", "death"]


def manifest():
    out = []
    dump = subprocess.run([str(ROOT / "build" / "dump_content")], capture_output=True, text=True, check=True).stdout
    for line in dump.splitlines():
        f = line.split("\t")
        if f[0] != "champion":
            continue
        body = f[4].split(" ")[1] if " " in f[4] else "humanoid"
        out.append({"id": f[1], "kind": "humanoid", "body": body, "colour": f[5], "accent": f[6],
                    "front": str(ROOT / "art" / "raw" / "champion_front" / f"{f[1]}.png"), "px": 128, "ortho": 2.2, "anchor": 0.85, "anims": HUMAN})
    # minions are grey: the game tints them in the team colour
    out += [
        {"id": "minion_melee", "kind": "humanoid", "body": "humanoid", "height": 1.25, "colour": "c8c8c8", "accent": "f0f0f0",
         "weapon": "sword", "px": 96, "ortho": 2.0, "anims": MOB},
        {"id": "minion_ranged", "kind": "humanoid", "body": "slim", "height": 1.15, "colour": "c8c8c8", "accent": "ffffff",
         "weapon": "staff", "px": 96, "ortho": 2.0, "anims": MOB},
        {"id": "minion_siege", "kind": "quad", "size": 1.15, "colour": "b8b0a0", "accent": "ffe080", "px": 96, "ortho": 2.4, "anims": MOB},
        {"id": "minion_super", "kind": "humanoid", "body": "broad", "height": 1.6, "colour": "d0d0d0", "accent": "ffffff",
         "weapon": "sword", "px": 112, "ortho": 2.4, "anims": MOB},
        # jungle (kMobNames): wolf, wandering spirit, cursed spirit, angry beast, bandit, Face Stealer (boss)
        {"id": "spirit_wolf", "kind": "quad", "size": 1.0, "colour": "8090b0", "accent": "a0f0ff", "px": 112, "ortho": 2.4, "anims": MOB},
        {"id": "wandering_spirit", "kind": "humanoid", "body": "slim", "height": 1.4, "colour": "a0e0e0", "accent": "e0ffff",
         "px": 112, "ortho": 2.4, "anims": MOB},
        {"id": "cursed_spirit", "kind": "humanoid", "body": "humanoid", "height": 1.5, "colour": "503060", "accent": "c060ff",
         "px": 112, "ortho": 2.4, "anims": MOB},
        {"id": "angry_beast", "kind": "quad", "size": 1.45, "colour": "6a4a30", "accent": "ff6040", "px": 128, "ortho": 3.2, "anims": MOB},
        {"id": "bandit", "kind": "humanoid", "body": "humanoid", "height": 1.7, "colour": "6a5a48", "accent": "c0a070",
         "weapon": "sword", "px": 112, "ortho": 2.5, "anims": MOB},
        # [U-304] built from its hitbox (the beast body) — the game scales it with its +60 % size
        {"id": "face_stealer", "kind": "hitbox", "body": "beast", "colour": "202028", "accent": "ffffff",
         "px": 160, "ortho": 5.0, "anchor": 0.62, "anims": MOB},
        # [U-364][U-366][U-401] new jungle monsters
        {"id": "platypus_bear", "kind": "quad", "size": 1.4, "colour": "6b5a3a", "accent": "e0a040", "px": 128, "ortho": 3.2, "anims": MOB},
        {"id": "knowledge_seeker", "kind": "quad", "size": 0.9, "colour": "2a2a30", "accent": "f0f0f0", "px": 112, "ortho": 2.4, "anims": MOB},
        {"id": "lion_turtle", "kind": "quad", "size": 2.6, "colour": "6f7a5a", "accent": "d8c890", "px": 192, "ortho": 6.0, "anchor": 0.7, "anims": MOB},
        {"id": "sun_dragons", "kind": "quad", "size": 2.0, "colour": "b02a20", "accent": "3060d0", "px": 176, "ortho": 5.0, "anchor": 0.7, "anims": MOB},
        {"id": "dark_spirit", "kind": "humanoid", "body": "slim", "height": 1.8, "colour": "1a1020", "accent": "8030c0",
         "px": 112, "ortho": 2.6, "anims": MOB},
        # [U-421] Twisted Grove's epic monster
        {"id": "swamp_monster", "kind": "humanoid", "body": "broad", "height": 3.0, "colour": "3f5a2a", "accent": "8fbf5a", "px": 176, "ortho": 5.4, "anchor": 0.7, "anims": MOB},
        # [U-407] travel companions (sky bison, polar bear-dog, monkey-cat, shirshu, badgermole, dragon)
        {"id": "companion_sky_bison", "kind": "quad", "size": 2.2, "colour": "f0ece0", "accent": "8a5a30", "px": 176, "ortho": 5.0, "anchor": 0.7, "anims": ["idle", "walk"]},
        {"id": "companion_polar_bear_dog", "kind": "quad", "size": 1.6, "colour": "f6f6f8", "accent": "202020", "px": 144, "ortho": 3.6, "anims": ["idle", "walk"]},
        {"id": "companion_monkey_cat", "kind": "quad", "size": 0.9, "colour": "c88a48", "accent": "f0d0a0", "px": 112, "ortho": 2.4, "anims": ["idle", "walk"]},
        {"id": "companion_shirshu", "kind": "quad", "size": 1.7, "colour": "7a6a90", "accent": "e05080", "px": 144, "ortho": 3.8, "anims": ["idle", "walk"]},
        {"id": "companion_badgermole", "kind": "quad", "size": 1.5, "colour": "6a5440", "accent": "e8e0d0", "px": 144, "ortho": 3.6, "anims": ["idle", "walk"]},
        {"id": "companion_dragon", "kind": "quad", "size": 2.1, "colour": "c02818", "accent": "e8b030", "px": 176, "ortho": 5.0, "anchor": 0.7, "anims": ["idle", "walk"]},
        # structures + nature (1 direction, 1 frame)
        {"id": "turret", "kind": "turret", "colour": "8a8a90", "accent": "c8a050", "px": 256, "ortho": 6.0, "anchor": 0.85, "dirs": 1, "anims": ["idle", "death"]},
        {"id": "inhibitor", "kind": "inhibitor", "colour": "707078", "accent": "60d0ff", "px": 192, "ortho": 3.6, "anchor": 0.8, "dirs": 1, "anims": ["idle", "death"]},
        {"id": "nexus", "kind": "nexus", "colour": "8a8478", "accent": "e0b040", "px": 256, "ortho": 8.0, "anchor": 0.78, "dirs": 1, "anims": ["idle", "death"]},
        {"id": "tree", "kind": "tree", "colour": "2f6b2a", "accent": "000000", "px": 160, "ortho": 5.5, "anchor": 0.85, "dirs": 1, "anims": ["idle"]},
        {"id": "bush", "kind": "bush", "colour": "3f7f3a", "accent": "000000", "px": 96, "ortho": 2.6, "anchor": 0.75, "dirs": 1, "anims": ["idle"]},
    ]
    # [U-304] hitbox circles from the game → "hitbox" models get them
    bodies = {}
    for line in dump.splitlines():
        f = line.split("\t")
        if f[0] == "body":
            bodies[f[1]] = [tuple(int(v) for v in c.split(",")) for c in f[4].split(";")]
    for m in out:
        if m["kind"] == "hitbox":
            m["circles"] = bodies[m["body"]]
    return out


def pack(m):
    src = WORK / m["id"]
    dst = PACK / m["id"]
    dst.mkdir(parents=True, exist_ok=True)
    dirs = m.get("dirs", 8)
    static = m["kind"] in ("turret", "inhibitor", "nexus", "tree", "bush")
    cols, lines = [], []
    col = 0
    for a in m["anims"]:
        n = (5 if a == "death" else 1) if static else FRAMES[a]  # structures: 5 collapse frames [U-310]
        lines.append(f"anim {a} {col} {n} {FPS[a]}")
        cols += [(a, f) for f in range(n)]
        col += n
    files = [str(src / f"{a}_{d}_{f}.png") for d in range(dirs) for (a, f) in cols]
    subprocess.run(["magick", "montage", *files, "-tile", f"{len(cols)}x{dirs}", "-geometry", "+0+0", "-background", "none",
                    str(dst / "sheet.png")], check=True)
    anchor = m.get("anchor", 0.8)
    (dst / "sheet.txt").write_text(
        "# [U-258] placeholder model sprite sheet (made by tools/art/make_models.py from a Blender model)\n"
        f"frame_px {m['px']}\ndirs {dirs}\northo_m {m['ortho']}\nanchor 0.5 {anchor}\n" + "\n".join(lines) + "\n")
    if (src / "model.glb").exists():
        shutil.copy(src / "model.glb", dst / "model.glb")


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    ms = manifest()
    if os.environ.get("EM_MODELS_NEW"):  # only models that have no sprite sheet yet
        ms = [m for m in ms if not (PACK / m["id"] / "sheet.png").exists()]
        print(len(ms), "new models", flush=True)
        if not ms:
            return
    mf = WORK / "manifest.json"
    mf.write_text(json.dumps(ms))
    subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools" / "art" / "blender_models.py"), "--", str(mf), str(WORK)],
                   check=True)
    for m in ms:
        pack(m)
        print("packed", m["id"], flush=True)


if __name__ == "__main__":
    main()
