#!/usr/bin/env python3
"""[U-510] Makes a model sheet from one file (the champion maker's buttons):

    python3 tools/art/model_from_file.py <picture.png | model.glb / .gltf / .fbx / .obj> <folder for sheet.png + sheet.txt>

A full-body picture is inflated into a body (art_model_blender.py); a 3D file is rendered as it is (mesh_model_blender.py).
Prints DONE at the end; a failure ends with an exit code above 0. Python: Blender's scripting language decides this one.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

if len(sys.argv) < 3:
    raise SystemExit(__doc__)
src, dest = Path(sys.argv[1]), Path(sys.argv[2])
sys.argv = sys.argv[:1]  # make_models reads its own arguments
os.environ["EM_HD"] = "1"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_models as mm  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ORTHO, PX, ANCHOR, HEIGHT = 2.6, 192, 0.85, 1.9


def main():
    cid = dest.name
    mm.PACK = dest.parent
    work = mm.WORK / cid
    work.mkdir(parents=True, exist_ok=True)
    (work / "done").unlink(missing_ok=True)
    picture = src.suffix.lower() in (".png", ".jpg", ".jpeg", ".webp", ".bmp")
    job = {"out": str(work), "ortho": ORTHO, "px": PX, "anchor": ANCHOR, "height": HEIGHT, "samples": 12, "frames": mm.FRAMES,
           ("picture" if picture else "model"): str(src.resolve())}
    (work / "job.json").write_text(json.dumps(job))
    script = "art_model_blender.py" if picture else "mesh_model_blender.py"
    subprocess.run(["blender", "-b", "--factory-startup", "-P", str(ROOT / "tools/art" / script), "--", str(work / "job.json")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not (work / "done").exists():
        raise SystemExit("Blender did not finish")
    mm.pack({"id": cid, "kind": "humanoid", "px": PX, "ortho": ORTHO, "anchor": ANCHOR, "anims": mm.HUMAN})
    print("DONE", flush=True)


if __name__ == "__main__":
    main()
