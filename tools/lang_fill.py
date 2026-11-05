#!/usr/bin/env python3
"""[U-508] Fills lang/<code>.txt with everything the game can show that is not translated yet, drafted by a local model (Ollama).

    python3 tools/lang_fill.py [--list] [--model qwen3:14b]

Collects tr / trf / hint / refuse strings, the text tables and the content (skills, items, runes …: ./build/dump_content).
Answers are checked: same {} placeholders, same numbers, one line, not empty. It appends (resumable). Review samples by hand.
"""
import argparse
import json
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANG = ROOT / "lang" / "pl.txt"
GLOSSARY = """skill = umiejętność; cooldown = czas odnowienia; autoattack = autoatak; cast = rzucenie; recast = ponowne rzucenie; stun = ogłuszenie;
slow = spowolnienie; root = unieruchomienie; knock up = podrzucenie; knock back = odrzucenie; shield = tarcza; bending = magia żywiołów;
waterbender = mag wody; firebender = mag ognia; earthbender = mag ziemi; airbender = mag powietrza; fire fuel = paliwo ognia; fire rage = furia ognia;
damage = obrażenia; true damage = obrażenia nieuchronne; sweet spot = czuły punkt; range = zasięg; hitbox = obszar trafienia; item = przedmiot; shop = sklep;
level = poziom; rank = ranga; gold = złoto; minion = stwór; turret = wieża; jungle = dżungla; companion = towarzysz; respawn = odrodzenie; lane = aleja; mana = mana"""
NOT_TEXT = re.compile(r"^(?:[A-Za-z0-9_./\\:-]+\.(?:png|bmp|wav|txt|cfg|json)|[a-z_]+|[A-Z0-9_]{1,6}|[0-9 .,:%+-]*)$")


def strings_from_sources():
    out = set()
    pats = [r'\btrf?\("((?:[^"\\]|\\.)*)"', r'\bhint\("((?:[^"\\]|\\.)*)"', r'\brefuse\(u, "((?:[^"\\]|\\.)*)"', r'\brefuse\(u, std::format\("((?:[^"\\]|\\.)*)"',
            r'\bno\("((?:[^"\\]|\\.)*)"', r'return "((?:[^"\\]|\\.)*)";  ?//']
    files = [*ROOT.glob("src/client/**/*.cpp"), *ROOT.glob("src/client/**/*.hpp"), *ROOT.glob("src/game/**/*.cpp")]
    for f in files:
        t = f.read_text(errors="replace")
        for p in pats:
            out |= set(re.findall(p, t))
    # text tables: const char* lists shown through tr(), status / modifier tables, setup names
    for f, pat in ((ROOT / "src/client/modifiers.cpp", r'"((?:[^"\\]|\\.)*)"'), (ROOT / "src/game/loadout/loadout.hpp", r'"((?:[^"\\]|\\.)*)"')):
        out |= set(re.findall(pat, f.read_text(errors="replace")))
    for f in (ROOT / "src/game/world.hpp", ROOT / "src/client/screens/options.cpp", ROOT / "src/client/skill_text.cpp"):
        for block in re.findall(r"(?:const char\*|std::string_view) k\w+\[\w*\] = \{(.*?)\};", f.read_text(errors="replace"), re.S):
            out |= set(re.findall(r'"((?:[^"\\]|\\.)*)"', block))
    out |= set(re.findall(r'return "((?:[^"\\]|\\.)*)"', (ROOT / "src/client/skill_text.cpp").read_text(errors="replace")))
    return out


def strings_from_content():
    out = set()
    dump = subprocess.run([str(ROOT / "build/dump_content")], capture_output=True, text=True, check=True).stdout
    champs = set()
    for line in dump.splitlines():
        f = line.split("\t")
        if f[0] == "champion" and len(f) > 3:
            champs.add(f[3])
    for line in dump.splitlines():
        f = line.split("\t")
        if f[0] == "skill" and len(f) >= 5:
            out.add(f[3])
            if f[4]:
                out.add(f[4])
        elif f[0] in ("item", "rune", "mastery") and len(f) > 3:
            out.add(f[3])
    return out - champs


def existing():
    keys = set()
    if LANG.exists():
        for line in LANG.read_text(errors="replace").splitlines():
            m = re.match(r"((?:[^=\\]|\\=|\\)*)=", line)
            if m and not line.startswith("#"):
                keys.add(m.group(1).replace("\\=", "="))
    return keys


def ask(model, batch):
    prompt = ("Translate these English texts from a fantasy MOBA game (four elements, bending) to natural Polish, short like game interface text.\n"
              "Every text must really be translated, also fantasy names of skills and items (Dart Volley = Salwa strzałek); never return the English text.\n"
              "Rules: keep every {} or {:...} placeholder exactly and in the same order; keep every number and % as it is; keep champion names and key letters; "
              "no explanations. Answer ONLY a JSON array of strings, same length and order as the input.\nGlossary: " + GLOSSARY + "\n\nInput:\n" + json.dumps(batch, ensure_ascii=False))
    body = json.dumps({"model": model, "stream": False, "think": False, "options": {"temperature": 0.2, "num_ctx": 8192},
                       "messages": [{"role": "user", "content": prompt}]}).encode()
    req = urllib.request.Request("http://127.0.0.1:11434/api/chat", data=body, headers={"Content-Type": "application/json"})
    txt = json.load(urllib.request.urlopen(req, timeout=600))["message"]["content"]
    return json.loads(txt[txt.index("["):txt.rindex("]") + 1])


def good(en, pl):
    if not isinstance(pl, str) or not pl.strip() or "\n" in pl:
        return False
    if pl.strip() == en.strip() and len(en.split()) > 1:  # the model copied the input
        return False
    if re.findall(r"\{[^}]*\}", en) != re.findall(r"\{[^}]*\}", pl):
        return False
    return sorted(re.findall(r"\d+(?:\.\d+)?", re.sub(r"\{[^}]*\}", "", en))) == sorted(re.findall(r"\d+(?:[.,]\d+)?", re.sub(r"\{[^}]*\}", "", pl).replace(",", ".")))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--model", default="qwen3:14b")
    a = ap.parse_args()
    keep = lambda s: len(s) > 1 and re.search(r"[A-Za-z]", s) and not NOT_TEXT.match(s) and "\\" not in s.replace("\\n", "")
    todo = sorted(s for s in (strings_from_sources() | strings_from_content()) if keep(s) and s not in existing())
    print(len(todo), "to translate", flush=True)
    if a.list:
        print("\n".join(todo))
        return
    with LANG.open("a") as out:
        for i in range(0, len(todo), 6):
            batch = todo[i:i + 6]
            try:
                ans = ask(a.model, batch)
            except Exception as e:  # noqa: BLE001  (one bad answer must not stop the run)
                print("batch failed:", e, flush=True)
                continue
            n = 0
            if len(ans) == len(batch):
                for en, pl in zip(batch, ans):
                    if good(en, pl):
                        out.write(en.replace("=", "\\=") + "=" + pl.strip() + "\n")
                        n += 1
                    else:
                        print("  rejected:", en, "→", pl, flush=True)
                out.flush()
            print(f"{i + len(batch)}/{len(todo)}  ok {n}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
