#!/usr/bin/env python3
"""[U-541] Models made from a picture can keep white bits of its background near the feet: this makes them see-through.
    python3 tools/art/clean_feet.py id id ...      (resourcepacks/default/models_hd/<id>/sheet.png, the bottom part of every frame)
"""
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
for mid in sys.argv[1:]:
    png = ROOT / "resourcepacks" / "default" / "models_hd" / mid / "sheet.png"
    txt = (png.parent / "sheet.txt").read_text()
    px = next(int(l.split()[1]) for l in txt.splitlines() if l.startswith("frame_px"))
    w, h = map(int, subprocess.run(["magick", "identify", "-format", "%w %h", str(png)], capture_output=True, text=True, check=True).stdout.split())
    raw = subprocess.run(["magick", str(png), "-depth", "8", "rgba:-"], capture_output=True, check=True).stdout
    a = np.frombuffer(raw, dtype=np.uint8).reshape(h, w, 4).copy()
    white = (a[:, :, :3].min(axis=2) > 205) & (a[:, :, 3] > 0)
    rows = np.arange(h) % px
    white &= (rows >= int(px * 0.62))[:, None]  # only near the feet of each frame
    a[white, 3] = 0
    subprocess.run(["magick", "-size", f"{w}x{h}", "-depth", "8", "rgba:-", str(png)], input=a.tobytes(), check=True)
    print(mid, int(white.sum()), "pixels")
