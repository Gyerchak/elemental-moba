#!/usr/bin/env python3
"""[U-541] The buildings of the two nations: art/raw/structure_front/<turret|inhibitor|nexus>_<fire|earth>.png -> models_hd/<same name> (Blender).

    EM_GPU=1 python3 tools/art/structure_models.py [id,id]
"""
import json
import os
import subprocess
import sys
from pathlib import Path

os.environ["EM_HD"] = "1"
sys.path.insert(0, str(Path(__file__).resolve().parent))
saved, sys.argv = sys.argv, sys.argv[:1]
import make_models as mm  # noqa: E402
sys.argv = saved

ROOT = Path(__file__).resolve().parents[2]
# the same width of the picture and the same feet as the old models, so they stand where the hitbox is
SIZES = {"air": (8.0, 0.8, 6.2, 448), "turret": (6.0, 0.85, 4.9, 384), "inhibitor": (3.6, 0.8, 2.5, 320), "nexus": (8.0, 0.78, 5.6, 448)}
only = set(filter(None, (sys.argv[1] if len(sys.argv) > 1 else "").split(",")))
for pic in sorted((ROOT / "art" / "raw" / "structure_front").glob("*.png")):
    sid = pic.stem
    if only and sid not in only:
        continue
    kind = sid.split("_")[0]
    ortho, anchor, height, px = SIZES[kind]
    work = mm.WORK / sid
    work.mkdir(parents=True, exist_ok=True)
    (work / "done").unlink(missing_ok=True)
    job = {"picture": str(pic), "out": str(work), "ortho": ortho, "px": px, "anchor": anchor, "height": height, "samples": 16,
           "frames": {"idle": 1, "death": 5}, "rigid": True}
    (work / "job.json").write_text(json.dumps(job))
    subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools/art/art_model_blender.py"), "--", str(work / "job.json")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    mm.pack({"id": sid, "kind": "turret", "px": px, "ortho": ortho, "anchor": anchor, "dirs": 1, "anims": ["idle", "death"]})
    print("structure", sid, flush=True)
