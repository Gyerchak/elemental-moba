#!/usr/bin/env python3
"""[U-259][U-263] Placeholder sounds and announcer voices for the default resource pack.

    python3 tools/art/make_sounds.py

Voices: English with Piper TTS (piper-tts + the "ryan" voice), Polish with espeak-ng (robotic, a placeholder).
Every voice gets a little echo so it sounds like an arena announcer.
Effects: synthesised with numpy (noise + tones + envelopes) — one sound per skill shape and element:
    sounds/fx/<shape>_<element>.wav    shape: bolt wave burst field cone tornado lantern auto towershot boulder heal steam dash
    sounds/ui/<name>.wav               click, buy, sell, level_up, gold, error
    sounds/world/<name>.wav            structure_down, recall, footstep, splash
    sounds/<lang>/announce/<id>.wav    the announcer (see ANNOUNCE)
Real sounds replace these files later (same names).
"""
import os
import sys
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SND = ROOT / "resourcepacks" / "default" / "sounds"
RATE = 22050
PIPER_MODEL = Path.home() / ".local/share/piper-voices/en/en_US/ryan/high/en_US-ryan-high.onnx"

ANNOUNCE = {  # id: (English, Polish)
    "first_blood": ("First blood!", "Pierwsza krew!"),
    "enemy_slain": ("An enemy has been slain.", "Wróg został zabity."),
    "ally_slain": ("An ally has been slain.", "Sojusznik został zabity."),
    "you_slain": ("You have slain an enemy.", "Zabiłeś wroga."),
    "you_died": ("You have been slain.", "Zginąłeś."),
    "double_kill": ("Double kill!", "Podwójne zabójstwo!"),
    "triple_kill": ("Triple kill!", "Potrójne zabójstwo!"),
    "quadra_kill": ("Quadra kill!", "Poczwórne zabójstwo!"),
    "penta_kill": ("Penta kill!", "Pięciokrotne zabójstwo!"),
    "shutdown": ("Shutdown!", "Seria przerwana!"),
    "killing_spree": ("Killing spree!", "Seria zabójstw!"),
    "rampage": ("Rampage!", "Szał!"),
    "unstoppable": ("Unstoppable!", "Nie do zatrzymania!"),
    "dominating": ("Dominating!", "Dominacja!"),
    "godlike": ("Godlike!", "Boski!"),
    "legendary": ("Legendary!", "Legendarny!"),
    "ace": ("Ace!", "As!"),
    "executed": ("Executed.", "Stracony."),
    "turret_destroyed_theirs": ("Your team has destroyed a turret.", "Twoja drużyna zniszczyła wieżę."),
    "turret_destroyed_ours": ("Your turret has been destroyed.", "Twoja wieża została zniszczona."),
    "inhibitor_destroyed_theirs": ("Your team has destroyed an inhibitor.", "Twoja drużyna zniszczyła inhibitor."),
    "inhibitor_destroyed_ours": ("Your inhibitor has been destroyed.", "Twój inhibitor został zniszczony."),
    "shrine_destroyed": ("A Nation Shrine has fallen!", "Świątynia narodu upadła!"),
    "objective_taken_ours": ("Your team has slain the Face Stealer.", "Twoja drużyna pokonała Złodzieja Twarzy."),
    "objective_taken_theirs": ("The enemy team has slain the Face Stealer.", "Wrogowie pokonali Złodzieja Twarzy."),
    "eclipse_begins": ("The sun goes dark. Firebenders lose their power!", "Słońce gaśnie. Magowie ognia tracą moc!"),
    "comet_arrives": ("The comet has arrived. Fire burns brighter!", "Kometa nadleciała. Ogień płonie jaśniej!"),
    "full_moon_rises": ("The full moon rises. Waterbenders grow stronger.", "Wschodzi pełnia. Magowie wody rosną w siłę."),
    "alignment_begins": ("The planets align. Every bender surges with power!", "Planety w jednej linii. Wszyscy magowie zyskują moc!"),
    "welcome": ("Welcome to the battle of the four nations.", "Witaj w bitwie czterech narodów."),
    "minions_spawned": ("Minions have spawned.", "Stwory wyruszyły."),
    "victory": ("Victory!", "Zwycięstwo!"),
    "defeat": ("Defeat.", "Porażka."),
}


def save(path, s):
    path.parent.mkdir(parents=True, exist_ok=True)
    s = np.clip(s, -1, 1)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((s * 30000).astype("<i2").tobytes())


def load(path):
    with wave.open(str(path)) as w:
        rate, n = w.getframerate(), w.getnframes()
        data = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float32) / 32768
        if w.getnchannels() == 2:
            data = data[::2]
    if rate != RATE:  # simple resample
        t = np.linspace(0, len(data) - 1, int(len(data) * RATE / rate))
        data = np.interp(t, np.arange(len(data)), data)
    return data


def announcer(s):
    """normalise + a short arena echo"""
    s = s / max(1e-3, np.abs(s).max()) * 0.85
    out = np.concatenate([s, np.zeros(int(0.35 * RATE))])
    for delay, g in ((0.07, 0.35), (0.16, 0.2), (0.27, 0.1)):
        d = int(delay * RATE)
        out[d:d + len(s)] += s * g
    return out / max(1e-3, np.abs(out).max()) * 0.9


def voices():
    tmp = Path(tempfile.mkdtemp())
    for vid, (en, pl) in ANNOUNCE.items():
        f = tmp / "en.wav"
        subprocess.run(["piper-tts", "--model", str(PIPER_MODEL), "--output_file", str(f)], input=en.encode(), check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        save(SND / "en" / "announce" / f"{vid}.wav", announcer(load(f)))
        g = tmp / "pl.wav"
        subprocess.run(["espeak-ng", "-v", "pl", "-s", "150", "-p", "35", "-w", str(g), pl], check=True)
        save(SND / "pl" / "announce" / f"{vid}.wav", announcer(load(g)))
        print("voice", vid, flush=True)


# ---------------------------------------------------------------- effects
rng = np.random.default_rng(7)


def env(n, attack=0.01, decay=6.0):
    t = np.arange(n) / RATE
    return np.minimum(1, t / max(attack, 1e-4)) * np.exp(-t * decay)


def noise(n, smooth=1):
    x = rng.standard_normal(n)
    if smooth > 1:  # low-pass by moving average
        x = np.convolve(x, np.ones(smooth) / smooth, mode="same") * np.sqrt(smooth)
    return x


def tone(n, f0, f1, kind="sine"):
    f = np.linspace(f0, f1, n)
    ph = 2 * np.pi * np.cumsum(f) / RATE
    return np.sin(ph) if kind == "sine" else np.sign(np.sin(ph)) * 0.5


SHAPE = {  # (seconds, attack, decay, pitch from→to factor)
    "bolt": (0.35, 0.005, 7, (1.6, 0.8)), "wave": (0.6, 0.05, 4, (0.8, 1.2)), "burst": (0.7, 0.002, 5, (1.0, 0.4)),
    "field": (0.9, 0.1, 2.5, (1.0, 1.0)), "cone": (0.5, 0.01, 5, (1.2, 0.7)), "tornado": (1.0, 0.2, 2, (0.7, 1.4)),
    "lantern": (0.6, 0.05, 3, (2.0, 2.4)), "auto": (0.12, 0.002, 25, (1.4, 0.9)), "towershot": (0.3, 0.003, 9, (0.9, 0.5)),
    "boulder": (0.6, 0.003, 4, (0.5, 0.3)), "heal": (0.8, 0.05, 3, (2.0, 3.0)), "steam": (0.8, 0.1, 3, (1.0, 1.0)),
    "dash": (0.4, 0.01, 6, (0.8, 1.6)),
}


# ---- mid-quality synthesis (v2): filtered noise layers, pitch sweeps, transients, a small room echo
from scipy import signal as sg


def bp(x, lo, hi):  # band-pass (Butterworth, 2nd order)
    b, a = sg.butter(2, [max(20, lo) / (RATE / 2), min(hi, RATE / 2 - 100) / (RATE / 2)], btype="band")
    return sg.lfilter(b, a, x)


def lp(x, hi):
    b, a = sg.butter(2, min(hi, RATE / 2 - 100) / (RATE / 2))
    return sg.lfilter(b, a, x)


def hp(x, lo):
    b, a = sg.butter(2, lo / (RATE / 2), btype="high")
    return sg.lfilter(b, a, x)


def sweep_bp(x, f0, f1, q=3.0, steps=24):
    """band-pass whose centre glides f0 → f1 (blocks, crossfaded) — the 'whoosh'"""
    out = np.zeros_like(x)
    edges = np.linspace(0, len(x), steps + 1).astype(int)
    for k in range(steps):
        f = f0 * (f1 / f0) ** (k / max(1, steps - 1))
        seg = bp(x, f / (1 + 1 / q), f * (1 + 1 / q))
        out[edges[k]:edges[k + 1]] = seg[edges[k]:edges[k + 1]]
    return out


_IR = None


def room(x, mix=0.18):
    """a short room echo: convolution with 0.25 s of decaying noise"""
    global _IR
    if _IR is None:
        n = int(0.25 * RATE)
        _IR = rng.standard_normal(n) * np.exp(-np.arange(n) / (0.06 * RATE))
        _IR /= np.abs(_IR).sum() / 6
    wet = sg.fftconvolve(x, _IR)[: len(x) + len(_IR) // 2]
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    return dry * (1 - mix) + wet * mix


def finish(x, gain=0.8):
    x = np.tanh(x / max(1e-4, np.abs(x).max()) * 1.6)  # soft clip = a little warmth, no harsh peaks
    return x / max(1e-4, np.abs(x).max()) * gain


def crackle(n, rate_hz, seed):
    r = np.random.default_rng(seed)
    x = np.zeros(n)
    for t in r.integers(0, n, int(rate_hz * n / RATE)):
        k = min(n - t, 60)
        x[t:t + k] += r.standard_normal(k) * np.exp(-np.arange(k) / 8) * r.uniform(0.4, 1.0)
    return hp(x, 1500)


def element_layer(el, n, f0, f1, seed):
    t = np.arange(n) / RATE
    z = np.random.default_rng(seed).standard_normal(n)
    if el == "Water":  # splash (band noise) + bubbles (short sine chirps)
        x = bp(z, 400, 3000) * 0.6
        r = np.random.default_rng(seed + 1)
        for st in r.integers(0, max(1, n - 800), 6 + n // 3000):
            k = min(800, n - st)
            fb = r.uniform(500, 1400)
            ph = 2 * np.pi * np.cumsum(np.linspace(fb, fb * 1.8, k)) / RATE
            x[st:st + k] += np.sin(ph) * np.exp(-np.arange(k) / 180) * 0.5
        return x
    if el == "Fire":  # roar (low band noise, wobbling) + crackles
        roar = lp(z, 900) * (0.75 + 0.25 * np.sin(2 * np.pi * 7 * t))
        return roar + crackle(n, 120, seed) * 0.8
    if el == "Earth":  # thump (falling sine) + gravel (band noise bursts)
        ph = 2 * np.pi * np.cumsum(np.linspace(f0 * 0.6, f1 * 0.35 + 30, n)) / RATE
        thump = np.sin(ph) * np.exp(-t * 7) * 1.4
        gravel = bp(z, 200, 1800) * (np.random.default_rng(seed).random(n) < 0.35) * 0.7
        return thump + lp(gravel, 2500)
    if el == "Air":  # whoosh: band-pass sweeping up then down
        return sweep_bp(z, max(300, f0), max(400, f1 * 1.5), q=2.5) * 1.3
    if el == "Lightning":  # crack (bright burst) + buzz (square-ish 90 Hz)
        crack = hp(z, 2500) * np.exp(-t * 40) * 1.5
        buzz = np.sign(np.sin(2 * np.pi * 90 * t)) * 0.25 * np.exp(-t * 5)
        return crack + bp(buzz, 80, 2000)
    # non-bender: steel swish (narrow band sweep) + a small ring
    swish = sweep_bp(z, 1200, 4000, q=4) * 0.8
    ring = np.sin(2 * np.pi * 2600 * t) * np.exp(-t * 18) * 0.25
    return swish + ring


ELEMENT = {"Water": (6, 420, 0.55), "Earth": (14, 110, 0.7), "Fire": (3, 260, 0.8), "Air": (2, 700, 0.75),
           "Non-bender": (4, 220, 0.4), "Lightning": (2, 300, 0.8)}


def fx(shape, element):
    secs, att, dec, (p0, p1) = SHAPE[shape]
    base = ELEMENT[element][1]
    n = int(secs * RATE)
    seed = abs(hash((shape, element))) % 2**31
    x = element_layer(element, n, base * p0, base * p1, seed)
    if shape == "heal":  # soft chime on top
        t = np.arange(n) / RATE
        x = x * 0.3 + sum(np.sin(2 * np.pi * f * t) * np.exp(-t * 3) for f in (880, 1320, 1760)) * 0.4
    if shape in ("burst", "boulder", "towershot"):  # an impact at the start
        k = min(n, 900)
        x[:k] += lp(rng.standard_normal(k), 1200) * np.exp(-np.arange(k) / 150) * 1.5
    return finish(room(x * env(n, att, dec)), 0.75)


def effects():
    for shape in SHAPE:
        for el in ELEMENT:
            save(SND / "fx" / f"{shape}_{el}.wav", fx(shape, el))
    # hits (short impacts per element), autoattack hit, deaths — same recipes, short and punchy
    for el in ELEMENT:
        n = int(0.22 * RATE)
        x = element_layer(el, n, 300, 150, 99) * env(n, 0.002, 18)
        x[: min(n, 600)] += lp(rng.standard_normal(min(n, 600)), 1500) * np.exp(-np.arange(min(n, 600)) / 90)
        save(SND / f"hit_{el}.wav", finish(room(x, 0.1), 0.7))
    n = int(0.12 * RATE)
    save(SND / "hit_auto.wav", finish(lp(rng.standard_normal(n), 1800) * env(n, 0.001, 35), 0.6))
    n = int(0.15 * RATE)
    save(SND / "auto_ranged.wav", finish(sweep_bp(rng.standard_normal(n), 2500, 900, q=3) * env(n, 0.003, 20), 0.55))
    n = int(0.5 * RATE)
    save(SND / "death.wav", finish(room(lp(rng.standard_normal(n), 700) * env(n, 0.01, 8)), 0.6))
    n = int(1.2 * RATE)
    t = np.arange(n) / RATE
    save(SND / "death_champion.wav", finish(room(np.sin(2 * np.pi * np.cumsum(np.linspace(220, 70, n)) / RATE) * np.exp(-t * 2.5)
                                                 + lp(rng.standard_normal(n), 500) * np.exp(-t * 4) * 0.5, 0.3), 0.7))
    n = lambda s: int(s * RATE)
    save(SND / "ui" / "click.wav", tone(n(0.05), 1500, 1100) * env(n(0.05), 0.001, 60) * 0.5)
    coin = sum(tone(n(0.35), f, f) * env(n(0.35), 0.002, 9) for f in (1800, 2400, 3200)) / 3
    save(SND / "ui" / "buy.wav", coin)
    save(SND / "ui" / "sell.wav", coin[::-1].copy() * 0.8)
    save(SND / "ui" / "gold.wav", tone(n(0.18), 2200, 2600) * env(n(0.18), 0.002, 18) * 0.5)
    lvl = np.concatenate([tone(n(0.12), f, f) * env(n(0.12), 0.005, 8) for f in (523, 659, 784, 1046)])
    save(SND / "ui" / "level_up.wav", lvl * 0.6)
    save(SND / "ui" / "error.wav", tone(n(0.18), 220, 180, "square") * env(n(0.18), 0.005, 12) * 0.4)
    crash = noise(n(1.6), 10) * env(n(1.6), 0.002, 2.2) + tone(n(1.6), 70, 35) * env(n(1.6), 0.01, 2) * 0.8
    save(SND / "world" / "structure_down.wav", crash * 0.8)
    save(SND / "world" / "recall.wav", (tone(n(2.0), 300, 600) * 0.5 + noise(n(2.0), 3) * 0.2) * env(n(2.0), 0.3, 0.8) * 0.5)
    save(SND / "world" / "footstep.wav", noise(n(0.08), 12) * env(n(0.08), 0.002, 40) * 0.5)
    save(SND / "world" / "splash.wav", noise(n(0.4), 5) * env(n(0.4), 0.005, 8) * 0.6)
    print("effects done", flush=True)


if __name__ == "__main__":
    effects()
    if "--effects-only" not in sys.argv:  # the voices take a while (piper-tts)
        voices()
