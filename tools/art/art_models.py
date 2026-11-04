#!/usr/bin/env python3
"""[U-504] Every champion's full-body art (art/raw/champion_front/<id>.png) → its ORIGINAL model sheet in models_hd/<id>.

    EM_GPU=1 python3 tools/art/art_models.py [id,id,…]     (a stopped run continues: finished champions are skipped)

The model is built from the picture (tools/art/art_model_blender.py): silhouette inflated into a body, the art as texture,
soft joints for real poses. 192 px frames, 34 frames per direction, wrapped to 4032 × 3072 (fits every GPU).
"""
import json
import os
import subprocess
import sys
from pathlib import Path

os.environ["EM_HD"] = "1"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_models as mm  # noqa: E402  (pack(), the HD folders + frame counts)

ROOT = Path(__file__).resolve().parents[2]
ORTHO, PX, ANCHOR, HEIGHT = 2.6, 192, 0.85, 1.9


def main():
    only = set(filter(None, (sys.argv[1] if len(sys.argv) > 1 else "").split(",")))
    pics = sorted((ROOT / "art" / "raw" / "champion_front").glob("*.png"))
    for n, pic in enumerate(pics):
        cid = pic.stem
        dst = mm.PACK / cid
        if (only and cid not in only) or (not only and (dst / ".from_art").exists()):
            continue
        work = mm.WORK / cid
        work.mkdir(parents=True, exist_ok=True)
        (work / "done").unlink(missing_ok=True)
        job = {"picture": str(pic), "out": str(work), "ortho": ORTHO, "px": PX, "anchor": ANCHOR, "height": HEIGHT, "samples": 16, "frames": mm.FRAMES}
        (work / "job.json").write_text(json.dumps(job))
        subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools/art/art_model_blender.py"), "--", str(work / "job.json")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        mm.pack({"id": cid, "kind": "humanoid", "px": PX, "ortho": ORTHO, "anchor": ANCHOR, "anims": mm.HUMAN})
        (dst / ".from_art").write_text("ok\n")
        print(f"[{n + 1}/{len(pics)}] {cid}", flush=True)


if __name__ == "__main__":
    main()
