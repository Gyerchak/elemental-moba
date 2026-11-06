#!/usr/bin/env python3
"""[U-512] Proofreading of lang/pl.txt with the local model: fixes grammar (gender agreement, cases) of the machine draft.

    python3 tools/lang_proof.py

Lines after a "written by hand" comment are left alone; so is every answer that loses a placeholder, a number or is English again.
It rewrites the file only for lines it could check. Safe to stop: the file is written after every batch.
"""
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANG = ROOT / "lang" / "pl.txt"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from lang_fill import GLOSSARY, good  # noqa: E402


def ask(pairs):
    prompt = ("Below are pairs [English, Polish] from a fantasy MOBA game's interface. The Polish was machine translated and often has grammar "
              "mistakes: wrong gender agreement (\"Duży leczenie\" must be \"Duże leczenie\"), wrong case, words copied from English, awkward wording.\n"
              "Correct ONLY the Polish. Keep it short, natural game text. Keep every {} or {:...} placeholder exactly and in the same order; keep every number and %. "
              "If a Polish text is already right, return it unchanged. Answer ONLY a JSON array of the corrected Polish strings, same length and order.\n"
              "Glossary: " + GLOSSARY + "\n\nInput:\n" + json.dumps(pairs, ensure_ascii=False))
    body = json.dumps({"model": "qwen3:14b", "stream": False, "think": False, "options": {"temperature": 0.1, "num_ctx": 8192},
                       "messages": [{"role": "user", "content": prompt}]}).encode()
    req = urllib.request.Request("http://127.0.0.1:11434/api/chat", data=body, headers={"Content-Type": "application/json"})
    txt = json.load(urllib.request.urlopen(req, timeout=600))["message"]["content"]
    return json.loads(txt[txt.index("["):txt.rindex("]") + 1])


def main():
    lines = LANG.read_text(encoding="utf-8").split("\n")
    todo, hand = [], False
    for i, l in enumerate(lines):
        if l.startswith("#"):
            hand = "by hand" in l
            continue
        m = re.match(r"((?:[^=\\]|\\.)*)=(.*)", l)
        if not m or hand or len(m.group(2)) < 4 or " " not in m.group(2):  # single words: names and terms stay
            continue
        todo.append((i, m.group(1), m.group(2)))
    print(len(todo), "lines", flush=True)
    changed = 0
    for b in range(0, len(todo), 12):
        batch = todo[b:b + 12]
        try:
            ans = ask([[en.replace("\\=", "="), pl] for _, en, pl in batch])
        except Exception as e:  # noqa: BLE001
            print("batch failed:", e, flush=True)
            continue
        if len(ans) != len(batch):
            continue
        for (i, en, pl), new in zip(batch, ans):
            if isinstance(new, str) and new.strip() != pl and good(en.replace("\\=", "="), new.strip()) and "\n" not in new:
                lines[i] = en + "=" + new.strip()
                changed += 1
        LANG.write_text("\n".join(lines), encoding="utf-8")
        print(f"{b + len(batch)}/{len(todo)} changed {changed}", flush=True)


if __name__ == "__main__":
    main()
