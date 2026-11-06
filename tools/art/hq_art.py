#!/usr/bin/env python3
"""[U-521][U-525] Better art from the local ComfyUI (FLUX.2 klein 9B): map textures for the default pack and an alternative pack,
champion icons and splash arts. Nothing leaves this computer.

    EM_COMFY_OUT=<ComfyUI's --output-directory> python3 tools/art/hq_art.py [textures,alt,icons,splash]    (existing pictures are kept)
    python3 tools/art/hq_art.py install      (copies them, at the game's sizes, into the packs)
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request
import uuid
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art" / "raw" / "hq"
PACK = ROOT / "resourcepacks" / "default"
ALT = ROOT / "resourcepacks" / "autumn_realm"
URL = "http://127.0.0.1:8188"
OUT = Path(os.environ.get("EM_COMFY_OUT", Path.home() / "ComfyUI" / "output"))
TEX = {
    "grass": "lush short green grass with small variations, tiny clover", "dirt": "dry cracked brown earth with small stones", "forest_path": "packed earth forest path with fallen leaves and pebbles",
    "stone_road": "old fitted cobblestone road, worn stones", "structure_stone": "carved temple floor of large stone slabs", "structure_wood": "dark temple floor of wooden boards",
    "sand": "fine desert sand with soft wind ripples", "shallow_water": "clear shallow water over pebbles with soft caustics", "deep_water": "deep dark blue sea water, soft waves",
    "lava": "molten lava with dark crust and glowing cracks", "glacier": "blue-white glacier ice with deep cracks", "ice": "smooth frozen ice sheet with white veins", "snow": "fresh soft snow",
    "wood": "wooden bridge planks with nails", "rock_wall": "rough grey rock surface", "mud": "wet dark mud with footprints", "ash": "grey volcanic ash ground", "burned": "scorched burned ground, charcoal",
    "tree_canopy": "dense tree canopy from straight above, layered green leaves", "bush": "dense green bush leaves from straight above",
}
ALT_LOOK = {
    "grass": "autumn meadow, short golden and rust grass, fallen red leaves", "dirt": "dark reddish earth with small stones", "forest_path": "forest path covered with orange fallen leaves",
    "stone_road": "mossy old cobblestone road with leaves", "structure_stone": "carved red sandstone temple floor", "structure_wood": "weathered reddish temple boards",
    "sand": "pale grey dune sand", "shallow_water": "shallow amber river water over dark pebbles", "deep_water": "deep teal water with soft waves",
    "lava": "molten lava with black crust", "glacier": "pale frosted glacier ice", "ice": "thin frosted ice", "snow": "first thin snow over brown earth",
    "wood": "old dark bridge planks", "rock_wall": "dark mossy rock surface", "mud": "wet reddish mud", "ash": "dark ash with embers", "burned": "scorched ground with red embers",
    "tree_canopy": "autumn tree canopy from straight above, red and orange leaves", "bush": "autumn bush from straight above, orange and red leaves",
}
STYLE = "hand painted fantasy game art, rich detail, confident brush work, cinematic light, Eastern fantasy world of the four elements, one original character, no text, no letters, no logo, no frame"


def graph(prompt, w, h, prefix, seed):
    return {
        "1": {"class_type": "UnetLoaderGGUF", "inputs": {"unet_name": "flux-2-klein-9b-Q8_0.gguf"}},
        "2": {"class_type": "CLIPLoaderGGUF", "inputs": {"clip_name": "Qwen3-8B-Q8_0.gguf", "type": "flux2"}},
        "5": {"class_type": "VAELoader", "inputs": {"vae_name": "flux2-vae.safetensors"}},
        "3": {"class_type": "CLIPTextEncode", "inputs": {"text": prompt, "clip": ["2", 0]}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": "text, letters, watermark, signature, logo, frame", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
        "7": {"class_type": "KSampler", "inputs": {"model": ["1", 0], "positive": ["3", 0], "negative": ["4", 0], "latent_image": ["6", 0], "seed": seed,
                                                   "steps": 12, "cfg": 1.0, "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["5", 0]}},
        "9": {"class_type": "SaveImage", "inputs": {"images": ["8", 0], "filename_prefix": prefix}},
    }


def make(dst, prompt, w, h, seed):
    if dst.exists():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    prefix = "emhq_" + uuid.uuid4().hex[:10]
    body = json.dumps({"prompt": graph(prompt, w, h, prefix, seed)}).encode()
    urllib.request.urlopen(urllib.request.Request(URL + "/prompt", data=body, headers={"Content-Type": "application/json"}), timeout=60)
    for _ in range(600):
        time.sleep(1)
        found = sorted(OUT.glob(prefix + "*.png"))
        if found:
            time.sleep(0.2)
            shutil.move(str(found[0]), dst)
            print(dst.relative_to(RAW), flush=True)
            return
    raise SystemExit(f"no picture for {dst}")


def champions():
    out = subprocess.run([str(ROOT / "build" / "dump_content")], capture_output=True, text=True, check=True).stdout
    champs, skills = {}, {}
    for line in out.splitlines():
        f = line.split("\t") + [""] * 5
        if f[0] == "champion":
            champs[f[1]] = (f[2], f[3], f[4])
        elif f[0] == "skill" and f[1] not in skills:
            skills[f[1]] = f[3]
    return champs


LOOK = {"Water": "a waterbender, flowing water and ice around the hands", "Earth": "an earthbender, stone and dust rising", "Fire": "a firebender, flames in the hands",
        "Air": "an airbender, wind and swirling air", "None": "a warrior without bending, weapons and chi"}


def who(cid, element, name, desc):
    seed_words = desc.lower().replace(",", " ")
    return f"{name}, {LOOK.get(element, LOOK['None'])}, {seed_words}, a clearly individual face, costume and silhouette unlike any other hero"


def seamless(path, size):
    """Cross-fades the edges so the tile repeats without a seam."""
    tmp = path.with_suffix(".rgb")
    subprocess.run(["magick", str(path), "-resize", f"{size}x{size}!", "-depth", "8", f"rgb:{tmp}"], check=True)
    a = np.frombuffer(tmp.read_bytes(), dtype=np.uint8).reshape(size, size, 3).astype(np.float32)
    tmp.unlink()
    n = size // 8
    w = np.linspace(0, 1, n)[:, None]
    b = a.copy()
    b[:n] = a[:n] * w[:, None] + a[-n:] * (1 - w[:, None])
    c = b.copy()
    wx = np.linspace(0, 1, n)[None, :, None]
    c[:, :n] = b[:, :n] * wx + b[:, -n:] * (1 - wx)
    c = c[:-n, :-n]
    raw = path.with_suffix(".rgb")
    raw.write_bytes(c.astype(np.uint8).tobytes())
    return raw, size - n


def install():
    for sub, dst in (("textures", PACK / "textures"), ("alt", ALT / "textures")):
        dst.mkdir(parents=True, exist_ok=True)
        for f in sorted((RAW / sub).glob("*.png")):
            raw, s = seamless(f, 584)
            subprocess.run(["magick", "-size", f"{s}x{s}", "-depth", "8", f"rgb:{raw}", "-resize", "512x512", str(dst / f.name)], check=True)
            raw.unlink()
    (ALT / "pack.txt").write_text("Autumn Realm\nThe whole map in autumn colours. Any file of the default pack can be replaced here: textures, sounds, music, models, animation scripts.\n")
    for f in sorted((RAW / "icons").glob("*.png")):
        d = PACK / "champions" / f.stem / "base"
        d.mkdir(parents=True, exist_ok=True)
        subprocess.run(["magick", str(f), "-resize", "256x256", "-strip", str(d / "icon.png")], check=True)
    for f in sorted((RAW / "splash").glob("*.png")):
        d = PACK / "champions" / f.stem / "base"
        d.mkdir(parents=True, exist_ok=True)
        subprocess.run(["magick", str(f), "-resize", "640x936", "-strip", "-colors", "4096", str(d / "splash.png")], check=True)


def main():
    what = set((sys.argv[1] if len(sys.argv) > 1 else "textures,alt,icons,splash").split(","))
    if "install" in what:
        return install()
    if "textures" in what:
        for i, (k, v) in enumerate(TEX.items()):
            make(RAW / "textures" / f"{k}.png", f"seamless tileable flat top-down game texture of {v}, even lighting, no shadows, no objects in the middle", 768, 768, 7000 + i)
    if "alt" in what:
        for i, (k, v) in enumerate(ALT_LOOK.items()):
            make(RAW / "alt" / f"{k}.png", f"seamless tileable flat top-down game texture of {v}, even lighting, no shadows, no objects in the middle", 768, 768, 7500 + i)
    champs = champions()
    if "customs" in what:  # [U-525] full-body pictures for own champions that have no model yet (tools/art/art_models.py makes the model)
        for i, (cid, look) in enumerate({"ash_monk": "a monk firebender in grey ash robes", "sky_monk_tenzo": "a tall airbender monk in yellow and orange robes, bald, blue arrow tattoos",
                                         "taliyah": "a young desert earthbender woman, woven blue and gold clothes, long dark braid"}.items()):
            dst = ROOT / "art" / "raw" / "champion_front" / f"{cid}.png"
            if not dst.exists():
                make(RAW / "front" / f"{cid}.png", f"one single figure, full body, front view, standing straight, arms slightly out, {look}, plain flat white background, even lighting, no text, hand painted fantasy game art", 768, 1024, 6000 + i)
                shutil.copy(RAW / "front" / f"{cid}.png", dst)
    if "icons" in what:
        for i, (cid, (el, name, desc)) in enumerate(champs.items()):
            make(RAW / "icons" / f"{cid}.png", f"close portrait bust of {who(cid, el, name, desc)}, strong rim light, dark painted background, {STYLE}", 768, 768, 8000 + i)
    if "splash" in what:
        for i, (cid, (el, name, desc)) in enumerate(champs.items()):
            make(RAW / "splash" / f"{cid}.png", f"full splash art of {who(cid, el, name, desc)}, dynamic pose in the middle of an action, a place of its own in the background, {STYLE}", 832, 1216, 9000 + i)


if __name__ == "__main__":
    main()
