# SPEC — elemental-moba (user plan)

Source: `zrodla/plan.txt`, `zrodla/MENU.txt`, `zrodla/map/mapplan.txt`.
Only Hubert's ideas are here, tagged `[U-xx]`. Wording is cleaned up, meaning is kept.
Where meaning was unclear → see `OPEN-QUESTIONS.md` (`[?-xx]` link in the line).
Additions outside the plan live in `ENGINE-EXTRAS.md`, never here.

---

## 1. Technical rules

- [U-01] Only C++23 code.
- [U-02] Fully optimised, lowest latency, most stable.
- [U-03] Simple map and models — everything is a placeholder, only for visualisation. Hubert gives adjustment instructions later.
- [U-04] Full open-source philosophy. No shop (no paid store) [?-01]. Anyone can privately host their own game.

## 2. Theme

- [U-05] Avatar: Legend of Aang / Korra / Seven Heavens inspired MOBA about elemental magic.
- [U-06] Default teams: **Team A = Fire Nation**, **Team B = Earth Nation**.
- [U-07] One Avatar per team max. One Dark Avatar per team max.
- [U-08] A unique character (e.g. Korra) can exist only once per game — either normal or dark version.

## 3. Match modes

- [U-09] Basic mode 5v5 (10 players). Default limit up to 32 players in one match.
- [U-10] Draft like League of Legends (pick/ban).
- [U-11] One skill is picked in champion select as the basic ability Q.

## 4. Elemental interactions

| ID | Rule |
|----|------|
| [U-12] | Earth-side minions clear obstacles out of the way while passing. |
| [U-13] | Fire minions carry explosives to clear the way. Minions cannot be blocked for more than 3 s on their path. |
| [U-14] | Fire burns trees and bushes. They regrow; bushes regrow fast. |
| [U-15] | Fire melts ice. |
| [U-16] | Water magic can melt ice [?-02]. |
| [U-17] | Water counters a fire attack: 1 skill blocks 1 skill. |
| [U-18] | Ice wall = weaker, faster-disappearing, more fragile elevated wall. |
| [U-19] | Water cannot stop air / stone attacks. |
| [U-20] | Earth stops air and fire attacks, and partly water. |
| [U-21] | Water can attack from above (from the up side). |
| [U-22] | Air / fire can attack through earth walls if elevated. |
| [U-23] | Some waterbenders can bend trees. |
| [U-24] | Earth walls have a collapse time. |
| [U-25] | Fire creates light at night → higher vision for **everyone**. |
| [U-26] | Waterbending on ice = unlimited resource. On water = very fast water collection. |
| [U-27] | Heavy stone throws: phase 1 raise ground, phase 2 move the stone. Two earthbenders can push it without the first raise. Stone can move in multiple directions. |
| [U-28] | Changing the state of water (water→ice, water→steam…) nerfs every aspect of the ability by 20 % (radius, range, damage…). |
| [U-29] | Melee mages can still use ranged abilities on a special aspect, but they are weak. |

## 5. Time and sky (exact numbers in `docs/time-and-sky.md`)

- [U-30] 1 real minute = 12 game hours (full day/night = 2 min).
- [U-31] Game starts at night, at new moon.
- [U-32] Moon cycle = 30 days (= 60 min). Full moon at 30 min.
- [U-33] Cosmic one-time event: all planets aligned, lasts 3 days (6 min). Starts at 60:00 (U-128).
- [U-34] One-time comet event at 25 min.
- [U-35] Two solar eclipses, at about 25 min and 35 min, only during day time.

## 6. Combat mechanics — League-like

- [U-36] CC types: stun, slow, burn.
- [U-37] Ability kinds: projectile, non-projectile, skillshot, first-person hit (point-and-click), multi-hit.
- [U-38] Projectile blocking.
- [U-39] Stats: movement speed, attack (base + bonus), elemental power (base + bonus; replaces AP), attack speed, per-level stats, melee/ranged.
- [U-40] **Elemental lethality** = flat magic pen. **Elemental penetration** = % magic pen. **No physical lethality.**
- [U-41] Autoattacks on right click.
- [U-42] Cast time.
- [U-43] "S button behaviour on F and G" [?-04].

## 7. Combat mechanics — new

- [U-44] Wall strength. Map walls = 6× typical ability strength.
- [U-45] Elevation change time.
- [U-46] Magic block.
- [U-47] Water magic nerf.
- [U-48] Unstoppable through stone walls / through ice walls.
- [U-49] Water elevation (only on deep water). Ice elevation (only on glacier or deep water).
- [U-50] Underground for earthbenders (short time); underground with movement (short time).
- [U-51] Fire flying, air flying.
- [U-52] Icy surface. Ground attack.
- [U-53] Cast time adjustable by stats. Moving while casting / no moving while casting. Movement speed changed while casting.
- [U-54] Skill speed. Skill radius (width + length, or radius). For radius-only skills, width and length stats count at 1/3 value.
- [U-55] Elemental autoattack speed = mainly attack speed + a little elemental haste.
- [U-56] Elemental autoattack damage = mainly attack + a little elemental power.
- [U-57] Elemental haste and non-elemental haste (two separate stats).
- [U-58] Melee have more effective armor / elemental defence.
- [U-59] New CC / status: cooling, freezing, freezing damage, burning, plant root, ice root, earth root, metal root.

## 8. Resources

| ID | Who | Resource |
|----|-----|----------|
| [U-60] | Waterbenders | **Water** — like mana, but regeneration depends on environment. Has a *water collection range*. |
| [U-61] | Firebenders | **Fuel** — breath / life fuel / rage fuel. |
| [U-62] | Earthbenders | No resource, but slow casting and long cooldowns. |
| [U-63] | Airbenders | No cost, low cooldowns, fast. |
| [U-64] | Non-benders | Ammo / energy. |

## 9. Bending styles and sub-elements

- [U-65] Styles: Water Tribe, Earth, Fire, Air, non-benders.
- [U-66] Water: water, ice, wood (plants), blood, mud, evaporated water.
- [U-67] Air: air, evaporated water.
- [U-68] Earth: earth, lava, metal, earth armouring, metal armouring, mud.
- [U-69] Fire: fire, blue fire, lightning, combustion.

## 10. Controls

- [U-70] Q W E R = abilities, **T = ultimate**.
- [U-71] F, G = special skills (pickable, see §11).
- [U-72] Space = switch ability/attack set (magic / non-magic, usually non-benders); for other creatures: other stance or no stance.
- [U-73] Right click = autoattack / move.
- [U-74] Turn rate (Dota-like but faster: not instant like LoL, not slow like Dota).
- [U-75] Vision based on terrain.

## 11. Special skills (F / G pickable)

- [U-76] **Earth:** metal bending, magma bending, water-style / air-style skill, seismic sense, stone armour, iron armour (only if metal bending is taken first).
- [U-77] **Air:** fly over terrain, fire-style, chi blocking (short duration, low CD, like energy bending), sense skills (fog), increased sound sense.
- [U-78] **Water:** bloodbending one person during full moon (CC like Malzahar R), healing water (self or ally), earth-style skill, nature bending, energy bending (longer CD, longer duration; chi blocking; no-magic; changes water state instead of using environment).
- [U-79] **Fire:** lightning bending, lightning redirection, water-style, air-style, lighting up the area, heat sense.

## 12. Items

- [U-80] Items for mages and non-mages; mages can build both efficiently.
- [U-81] Unique item **Spiritual Water Flask**: drink → 60 s intense flat HP healing + immortality. If you would die: healing stops, 3 s "dead", then revive with 5 × 5 % max-HP heals (1 per second). Cooldown 6 min. Stats: health, elemental haste, elemental power.

## 13. Jungle

- [U-82] Jungle monsters: spirits / evil spirits and (mixed magical) animals.

## 14. Default map (details in `docs/map-layout.md`)

- [U-83] Symmetric. No elevation at all on the default map.
- [U-84] Top lane and bot lane: between the 1st-tier turrets there is a pole (north pole on top, south pole on bot).
- [U-85] Sea between mid and top, and between mid and bot.
- [U-86] Mid = land bridge between Fire Nation and Earth Nation. Tropical climate.
- [U-87] Bases: drylands, deserts, stone lands. Rivers flow there.
- [U-88] All rivers are shallow water (not deep water).
- [U-89] Jungle full of bushes, water sources and trees.

## 15. Map data (`zrodla/map/mapplan.txt`)

- [U-90] Map 16 384 game units; grid cell = 32 units → grid 512 × 512.
- [U-91] Max 32 bits per cell.
- [U-92] Surfaces: land, dryland, glacier, water, sand, stone, wood, deep water, magma, metal, platinum.
- [U-93] Elevation levels −2 … 6. 1 level difference = walkable; 2 levels = not walkable.
- [U-94] Compressed cell layout (implemented in `src/core/cell.hpp`):

| Bits  | Field | Bits |
|-------|-------|------|
| 0–3   | surface enum | 4 |
| 4–7   | elevation (level + 2) | 4 |
| 8–11  | temp set elevation (level + 2, 15 = none) | 4 |
| 12–13 | vision fog | 2 |
| 14–15 | sound fog | 2 |
| 16–17 | skill fog | 2 |
| 18–19 | walkable | 2 |
| 20–21 | ice / stone / temp wall | 2 |
| 22    | temp walkable | 1 |
| 23–24 | tree / burned / destroyed | 2 |
| 25–26 | seismic fog | 2 |
| 27–28 | heat fog | 2 |
| 29–31 | reserved | 3 |

## 16. Menu (`zrodla/MENU.txt`)

- [U-95] **Version**: current official or own custom snapshot (for hosting only; when joining, auto-fallback to the server's version).
- [U-96] Per selected version: rune & mastery sets, build sets, own skin sets, own special-ability picks per character type, (room for more).
- [U-97] Resource packs: original / own custom textures. Character resource packs (skins).
- [U-98] Character skin display settings: show other players' own skins (default ON); force own/selected skins over server ones (default OFF, group by own / enemy / allies / nicknames, per skin per champion); use only server-native skins (default OFF); option per server (pick by number and name).
- [U-99] Settings: controls, sound, colourblind, HUD, minimap, chat, voice chat, shown nicknames.
- [U-100] Single player (practice tool).
- [U-101] Multiplayer (join server or match lobby).
- [U-102] Host match: server tick rate (default **128 Hz**); force server-only textures (default OFF); allow duplicate nicknames (default OFF).
- [U-103] Host server: tick rate (default 128 Hz); force server-only textures (default OFF); unique nicknames with registration + login (default OFF).

## 17. Added in chat (2026-10-23)

- [U-104] **P = passive** slot for every champion.
- [U-105] **Every skill can have its own passive** (Q, W, E, R, T, F, G).
- [U-106] **Multicasts / recasts** like Taliyah (LoL): several casts of one skill in a window.
- [U-107] Sky events (comet, eclipses, full/new moon, alignment, day/night) give **special effects, buffs and nerfs to different elemental mages**. Concept accepted; concrete effects = X-50…X-53 (numbers are placeholders).
- [U-108] **Moon phase changes vision range at night** (new moon = darkest, full moon = brightest).

## 18. Scaling and targeting (added in chat 2026-10-23)

- [U-109] **Targeted (point-and-click) skills close to 0.** Almost everything is a skillshot / area / projectile.
- [U-110] Any ability can scale with: **elemental power**, **ability rank**, **attack**, **bonus attack**, **champion level**, **stacks** (if the skill has stacks). **Non-mages: always scale with attack / bonus attack.**
- [U-111] **No champion level cap. No ability rank cap.**
- [U-112] Any **size or speed** (range, width, length, radius, projectile speed, cast time, cooldown…) can grow with ability rank or champion level.
- [U-113] **Items** that increase skill sizes / speeds / range.
- [U-114] **Effects** (buffs) that increase casting (cast speed etc.).

## 19. Light, night vision and XP (added in chat 2026-10-23)

- [U-115] **New moon night: vision close to 0.**
- [U-116] **Fire skills give off light** → higher vision for **everyone** (both teams). Extends U-25.
- [U-117] **Light vision** = a new kind of vision. Light works like a layer over the vision fog: a lit area can be seen from far away even at night [?-19].
- [U-118] Firebenders have a skill that **makes light at night** (links U-79 "lighting up the area").
- [U-119] **XP:** level 18 is expected at the standard game time [?-18]. Every level needs more XP than the one before, so the longer the game, the slower the levels (like normal MOBAs). No cap (U-111).
- [U-120] **Ultimate T unlocks / ranks up at levels 9, 18 and 27.** Level 27 is the LoL "level 16" moment (3rd ult rank). Kits have more skills to learn (P, Q, W, E, R, T, F, G), so T comes slower [?-20].
- [U-121] **60:00 = late game.** Game phases can be a bit longer than in LoL. Level 27 (3rd ult rank) is the late-game moment at about 60:00.

## 20. Sky event power and CC (added in chat 2026-10-23)

- [U-122] During the **full moon** and the **comet**, their benders level up **faster** (more XP) [?-21].
- [U-123] Full moon (waterbenders) and comet (firebenders) empower their benders **hugely**, **skill sizes too**. **The comet is stronger** than the full moon.
- [U-124] **Solar eclipse:** elemental power drops **linearly from 100 % to 0 %** at the middle of the eclipse, then **linearly back to 100 %** at its end [?-22].
- [U-125] In the middle of the eclipse **all firebending abilities and autoattacks are muted for 6 s**.
- [U-126] **Planetary alignment:** every skill is stronger for **every elemental mage**.
- [U-127] **Tenacity** (shorter CC on you) and **CC duration increase** effects (longer CC you apply).
- [U-128] **Planetary alignment starts at 60:00** (late game), lasts 6 min → 60:00–66:00.

## 21. Hitboxes, senses, combos (notes 2026-10-23, `zrodla/notes-2026-10-23-hitboxes-senses.txt`)

- [U-129] **Model size** is a statistic. It sets the **hitbox size** (bigger model = bigger hitbox). Items and effects can change it, like the other sizes (U-113, U-114).
- [U-130] **Champion / mob hitbox = simplified shape of the model's core body**, not a circle between the legs. It reaches the farthest edges of the core body (shoulders, belly, butt, tail…) and **moves with the movement animation**.
- [U-131] **Skill hitboxes have fluid shapes**, especially water and fire (not the hard geometric shapes of LoL). The skill indicator / visual still shows the **exact edges** of the skill.
- [U-132] **One hit mode only, on edges:** if even **1 pixel** of a skill hitbox touches the hitbox of a character / mob, it is hit.
- [U-133] **Autoattack range is edge to edge:** from the attacker's hitbox edge to the **closest edge** of the target (room for mechanics: turning, leaning in, model size).
- [U-134] **Combinable skills:** some skill types can be combined with selected elements → **elemental combos**.
- [U-135] **Group bending:** several benders can bend one thing together — e.g. a **tornado that grows** when more airbenders feed it; the same for every element.
- [U-136] **Defensive items against a specific element.**
- [U-137] **Airbenders** can pick the special skill **sandbending**: weaker than the earthbender version, but a lower cooldown.
- [U-138] During the **comet** firebenders have **unlimited fuel**.
- [U-139] **Sun height:** during the day firebenders get slightly more fuel regeneration; it **peaks at noon**.
- [U-140] **Bushes:** you are hidden only when **every pixel of your hitbox** is inside the bush. One pixel outside reveals you.
- [U-141] **Seeing skills:** skills follow normal vision (typical fog of war). Outside vision only an **airbender with a special skill** can detect them, and then only **how they move, as waves** — not what they are.
- [U-142] **Seismic sense (earth):** no direct vision; it shows **waves** inside the seismic sense range, **shared with the whole team**, like Rek'Sai's tremor sense. It **cannot sense flying** things. Other people's earthbending shows as a **different wave** ("the terrain changed under the fog").
- [U-143] **Heat sense (fire):** works the same way, showing **heat sources as waves going outward**. A **frozen / self-frozen** unit has no heat → invisible to it.

## 22. Collision, jungle, controls, eyes and ears (added in chat 2026-10-24)

- [U-144] **Structure hitbox:** towers, inhibitors, Nation Shrines work like a **wall that cannot be modified**; usually they **cannot be bent** by earth / metal bending (exceptions come later).
- [U-145] **Unit hitbox** for every minion, monster and champion — its own collision layer; some champions, monsters or items can have **unit collision enabled**.
- [U-146] **Collision types:** tree, wall, temporary wall, elevation level, deep water… Checked on the **borders of the body**, not its middle: no part of the body may be inside a tree / wall / anything that restricts movement. (Nothing that already exists is changed.)
- [U-147] **Aggressive jungle monsters:** cursed spirits, angry animals, bandits.
- [U-148] **Attack-move:** **A** = attack-move that targets only the **closest champion** (no other entity); **D** = attack-move on anything.
- [U-149] The **shop has a range**.
- [U-150] **Vision depends on the eyes** and is realistic.
- [U-151] What is **outside vision but inside hearing (sound range)** shows as **sound waves** coming out of the fog.
- [U-152] **Bushes:** to **see inside** a bush, the entity's **eyes** must be inside it; to be **invisible** there, its **full hitbox** must be covered (U-140).

## 23. Test round 2: HUD, controls, day light, network (added in chat 2026-10-24)

- [U-153] **Day and night change smoothly**, like real life: early / late night is not as dark, and full visibility comes before and stays after the middle of the day.
- [U-154] Mob **health must not reset randomly** (monsters keep fighting while you fight them).
- [U-155] **Shields** like LoL: an extra part of the health bar, visible for everyone, showing **what kind** of shield it is.
- [U-156] **Minions are less aggressive towards players**, with LoL-like limiting factors.
- [U-157] **More item slots.**
- [U-158] **Autoattacks on / off** in the settings and with a key (default **H**).
- [U-159] Killing minions and champions gives **gold**; option (default ON) to **show the gains** as small transparent text where it died: "+10xp +15g".
- [U-160] **Blind:** the blinded entity has **no vision** (relies on its other senses) and **cannot use targeted abilities or attacks**.
- [U-161] **Tooltips / info** for structures, champions and mobs (hover and left click). On enemies: cooldowns and damages, **no cooldown timers**; on allies: timers too.
- [U-162] **X** (default) = **target champions only** (like LoL): for autoattacks and targeted abilities.
- [U-163] **Quick cast per ability** in the options + **quick cast all** (LoL).
- [U-164] **Left + right click together = camera on yourself**; changeable in the settings.
- [U-165] **Key bindings** can be changed.
- [U-166] **Health bars** like LoL: clean, with **marks every 100 HP and 1 000 HP**; **resource bars visible**.
- [U-167] **Buff / debuff icons** (all ongoing effects) with the **seconds left** and a **duration animation**.
- [U-168] **Skill tooltips:** hover = the most important numbers (damage, cast time, channel time, cast speed…, only what the skill has); hover + **Alt** = the formulas (base + rank + elemental power + level scaling…); hovering the **+** = what the next rank improves.
- [U-169] HUD **keys shown only on their icons** (shop P, recall B, portal V, scoreboard Tab), no loose help text; the icons can be clicked; **channelling bars** (recall and every other) are small, above the abilities.
- [U-170] **Health and resource bars under the skills**; champion **stats on the left** with the **champion icon**, name next to it.
- [U-171] Every champion has **its own icon**; portraits on the side (LoL-like) with **health** and **ultimate availability**; clicking one centres the camera on them.
- [U-172] **File structure** easy to modify: folders for champions, mobs, mechanics, interactions — the base everything else builds on.
- [U-173] The **ping must not grow** — find and fix the cause.
- [U-174] **Every team sees itself bottom-left** (own base bottom-left on the map and minimap, the enemy top-right) — no different feeling between sides.
- [U-175] The **game view is tilted** like LoL (for skins and 3D models), not straight from above; the top-down view stays as a **debug / design option**, chosen in the practice tool before launching (and in the settings).
- [U-176] The **hitbox** is shown in the game view only as a **grey shadow**, opacity adjustable (**default 10 %**); in **debug mode** it stays fully visible (as before). Picked in the practice tool before launching.
- [U-149] (extended) the shop range is a visible **territory** at the base near the spawn.

## 24. Test round 3: loadouts, champion select, resource packs, water, chats, platforms (added in chat 2026-10-24)

**Loadouts (runes, specialisations, keystones)**
- [U-177] **Runes, specialisation trees and keystones** (from the planned menu, U-96) — three systems that work differently, each with **sets** to pick from.
- [U-178] In champion select: pick a **full saved setup** (runes + specialisations + keystone) in one click, then change any subset or detail. Changes are **saved only in the pre-game setup editor**. Nothing picked = play with nothing. By default the **last full setup used on that champion** is loaded. The same for Q and F/G picks; if those were never set, the **first options** are taken, so there is always something to play with.
- [U-179] After a game: a **Continue** button → a post-game lobby with **statistics**, incl. the **impact of the chosen keystone**.

**Champion select and lobby**
- [U-180] Champion select like **LoL**: champion **icons**, sort / filter by **class**, by **element**, **non-mages**, **search by name**, all **alphabetical**; **right click** = add to **favourites**. The same screen in **practice mode** (to tweak runes and masteries there too).
- [U-181] A **ban phase** like LoL (also in lobbies) — **turned off** in the draft modes for now.
- [U-182] Chats: **champion-select chat**, **lobby chat**, **server chat**; in game **team** and **all** chat; **friends**: add friends on a server and chat with people from the friend list.

**Champion data layout**
- [U-183] **Q options and special skills live in their own place** (skill pools, next to the skill and rune data). Champions only say **what kind of bender** they are → they get that element's set.
- [U-184] Champion skills declare in code whether they **turn the caster** while casting, whether they **pass through walls / objects** (most do not), and whether they **aim at the cursor** (some mechanics follow where the champion looks, Cassiopeia-like — an option, not for the current champions).
- [U-185] **Lightning redirection:** a champion with this skill, well timed, sends an incoming lightning bolt towards **where their cursor is**.
- [U-186] **Autoattacks and spells** can each be a **projectile or not** (per champion / skill).
- [U-187] All champions show **every formula and changeable number** at least as clearly as the LoL wiki (even more detailed, like the code).

**Water**
- [U-188] **Deep water:** without a water skill you can only **swim** through it — **slow**, **no spells**. **Freezing** deep water makes it walkable.
- [U-189] **Waterbenders** have mobility on water: **turn water into ice** near them to stand on it, or **elevate themselves on water** (like Korra) and move with it while **casting freely**, incl. special skills.
- [U-190] **Swimming** makes you **more vulnerable**: stronger freezing / icing CC.
- [U-191] **Flying / elevated** units (air flight, water elevation…) **cannot be hit by melee attacks, melee abilities or ground abilities** — clean, clear rules.

**Combat display**
- [U-192] **Damage numbers** like LoL.
- [U-193] **CC timers above characters** (how long the CC still lasts), like LoL.

**Minions, jungle, XP**
- [U-194] Minion behaviour like **LoL 2016**, with **cleanly configurable modifiers**.
- [U-195] **XP range adjustable**; minion XP range **1 000**; monster XP range = minion XP range. Jungle XP is shared by everyone who **attacked it in the last 3 s** and is in XP range; the **killer** needs no range and **counts as N people** (N = number of sharers), every helper counts as 1: 2 people → 2/3 + 1/3, 3 people → 3/5 + 1/5 + 1/5 … Easy to change in one place in the mob code.
- [U-196] **Minions must not get stuck on structures** (turrets, temporary walls): go around them on the **shortest path** back to their destination, no wandering.

**Look, models, sound**
- [U-197] **Resource packs**: folders for **skin packs**, **model packs**, **sound packs**; a **base texture folder** for every champion (no skin). **Sounds and music** are part of packs; the **default pack** has music.
- [U-198] **Placeholder 3D models, animations for spells and everything, placeholder textures**; a **developer mode** shows the current look (no textures).
- [U-199] **Loading screen** with the skins' **splash arts**.
- [U-200] **Skins like Minecraft:** before the game the client sends its skin to the server, the server sends it to every player who allows other players' skins.
- [U-201] **Every sound has its own volume**; **skill sounds have the same range as hearing** (U-151) — sound is a core mechanic tied to the visual sound waves.
- [U-202] **Trees, bushes, walls and other raised things** have simple models.
- [U-203] **Paths** (mine and my allies') shown on the map and minimap.
- [U-204] Non-developer view: **cleaner fog of war** with **smooth edges**.

**Interface and window**
- [U-205] Hovering a minion / creature shows **nothing**; the info window (after a click) is at the **top left**, not next to the unit.
- [U-206] The **time window** goes **top right**; the **minimap bottom right**.
- [U-207] **True fullscreen**; the mouse **cannot leave the window** (default) — an option for windowed / two screens; Esc in game opens **live game options** without leaving the game.
- [U-208] **Much more detailed options**.
- [U-209] **Item tooltips** calculate the **impact** of every non-stat effect (e.g. "−25 % fire damage: X damage prevented"), like some LoL items.

**Custom content, multiplayer, platforms, code**
- [U-210] **Own champion lists** (menu button): picked custom champions join the practice roster; servers / private lobbies may allow them; they must match the base game version — with **any** error they are **ignored**.
- [U-211] **Servers send only core numbers and results**; everything visual (animations, skill display) runs on the client.
- [U-212] **Hosting** a match or server as easy as **Minecraft**; the **multiplayer window** works like Minecraft's (server list, add, direct connect).
- [U-213] Versions for **Windows, Linux and macOS** (own launchers, host and play together) from **one code base**; servers can **restrict systems** (default: all allowed).
- [U-214] Graphics on **Vulkan** (not OpenGL), best C++23 game-dev libraries, tuned.
- [U-215] Code **readable and well organised**: clear names, clear comments, logical order; then hunt bugs, wrong code and slow code; **better ping**.

**Added while building round 3**
- [U-216] **Hitbox height:** some abilities can hit units **in the air** (flying, knocked up) or on **raised terrain**; some abilities can be **aimed higher**. A mechanic only — nothing set up yet.
- [U-217] **Autoattack mechanics:** champions, abilities or items may let you **autoattack while moving**; autoattacks (right clicks) may have **AoE / skill-like hitboxes / unique mechanics** (like Aphelios, Twitch ult). A mechanic only — nothing set up yet.
- [U-218] **Slow resistance**: one for **ice** slows and one for **non-ice** slows.
- [U-219] Clear **stacking rules** for every stat system (defences, haste, attack speed: base + base growth + bonus, move speed: flat + bonus %, items with % stats…) and a good way to **show them** in the menus.
- [U-220] The **item shop** as well designed as LoL's, with all the **sorting**; in the **main menu**: browse all items and **create own builds**. **No recommended items.**
- [U-221] **No critical strikes** at all. Bigger hits get **flashier** numbers — but they are not crits.
- [U-222] **Sweet spots** of skills (a better hit, not a crit) show in **another colour**.
- [U-223] **Physical and elemental damage** text has different colours, and **every element** has its own colour.

## 25. Test round 4: flight, environment, animations, vision, locale, stacking (added in chat 2026-10-24)

**Movement in the air**
- [U-224] Flying with fire is **Jet Propulsion**; the casual air lift gets its **own name**; a **rare** ability **Flight** lets you fly freely over all levels and spaces, with **height control** (aimed like abilities). All rare — kept as possible mechanics.
- [U-249] Every ability and autoattack type says whether it can be used **in the air**: lifted by **yourself**, by an **ally**, by an **enemy**. By default **melee can't be used in the air**, unless you and the target are close enough and on the **same height** (air / water elevation).

**Bending the environment**
- [U-225] **Water and earth benders** can bend the environment and even **temporarily change the base map** (rocks, icebergs). An **airbender** with environment bending can bend created **steam or sandstorms**. A **firebender** can draw the **heat out of lava** made by a lavabender, stopping its ground effects.
- [U-227] Burned trees **regrow after 1 game day** by default. **Lavabending** burns trees too. **Every ability** has a "destroys trees" option: usually fire and magma, sometimes earth, very rarely air (very powerful skills), powerful water skills or plant bending.
- [U-241] **Interactions:** water skills **cool magma** → a **steam cloud** that blocks vision for **both sides** like a wall but can be walked through; **burning trees** leave **smoke** for a while (same rules, airbenders can bend it); **air on a fire area** makes the fire reach further and stronger (attacks there deal the air damage **and +25 % fire**; some passives can push this higher); **fire meeting fire** (ally or enemy, cast at the same time) **clash and cancel** at the middle — the stronger one (more elemental power) gets further and squeezes the weaker one; **lightning +25 %** vs **WET** units (wet from swimming or being hit by water); waterbenders lose wet very fast (**1 s**, changeable), others after **1 game hour**; any **fire hit removes wet** (ally or enemy), and so does waterbending (a water mage casting right away can pull the water out of themselves or allies on the skill's path); **lightning +25 %** vs **iron-armoured** units (adds to the wet bonus).
- [U-247] **Snow bending** for some water mages; air mages that bend the environment can bend snow too; snow gives a **weaker blind**.

**Light and vision**
- [U-228] Keep the **Sky Lantern**; add a **passive that carries fire around yourself**. Fire / light vision should be **more realistic and sensible**.
- [U-244] Firebenders can **carry fire in their hands** (much stronger light). **Any non-firebender** can carry a **lantern** as an extra skill at night: more vision but reveals you (smaller than a fire mage's). **Every fire skill** gives light while casting; abilities have a **"burns the ground"** option: fire hitting the ground leaves it burning for a few seconds.
- [U-234] **Vision ranges adjustable** for every entity and structure in code; fix the light / vision bugs.
- [U-235] Vision through **falcons**: sent to watch a place, they **come back** and show **delayed** information on the map as **ghosts** of what happened there (grey champions walking, grey skills, the enemy champion icon moving with a **grey** border; allies have **green** borders).
- [U-237] The **comet** makes all light **reddish**; the **moon** gives **whiter** light. Comet / full moon / eclipse modifiers come **first** in the order (environment buffs / nerfs), before ability modifiers. **Every modifier** shows **under the bars** and in the selection panel, like LoL.

**Animation and sound**
- [U-226] **Every ability, movement and interaction** can have its own **animation** and **voice / sound / effect**, split into resource packs, skin packs and base-skin packs. Slots: swimming, death, recall, autoattacks, slow / fast movement, flight (self / lifted by ally / lifted by enemy), bloodbent, stunned, electrocuted, special skills, being pushed (generic, with direction: stone / air / ice / water), slows (different forms), blinded (hands over the eyes). Generic Q bender abilities share the same animations for everyone.
- [U-246] **Pings** like LoL (same information) **+ the bait ping**; an **emote wheel**; **Ctrl+1…6** = laugh, taunt, dance, joke… with their own animations and sounds per pack.

**Interface and controls**
- [U-231] **Space** = centre on my champion; **F1…F12** = my teammates in order; **Ctrl+Space** = switch stance.
- [U-232] Lines showing what minions focus: **developer mode only**.
- [U-233] The app must act like a **real game**: no peeking out, no desktop shortcuts while playing.
- [U-236] **Icons in the shop**; **champion icons on the map** at sensible sizes.
- [U-238] Ability tooltips sit **right above the modifier row**; the selection panel shows modifiers under its main part, the items and the stats; **C** expands all statistics (mine and the selected unit's), default = basic stats like LoL; buttons to view **runes etc.** and their tracked values (hover = what each part gives).
- [U-243] **Alt + ability / item key = self-cast.**
- [U-245] Items say whether they can be bought **several times**, **once**, or **one per category**. The shop resembles **LoL's** closely: sorting, **icons**, the tooltip with stats only after **clicking** an item, the **price at the bottom**.

**Rules and code**
- [U-229] **Teleportation** possible in code; for now **no dash teleports** (at most very fast dashes).
- [U-230] LoL and Dota mechanics from their wikis kept in code as **unused** mechanics flagged **LoL_** and **Dota_** (with their interactions), so games / mechanics are easy to mix.
- [U-239] **Languages:** English default, **Polish** too; a structure easy to replace; option to change **only the text** or **text + voice**.
- [U-240] The **cosmic event timing** seems a little out of sync with the original plan — check.
- [U-242] **Stacking rules** for everything: additive, multiplicative, multiplicative %, diminishing returns; defences like LoL; every effect may stack or not, refresh or not, prefer the newest or the first source, stack per source type (e.g. the same item) — all listable.
- [U-248] **Penetration order exactly as LoL** (flat reduction → % reduction → % penetration → flat penetration).

## 26. Test round 4 feedback + answered open questions (2026-10-25)

**Bugs reported**
- [U-250] Occasional **graphical glitches**; **a lot of text hides other text** (overlaps).
- [U-251] **Space does not work at all.** Space = the same as clicking both mouse buttons (centre); **holding** Space keeps the camera centred.
- [U-252] **Pathfinding often gets stuck**: it plans for the unit's centre only, not its edges. Must hold for everything that uses pathfinding.
- [U-253] The **sky power** (cosmic events) does not show clearly enough what it does.
- [U-254] **DPS from passives / timed effects** (burns, fields, damage over time) is **not shown at all**.
- [U-255] The **modifiers still do not show** (under the bars / in the panel).
- [U-256] Night light bugged in **random places**; **fire light does not work correctly** [?-32].

**Art, sound, models**
- [U-257] Generate **placeholder art with local ComfyUI**, many prompts, in the background: icons and textures for everything (champions, skills, items, surfaces, screens).
- [U-258] **Simple 3D models made in Blender** with those textures, as working placeholders [?-34][?-35].
- [U-259] **Placeholder sounds** for everything; **voices** generated with local tools (poor quality is fine).
- [U-260] **Burning / freezing looks on the model**: flames on a burning unit, iced skin on a frozen / chilled one.

**Items**
- [U-261] **More placeholder items** and **build paths like LoL**: bigger items are built from smaller ones; the price = components + a recipe cost.
- [?-01 answer] **No special shop like Dota's** (side / secret shop). The normal shop stays.

**Death recap, announcements**
- [U-262] **Death recap like LoL**: after death a window (expandable) shows **who dealt how much damage, with what** (skill, autoattack, item, passive, effect) and **what type** (physical / element), with graphs. Damage is remembered for the **needed combat time** (the window before the death).
- [U-263] **Announcements**: kills, **multikills** (double … penta), first blood, shutdowns, **turrets destroyed**, **objectives taken** — with a **window / feed** for them and announcer **voice**.

**Answers to open questions → new rules**
- [U-264] [?-03] **Planetary alignment at 60:00**: every bender's **magic** skills get +25 % power, +25 % width, +25 % duration, +25 % DoT duration, +25 % radius, +25 % speed (skill duration and DoT duration become skill modifiers in the system). **Non-benders** get special abilities that let them enter an **elemental magic stance** (access to magic skills).
- [U-265] [?-04] **S does not cancel channels.** **Z = cancel the cast / channel.**
- [U-266] [?-06] The **comet starts at 30:00.**
- [U-267] [?-09] Team size up to 16 v 16 is an adjustable **absolute limit**, same map.
- [U-268] [?-10] Fire fuel: **breath and life are ONE bar** (calm-flame colour); **rage** is a separate bar (chaotic-flame colour).
- [U-269] [?-12] **Every surface matters for bending**: wood (bridges) cannot be bent by most benders; **earthbenders can bend on shallow water**; **stone roads** and **forest paths** exist (earth-based); **glacier is ice** (water) — no earthbending on it.
- [U-270] [?-13] **3 Q options per element** for now (no fixed number later).
- [U-271] [?-16] The ultimate rank rule (T at 9, 18, 27…) is **per champion**, changeable in code.
- [U-272] [?-18] Standard time for level 18 is **between 25 and 66 min**, depends on the game.
- [U-273] [?-20] Leave the levelling as it is now.
- [U-274] [?-25] **Wind Sense is a passive** like the other senses.
- [U-275] [?-27] Sandbending differs per skill — generally it is **earth bent in other ways**.
- [U-276] [?-30] On a dedicated server the match **starts after a lobby timer**; if somebody has not picked, the lobby is **disbanded**. Servers keep **Elo / MMR** after every match, **skill-based matchmaking** with **LP** (on by default), a **player ranking**; servers may turn MMR / matchmaking off.
- [U-277] [?-33] **Custom champions are sent** by the server to every player who joins the lobby (saved into their custom champions folder).
- [U-278] [?-34][?-35] **Make real models** as working placeholders.
- [U-279] A new mage type: **Avatar** — really unique, powerful and hard champions (all elements), with the picking limits from [U-07] (max one Avatar per team).
- [U-280] Find bugs, **optimise**, improve **ping**.

## 27. Round 5 feedback (2026-10-25)

- [U-281] Bug: the practice game did not open (a crash at the match start). Fixed (bot shopping recursion).
- [U-282] Champion select: with the Avatar the picks did not fit. **Make it never happen** (rows wrap, the panel scrolls, nothing leaves the screen).
- [U-283] Champion select filters: **Avatars** and **Dark Avatars** (they still belong to their native element's filter).
- [U-284] No notices on the main menu about missing own versions.
- [U-286] **Wet vs fire:** the first hit from a fire source on a wet unit deals **25 % less**, then the wet status disappears. A fire skill from an **ally** deals no damage (as always) but still removes wet.
- [U-285] Test content: one own champion with its own pictures + model, one own skin (pictures + model) for an existing champion, one own game version (copy of the official one with small balance changes).

- [U-287] A **huge glitch with the ping jumping** was seen once → make the engine work better and flawless (optimisation).
- [U-288] **Terrain changes by time** (trees regrowing, walls falling, lava cooling…) update on **everyone's map at once**, seen or not. **Player-made walls have HP**; every element is more or less effective against each wall kind (own formulas, also for ice); **items / modifiers** add damage to all walls or to one kind; **metal walls**: the strongest and longest lasting; every wall can be **bent by other mages** (own or enemy): divided, moved, destroyed, or used as an **elemental source** (Avatar / Korra / Seven Heavens logic).
- [U-289] **One more skill: Q W E R T Y.** Keys that were on Y move one place right (camera lock → U); **specials F/G → G/H**; what was on H moves right (autoattack → J). Shop not on P. **Holding `** shows the autoattack range circle like LoL, **Ctrl + `** switches it on / off. **X** is a switch by default.
- [U-290] Small graphic glitches still seen.

- [U-291] **Shop on I** (like "item"). **Y is the ultimate**: learned at level 6, ranks at 12, 18 and every 6 levels. Other skills: max rank 2 before level 6, 3 before 12, 4 before 18, … freely. These are the **defaults**; every part can be changed per skill and per champion. A champion may have **0, 1 or 2 ultimates** (ultimate is a property of a skill, not of a key). Skills may have **their own passives** (like P) and **several cast stages** (the system exists, few use it yet). Tooltips and the ally info show whether a skill is an ultimate, its passive, stages and rank rule (like LoL, more details where useful). Replaces the T rule of [U-120] / [U-271].

- [U-292] **Heat Draw on steam:** a firebender's heat-drawing skill turns a steam cloud into **water on the ground**. Units inside the cloud at that moment become **wet (normal time)** and nothing else. Walking on that water — or on **shallow water** or any water on the ground (not swimming) — makes a unit **wet only while it stands there**.

- [U-293] **Freezing through steam:** a waterbender's ice skill passing through a steam cloud condenses the steam **around it** into water on the ground (those there get wet, like with Heat Draw); the rest of the cloud stays. Waterbenders choose whether water taken from a **source** becomes **water or ice** directly.
- [U-294] **Waterbenders carry only a little water:** they cannot create **ice walls** without a big water source nearby — they excel near water sources and on the poles.

- [U-295] Better sounds (FL Studio is installed; mid quality, efficient).

- [U-296] **Non-benders awaken at the planetary alignment (60:00):** a predefined element per champion, else random from the damage taken from champions (÷ 4, the rest: 0 air, 1 water, 2 earth, 3 fire). They get the element's basic skill set (one per element); predisposed non-benders may have their own set.

## 28. Round 6 (2026-10-25) — bugs, systems, architecture, content

**Bugs / fixes**
- [U-297] R cannot be levelled past rank… at level 6 (rank rules must allow it as intended).
- [U-298] Leaving deep water must be simple: the path goes out of the water normally (no exact clicking on the shore). Pathing in general is often bad / blocking; paths also weigh the **travel time** (deep water is much slower).
- [U-299] Minions walk into each other without logic; minions must **not regenerate** health by themselves.
- [U-300] Kill credit: once a champion damaged a champion, a mob / turret cannot take the credit (like LoL, not Dota 2).
- [U-301] If a skill cannot be ranked, do not rank another one instead — only a "cannot level up" notice.
- [U-302] The death recap shows a lot of "unknown": track every damage source correctly.
- [U-303] Selecting the right minion is hard when units are close; an option for attack-move: closest to me or closest to the cursor.
- [U-304] The Face Stealer's model does not match its hitbox. **Models must be tied to the hitbox** (the hitbox defines the model's metrics); an easy way to start models from hitboxes.
- [U-305] Own champions do not appear in champion select. Sorting: group by built-in / own champions outside the standard filters.
- [U-306] Occasional graphics glitches remain; re-check the graphics systems for performance.

**Ranges, mobs, turrets, structures**
- [U-307] The range button also shows **turret ranges, allies' autoattack ranges and jungle monsters' patience ranges**.
- [U-308] Jungle monsters: LoL-like **patience range**, they **regenerate when the fight ends**; gold, XP, health, stats and patience **easy to balance** per monster (all adjustable).
- [U-309] Turrets focus champions that attack champions (better rules); turret damage per target category adjustable; any kind of damage formula; turrets may cast skills later (not set now).
- [U-310] Destroyed turrets / structures: a **destruction animation**, then a **destroyed texture** (end state).
- [U-311] Every unit has a **category** (minion, monster, epic monster, champion, structure …) used by skill interactions and other rules.

**Skills and combat systems**
- [U-312] Default: a single-target projectile that kills its target **disappears** (like hitting a living one); adjustable per skill and per target category (minions, monsters, epic monsters, non-champions …).
- [U-313] Damage / heal formulas can use **current % HP, max % HP, missing % HP**.
- [U-314] Walls made of **parts** (easier to bend in many ways); default part size fixed but adjustable per skill; each wall has a **weight** (from its area; 1 = standard, bendable by every skill). Weak skills cannot bend heavy walls. A stat **elemental strength** (earth, ice for waterbenders, Avatars) scaling skills, with modifiers (additive / multiplicative like the others).
- [U-315] **Sky power** must follow the **element of each skill**, not the champion's nation (the Avatar's water skills feel the moon, his fire skills the sun / comet / eclipse); not shown as a global number — only as each character's modifiers. Full moon / comet / eclipse affect the **sizes** of all kinds of abilities.
- [U-316] No lifesteal, no omnivamp, no healing reduction (a different kind of MOBA).

**Stats, runes, masteries, items, shop**
- [U-317] New stats: an **adaptive resource** stat (raises every resource's amount / regeneration evenly; adjustable), adaptive damage, adaptive attack speed / haste, adaptive penetration, adaptive lethality, **basic ability haste** and **ultimate haste**, physical penetration and lethality (and items for them).
- [U-318] A **classic-runes** system (like old LoL marks / glyphs / seals / quintessences, free, no prices): sections for kinds of stats (defence, utility, offence) with points, quintessences few but strong.
- [U-319] A **classic-masteries** tree (30 / 30 / 30 like old LoL), adjusted for this game.
- [U-320] **Combination rules**: combining certain things / groups of stats gives bonuses.
- [U-321] Items: many more; starting items, early / mid / late items, cost-effective ones, a **mythic tier**, **potions** of many kinds; better build trees. Default sort: price low → high.
- [U-322] Inventory: **10 item slots** in a 3 × 3 box + the **boots slot** (bottom left, no number, no active) + one **special slot** (reserved slot 1: wards or similar, tied to roles, not bought like other items).
- [U-323] **Build creator** in setups (step by step, with texts, like LoL); a shop tab with the saved build; **auto-buy** option (default off; switchable in game settings, not in the shop).
- [U-324] **Auto-levelling** script per champion following the rules: default Q → W → E → R → T, the ultimate whenever possible; priority rules or an exact order; option in setups (default off), switchable in game settings.
- [U-325] The champion list shows **stance stats**; non-benders show their magic stances (or the possibilities) in tabs per element, with a note "available from the planetary alignment at 60:00".

**Map, art, architecture**
- [U-326] The map editable as a **pixel map** (one colour per map thing); a **texture for the whole map** possible (not only the terrain layout).
- [U-327] **Everything adjustable and easy to find**: champions in folders with everything about them (skills, own skills, art, animations, sounds); own champions get **full access** (own skills, defaults for art, everything) in folders. Skill logic and interactions never inside champions — in the skill / interaction systems; folders organised the same way for every system (events, mobs, map, textures …).

**Content, platforms, network**
- [U-328] More champions: every nation in every role (healers only water), creative melee champions, many non-benders (last task).
- [U-329] Windows / macOS builds (exe), also for hosting; check optimisation for them.
- [U-330] The server ping and the server's own processing time shown in the practice debug (to find glitches).
- [U-331] Servers must work on LAN and public IPs like Minecraft — test with a few clients.
- [U-332] Publish on Hubert's GitHub as public.
- [U-333] Ctrl + right click plans movement waypoints (each reached point is removed, movement continues to the next).
- [U-335] The death recap shows the **icon** of what dealt the damage (ability, effect, item); autoattacks have their own icons too.
- [U-334] FL Studio through Wine should be used for sounds (with its plugins).

## 29. Round 7 (2026-10-25)

- [U-336] Not as many item slots as planned (see [U-322]: 10 in a 3 × 3 box + boots slot + special slot).
- [U-337] **Channel interruption rules** for everything: by default most channels break on crowd control; some (e.g. recall) on damage taken. Exactly what interrupts each channel is adjustable (CC kinds, damage, displacement kinds, being lifted by self / ally / enemy …). Like LoL: damage fully absorbed by a **shield does not interrupt** (only damage to HP) — a core mechanic, adjustable in detail.
- [U-338] Random graphics glitches still appear on screen — must be fixed.
- [U-339] **Water as numbers**: the Avatar carries every kind of resource (uses water skills away from water from his own carried water). The water mechanic must be more flexible and useful with numeric values (not just "big source needed"). A waterbender can make a **temporary water source** by moving water out of a source; bending range from a source is **25 % longer**; **ice walls count as water sources** (a skill can take its water from a wall).
- [U-340] Skills that need a source: **first select the source, then the destination**. The same for bending big rocks (first raise a wall / rock, then throw it).
- [U-341] Wall damage: better wall **falling / breaking animations** with a direction.
- [U-342] The shop window can be **moved** (like LoL) and has an **X** button.
- [U-343] The death recap also lists **crowd control** effects and their times.
- [U-344] A **wet** unit does not get the **burning** effect from fire.
- [U-345] Some assassins have abilities that make them **quieter** (reduce others' sound senses). **Seismic Sense** does not work at all while not on the ground (lifted by self, ally or enemy).
- [U-346] The jungle has **stone hills and stone walls** too, not only trees. Most **dashes cannot pass walls**, except the ones over the air.
- [U-347] **Skill lockouts / concurrency**: skills can put other skills on cooldown or block them (all others, chosen ones, non-ultimates …) while casting, channelling or while their effect lasts; how many skills can be bent **at the same time** may be a stat (e.g. an item +1).
- [U-348] The range view (` / Ctrl + `) also shows **water source ranges** for my water skills.
- [U-349] **Fountains of water** in the bases near the shrine and the inhibitors (water mages are not handicapped there).
- [U-350] **Spirit portals** at the far north-west and south-east edges, shown as **beams of light** (Legend of Korra); teleporting takes time (= recall) and the arrival point shows that someone is coming. **Air Nomad temples** in the south-west and north-east as places / decoration.
- [U-351] Turrets and structures give **much more vision** (easy to adjust).
- [U-352] **Elevation rework**: some walls are **round raised rocks** (small / medium / big) = a temporary raised ground level. 1 level up can be walked off; 2 levels only with mobility skills that go over the ground (flight, jet propulsion, water lift carrying self or an ally). **Dents** (−1 / −2) work the same in reverse, drawn differently, and other earthbenders cannot use them like raised rocks. **Thrown rocks collide** with each other (like the fire clash, adjusted: a bigger stone partly passes and deals partial damage with a partial hitbox).
- [U-353] Problems with own champions; search for bugs and fix everything; publish the next patch on GitHub.

## 30. Round 8 (2026-10-28)

- [U-354] The range view (` and Ctrl+`) also shows the **XP range** (where you get experience from dying enemies).
- [U-355] **Portals** stand at the **farthest** NW / SE edges of the map (within ~300 units of the real corner), not just near the edge.
- [U-356] **Lava on the ground** makes far fewer stones. It slowly **melts stone walls** and melts **ice walls** faster; ice cools the lava — while melting ice the lava's remaining time is halved.
- [U-357] **Pathing**: bots still get stuck on walls. The path must keep the unit's real hitbox clear (irregular / unique shapes and sizes, the body moves while walking) on long paths; the player can still go to an exact spot.
- [U-358] Odd **unwalkable spots around the bases**: explain them or remove them. Only skills or real elevated terrain may raise ground, and it must be visible.
- [U-359] **Walls**: much easier to destroy with abilities; varied shapes (thick, long, small, most a single rock). A thrown rock leaves a small rock where it lands (a magma rock leaves a magma rock).
- [U-360] **Natural rocks** are stronger than bent ones, but much softer than now.
- [U-361] Waterbenders' **Ice Path** works on **deep and shallow** water.
- [U-362] **Lava throw** passes through walls — it must not.
- [U-363] **Death by a jungle mob** shows "unknown" — track it properly.
- [U-364] **Epic monsters** (Avatar-universe themed) with effects: one with a stackable effect, one giving gold; respawn timers on Tab and on the map. Big monsters show their HP bar over them; epic monsters show the number on the bar.
- [U-365] **Jungle tuning**: stat growth per respawn, XP, gold, HP etc. adjustable per mob for easy balance; more jungle mobs; camp respawn timers visible on the minimap.
- [U-366] **Jungle buffs**, **jungle items**, **support items** (like LoL).
- [U-367] **Collision for everything**: allies, enemy minions, minions with each other, players, monsters — no one passes through anyone, unless an item / effect allows it. **Spirits** pass by default.
- [U-368] **Displacement time** counts as CC (for the recap, tenacity-like rules, stats).
- [U-369] **Recall and portal channel: 6 s**.
- [U-370] **Moon phase** tracked all the time (0 % new moon → 100 % full moon → back down). Waterbenders gain from the **moon**, not the night: **+0.5 % damage per 1 % moon phase** (exact formula in code). At **full moon**: +25 % to all ranges (also water-based autoattacks) and radius / length, and **−10 % cooldowns** (flat cooldown reduction, not haste — add the stat type if missing). Fix the inaccurate tooltip.
- [U-371] **Post-game screen**: local chat (team / all, default all); **graphs** to pick from (gold, XP, leads, damage dealt over time, healing, highest hit, highest autoattack, damage / min, gold / min, CS / min, damage taken (+ / min), damage to minions, damage to monsters).
- [U-372] **Ping wheel** with icons and sounds; **emote wheel**; official emotes, own emotes (like own skins / champions, server decides if others may bring theirs), emote skins, own sounds per emote.
- [U-373] **Everything visual / audio is configurable in resource packs**: every graphic, animation, sound, picture, ping, SFX, music, model, loading screen, champions on the loading screen, the menu — anything.
- [U-374] **Server content download**: a server with another game version or its own resource packs offers them for download (forced → agree or leave); server-side options. Optional **server password** (separate from login); the client may remember it.
- [U-375] **Named versions + version ID**: users name versions (no name → server name + creation date); every version gets an ID computed from its files (a hash, like Europa Universalis 4 checksums) so clients can see if versions match.
- [U-376] Update everything on GitHub; continue the plan.

## 31. Round 9 (2026-10-29)

- [U-377] **Comet**: firebending gets **2 × the full moon's** bonus for waterbending (full moon water +50 % → comet fire +100 %).
- [U-378] **Bending solid objects like Taliyah's W**: pick a rock (first click), then where it goes (second click) — the rock itself is bent.
- [U-379] **Wall HP shown in 3 states**: untouched (< 1/3 damage), mildly cracked (1/3–2/3), almost torn apart (> 2/3).
- [U-380] **Wall standard for every wall ability**: walls are built from **segments 100 units long**, **33 / 66 / 99 units thick**, raised **1 or 2 levels**. Any shape the skill chooses. A unit whose hitbox fits on a segment is **lifted on top** of it; level 2 traps a unit without mobility. **Segment HP**: abilities **300**, **ice 100**; settable per skill and per rank (skills may raise segment HP per level etc.). Nothing hard-coded beyond these standards; earlier wall rules stay where they do not conflict.
- [U-381] **Ally lightning redirect**: an ally may hit an ally with lightning; the ally can redirect it (ally → ally → enemy).
- [U-382] Keep the established standards; everything configurable.

---
*Hubert continues editing the plan later — new ideas get the next free `[U-xx]` number.*
