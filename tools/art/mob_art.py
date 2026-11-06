#!/usr/bin/env python3
"""[U-520] Monsters and minions get what champions have: full-body art (local ComfyUI) → an art model (Blender) in models_hd/<id>.

    python3 tools/art/mob_art.py art        (ComfyUI must run: ~/ComfyUI/start-comfyui.sh; existing pictures are kept)
    EM_GPU=1 python3 tools/art/mob_art.py models [id,id]

Everything stays on this computer. Structures keep their own models.
"""
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.request
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art" / "raw" / "mob_front"
URL = "http://127.0.0.1:8188"
STYLE = "stylized fantasy game art, clean shapes, painterly, high detail, Eastern fantasy world of four elements"
MOBS = {
    "wandering_spirit": "a small glowing forest spirit, soft translucent body, round head",
    "spirit_wolf": "a wolf spirit standing upright on its hind legs, pale blue fur",
    "cursed_spirit": "a thin cursed spirit in tattered dark robes, glowing eyes",
    "angry_beast": "a huge angry horned beast standing on two legs, thick brown fur",
    "bandit": "a human bandit with a curved sword, patched leather armour, hood",
    "bandit_archer": "a human bandit archer with a bow, patched leather armour, hood",
    "face_stealer": "a tall faceless spirit with many masks, long dark body",
    "platypus_bear": "a big platypus bear creature standing on two legs, duck bill, brown fur",
    "knowledge_seeker": "a small fox spirit scholar standing upright carrying scrolls",
    "dark_spirit": "a dark spirit with purple glowing veins, smoky body",
    "swamp_monster": "a monster made of vines, roots and swamp water, huge arms",
    "lion_turtle": "an ancient lion turtle creature standing upright, stone shell, lion mane",
    "sun_dragons": "a red and gold eastern dragon standing upright, coiled, fiery mane",
    "minion_melee": "a small armoured foot soldier with a spear",
    "minion_ranged": "a small robed soldier with a short bow",
    "minion_siege": "a stocky soldier carrying a heavy stone launcher on the back",
    "minion_super": "a large heavily armoured soldier with a big hammer",
}


def graph(prompt, prefix, seed):
    return {
        "1": {"class_type": "UnetLoaderGGUF", "inputs": {"unet_name": "flux-2-klein-4b-Q8_0.gguf"}},
        "2": {"class_type": "CLIPLoaderGGUF", "inputs": {"clip_name": "Qwen3-4B-Q8_0.gguf", "type": "flux2"}},
        "5": {"class_type": "VAELoader", "inputs": {"vae_name": "flux2-vae.safetensors"}},
        "3": {"class_type": "CLIPTextEncode", "inputs": {"text": prompt, "clip": ["2", 0]}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": "text, watermark, multiple views, cropped", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage", "inputs": {"width": 768, "height": 1024, "batch_size": 1}},
        "7": {"class_type": "KSampler", "inputs": {"model": ["1", 0], "positive": ["3", 0], "negative": ["4", 0], "latent_image": ["6", 0], "seed": seed,
                                                   "steps": 9, "cfg": 1.0, "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["5", 0]}},
        "9": {"class_type": "SaveImage", "inputs": {"images": ["8", 0], "filename_prefix": prefix}},
    }


def art():
    RAW.mkdir(parents=True, exist_ok=True)
    out_dir = Path(os.environ.get("EM_COMFY_OUT", Path.home() / "ComfyUI" / "output"))  # ComfyUI's --output-directory
    for n, (mid, what) in enumerate(MOBS.items()):
        dst = RAW / f"{mid}.png"
        if dst.exists():
            continue
        prompt = f"full body character design sheet, front view, standing straight, whole body visible, {what}, plain flat white background, even lighting, {STYLE}"
        prefix = f"em_mob_{mid}_{uuid.uuid4().hex[:6]}"
        body = json.dumps({"prompt": graph(prompt, prefix, 1000 + n)}).encode()
        urllib.request.urlopen(urllib.request.Request(URL + "/prompt", data=body, headers={"Content-Type": "application/json"}), timeout=60)
        for _ in range(240):
            time.sleep(2)
            found = sorted(out_dir.glob(prefix + "*.png"))
            if found:
                shutil.move(str(found[0]), dst)
                print("art", mid, flush=True)
                break
        else:
            raise SystemExit(f"no picture for {mid}")


def models(only):
    os.environ["EM_HD"] = "1"
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    saved = sys.argv
    sys.argv = saved[:1]
    import make_models as mm
    sys.argv = saved
    for pic in sorted(RAW.glob("*.png")):
        mid = pic.stem
        if only and mid not in only:
            continue
        work = mm.WORK / ("mob_" + mid)
        work.mkdir(parents=True, exist_ok=True)
        (work / "done").unlink(missing_ok=True)
        frames = {k: mm.FRAMES[k] for k in mm.MOB}
        job = {"picture": str(pic), "out": str(work), "ortho": 2.6, "px": 192, "anchor": 0.85, "height": 1.9, "samples": 12, "frames": frames}
        (work / "job.json").write_text(json.dumps(job))
        subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools/art/art_model_blender.py"), "--", str(work / "job.json")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        old = mm.WORK
        mm.WORK = work.parent
        link = work.parent / mid
        if link.exists() or link.is_symlink():
            link.unlink()
        link.symlink_to(work.name)
        mm.pack({"id": mid, "kind": "humanoid", "px": 192, "ortho": 2.6, "anchor": 0.85, "anims": mm.MOB})
        mm.WORK = old
        print("model", mid, flush=True)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "art"
    if what == "art":
        art()
    else:
        models(set(filter(None, (sys.argv[2] if len(sys.argv) > 2 else "").split(","))))
