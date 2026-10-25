#!/usr/bin/env python3
"""[U-257] One-off pictures from ComfyUI for own content (own champions, skins, packs).

    python3 tools/art/gen_custom.py --comfy-out DIR  OUT.png  WIDTH HEIGHT  "prompt"  [OUT2.png W H "prompt2" ...]

Same model / settings as gen_art.py (FLUX.2 klein 4B). ComfyUI must be running on 127.0.0.1:8188.
"""
import argparse, shutil, sys, time, zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gen_art import graph, post  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--comfy-out", required=True)
    ap.add_argument("jobs", nargs="+")
    a = ap.parse_args()
    j = a.jobs
    for i in range(0, len(j) - 3, 4):
        out, w, h, prompt = Path(j[i]), int(j[i + 1]), int(j[i + 2]), j[i + 3]
        out.parent.mkdir(parents=True, exist_ok=True)
        pid = post("/prompt", {"prompt": graph(prompt, w, h, "em_custom", zlib.crc32(str(out).encode()))})["prompt_id"]
        while True:
            time.sleep(1)
            hist = post(f"/history/{pid}")
            if pid in hist:
                break
        for node in hist[pid]["outputs"].values():
            for img in node.get("images", []):
                shutil.move(str(Path(a.comfy_out) / img.get("subfolder", "") / img["filename"]), out)
        print("made", out, flush=True)


if __name__ == "__main__":
    main()
