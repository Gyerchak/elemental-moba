#!/usr/bin/env python3
"""[U-392] Sound effects made with FL Studio (through Wine) instead of the scipy generator.

    python3 tools/art/fl_sounds.py [--list] [--only NAME,NAME]

How it works
1. Projects: tools/art/fl_sounds/<sound name>.flp — one FL Studio project per game sound (the name = the file the game
   plays: resourcepacks/default/sounds/<sound name>.wav, e.g. emote_laugh, companion_0, fx/bolt_fire, hit_fire).
   Make / change them in FL Studio and save them there. `--list` prints every sound name the game uses.
2. Variants (needs PyFLP): VARIANTS below derives many sounds from one project — e.g. every element of a skill
   shape from one "fx_bolt" project, by moving the notes (semitones), stretching them and changing the tempo.
   PyFLP: python3 -m venv --system-site-packages .venv-fl && .venv-fl/bin/pip install pyflp   (in the box folder),
   then run this script with .venv-fl/bin/python.
3. Rendering: FL Studio's command line (`FL64.exe /R /Ewav project.flp`, through ~/Wine/FlStudio/fl-wine.sh) — tested
   2026-10-30: it writes project.wav next to the project. FL stays open after a render, so this script closes the
   Wine prefix after each file. It REFUSES to run while you have FL Studio open (it would close your session).
4. The WAV is trimmed (silence), normalised and saved as mono 44.1 kHz into the default resource pack.

Every step prints what it does; nothing outside the box / scratch is written except FL's own temp files.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[2]
PROJECTS = ROOT / "tools" / "art" / "fl_sounds"
SND = ROOT / "resourcepacks" / "default" / "sounds"
WORK = Path(os.environ.get("OCC_BOX_SCRATCH", "/tmp/em-fl")) / "fl_sounds"
FL_DIR = Path.home() / "Wine" / "FlStudio"
FL_RUN = FL_DIR / "fl-wine.sh"
WINESERVER = Path("/usr/share/steam/compatibilitytools.d/proton-cachyos-native/files/bin/wineserver")

ELEMENTS = ["water", "earth", "fire", "air", "none"]
# derived sounds: target name → (project name, semitones, length factor, tempo factor)
# one project per skill SHAPE; the elements differ in pitch / speed (water low + slow, air high + fast …)
EL_SHIFT = {"water": (-5, 1.2, 0.9), "earth": (-9, 1.3, 0.85), "fire": (0, 1.0, 1.0), "air": (7, 0.8, 1.2), "none": (3, 0.9, 1.0)}
VARIANTS = {}
for shape in ["bolt", "wave", "burst", "field", "cone", "dash", "wall", "tornado"]:
    for el in ELEMENTS:
        st, ln, tp = EL_SHIFT[el]
        VARIANTS[f"fx/{shape}_{el}"] = (f"fx_{shape}", st, ln, tp)
for el in ELEMENTS:
    st, ln, tp = EL_SHIFT[el]
    VARIANTS[f"hit_{el}"] = ("hit", st, ln, tp)


def game_sound_names():
    return sorted(str(p.relative_to(SND).with_suffix("")) for p in SND.rglob("*.wav"))


def fl_running():
    # exact process name (-f would match any command line that merely CONTAINS "FL64.exe", e.g. this script's shell)
    return subprocess.run(["pgrep", "-x", "FL64.exe"], capture_output=True).returncode == 0


def close_fl():
    env = dict(os.environ, WINEPREFIX=str(FL_DIR / "prefix"))
    subprocess.run([str(WINESERVER), "-k"], env=env, capture_output=True)


def render(flp: Path) -> Path | None:
    """FL Studio renders flp → flp.wav (same folder). Returns the WAV or None."""
    out = flp.with_suffix(".wav")
    if out.exists():
        out.rename(out.with_suffix(".old.wav"))  # (a file this script made earlier in the work folder; never deleted)
    win = "Z:" + str(flp).replace("/", "\\")
    proc = subprocess.Popen([str(FL_RUN), "FL64.exe", "/R", "/Ewav", win], cwd=FL_DIR, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    last, stable, t0 = -1, 0, time.time()
    while time.time() - t0 < 180:  # the WAV appears, then stops growing → done
        time.sleep(1)
        size = out.stat().st_size if out.exists() else -1
        stable = stable + 1 if size > 0 and size == last else 0
        last = size
        if stable >= 3:
            break
    proc.kill()
    close_fl()
    time.sleep(1)
    return out if out.exists() and out.stat().st_size > 44 else None


def make_variant(src: Path, dst: Path, semis: int, length: float, tempo: float) -> bool:
    try:
        import pyflp  # noqa: PLC0415
    except ImportError:
        print("  PyFLP is not installed — variants are skipped (see the top of this file)")
        return False
    project = pyflp.parse(str(src))
    try:
        project.tempo = float(project.tempo) * tempo
    except Exception as e:  # older / newer PyFLP versions name things differently
        print("  (tempo not changed:", e, ")")
    for pattern in project.patterns:
        for note in pattern.notes:
            note.key = max(0, min(131, int(note.key) + semis))
            note.length = max(1, int(note.length * length))
    pyflp.save(project, str(dst))
    return True


def install(wav: Path, name: str):
    rate, x = wavfile.read(wav)
    x = x.astype(np.float32)
    if x.ndim > 1:
        x = x.mean(axis=1)
    peak = np.abs(x).max() or 1.0
    x = x / peak
    loud = np.nonzero(np.abs(x) > 0.01)[0]  # trim silence at both ends (FL renders a tail)
    if len(loud):
        x = x[max(0, loud[0] - 64): min(len(x), loud[-1] + int(0.05 * rate))]
    if rate != 44_100:
        x = resample_poly(x, 44_100, rate).astype(np.float32)
    dst = SND / f"{name}.wav"
    dst.parent.mkdir(parents=True, exist_ok=True)
    wavfile.write(dst, 44_100, (np.clip(x * 0.85, -1, 1) * 32767).astype(np.int16))
    print(f"  → {dst.relative_to(ROOT)}  ({len(x) / 44_100:.2f} s)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="print every sound name the game uses")
    ap.add_argument("--only", default="", help="comma-separated sound names")
    a = ap.parse_args()
    if a.list:
        print("\n".join(game_sound_names()))
        return
    if fl_running():
        sys.exit("FL Studio is open — close it first (rendering would close your FL session).")
    PROJECTS.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    only = set(filter(None, a.only.split(",")))
    jobs = []  # (sound name, project to render)
    for flp in sorted(PROJECTS.glob("*.flp")):  # 1. direct projects (the file name = the sound name; "/" as "__")
        name = flp.stem.replace("__", "/")
        if not only or name in only:
            work = WORK / f"{flp.stem}.flp"
            shutil.copy(flp, work)
            jobs.append((name, work))
    for name, (proj, semis, length, tempo) in VARIANTS.items():  # 2. variants of a shape project
        src = PROJECTS / f"{proj}.flp"
        if not src.exists() or (only and name not in only) or (PROJECTS / f"{name.replace('/', '__')}.flp").exists():
            continue
        work = WORK / f"{name.replace('/', '__')}.flp"
        print(f"variant {name} ← {proj} ({semis:+d} semitones, ×{length} length, ×{tempo} tempo)")
        if make_variant(src, work, semis, length, tempo):
            jobs.append((name, work))
    if not jobs:
        print(f"No projects in {PROJECTS.relative_to(ROOT)} yet. Save FL projects there, named like a game sound "
              f"(--list shows the names; use '__' for '/', e.g. fx__bolt_fire.flp), or fx_<shape>.flp / hit.flp for variants.")
        return
    for name, work in jobs:
        print(f"render {name}")
        wav = render(work)
        if wav:
            install(wav, name)
        else:
            print("  FL Studio did not write a WAV (open the project in FL once to check it plays)")


if __name__ == "__main__":
    main()
