# OPEN QUESTIONS — `[?-xx]`

Unclear or conflicting points in the plan. Each has the **assumption used now**.
Every assumption is a **Claude idea**, accepted by Hubert on 2026-10-23.
Change the assumption → update the linked `[U-xx]` in `SPEC.md`.

| ID | Where | Question | Assumption used now (Claude idea) |
|----|-------|----------|---------------------|
| [?-01] | U-04 / U-80 | "No shop" but there are items. | "No shop" = no paid/real-money store. An **in-game item shop** (gold) still exists. |NO SPECIAL SHOP I MEAN LIKE IN DOTA FOR EXAMPLE, NORMAL SHOP YES
| [?-02] | U-16 | Plan says "water can melt water by magic". | Typo → **water can melt ice** by magic. | IGNORE THAT INSTRUCTION THIS IS JUST SOME NONSENSE
| [?-03] | U-33 | When does the planetary alignment (6 min) start? What does it do? | **Answered:** starts at 60:00 (U-128), effect = U-126 (all benders +25 % power, +10 % size, X-67). | AT 60 GAME MIN , ALL BENDERS GET 25% POWER , 25% MORE WITH OF SKILLS 25% MORE DURATION OF SKILLS 25% MORE DURATION OF DPS SKILLS (THOSE TWO SHOULD BE ADDED TO THE MECHANIC SYSTEM MODIFIERS OF SKILLS) 25% SKILL RADIUS 25% SKILL SPEED 25% I MEAN JUST FOR MAGIC ONES, ALSO THERE SHOULD BE A SPECIAL ABILITIES FOR NON BENDERS TO GAIN THEM ACCES TO MAGICAL SKILLS TOO THEY CAN ENTER ELEMENTAL MAGIC STANCE
| [?-04] | U-43 | "S button behaviour on F and G". | In LoL, **S = stop**. Assumption: F/G special skills follow the same cast/cancel rules as Q-T, and **S cancels a channel/cast of F/G too**. JUST IGNORE THE THING ABOUT F AND G ,S SHOULD NOT CANCEL THE CHANNEL TOO Z SHOULD BE A BUTTON TO CANCEL CASTS|
| [?-05] | U-35 | Real solar eclipses happen at new moon; plan puts them at 25 and 35 min (moon is not new). | Keep plan timing. Physics ignored, it is a game. |GOOD SUGGESTION BUT NOW KEEP IT UNREALISTIC
| [?-06] | U-34 / U-35 | Comet at 25:00 = same moment as eclipse #1. Intended? | Yes, both together (25:00 = noon of game day 12). Effect: fire is at 0 % and muted at 25:00, then power climbs back/ while the comet buff is already on → 25:15 = 90 %, 25:30+ = 175 %. |THE COMET SHOULD START I REPEAT AT 30:00
| [?-07] | U-84 | "South and north poles" — the poles are **glacier zones** on top/bot lane? | Yes: a glacier circle (surface `glacier`) between the 1st-tier turrets. Top = north pole, bot = south pole. |YES
| [?-08] | U-85 | Sea between mid and top / mid and bot — deep water or shallow? | **Deep water** (rivers are the shallow ones). Not walkable for normal units. | YES DEEP WATER
| [?-09] | U-09 | 32 players — how many teams / lanes? | Still 2 teams (up to 16 v 16) on the same map. More teams = later. | SAME MAP JUST THIS IS ADJUSTABLE ABSOLUTE LIMIT
| [?-10] | U-61 | Fire fuel "breath / life / rage" — 3 variants or one bar? | One bar; champion picks its variant (breath = regen when not casting; life = pays HP; rage = gained by dealing/taking damage). | BREATH-LIFE SHOULD BE ONE IT SHOULD BE LIKE CALM FLAME COLOR BAR, THE RAGE BAR SHOULD BE COLOR OF HAOTIC FLAME 
| [?-11] | U-93 | Elevation −2…6 = 9 values; mapplan gives 8 bit in the old format, 4 bit in the new. | Use the **compressed format** (4 bit, value = level + 2). |YEAH BETTER WOULD BE THE SIMPLIFIED VERSION THAT IS 4 BIT LEVELS 
| [?-12] | U-92 | 11 surfaces, but "wood" is a surface and "tree" is a separate field. | `wood` = wooden floor/bridge surface. Trees = the separate tree field (bits 23–24). | YES THE BRIDGE SHOULD BE WOOD, NOTE THAT IT IS UNBENDABLE SURFACE FOR A LOT OF MAGES , THIS ALL SHOULD BE TAKEN IN ACCOUNT WHAT SURFACE EVERYTHING HAS , EARTH BENDERS CAN STILL BEND ON SHALLOW WATER , THERE SHOULD BE ALSO A STONE ROADS AND LIKE FOREST PATHS BUT OFC THEY ARE ALL EARTH ELEMENT BASED THINGS, GLACIER IS ICE BASED , FORM OF WATER SO NO EARTH BENDER CAN BEND AROUND THAT SURFACE 
| [?-13] | U-11 | "One skill is picked in champion select for basic ability Q" — from a list per champion? | Each champion has 2–3 Q options; pick one in champ select. | THERE IS NOT DEFINIED NUMBER BUT FOR NOW LETS HAVE 3 OPTIONS FOR EACH 
| [?-14] | U-87 | Rivers "coming there" — from where to where? | Rivers flow from the jungle springs into the bases (dryland), and into the sea. |FOR NOW ITS OK
| [?-15] | U-94 | Fog fields are 2 bits in ONE cell, but 2+ teams need separate visibility. | Cell fog = how much this cell **blocks** that sense (none/partial/full/brush). Per-team visibility = separate bitmaps [X-10]. |
| [?-16] | U-111 | No rank cap — what stops putting every point into the ultimate T? | **Answered by U-120** (T at 9, 18, 27…). Other skills: any time. |YEAH BUT EVERY CHAMPION CAN HAVE THIS THING ALSO MODIFIABLE SEPARATELLY AS FEATURE POSSIBLE TO CHANGE IN CODE
| [?-17] | U-109 | Do F/G special skills count in the "close to 0 targeted" limit? | Yes — whole kit P…G, max 1 targeted [X-43]. | YES
| [?-18] | U-119 | What is the "standard game time" for level 18? | **35:00** (after eclipse #2, before the alignment). Change one number in `progression.hpp`. | 25-66MIN can be less or more depending on lot of thingss
| [?-19] | U-117 | "Light vision fog" — a new 2-bit field in the cell, or a separate layer? | **Separate runtime layer** (light changes every moment; the 32-bit cell is full). What blocks light = the cell's vision fog. A unit sees a target if `distance ≤ sight × max(night %, light %)`. | YEAH KINDA I JUST MEAN THE LIGHT FROM FIRE SKILLS BURNING GROUND OR LATERNS OR ANYTHING THAT PRODUCES IT
| [?-20] | U-120 / U-119 | (a) After 27 — does T keep ranking every 9 levels (36, 45…)? (b) Do F/G take skill points too? (c) When is level 27? | (a) yes, every 9 levels; (b) yes; (c) **answered by U-121:** level 27 at 60:00 (XP income grows over time, X-64). | LET THEM BE AS THEM ARE NOW, NO LEVELING UP
| [?-21] | U-122 | Faster XP during full moon / comet — for everyone, or only the empowered benders? | Only the empowered benders: waterbenders at full moon (+25 % XP), firebenders during the comet (+40 % XP). | ONLY THE EMPOWERED BENDERS THIS IS GOOD IDEA FOR THE XP
| [?-22] | U-124 | Does the eclipse drop power to 0 % for **all** benders, or only firebenders? | **Only firebenders** (Day of Black Sun). Change = one line in `sky_effects.hpp`. Eclipse = 1:00 real (24:30–25:30, 34:30–35:30), middle 25:00 / 35:00, mute = middle ± 3 s. | YES ONLY FOR ALL FIREBENDERS NERFS
| [?-23] | U-130 | How exact is the "simplified body shape"? | 3–5 circles per body type (X-69), moving with the walk animation. Real models later replace the circle lists. | THEY ARE OK NOW
| [?-24] | U-132 | "1 pixel" — the simulation has no pixels. | 1 pixel = 1 game unit; touching counts (X-71). | YES I MEAN GAME UNIT
| [?-25] | U-142 / U-143 / U-76 / U-79 | Are seismic / heat sense passive element traits or the special skills from §11? | They are the **special skills** (F/G) from U-76 / U-79, working always-on while picked. Wind Sense = the air "sense skill" (U-77), an active. | AIR SENSE SKILL SHOULD BE ALSO A PASSIVE LIKE OTHER SENSES
| [?-26] | U-142 / U-143 | Does seismic show standing units? Does heat? | Seismic: only moving ground units (Rek'Sai-like). Heat: every warm unit, moving or not. | IT IS GOOD AS IT IS NOW
| [?-27] | U-137 | What does sandbending do? | A sand blast cone that damages and blinds (sight −70 % / −50 %), X-75. | DIFFERS FOR EACH SKILL BUT GOOD IDEAS, GENERALLY IT CAN BE JUST EARTH BENT IN OTHER WAYS
| [?-28] | U-134 | Which element pairs combine? | All 6 pairs (X-72). The combo happens when a moving skill touches an allied zone of another element. | YES
| [?-29] | U-10 | LoL draft = picks + bans + timers. | Built: picks with unique champions (U-08), teams, ready, host starts. Not yet: bans and pick timer (X-29). |YES
| [?-30] | U-103 | Who starts the match on a dedicated server (no host player)? | Any player in the lobby can press Start; bots fill to 5 v 5. | IT STARTS AFTER TIME LOBBY IS DISBANDEED IF SOMEONE ISNT PICKED, SEVERS SHOULD HAVE ITS ELO CALCULATION SYSTEM AFTER EVERY MATCCH AND THERE SHOULD BE SKILL BASED MATCH MAKING, WITH LP ETC ON SERVERS BY DEFAULT ON, AND THERE SHOULD BE PLAYER RANKING ON SERVER AVIABLE, THERE SHOULD BE OPTION FOR SERVERS TO NOT MAKE/USE MMR FOR MATCHMAKING
| [?-31] | U-240 | "Cosmic event timing is a little out of sync." | Checked against the plan, all match: comet 25:00, eclipses 24:30–25:30 and 34:30–35:30 (noon 25:00 / 35:00), full moon 30:00–32:00, alignment 60:00–66:00. What changed in round 2 is the **light**: the day is now smooth (full light only 08:30–15:30), so "day" may look later / shorter than before. Tell me which event looked off. | THIS IDEA LOOKS AIGHT
| [?-32] | U-234 | "There are light / vision bugs." | Found and fixed one: clouds (steam, smoke, ash) were cell edits that could last as long as a burned tree's 2-minute regrow. If you see another, tell me where (bush, night light, turret, after a wall…). | THERE IS BUGGED LIGHT AT NIGHT IN RANDOM PLACES, NO CORRECTLY WORKING LIGHT FROM FIRE 
| [?-33] | U-210 | Custom champions on servers. | Work in practice / on the same machine now. On a server every player must have the same files enabled (no download yet) — the server will later send the definitions. |YES SEND IT FOR ANYONE TO GAME FILES WHILE JOINING TO LOBBY AS CUSTOM CHAMPIONS FOLDER
| [?-34] | U-214 | "Vulkan, not OpenGL." | The renderer is SDL3's **Vulkan** backend (OpenGL only if a machine has no Vulkan). A real 3D renderer (SDL_GPU on Vulkan) comes with real models. | YES YOU SHOULD MAKE REAL MODELS SO THEY COULD BE PLACEHOLDERS
| [?-35] | U-198 / U-226 | Real 3D models + animations. | The slots, file layout and placeholder cues exist; the default pack is generated. Real models need artwork / a model format — planned with the 3D renderer. | YES NEEDS THIS AS THE WORKING PLACEHOLDER

THERE SHOULD BE ANOTHER CHARACTER MAGE TYPES WHICH IS AVATAR, THOSE SHOULD BE REALLY UNIQUE POWERFULL AND HARD CHARACTERS WITH THE LIMITATIONS I ONCE MENTIONED FOR PICKING ETC


## Round 5 (2026-10-25) — what was done with Hubert's answers above

| ID | Done |
|----|------|
| ?-01 | No Dota-like side / secret shop; the normal shop stays (U-261 build paths added). |
| ?-02 | Ignored. |
| ?-03 | Alignment at 60:00: +25 % power, width, radius, speed, duration, DoT duration for magic skills (U-264); non-bender magic stances (X-138). |
| ?-04 | S no longer cancels; **Z** cancels casts / channels (U-265). |
| ?-06 | Comet at **30:00** (U-266). |
| ?-10 | Calm flame (breath + life) and rage bars (U-268, X-135). |
| ?-12 | Surfaces + earthbending rules, roads, paths, bridges (U-269, X-136). |
| ?-13 | 3 Q options per champion (U-270). |
| ?-16 | Ultimate step per champion (`ult_step`, U-271). |
| ?-25 | Wind Sense is a passive (U-274). |
| ?-30 | Pick timer, disband, MMR / LP / ranking / matchmaking (U-276, X-139). |
| ?-32 | Light bugs fixed (X-140) — tell me if you still see dark / bright spots. |
| ?-33 | The server sends its custom champions on joining (U-277). |
| ?-34 / ?-35 | Blender placeholder models → sprites + .glb (U-258). |

| [?-36] | U-276 | Should a player who leaves the lobby during the pick timer also lose LP (like a LoL dodge)? | No LP loss now; only sent away from the lobby. |
| [?-37] | U-279 | The dark Avatar: a separate champion (same unique group) — which elements / theme? | Not made yet; the rule for it exists (`unique_group`). |
| [?-38] | U-258 | Sprites come from simple primitive models. Should models be made in more detail (Blender sculpting by hand is not possible here)? | Primitive models + ComfyUI texture projection. |
| [?-39] | U-289 | Which key is the ultimate now — still **T**, or should the ultimate move to **Y** (the last skill key, like R in LoL)? | T stays the ultimate (all rank rules unchanged); Y is a new normal skill. |
| [?-40] | U-289 | "Shop not on P" — which key? | **O** (changeable in Options → Controls). |
| [?-41] | U-288 | Bending enemy / own walls: dividing (holes), destroying, using as a source are done. **Moving** a wall (shifting it as a whole) is not yet. | Planned. |
| [?-42] | U-290 | Which small graphic glitches exactly (where, when)? | The fog edge stepping 10× a second was smoothed; other glitches need a description. |
| ?-39 / ?-40 | U-291 | Answered: the ultimate is **Y**; the shop is on **I**. | Done (2026-10-25). |
| [?-43] | U-361 | Ice Path already freezes deep AND shallow water (radius 160, 6 s). Where did it fail? | Assumed it works; tell me the place / champion if not. |
| [?-44] | U-367 | "Unless they are spirits": read as spirits PASS through units by default. | Spirits pass; change `spirit` in monsters.hpp. |
| [?-45] | U-364 | Which epic monsters? | Claude picks: Lion Turtle (stacking power), Sun Dragons (gold); Face Stealer stays. Names / numbers editable in monsters.hpp. |
| [?-46] | U-356 | "Lava shouldn't create stones as much" — how much? | 12 % of lava cells become a rock (kLavaRockPct). |
