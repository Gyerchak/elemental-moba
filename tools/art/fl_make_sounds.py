#!/usr/bin/env python3
"""[U-427] Game sounds MADE WITH FL STUDIO (through Wine) — no PyFLP needed.

    python3 tools/art/fl_make_sounds.py [--only name,name] [--list]

How:
1. A base project is built from FL's own templates (tools/art/flp.py reads / writes .flp files):
   the "Basic" template (channels 0-3: 808 kick, clap, hi-hat, snare — samplers) + five 3x Osc synth channels
   taken from the "Electro" template (4 sub, 5 bass 1, 6 bass 2, 7 synth 1, 8 synth 2).
2. Every sound below is a short note recipe (channel, start, length, key, velocity) written into pattern 1.
3. FL Studio renders the project from its command line (FL64.exe /R /Ewav, tested 2026-10-30) → a WAV.
4. The WAV is trimmed, normalised and saved (mono 44.1 kHz) into resourcepacks/default/sounds/<name>.wav.
FL is closed after every render (its Wine prefix) — so the script REFUSES to run while FL Studio is open.
"""
import argparse
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

sys.path.insert(0, str(Path(__file__).resolve().parent))
import flp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SND = ROOT / "resourcepacks" / "default" / "sounds"
WORK = Path(os.environ.get("OCC_BOX_SCRATCH", "/tmp/em-fl")) / "fl_make"
FL_DIR = Path.home() / "Wine" / "FlStudio"
TEMPL = FL_DIR / "Data" / "Templates"
WINESERVER = Path("/usr/share/steam/compatibilitytools.d/proton-cachyos-native/files/bin/wineserver")
KICK, CLAP, HAT, SNARE, SUB, BASS1, BASS2, SYN1, SYN2 = range(9)


def base_project():
    hdr, ev = flp.read(TEMPL / "Minimal" / "Basic" / "Basic.flp")
    _, ev2 = flp.read(TEMPL / "Advanced" / "Electronic" / "Electro.flp")
    ks = [k for k, (i, _) in enumerate(ev2) if i == 64]
    k3 = [k for k, (i, _) in enumerate(ev) if i == 64][3]
    end = next(k for k in range(k3, len(ev)) if ev[k][0] == 99)  # after the last channel
    add = []
    for n, src in enumerate((1, 2, 3, 4, 5)):  # Electro's 3x Osc channels → our channels 4 … 8
        blk = list(ev2[ks[src]:ks[src + 1]])
        blk[0] = (64, struct.pack("<H", 4 + n))
        add += blk
    ev = ev[:end] + add + ev[end:]
    return (hdr[0], 9, hdr[2]), ev


def with_notes(hdr, ev, notes, tempo=None):
    ppq = hdr[2]
    first = next(k for k, (i, _) in enumerate(ev) if i == 64)
    data = b"".join(flp.note(int(p * ppq), max(1, int(l * ppq)), k, v, ch=c) for c, p, l, k, v in notes)
    out = ev[:first] + [(65, struct.pack("<H", 1)), (224, data)] + ev[first:]
    if tempo:
        out = [(156, struct.pack("<I", int(tempo * 1000))) if i == 156 else (i, v) for i, v in out]
    return out


# ---- the recipes: (channel, start in beats, length in beats, key (60 = C4), velocity) — Claude ideas
def arp(ch, keys, step=0.25, length=0.25, vel=90, start=0.0):
    return [(ch, start + i * step, length, k, vel) for i, k in enumerate(keys)]


RECIPES = {
    # emotes
    "emote_laugh": arp(SYN1, [79, 77, 76, 74, 72], 0.12, 0.1, 85),
    "emote_taunt": [(BASS1, 0, 0.3, 43, 95), (BASS1, 0.4, 0.6, 38, 95), (SNARE, 0.4, 0.2, 60, 70)],
    "emote_dance": arp(SYN2, [60, 64, 67, 72, 67, 64], 0.18, 0.15, 85) + [(KICK, 0, 0.2, 60, 90), (KICK, 0.72, 0.2, 60, 90)],
    "emote_joke": arp(SYN1, [60, 63, 66, 69, 72, 75], 0.07, 0.07, 80),
    "emote_wave": [(SYN2, 0, 1.2, 84, 75), (SYN2, 0.15, 1.0, 88, 65)],
    "emote_cheer": [(CLAP, i * 0.12, 0.1, 60, 70 + 4 * i) for i in range(6)] + [(SYN1, 0.7, 0.8, k, 85) for k in (72, 76, 79)],
    # companions arriving
    "companion_0": [(SUB, 0, 1.6, 31, 110), (BASS2, 0.2, 1.2, 38, 70)],                 # sky bison
    "companion_1": [(BASS1, 0, 0.25, 45, 110), (KICK, 0, 0.2, 55, 90), (BASS1, 0.4, 0.3, 43, 110)],  # polar bear-dog
    "companion_2": arp(SYN1, [88, 91, 86], 0.08, 0.07, 80),                             # monkey-cat
    "companion_3": [(HAT, i * 0.07, 0.06, 60 + (i % 3), 60 + 5 * i) for i in range(8)],  # shirshu sniffing
    "companion_4": [(KICK, i * 0.15, 0.15, 40 - i, 100) for i in range(6)] + [(SUB, 0, 1.0, 28, 90)],  # badgermole digging
    "companion_5": [(BASS2, 0, 1.5, 33, 115), (SYN2, 0, 1.2, 57, 70), (SNARE, 0.1, 0.4, 48, 90)],  # dragon roar
    # interface
    "ui/click": [(HAT, 0, 0.05, 72, 80)],
    "ui/buy": arp(SYN2, [84, 88, 91], 0.06, 0.12, 75),
    "ui/sell": arp(SYN2, [91, 88, 84], 0.06, 0.12, 70),
    "ui/gold": [(SYN2, 0, 0.15, 96, 70)],
    "ui/level_up": arp(SYN1, [72, 76, 79, 84], 0.1, 0.2, 85),
    "ui/error": [(BASS1, 0, 0.15, 40, 90), (BASS1, 0.18, 0.2, 38, 90)],
    # pings
    "ping_0": [(SYN2, 0, 0.3, 84, 80)],
    "ping_1": arp(SYN1, [79, 79, 79], 0.15, 0.1, 95) + [(SNARE, 0, 0.2, 60, 80)],
    "ping_2": [(SYN2, 0, 0.4, 76, 80), (SYN2, 0.3, 0.4, 72, 70)],
    "ping_3": arp(SYN2, [72, 76, 79], 0.1, 0.15, 75),
    "ping_4": arp(SYN1, [76, 72, 76, 72], 0.1, 0.1, 85),
    "ping_5": [(SYN2, 0, 0.6, 67, 70), (HAT, 0.3, 0.1, 60, 60)],
    # hits per element + autoattack + deaths
    "hit_water": [(SYN2, 0, 0.2, 55, 90), (CLAP, 0, 0.15, 64, 70)],
    "hit_earth": [(KICK, 0, 0.3, 40, 115), (BASS1, 0, 0.2, 36, 90)],
    "hit_fire": [(SNARE, 0, 0.25, 62, 100), (BASS2, 0, 0.2, 48, 80)],
    "hit_air": [(HAT, 0, 0.2, 66, 90), (SYN1, 0, 0.15, 84, 60)],
    "hit_none": [(SNARE, 0, 0.15, 55, 100)],
    "hit_auto": [(SNARE, 0, 0.1, 64, 90), (KICK, 0, 0.1, 60, 70)],
    "death_champion": [(SUB, 0, 1.5, 36, 110), (BASS2, 0, 1.5, 41, 80), (SYN1, 0, 1.2, 48, 60)],
    "world/structure_down": [(KICK, i * 0.1, 0.3, 36 - 2 * i, 120) for i in range(6)] + [(SUB, 0, 2.0, 24, 120)],
}
# skill casts: one shape per pattern, the element moves the pitch / the instruments
EL = {"water": (SYN2, -5), "earth": (BASS1, -12), "fire": (SYN1, 0), "air": (SYN2, 7), "none": (BASS2, 3)}
SHAPES = {"bolt": [(0, 0.3, 67)], "wave": [(0, 0.8, 60), (0.1, 0.7, 64)], "burst": [(0, 0.5, 55), (0, 0.5, 62)],
          "field": [(0, 1.2, 52), (0.2, 1.0, 59)], "cone": [(0, 0.4, 64), (0.05, 0.35, 67)], "dash": [(0, 0.2, 72), (0.15, 0.2, 76)],
          "wall": [(0, 0.6, 43), (0.1, 0.5, 48)], "tornado": [(i * 0.1, 0.12, 70 + (i % 2) * 5) for i in range(8)]}
for shape, parts in SHAPES.items():
    for el, (ch, shift) in EL.items():
        drum = {"earth": [(KICK, 0, 0.3, 45, 100)], "fire": [(SNARE, 0, 0.2, 58, 80)], "air": [(HAT, 0, 0.3, 62, 70)]}.get(el, [])
        RECIPES[f"fx/{shape}_{el}"] = [(ch, p, l, k + shift, 85) for p, l, k in parts] + drum


def fl_running():
    return subprocess.run(["pgrep", "-x", "FL64.exe"], capture_output=True).returncode == 0


def render(path: Path):
    out = path.with_suffix(".wav")
    if out.exists():
        out.rename(out.with_suffix(".old.wav"))
    win = "Z:" + str(path).replace("/", "\\")
    proc = subprocess.Popen([str(FL_DIR / "fl-wine.sh"), "FL64.exe", "/R", "/Ewav", win], cwd=FL_DIR,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    last, stable, t0 = -1, 0, time.time()
    while time.time() - t0 < 120:
        time.sleep(1)
        size = out.stat().st_size if out.exists() else -1
        stable = stable + 1 if size > 0 and size == last else 0
        last = size
        if stable >= 3:
            break
    proc.kill()
    subprocess.run([str(WINESERVER), "-k"], env=dict(os.environ, WINEPREFIX=str(FL_DIR / "prefix")), capture_output=True)
    time.sleep(1)
    return out if out.exists() and out.stat().st_size > 1000 else None


def install(wav: Path, name: str):
    rate, x = wavfile.read(wav)
    x = x.astype(np.float32)
    if x.ndim > 1:
        x = x.mean(axis=1)
    peak = float(np.abs(x).max())
    if peak <= 0:
        print("  silent — skipped")
        return False
    x = x / peak
    loud = np.nonzero(np.abs(x) > 0.02)[0]
    x = x[max(0, loud[0] - 32): min(len(x), loud[-1] + int(0.08 * rate))]
    fade = min(len(x), int(0.03 * rate))  # no click at the end
    x[-fade:] *= np.linspace(1, 0, fade)
    if rate != 44_100:
        x = resample_poly(x, 44_100, rate).astype(np.float32)
    dst = SND / f"{name}.wav"
    dst.parent.mkdir(parents=True, exist_ok=True)
    wavfile.write(dst, 44_100, (np.clip(x * 0.8, -1, 1) * 32767).astype(np.int16))
    print(f"  → {dst.relative_to(ROOT)}  {len(x) / 44_100:.2f} s")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    if a.list:
        print("\n".join(RECIPES))
        return
    if fl_running():
        sys.exit("FL Studio is open — close it first (each render closes FL's Wine prefix).")
    WORK.mkdir(parents=True, exist_ok=True)
    hdr, ev = base_project()
    only = set(filter(None, a.only.split(",")))
    done = 0
    for name, notes in RECIPES.items():
        if only and name not in only:
            continue
        if fl_running():
            sys.exit("FL Studio was opened — stopping.")
        path = WORK / (name.replace("/", "__") + ".flp")
        flp.write(path, hdr, with_notes(hdr, ev, notes, tempo=120))
        print("render", name, flush=True)
        wav = render(path)
        if wav and install(wav, name):
            done += 1
        else:
            print("  FL did not make it — the old sound stays", flush=True)
    print(f"{done} sounds made with FL Studio", flush=True)


if __name__ == "__main__":
    main()
