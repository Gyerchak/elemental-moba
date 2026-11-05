#!/usr/bin/env python3
"""[U-508] Looks for skills whose numbers contradict their text or cannot work: python3 tools/skill_audit.py"""
import re, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
KINDS = ["None","Bolt","Wave","Burst","Field","Cone","Dash","Wall","Pillar","Tornado","Shield","Heal","Haste","Blink","Lantern","WindSense","Flight","SelfFreeze","Bloodbend","Boulder","Passive","WaterSpout","IcePath","Redirect","AvatarState","MagicStance","StonePush","Transform","MoveWater"]
CC = ["Stun","Root","Slow","Silence","Mute","Airborne","Frozen","Blind","Burn","Shield","Haste","Flying","Bloodbound","Channel","Swimming","WaterRide","Redirect","Wet","Grievous","Fear","Disarm","Subdue","Hex","Break","DebuffImmune","Polymorph","Invisible","None"]
rows = [l.split("\t") for l in subprocess.run([str(ROOT/"build/dump_content"),"--audit"],capture_output=True,text=True).stdout.splitlines()]
out = {}
def flag(w, r): out.setdefault(w, []).append(f"{r[0]} / {r[2]}: {r[-1]}")
for r in rows:
    owner,slot,name,kind,cc,cc_ms,cc_val,knock,pierce,ally,dur,casts,recast,rng,width,radius,speed,dmg,sweet = r[0],r[1],r[2],KINDS[int(r[3])],CC[min(int(r[4]),len(CC)-1)],*map(int,r[5:19])
    d = r[19].lower() if len(r) > 19 else ""
    if kind in("Bolt","Wave","Tornado","Dash") and speed <= 0: flag("moves but speed 0", r)
    if kind in("Bolt","Wave","Cone","Dash") and rng <= 0: flag("no range", r)
    if kind in("Wave","Cone") and width <= 0: flag("no width", r)
    if kind in("Burst","Field") and radius <= 0: flag("no radius", r)
    if kind == "Field" and dur <= 0: flag("field with no duration", r)
    if kind == "Shield" and dmg <= 0: flag("shield of 0", r)
    if kind == "Haste" and cc_val <= 0: flag("haste of 0", r)
    if casts > 1 and recast <= 0: flag("multicast with no window", r)
    if cc != "None" and cc_ms <= 0 and cc not in("Shield","Haste"): flag("crowd control with no time", r)
    if cc == "Slow" and cc_val <= 0: flag("slow of 0 %", r)
    if cc == "Burn" and cc_val <= 0: flag("burn of 0", r)
    for words, want in ((r"\bstun", "Stun"),(r"\broots?\b","Root"),(r"\bslows?\b","Slow"),(r"\bsilenc","Silence"),(r"\bblind","Blind"),(r"\bburns?\b","Burn"),(r"\bfreez|\bfrozen","Frozen"),(r"knocks? up|throws? .*up|\bairborne","Airborne")):
        if re.search(words, d) and cc != want and not (kind in("Passive",) or "burning ground" in d): flag(f"text says {want}, skill applies {cc}", r)
    if re.search(r"\bpush|knock.?back|throws? (enem|them)", d) and knock <= 0 and cc != "Airborne" and kind not in("StonePush","Passive"): flag("text says push, knock 0", r)
    if re.search(r"pierc|through enem|goes through", d) and not pierce and kind in("Bolt",): flag("text says pierce, pierce off", r)
    if re.search(r"\bally\b|allies", d) and not ally and kind in("Shield","Heal","Haste"): flag("text says ally, ally off", r)
    if kind == "Shield" and ally: flag("ALLY SHIELD (only the caster gets it)", r)
    if kind == "Haste" and ally: flag("ALLY HASTE (only the caster gets it)", r)
    m = re.search(r"(\d+) casts|up to (\d+) times|(\d+) (?:dashes|rounds|rolls|stones)", d)
    if m and casts != int(next(g for g in m.groups() if g)): flag("text says a number of casts, casts differ", r)
    m = re.search(r"\+(\d+) ?%.*(?:sweet|tip|centre|last 20)", d)
    if m and sweet != int(m.group(1)): flag("text says a sweet %, value differs", r)
print(len(rows), "skills")
for k in sorted(out, key=lambda k: -len(out[k])):
    print(f"\n== {k}: {len(out[k])}")
    for x in out[k][:9]: print("  ", x[:125])
