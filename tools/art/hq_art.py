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
    "grass": "lush short green grass with small variations, tiny clover", "dirt": "dry cracked brown earth with small stones", "forest_path": "uniform packed brown earth with small pebbles and a few small leaves spread evenly, no path shape, no edges, no border",
    "stone_road": "old fitted cobblestone road, worn stones", "structure_stone": "floor of plain large grey stone slabs of equal size in a regular grid, no carvings, no border", "structure_wood": "dark wooden floor boards all running in one direction, no border, no frame",
    "sand": "fine desert sand with soft wind ripples", "shallow_water": "clear shallow water over pebbles with soft caustics", "deep_water": "deep dark blue sea water, soft waves",
    "lava": "molten lava with dark crust and glowing cracks", "glacier": "blue-white glacier ice with deep cracks", "ice": "smooth pale blue frozen ice sheet with soft faint white veins, low contrast", "snow": "fresh soft snow",
    "wood": "weathered wooden planks all running in one direction, small nails, no border", "rock_wall": "rough grey rock surface", "mud": "wet dark mud with footprints", "ash": "grey volcanic ash ground", "burned": "scorched burned ground, charcoal",
    "tree_canopy": "dense tree canopy from straight above, layered green leaves", "bush": "dense green bush leaves from straight above",
}
ALT_LOOK = {
    "grass": "autumn meadow, short golden and rust grass, fallen red leaves", "dirt": "dark reddish earth with small stones", "forest_path": "forest path covered with orange fallen leaves",
    "stone_road": "mossy old cobblestone road with leaves", "structure_stone": "floor of plain red sandstone slabs of equal size in a regular grid, no carvings, no border", "structure_wood": "weathered reddish floor boards all running in one direction, no border",
    "sand": "pale grey dune sand", "shallow_water": "shallow amber river water over dark pebbles", "deep_water": "deep teal water with soft waves",
    "lava": "molten lava with black crust", "glacier": "pale frosted glacier ice", "ice": "smooth pale grey frozen ice with soft faint veins, low contrast", "snow": "thin fresh snow evenly covering the ground, soft white with faint grey shadows, no earth showing",
    "wood": "old dark planks all running in one direction, no border", "rock_wall": "dark mossy rock surface", "mud": "wet reddish mud", "ash": "dark ash with embers", "burned": "scorched ground with red embers",
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
    for attempt in range(4):  # the graphics card may run out of memory for a moment (another program): just try again
        prefix = "emhq_" + uuid.uuid4().hex[:10]
        body = json.dumps({"prompt": graph(prompt, w, h, prefix, seed)}).encode()
        urllib.request.urlopen(urllib.request.Request(URL + "/prompt", data=body, headers={"Content-Type": "application/json"}), timeout=60)
        for _ in range(240):
            time.sleep(1)
            found = sorted(OUT.glob(prefix + "*.png"))
            if found:
                time.sleep(0.2)
                shutil.move(str(found[0]), dst)
                print(dst.relative_to(RAW), flush=True)
                return
        print(f"no picture for {dst.name}, try {attempt + 2}", flush=True)
    print(f"GAVE UP on {dst}", flush=True)


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


NATION = {"Fire": "deep red and gold clothes of the fire nation, flames around the hands", "Water": "blue and white clothes of the water tribes, flowing water and ice around the hands",
          "Earth": "green and brown clothes of the earth kingdom, stone and dust rising", "Air": "saffron yellow and orange clothes of the air nomads, swirling wind around the hands",
          "Non-bender": "grey steel armour with purple cloth, no magic, weapons and tools"}

# [U-521] each champion is one person: age, face, hair, what makes them unlike the others (the colours of the nation stay, as on the model)
PERSON = {
    "ember_monk": "calm old monk with a shaved head, grey eyebrows and a crown of small flames", "storm_daughter": "fierce young woman, wild black hair with white streaks, sparks in her eyes",
    "stone_warden": "huge broad man with a square jaw, a stone shield and a short grey beard", "iron_sentinel": "stern woman in her forties, cropped hair, iron plates on the shoulders, a scar over the lip",
    "magma_herald": "dark skinned man with glowing cracks on his arms, a molten stone collar, long braids", "tide_healer": "gentle middle aged woman with white hair in a bun, a shell necklace, kind eyes",
    "frost_hunter": "lean young man with pale blue hair, a fur hood and a bone spear", "wind_nomad": "cheerful teenage girl with a messy ponytail and a glider staff, blue arrow on the forehead",
    "mist_dancer": "graceful androgynous dancer with long silver hair, half hidden in mist, a thin silk veil", "chi_blocker": "small quick woman with a short bob, a grey mask under the chin, gloved fingers ready",
    "avatar_kyren": "young serious person with glowing white eyes, short dark hair and small symbols of all four elements around", "glacier_warden": "giant old man with a white beard like ice, a heavy fur cloak, a block of ice on one arm",
    "brine_sage": "wrinkled old woman with seaweed in grey hair, coral earrings, a driftwood staff", "moon_weaver": "pale young woman with very long black hair, a silver crescent on the brow, threads of water",
    "mire_stalker": "heavy swamp man with a mossy beard, a wide reed hat and a bark armour vest", "boil_caller": "angry red faced man with a thick moustache, steam coming from boiling water around him",
    "thorn_binder": "thin woman with dark green eyes, vines of frozen thorns around her wrists, messy hair", "rime_archer": "young blonde woman with frost on her eyelashes, an ice bow, a white fur collar",
    "rain_caller": "soft spoken boy with wet curly hair and a paper umbrella, rain falling behind him", "tidal_duelist": "elegant man with a thin beard and a slim curved water blade, a tied back ponytail",
    "snowdrift_trickster": "grinning girl with freckles and short white hair, a snowball in hand, a fox fur hat", "marsh_shaman": "tribal woman with painted face, feathers and bones in dark hair",
    "dew_mystic": "very young monk with a shaved head and closed eyes, small floating drops of water all around", "blade_wanderer": "tired man with a long scar and an unshaven face, a worn straw hat, two swords",
    "siege_breaker": "enormous bald woman with arms like tree trunks and a big iron hammer", "dusk_hunter": "hooded woman with amber eyes, a crossbow, half of the face in shadow",
    "cairn_mason": "stocky bearded dwarf-like man with a mason's hammer and stone dust in his hair", "dune_reaver": "desert raider with a cloth wrapped over the head, sun dark skin, two sand-worn blades",
    "tremor_sage": "old bald man with closed eyes and bare feet, listening to the ground, a long thin beard", "ashen_smith": "muscled woman blacksmith with soot on the face, a leather apron and tied up red hair",
    "cinder_oracle": "blind old woman with ash white eyes and grey skin, a veil of smoke, a burning stone in hand", "rill_quarrier": "wiry young man with a miner's headband, a sling and stone chips, big grin",
    "gantry_warden": "square faced engineer man with goggles on the forehead and a stone hook", "shard_dancer": "slim young woman with sharp cheekbones, floating crystal shards, cropped dark hair",
    "mire_weaver": "stooped middle aged man with muddy hands, long wet hair and a net of mud", "moss_titan": "giant green skinned man overgrown with moss and small ferns, stone tusks",
    "plate_vanguard": "proud soldier woman with a tall helmet, a big rectangular stone shield and a stern look", "basalt_hermit": "wild old hermit with a huge grey beard and black stones circling his head",
    "oathless_duelist": "handsome man with a broken noble crest, long dark hair and a thin rapier, a mocking smile", "thornwick_trapper": "young woman with a tangle of brown hair, a coil of wire and small traps on the belt",
    "hush_alchemist": "thin pale man with round glasses, a scarf over the mouth and glass flasks on a bandolier", "ashen_duelist": "white haired young man with burnt sleeves and a narrow sword wreathed in fire",
    "voltaic_seer": "woman with short electric blue streaked hair, one blind white eye, lightning between her fingers", "thunder_warden": "broad old general with a thick black beard and a heavy war coat, lightning on his armour",
    "azure_pyre": "cold looking young noblewoman with a high topknot and blue flames in both hands", "cobalt_wraith": "gaunt masked person with hollow eyes and a thin blue flame, a tattered hood",
    "powder_saboteur": "grinning soot-faced man with a singed moustache and fuses and bombs on the chest", "ember_glider": "young woman pilot with leather goggles, a fire red scarf and a wide wing frame behind her",
    "hearth_keeper": "warm plump grandmother with rosy cheeks, a shawl and a small house fire in a lantern", "bolt_dancer": "barefoot young man with a long ponytail, twin sparking blades, a wild laugh",
    "pyre_colossus": "gigantic bald man with burning cracks on his bare chest and a chained iron brazier", "dusk_marshal": "tall woman officer with a long coat, a broad hat and a fire lance rifle",
    "sunspear_zealot": "shouting priest with a sun mask pushed up, a shaved head and a burning spear", "thornwood_ranger": "bearded forest man with a green hood over steel, a long bow, leaves in his hair",
    "ashgrove_spearman": "dark skinned young soldier with short hair, a long spear with a burnt tip and a calm stare", "silent_needle": "tiny pale woman with a tight black bun, needles held between the fingers, no expression",
    "gale_warden": "broad monk with big arms, a shaved head with blue arrow tattoos and a heavy iron staff", "cirrus_archer": "tall elf-like woman with white hair in a long braid and a thin bow of wind",
    "hollow_breeze": "sad thin monk with sunken cheeks, an empty look and a bell around the neck", "typhoon_caller": "wild haired woman with her arms up, a spinning storm behind her and glowing eyes",
    "cloud_courier": "laughing boy with a satchel of letters, a big orange scarf and wind sandals", "vapor_sage": "old woman with long thin white hair and a pipe, pale green wisps around her",
    "skylark_dancer": "young woman with ribbons in brown hair, a feathered fan in each hand, spinning", "wisp_hermit": "tiny old man with an enormous white beard, sitting on a small cloud, smiling",
    "thermal_rider": "tanned young man with wild windblown hair, aviator goggles and a wide glider", "eddy_binder": "thin young woman with curled hair and small spirals of air tied with thread",
    "monsoon_herald": "tall dark skinned man with long wet locks and a big horn, rain and wind behind him", "glider_scout": "short young woman with a leather cap, a spyglass and folded mechanical wings",
    "gadget_smith": "bald tinkerer with a magnifying lens on one eye, a mechanical arm and many tools", "spirit_wanderer": "pale young traveller with a spirit mask on the side of the head and faint glowing wisps",
    "hush_fang": "lean woman with sharp teeth, a wolf skin hood and a curved knife", "dark_avatar_vael": "pale young person with black glowing eyes, dark purple grey robes and black smoke around the hands",
}


def who(cid, element, name):
    return f"{PERSON.get(cid, name)}, {NATION.get(element, NATION['Non-bender'])}, a clearly individual face, costume and silhouette unlike any other hero"


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
                                         "taliyah": "a young desert earthbender woman, woven blue and gold clothes, long dark braid",
                                         "dark_avatar_vael": "a pale young person in dark purple and grey air nomad robes, black smoke around the hands"}.items()):
            dst = ROOT / "art" / "raw" / "champion_front" / f"{cid}.png"
            if not dst.exists():
                make(RAW / "front" / f"{cid}.png", f"one single figure, full body, front view, standing straight, arms slightly out, {look}, plain flat white background, even lighting, no text, hand painted fantasy game art", 768, 1024, 6000 + i)
                shutil.copy(RAW / "front" / f"{cid}.png", dst)
    if "structures" in what:  # [U-541] the buildings of the two nations (tools/art/structure_models.py makes the models)
        looks = {"fire": "red and gold fire nation style, dark iron, burning brazier on top", "earth": "green and bronze earth kingdom style, carved stone, glowing green crystal on top"}
        shapes = {"turret": "one tall narrow defence tower, square base, tapering shaft, a lookout with a pointed roof",
                  "inhibitor": "one small wide shrine with a low roof and a big glowing crystal in the middle, on a round stone plinth",
                  "nexus": "one great temple tower of three stepped levels, wide base, a huge glowing heart stone at the top"}
        for i, (sid, shape) in enumerate(shapes.items()):
            for j, (team, look) in enumerate(looks.items()):
                dst = ROOT / "art" / "raw" / "structure_front" / f"{sid}_{team}.png"
                if not dst.exists():
                    make(RAW / "structures" / f"{sid}_{team}.png", f"{shape}, {look}, a single building seen straight from the front, whole building visible, standing on nothing, plain flat white background, even lighting, no ground, no shadow, no text, hand painted fantasy game art, rich detail", 768, 1024, 6500 + i * 10 + j)
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy(RAW / "structures" / f"{sid}_{team}.png", dst)
    if "temple" in what:  # [U-350] an Air Nomad temple, a place on the base map
        dst = ROOT / "art" / "raw" / "structure_front" / "air_temple.png"
        if not dst.exists():
            make(RAW / "structures" / "air_temple.png", "one air nomad temple, tall slender towers with blue pointed roofs and golden tops around a white stone hall, arches, a single building seen straight from the front, whole building visible, standing on nothing, plain flat white background, even lighting, no ground, no shadow, no text, hand painted fantasy game art, rich detail", 768, 1024, 6600)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(RAW / "structures" / "air_temple.png", dst)
    if "icons" in what:
        for i, (cid, (el, name, desc)) in enumerate(champs.items()):
            make(RAW / "icons" / f"{cid}.png", f"close portrait bust of {who(cid, el, name)}, strong rim light, dark painted background, {STYLE}", 768, 768, 8000 + i)
    if "splash" in what:
        for i, (cid, (el, name, desc)) in enumerate(champs.items()):
            make(RAW / "splash" / f"{cid}.png", f"full splash art of {who(cid, el, name)}, dynamic pose in the middle of an action, a place of its own in the background, {STYLE}", 832, 1216, 9000 + i)


if __name__ == "__main__":
    main()
