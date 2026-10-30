#!/usr/bin/env python3
"""[U-197][U-198] Placeholder art from the local ComfyUI (FLUX.2 klein 4B, GGUF).

Reads the game content (./build/dump_content), builds one prompt per asset and sends them to
ComfyUI at 127.0.0.1:8188. Raw PNGs go to art/raw/<category>/<id>.png; an existing PNG is skipped, so
the script can be re-run after new champions / skills / items are added.

    python3 tools/art/gen_art.py [--only champion,skill,item,texture,screen] [--comfy-out DIR]

--comfy-out = ComfyUI's --output-directory (the script moves finished images from there).
Then tools/art/install_art.py converts the PNGs into the default resource pack (BMP, right sizes).
"""
import zlib, argparse, json, os, shutil, subprocess, sys, time, urllib.request, uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art" / "raw"
URL = "http://127.0.0.1:8188"

ELEMENT_LOOK = {
    "Water": "waterbender, blue and white robes, flowing water and ice",
    "Earth": "earthbender, green and brown robes, stone and rock",
    "Fire": "firebender, red and gold robes, flames",
    "Air": "airbender, yellow and orange robes, swirling wind",
    "None": "non-bender warrior, grey and purple leather, steel and chi",
}
ELEMENT_FX = {
    "Water": "blue water and ice magic", "Earth": "green-brown rock and earth magic",
    "Fire": "orange-red fire magic", "Air": "pale swirling wind magic", "None": "steel weapon and chi strike",
}
STYLE = "stylized fantasy game art, painterly, Avatar the Last Airbender inspired, vivid colours, clean shapes"
NEG = "text, letters, watermark, signature, logo, frame, border, blurry, low quality, extra limbs"

TEXTURES = {  # [?-12] every surface has its own look
    "grass": "lush short green grass", "dirt": "dry brown dirt ground", "forest_path": "forest path, packed earth with leaves",
    "stone_road": "old cobblestone road", "sand": "fine desert sand with ripples", "shallow_water": "shallow clear river water over pebbles",
    "deep_water": "deep dark blue sea water with waves", "lava": "glowing molten lava with dark crust",
    "glacier": "blue-white glacier ice with cracks", "ice": "frozen ice sheet", "snow": "fresh snow", "wood": "wooden bridge planks",
    "rock_wall": "grey rock cliff surface", "mud": "wet mud", "ash": "grey volcanic ash ground", "burned": "scorched burned ground",
    "tree_canopy": "tree canopy seen from above, dense green leaves", "bush": "dense green bush leaves seen from above",
    "structure_stone": "carved temple stone blocks", "structure_wood": "painted temple wood with gold trim",
}
SOURCES = {  # [U-335] death recap icons for damage that is not a skill / item
    "autoattack": "a crossed sword and fist striking, basic attack", "burn": "flames burning on skin, damage over time",
    "keystone": "a glowing carved keystone rune", "combo": "two elements colliding, fire and wind swirl",
    "chain": "lightning arcing through water", "turret": "a stone watchtower firing a bolt of fire",
    "minion": "a small armoured foot soldier with a spear", "monster": "a snarling spirit beast",
    "passive": "a glowing inner aura", "other": "a question mark carved in stone", "skill": "a glowing spell scroll",
    "item": "a treasure pouch",
}
EMOTES = {  # [U-414] emote pictures (emotes/<name>/emote.png + icon.png); a resource pack can replace them
    "laugh": "a cartoon face laughing with tears of joy", "taunt": "a cocky smirking face with a raised eyebrow",
    "dance": "a small happy figure dancing with music notes", "joke": "a jester mask winking", "wave": "a friendly waving hand",
    "cheer": "two raised fists with confetti",
}
PINGS = {  # [U-414] ping wheel icons (icons/pings/<kind>.png)
    "1": "a red warning triangle with an exclamation mark", "2": "a yellow question mark over an empty footprint",
    "3": "a green arrow running forward", "4": "an orange shield with a helping hand", "5": "a purple fishing hook with bait",
}
SCREENS = {
    "menu": "epic landscape, four nations, water tribe glacier, earth kingdom walls, fire nation volcano, air temple mountains, sunset",
    "loading_1": "two armies of elemental benders facing each other across a river valley, dramatic sky",
    "loading_2": "a comet with a red tail over a volcanic island at night",
    "loading_3": "full moon over an ice city, waterbenders on the walls",
    "loading_4": "air temple on mountain peaks above clouds, gliders",
    "loading_5": "solar eclipse over a fire nation palace",
    "victory": "triumphant banners over a temple at sunrise, golden light",
    "defeat": "ruined temple in rain, broken banners, grey light",
}


def content():
    exe = ROOT / "build" / "dump_content"
    out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        kind, ident, element, name, desc = (line.split("\t") + [""] * 5)[:5]
        yield kind, ident, element, name, desc


def jobs(only):
    seen = set()
    for kind, ident, element, name, desc in content():
        if (kind, ident) in seen:
            continue
        seen.add((kind, ident))
        if kind == "champion" and "champion" in only:
            role, body = (desc.split(" ") + ["", ""])[:2]
            look = ELEMENT_LOOK.get(element, ELEMENT_LOOK["None"])
            who = f"{name}, a {body} {role.lower()} {look}"
            yield "champion_icon", ident, f"portrait bust of {who}, facing viewer, dark background, {STYLE}", 512, 512
            yield "champion_splash", ident, f"full splash art of {who}, dynamic action pose casting, {STYLE}", 832, 1216
            yield "champion_front", ident, (f"full body character design sheet, front view, standing straight in A-pose, arms slightly out, "
                                            f"{who}, plain flat white background, even lighting, {STYLE}"), 768, 1024
        elif kind == "skill" and "skill" in only:
            fx = ELEMENT_FX.get(element, ELEMENT_FX["None"])
            yield "skill", ident, (f"square ability icon for a game, '{name}': {desc}. {fx}, bold silhouette, centered, "
                                   f"dark vignette background, {STYLE}"), 512, 512
        elif kind == "rune" and "rune" in only:  # [U-432]
            yield "rune", ident, (f"square game rune icon: a glowing carved stone rune called '{name}', single object, centered, "
                                  f"dark background, rpg icon, {STYLE}"), 512, 512
        elif kind == "mastery" and "mastery" in only:
            yield "mastery", ident, (f"square game talent icon: '{name}' ({desc} mastery), symbolic emblem, centered, "
                                     f"dark background, rpg icon, {STYLE}"), 512, 512
        elif kind == "item" and "item" in only:
            yield "item", ident, (f"square item icon for a game: {name}, a single object, centered, dark background, "
                                  f"rpg inventory icon, {STYLE}"), 512, 512
    if "texture" in only:
        for ident, what in TEXTURES.items():
            yield "texture", ident, f"seamless tileable top-down texture of {what}, flat even lighting, no shadows, game texture", 512, 512
    if "source" in only:
        for ident, what in SOURCES.items():
            yield "source", ident, f"square game icon: {what}, centered, dark background, {STYLE}", 512, 512
    if "emote" in only:
        for ident, what in EMOTES.items():
            yield "emote", ident, f"square game emote sticker: {what}, bold outline, centered, plain dark background, {STYLE}", 512, 512
    if "ping" in only:
        for ident, what in PINGS.items():
            yield "ping", ident, f"square flat game ping icon: {what}, simple bold shape, centered, transparent-looking dark background", 512, 512
    if "screen" in only:
        for ident, what in SCREENS.items():
            yield "screen", ident, f"{what}, wide establishing shot, {STYLE}", 1216, 832


def graph(prompt, w, h, prefix, seed):
    return {
        "1": {"class_type": "UnetLoaderGGUF", "inputs": {"unet_name": "flux-2-klein-4b-Q8_0.gguf"}},
        "2": {"class_type": "CLIPLoaderGGUF", "inputs": {"clip_name": "Qwen3-4B-Q8_0.gguf", "type": "flux2"}},
        "5": {"class_type": "VAELoader", "inputs": {"vae_name": "flux2-vae.safetensors"}},
        "3": {"class_type": "CLIPTextEncode", "inputs": {"text": prompt, "clip": ["2", 0]}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": NEG, "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
        "7": {"class_type": "KSampler", "inputs": {"model": ["1", 0], "positive": ["3", 0], "negative": ["4", 0],
                                                   "latent_image": ["6", 0], "seed": seed, "steps": 9, "cfg": 1.0,
                                                   "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["5", 0]}},
        "9": {"class_type": "SaveImage", "inputs": {"images": ["8", 0], "filename_prefix": prefix}},
    }


def post(path, data=None):
    req = urllib.request.Request(URL + path, data=json.dumps(data).encode() if data else None,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="champion,skill,item,texture,screen,source")
    ap.add_argument("--comfy-out", required=True)
    a = ap.parse_args()
    only = set(a.only.split(","))
    todo = [j for j in jobs(only) if not (RAW / j[0] / f"{j[1]}.png").exists()]
    print(f"{len(todo)} images to make", flush=True)
    client = str(uuid.uuid4())
    for n, (cat, ident, prompt, w, h) in enumerate(todo, 1):
        dest = RAW / cat / f"{ident}.png"
        dest.parent.mkdir(parents=True, exist_ok=True)
        prefix = f"em_{cat}_{ident}"
        seed = zlib.crc32(f"{cat}/{ident}".encode())
        pid = post("/prompt", {"prompt": graph(prompt, w, h, prefix, seed), "client_id": client})["prompt_id"]
        t0 = time.time()
        while True:
            time.sleep(1)
            hist = post(f"/history/{pid}")
            if pid in hist:
                break
            if time.time() - t0 > 600:
                print(f"timeout {cat}/{ident}", flush=True)
                break
        if pid not in hist:
            continue
        for node in hist[pid].get("outputs", {}).values():
            for img in node.get("images", []):
                src = Path(a.comfy_out) / img.get("subfolder", "") / img["filename"]
                shutil.move(str(src), dest)
        print(f"[{n}/{len(todo)}] {cat}/{ident} {time.time() - t0:.1f}s", flush=True)


if __name__ == "__main__":
    main()
