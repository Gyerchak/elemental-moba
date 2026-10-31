# ENGINE EXTRAS — Claude ideas `[X-xx]`

Everything here is **NOT from Hubert's plan** — every row is a **Claude idea**. Hubert accepted all of them on 2026-10-23 ("your assumptions are good … mark as claude idea"). Two groups:

- **A. Engine needs** — a working MOBA cannot run without it.
- **B. Creative suggestions** — ideas that fit the theme. Optional.

Status column: `accepted · Claude idea` (Hubert can still `change` / `drop` any row).

## A. Engine needs

| ID | Area | What | Why needed | Status |
|----|------|------|------------|--------|
| [X-01] | Netcode | **Server-authoritative** simulation, clients send inputs only. UDP via `libenet` (installed). | Stable + cheat-resistant on private hosts (U-04, U-102). | accepted · Claude idea |
| [X-02] | Netcode | Client-side prediction for own movement + interpolation of others (≈2 ticks buffer). | Lowest latency feel (U-02). | accepted · Claude idea |
| [X-03] | Netcode | Lag compensation for skillshot hits (server rewinds ≤ 100 ms). | Fair skillshots at 128 Hz. | accepted · Claude idea |
| [X-04] | Simulation | **Fixed-point math** (built as Q24.8: 1 game unit = 256 sub-units, the 16 384-unit map fits easily; geometry in whole units) → deterministic, replays exact. | Stability, replays, desync detection. | accepted · Claude idea |
| [X-05] | Simulation | Fixed tick (default 128 Hz, U-102) decoupled from render FPS. | Core loop. | accepted · Claude idea |
| [X-06] | Architecture | ECS (data-oriented, structure-of-arrays). `EnTT` is installed — or own minimal ECS. | Performance with 32 players + minions + projectiles. | accepted · Claude idea |
| [X-07] | Map | Lanes = 3 (top, mid, bot), turrets 3 tiers per lane + 2 base turrets, inhibitors, nexus. Names themed: *Nation Shrine* = nexus. | Standard MOBA structure; plan mentions "1st-tier turrets". | accepted · Claude idea |
| [X-08] | Minions | Waves every 30 s from 1:05; melee / ranged / siege (fire: explosive siege, earth: digger siege). | U-12/U-13 need minions to exist. | accepted · Claude idea |
| [X-09] | Pathfinding | Grid A* on 512×512 + flow fields for minion lanes; re-path when a wall appears. | Walls/elevation change at runtime (U-24, U-45). | accepted · Claude idea |
| [X-10] | Vision | Fog of war per team computed on grid (shadowcast); terrain + elevation + bushes block; fire light radius (U-25). | U-75 terrain vision. | accepted · Claude idea |
| [X-11] | Economy | Gold (last hits, kills, passive), XP, levels — **no cap** (U-111), curve in X-44. | Items (U-80) and per-level stats (U-39) need it. | accepted · Claude idea |
| [X-12] | Rules | Respawn timers, win condition (destroy enemy nexus), surrender vote. | Match end. | accepted · Claude idea |
| [X-13] | Data | **Data-driven content**: champions, skills, items as text files (TOML/JSON) per version. | Needed for version snapshots (U-95) + modding (U-04). | accepted · Claude idea |
| [X-14] | Data | Version snapshot = folder `versions/<name>/` with hash; client downloads from server on join. | Makes U-95 fallback work. | accepted · Claude idea |
| [X-15] | Damage | Formula: `dmg × 100/(100 + effectiveDef)`, `effectiveDef = def × (1 − %pen) − lethality`, floor 0. | U-40 needs a formula. | accepted · Claude idea |
| [X-16] | Input | Input buffer (≈ 100 ms) + quick-cast / normal-cast / self-cast options. | Responsiveness. | accepted · Claude idea |
| [X-17] | Render | Placeholder renderer: **SDL3 + OpenGL/Vulkan**, top-down 3/4 camera, coloured boxes/cylinders. | U-03 visualisation. | accepted · Claude idea |
| [X-18] | Audio | SDL3 audio, positional; sound fog (U-94 field) decides who hears what. | Sound fog has no consumer otherwise. | accepted · Claude idea |
| [X-19] | Replays | Record inputs per tick (deterministic → tiny files). | Cheap thanks to X-04. | accepted · Claude idea |
| [X-20] | Spectator | Spectator slot with delay; uses replay stream. | Private tournaments. | accepted · Claude idea |
| [X-21] | Practice | Bots (simple behaviour tree) for practice tool. | U-100 is useless without targets. | accepted · Claude idea |
| [X-22] | Reconnect | Rejoin a running match; snapshot + replay delta. | Stability. | accepted · Claude idea |
| [X-23] | Communication | Ping wheel, text chat, voice (Opus codec). | U-99 lists chat/voice. | accepted · Claude idea |
| [X-24] | Security | Server validates every input (range, cooldown, resource). Open source ⇒ client is never trusted. | Open-source clients are easy to modify. | accepted · Claude idea |
| [X-25] | Debug | In-game overlay: tick time, ping, cell under cursor (decoded 32 bits). | Fast iteration. | accepted · Claude idea |
| [X-26] | Map data | Second per-cell layer (16 bit) for **water amount / temperature**. | Needed by U-26 water collection, U-15 melting, U-60 env. regen; 32-bit cell is full. | accepted · Claude idea |
| [X-27] | Status | Generic status-effect system: stacks, duration, tenacity; all CC in U-36/U-59 use it. | One system for all CC. | accepted · Claude idea |
| [X-28] | Localisation | UI strings in PL + EN files. | Hubert writes both. | accepted · Claude idea |
| [X-29] | Draft | Draft timer, ban phase 3+2 bans per team (LoL tournament style). | U-10 details. | accepted · Claude idea |
| [X-30] | Map data | **Bush** field (none / alive / burned) in reserved bits 29–30 of the cell. | Bushes exist in plan (U-14, U-89) but had no field. | accepted · Claude idea |
| [X-31] | Map data | Fog field values: 0 none, 1 partial, 2 full, 3 brush (hide inside). | Plan gives size only; see [?-15]. | accepted · Claude idea |
| [X-32] | Map data | Walkable values: 0 blocked, 1 walk, 2 fly-only, 3 bender-only. | Plan gives size only. | accepted · Claude idea |
| [X-33] | Stats | Melee defence bonus = +15 %. | U-58 needs a number. | accepted · Claude idea |
| [X-34] | Stats | Elemental auto: speed = AS + 10 % e-haste; damage = attack + 15 % e-power. | U-55/U-56 need numbers. | accepted · Claude idea |
| [X-35] | Combat | "Partial" block lets 50 % through. | U-20 "partly water". | accepted · Claude idea |
| [X-36] | Combat | Blocking skills of air / fire — rules not in plan. Now: block nothing. | Gap. | accepted · Claude idea |
| [X-37] | Combat | Typical ability strength = 100 units. | U-44 needs a unit. | accepted · Claude idea |
| [X-38] | Combat | Stone wall 200 strength / 4.0 s; ice wall 100 / 2.5 s. | U-18/U-24 numbers. | accepted · Claude idea |
| [X-39] | Vision | Night vision **5 %** (new moon, U-115) → 85 % (full moon); day 100 %. | U-108 numbers. | accepted · Claude idea |
| [X-40] | Vision | Solar eclipse darkens day to new-moon night vision. | Fits U-35 + U-108. | accepted · Claude idea |
| [X-41] | Abilities | Multicast falloff % per extra hit on same target. | Taliyah-style balance (U-106). | accepted · Claude idea |
| [X-42] | Map | Sand beaches (5 cells) around the sea. | Tropical look (U-86). | accepted · Claude idea |
| [X-43] | Abilities | Max **1 targeted skill per champion** (e.g. bloodbending U-78). Checked by `kit_ok()`. | Makes U-109 "close to 0" a hard rule. | accepted · Claude idea |
| [X-44] | Economy | Uncapped XP curve: level n → n+1 needs `280 + 100 × (n − 1)` XP (= LoL up to 18, total 18 360). Every level = 1 skill point (T ranks: U-120). Code: `src/core/progression.hpp`. | U-111 / U-119 need a curve. | accepted · Claude idea |
| [X-45] | Balance | Optional soft cap: after level 30 each level gives 50 % of the normal growth. Off by default. | Linear scaling without a cap can explode in long games. | accepted · Claude idea |
| [X-46] | Items | Placeholder items for U-113/U-114: *Tidecaller's Reach* (+range), *Ember Lens* (+skill speed), *Monk's Beads* (+cast speed), *Wide Stance Bracers* (+width/length/radius), *Lotus Tile* (timed effect: +40 cast speed for 5 s on use). | U-113/U-114 need something to test with. | accepted · Claude idea |
| [X-47] | Formula | Cast time and cooldown use haste style: `time × 100 / (100 + points)` → never reaches 0. | U-114 needs a formula. | accepted · Claude idea |
| [X-48] | Formula | U-28 water-state nerf touches damage, range, width, length, radius, speed — **not** cast time / cooldown. | "Every aspect" is unclear for times. | accepted · Claude idea |
| [X-68] | CC | Tenacity cap **80 %**. Formula: `base × (1 + CC amplify) × (1 − tenacity)`. Some CC (airborne, bloodbending) can ignore tenacity. Code: `cc_duration_ms()` in `stats.hpp`. | U-127 needs a formula. | accepted · Claude idea |
| [X-60] | Vision | Light layer: 0–100 % per cell, team-neutral. Full light in the inner half of the radius, then linear to 0. Brightest source wins (no sum). Full vision fog stops light, partial / brush = 50 %. Rebuilt at 16 Hz. See `light.hpp`. | U-117 needs rules. | accepted · Claude idea |
| [X-61] | Vision | Every fire-family skill (fire, blue fire, lightning, combustion) lights **6 cells (192 units)** unless its data says more. | U-116 default. | accepted · Claude idea |
| [X-63] | Economy | ~~Extra `25 × (n − 17)²` XP after level 18.~~ Replaced by the growing XP income in X-64 (U-121). | — | dropped |
| [X-64] | Economy | XP income target per player grows from **331 XP/min** (0:00) to **995 XP/min** (60:00), then stays flat. Result: 15:00 → lvl 10, 35:00 → 18, 60:00 → 27, 90:00 → 36, 120:00 → 43. Minion/kill XP gets tuned to it in M5. | U-119 + U-121 need a pace. | accepted · Claude idea |
| [X-66] | Rules | Game phases: **early 0:00–18:00, mid 18:00–40:00, late 40:00+**; the alignment at 60:00 (U-128) is the deep late game. LoL is about 0–14 / 14–30 / 30+. | U-121 "a bit longer than LoL". | accepted · Claude idea |

## B. Creative suggestions

| ID | Idea | Status |
|----|------|--------|
| [X-50] | **Comet (25:00–29:00):** firebenders power +75 %, sizes +40 %, XP +40 % (U-122, U-123). Global for now; later idea: stronger inside the crater. | accepted · Claude idea |
| [X-51] | **Full moon (30:00–32:00):** waterbenders power +50 %, sizes +25 %, XP +25 % (U-122, U-123); bloodbending (U-78) only now. **New moon:** waterbenders −10 %. | accepted · Claude idea |
| [X-52] | **Eclipse:** firebenders' power follows the U-124 V-curve (100 → 0 → 100 %) and they are muted 6 s in the middle (U-125). | accepted · Claude idea |
| [X-53] | **Night/day:** fire +5 % at day, water +5 % at night. | accepted · Claude idea |
| [X-54] | Spirit portals at the two poles (U-84): channel 4 s to enter the Spirit World lane shortcut. | accepted · Claude idea |
| [X-55] | Jungle boss **"Koh-like Face Stealer"** spirit: steals one ability of a random killer for 2 min. | accepted · Claude idea |
| [X-56] | Lava (earth sub-element) turns into stone after 8 s → becomes a temporary wall (feeds U-24). | accepted · Claude idea |
| [X-57] | Steam: fire on shallow water creates a fog cloud = vision fog for 3 s. | accepted · Claude idea |
| [X-58] | Lightning on water: chains to every unit standing in the same connected shallow-water area. | accepted · Claude idea |
| [X-59] | Placeholder champion roster (10): see table below. | accepted · Claude idea |
| [X-67] | **Event numbers** (U-123, U-126): comet fire +75 % power / +40 % size; full moon water +50 % / +25 %; alignment all benders +25 % power / +10 % size. Power multiplies ability damage; size multiplies range, width, length, radius, skill speed. | accepted · Claude idea |
| [X-62] | **Sky Lantern** (firebender F/G, U-118): slow flame that hangs in the air, lights 20 cells for 20 s, +2 cells per rank, CD 60 s. In `light.hpp`. | accepted · Claude idea |
| [X-65] | Firebender champions glow a little at night (2 cells) → they see more, but are also easier to spot. | accepted · Claude idea |

### [X-59] Placeholder roster (names are generic, not copyrighted) — Claude idea

| # | Name | Style | Sub-element | Role |
|---|------|-------|-------------|------|
| 1 | Ember Monk | Fire | fire / combustion | burst mage |
| 2 | Storm Daughter | Fire | lightning | ranged carry |
| 3 | Stone Warden | Earth | earth | tank |
| 4 | Iron Sentinel | Earth | metal | fighter |
| 5 | Magma Herald | Earth | lava | zone mage |
| 6 | Tide Healer | Water | water / healing | support |
| 7 | Frost Hunter | Water | ice | skirmisher |
| 8 | Wind Nomad | Air | air | assassin |
| 9 | Mist Dancer | Air | evaporated water | utility |
| 10 | Chi Blocker | Non-bender | ammo / energy | melee assassin |

## C. Claude ideas added while building M1–M9 (2026-10-24) — status: `Claude idea · new`

| ID | Area | What | Why | Status |
|----|------|------|-----|--------|
| [X-69] | Hitbox | Body hitbox = 3–5 circles of the core body (2 shoulders, belly, butt; beasts: chest, belly, butt, tail). Shoulders / hips / tail **swing with the walk cycle**. Head and limbs are not hitbox (head drawn faintly). `core/hitbox.hpp`. | U-130 shape. | Claude idea · new |
| [X-70] | Hitbox | Fluid skill shape = union of up to 12 wobbling circles: water ±14 %, fire ±20 %, air ±10 %, earth ±3 % of the radius, smooth noise over ticks. Client and server compute the same shape (`fx_shape`). | U-131. | Claude idea · new |
| [X-71] | Hitbox | "1 pixel" = **1 game unit**; touching (distance = r1 + r2) counts as a hit. | U-132 in a pixel-free sim. | Claude idea · new |
| [X-72] | Combos | Element pairs: fire+air = **Firestorm** (+50 % size & damage, burn), fire+water = **Steam** fog 3 s, fire+earth = **Lava** pool → stone, water+air = **Ice shards** (slow 40 %, −20 % U-28), water+earth = **Mud** (slow 50 %), air+earth = **Sandstorm** (blind). A moving skill touching an allied zone of another element triggers it. `core/combos.hpp`. | U-134. | Claude idea · new |
| [X-73] | Group bending | Same-element ally feeds a zone marked "group" (tornado, wildfire, whirlpool, quake, monsoon): **+25 % size, +20 % damage, +1 s** per feed, max 4 feeds (5 benders). Two tornadoes of allied airbenders merge. | U-135. | Claude idea · new |
| [X-74] | Items | Element-defensive items: Wet Wraps (vs fire), Clay Plate (vs water), Windbreaker (vs air), Rootweave (vs earth): −25 % damage from that element; cap 60 % per element. | U-136. | Claude idea · new |
| [X-75] | Skills | Sandbending: earth version 90 dmg, blind 70 % 1.5 s, CD 12 s; air version 60 % damage, blind 50 % 1 s, CD 7 s. | U-137 numbers. | Claude idea · new |
| [X-76] | Resources | Sun height fuel bonus: triangle 0 % (06:00) → **+30 % at noon** → 0 % (18:00); none at night or in an eclipse. | U-139. | Claude idea · new |
| [X-77] | Resources | "During the comet" = the comet buff window 25:00–29:00: fire costs nothing, fuel bar stays full. | U-138 window. | Claude idea · new |
| [X-78] | Bushes | Bush hiding test: centre + 11 edge points of every body circle must be on brush cells. | U-140 in practice. | Claude idea · new |
| [X-79] | Senses | **Wind Sense** (air special, active): 10 s, range 2 400; enemy skills outside vision show as white arcs pointing the way they move — only to this airbender. | U-141. | Claude idea · new |
| [X-80] | Senses | **Seismic Sense** (earth special, always on): range 1 400, every 0.5 s a brown ring wave near each MOVING ground enemy outside vision (±24 units jitter), team-shared; flying / airborne = nothing; enemy earthbending (walls, pillars, boulders) = square sand-coloured "terrain changed" waves. | U-142. | Claude idea · new |
| [X-81] | Senses | **Heat Sense** (fire special, always on): range 1 200, every 0.75 s orange outward waves at every warm enemy (moving or not) and enemy fire skills outside vision; frozen units give nothing. Team-shared. | U-143. | Claude idea · new |
| [X-82] | Stats | Model size never below 50 %. Items: Giant's Belt +15 % size (+380 HP), Feather Sash −15 % size; Iron Armour +15 %, Stone Armour +20 % while active; the Face Stealer is +60 %. | U-129 needs sources. | Claude idea · new |
| [X-83] | Architecture | Own minimal ECS: plain vectors of units / effects with id → index map, compacted each tick (no EnTT). | Simple, fast enough (one 5v5 tick ≈ 0.8 ms). | Claude idea · new |
| [X-84] | Items | Basic stat items (Long Spear, Spirit Pearl, Ruby Heart, Cloth Armour, Lotus Cape, Swift Dagger, Sky Boots, Hourglass Charm, Mercury Treads, Binding Chains, Void Shard). | A shop needs basics. | Claude idea · new |
| [X-85] | Vision | Turrets and Nation Shrines carry **braziers** (light 420 units) so lanes are not pitch black at new moon. | U-115 makes nights ~blind. | Claude idea · new |
| [X-86] | Map | **Spirit island**: the mid bridge widens to a circle (r 30 cells) in the map centre; the Face Stealer lives there. | One symmetric boss spot. | Claude idea · new |
| [X-87] | Rules | Recall (B): 8 s channel → fountain; any order or damage cancels it. | Needed to shop / heal. | Claude idea · new |
| [X-88] | Movement | Turn rate 4 full turns / s; a unit walks / casts only when facing within 60° of the target direction. | U-74 numbers. | Claude idea · new |
| [X-89] | Resources | Water regen × (1 + 0.25 × water cells nearby, max ×6); standing on glacier = full water. | U-26 / U-60 numbers. | Claude idea · new |
| [X-90] | Combat | "Call for help": hitting an enemy champion makes their minions and turrets within 900 target you. | Standard MOBA lane rule. | Claude idea · new |
| [X-91] | Terrain | Burned trees regrow after 90 s, bushes after 20 s; fire spreads to a neighbouring tree 12 % of the time. | U-14 numbers. | Claude idea · new |
| [X-92] | Vision | The client shows terrain changes only where its team has vision; under the fog it keeps the last seen terrain (the server still sends all cells). | U-142 says only seismic sense shows bending under fog. | Claude idea · new |
| [X-93] | Netcode | Snapshots every 2 ticks (64 Hz at the 128 Hz tick), unreliable; terrain, feed and commands reliable. `--snap-div 1` = full 128 Hz. | Bandwidth. | Claude idea · new |
| [X-94] | Collision | Layers per unit: terrain (deep water, elevation step > 1), tree, wall (map + temporary), structure, unit. Each body circle is tested at its centre + 8 border points; structures / units by exact circle overlap. A stuck unit may only move out (overlap depth must shrink). A* keeps a body-sized distance from blockers (clearance map). Flying ignores terrain, trees and walls. | U-145, U-146. | Claude idea · new |
| [X-95] | Structures | Structure footprint = its body circle; the cells under it are protected: no wall, pillar, lava or fog can be bent there. Destroyed structures stop blocking. | U-144. | Claude idea · new |
| [X-96] | Collision | Unit collision ON for: Stone Warden, Iron Sentinel, Angry Beast, Face Stealer, item **Bulwark Pauldrons** (1 000 g, +250 HP, +15 DEF, +5 % size). Overlapping units are pushed apart (max 3 units / tick). | U-145 examples. | Claude idea · new |
| [X-97] | Jungle | Aggressive camps (3 per side): **Cursed Spirit** (ranged curse), **Angry Beast** (big, body-blocks), **2 Bandits** (melee + ranged). Attack anyone within 420 units near their camp, leash 700. The old passive camps stay. | U-147. | Claude idea · new |
| [X-98] | Controls | A + left click: attack-move, closest enemy champion only (within 600). D + left click: attack-move, anything. | U-148 keys. | Claude idea · new |
| [X-99] | Combat | Auto-acquire: an idle champion autoattacks the closest enemy already in range, and does not chase it (LoL default). | Autoattacks felt dead without it. | Claude idea · new |
| [X-100] | Shop | Shop at the fountain, range **1 100 units** (gold ring on the map); buy only inside it (or while dead). | U-149. | Claude idea · new |
| [X-101] | Vision | Eyes: champions, minions and monsters see full range only inside a **210° cone** around their facing; behind them only **160 units** ("body sense"). Structures see all around. Turning around = seeing again. | U-150. | Claude idea · new |
| [X-102] | Senses | Hearing (every champion, team-shared): footsteps up to **900**, attacks / casts / explosions / storms up to **1 600** units; outside vision only; every 0.4 s a ripple about 40 units off the real spot. Grey thin = steps, bright = fighting. | U-151. | Claude idea · new |
| [X-103] | Feedback | Floating damage numbers, a white slash for autoattacks, and the server's reason when an order does nothing ("not learned", "cooldown", "stand in the shop ring"…), shown above the skill bar. | Hubert could not tell if attacks / skills worked. | Claude idea · new |
| [X-104] | Vision | Eye point = head position: 20 units in front of the body centre (beasts 40), × model size. All vision rays start there; "inside the bush" = eye cell is brush. | U-152. | Claude idea · new |
| [X-105] | Netcode | Snapshots (several KB) are sent with `ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT`. Without it enet sends the fragments of a big "unreliable" packet RELIABLY → 64 reliable bursts / s → growing ping (U-173). | Fix. | Claude idea · new |
| [X-106] | Light | Smoothstep curve: full light 08:30–15:30, dusk 15:30 → 21:30, night 21:30–02:30, dawn 02:30 → 08:30; light = moon night level + (100 − night) × sun. Eclipse: smooth V to new-moon dark. `vision_range_pct()` in `world_clock.hpp`. | U-153. | Claude idea · new |
| [X-107] | Jungle | Leash 1 100 (boss 1 400); a monster gives up only when the target is gone / far AND it was not hit for 2.5 s; walking home heals 5 %/s (was 64 %/s instantly). | U-154. | Claude idea · new |
| [X-108] | Minions | Minions pick minions and structures first; champions only within 350 units or after a "call for help" (lasts 2.5 s); minion autoattacks do 60 % damage to champions. | U-156. | Claude idea · new |
| [X-109] | Economy | Kill rewards (gold to the last hitter, XP shared) are sent to the player and drawn as fading "+60xp +21g" where the unit died. | U-159. | Claude idea · new |
| [X-110] | Settings | `settings.cfg` (key=value) where the game is started; keys stored by name; defaults: skills Q W E R T F G, S stop, Space stance, B recall, V portal, P shop, Tab score, Y camera lock, F1 centre, A champ attack-move, D attack-move, X champions only, H autoattack, Enter chat, 1–8 items. | U-158, U-163, U-165. | Claude idea · new |
| [X-111] | HUD | Placeholder champion icons: champion colour, initials, element badge (water drop, earth square, fire triangle, air ring, non-bender diamond). Item slots 8 (4 × 2). | U-157, U-171. | Claude idea · new |
| [X-112] | Camera | Tilted view = orthographic tilt: ground squashed to 78 % vertically, heights drawn at 62 % (≈ LoL's angle without perspective). Units: footprint (hitbox) on the ground, body = upright cylinders over each hitbox circle (champion 70, minion 44, turret 190, Shrine 150 units high), head on top; drawn back to front. Earth team's view is rotated 180°. | U-174, U-175. | Claude idea · new |

## D. Claude ideas from test rounds 3 and 4 (2026-10-24) — status: `Claude idea · new`

| ID | Area | What | Why | Status |
|----|------|------|-----|--------|
| [X-113] | Loadouts | 5 keystones (Elemental Surge, Stoneheart, Tailwind, Fuel the Fire, Chi Rush), 4 specialisation trees × 3 tiers × 2 nodes (Ferocity, Resolve, Insight, Harmony), runes = 3 slots × 3 shards. Numbers in `game/loadout/loadout.hpp`. | U-177 needs content. | Claude idea · new |
| [X-114] | Minions | LoL-2016 target priority (7 steps + structures), call for help 2 s within 900, acquire 650, drop 900, champions take 60 %; all in `mobs/rules.hpp`. | U-194. | Claude idea · new |
| [X-115] | Water | Swimming = half speed, no spells except Water Spout / Ice Path; swimmers take 50 % longer freeze / ice-slow and an extra 0.6 s freeze from ice hits. Water Spout 6 s; Ice Path freezes a 160 radius for 6 s; ice bursts freeze water too. | U-188 … U-190. | Claude idea · new |
| [X-116] | Heights | Unit height = terrain level × 40 + airborne 60 / water ride 80 / flying 120 (Sky Ascension 40–200). Skill reach: melee / ground / dash 40, others 160, pillars 200; `reach_height` overrides. Melee autoattacks reach 40. | U-191, U-216, U-249. | Claude idea · new |
| [X-117] | Combat | Sweet spot zones: blast centre 40 % of the radius, cone tip = outer 25 %, bolt = last 20 % of its flight. Combustion +30 %, Fire Whip +25 %. | U-222. | Claude idea · new |
| [X-118] | Items | Slow resistance boots (Current Striders −25 % non-ice, Seal Fur Boots −40 % ice), Sky Bison Charm +7 % move speed, Bulwark Pauldrons, Messenger Falcon; limits: boots 1 per group, resist wards / actives 1 each. | U-218, U-219, U-235, U-245. | Claude idea · new |
| [X-119] | Element rules | Wet 5 s (1 game hour) / 1 s for waterbenders; lightning +25 % wet, +25 % iron (adds up); air empowers fire +25 %; burning ground 3 s, 12 dps; smoke 6 s; trees regrow 120 s (1 game day); fire clash keeps (1 − weak/strong) of the stronger fire. `mechanics/element_rules.hpp`. | U-227, U-241, U-244. | Claude idea · new |
| [X-120] | Light | Fire in Hand 300, lantern 190, firebender glow 64, fire while casting 160 (game units). Comet: reddish night; moon: whiter with its phase. | U-228, U-237, U-244. | Claude idea · new |
| [X-121] | Falcons | Messenger Falcon item (400 g, cd 90 s): flies 900 u/s, watches 8 s, records every 0.25 s within 900, flies back, the team replays the record as ghosts for 8 s. | U-235. | Claude idea · new |
| [X-122] | Light curve | Smoothstep day: full 08:30–15:30, dusk to 21:30, dawn from 02:30. | U-153. | Claude idea · new |
| [X-123] | Controls | Ping wheel = Ctrl + left click and drag; practice cheats moved to Ctrl+Shift+N/G/L (F-keys are for teammates). | U-231, U-246. | Claude idea · new |
| [X-124] | Clouds | Steam / smoke / sand / snow are cloud objects (centre, radius, end time) instead of cell edits; vision stops at the first cloud cell, for every team. | U-241; fixed a bug where a cloud on a burned tree lasted until the tree regrew. | Claude idea · new |
| [X-125] | Network | The client services enet in its own thread every 1 ms (acks no longer wait for the next frame). | U-173 / U-215 ping. | Claude idea · new |
| [X-126] | Content | Custom champion files copy a base champion and may change listed stats and swap W/E/R/T/P for existing skills by name; the Q pool / specials follow the element. | U-210 (a first, safe version). | Claude idea · new |
| [X-127] | Pathfinding | A* now really checks the body size (a shadowed variable compared the clearance with the direction index). Bodies keep a cell margin = their reach. | U-252 bug. | Claude idea · new |
| [X-128] | Text | All text in DejaVu Sans Mono (bundled, SDL3_ttf) with a dark outline; `fit_text()` cuts long text with "…"; cached per string. | U-250 (Polish letters, overlaps). | Claude idea · new |
| [X-129] | Game folder | The game finds `resourcepacks/` + `lang/` itself (current folder, next to the program, or one / two folders up) and works from there. | U-250 / U-259 (no sounds when started from `build/`). | Claude idea · new |
| [X-130] | Modifiers | One list: sky → terrain → statuses → keystone / item / ability buffs (ModKind on the wire as kind ≥ 64). Under bars: 3-letter chips (gold frame = sky, red = debuff); "minor" sky info only in my row. | U-237, U-253, U-255. | Claude idea · new |
| [X-131] | Damage tags | Every hit carries DmgFrom (auto, skill slot, item, burn, keystone, combo, chain, turret, minion, monster). Burn ticks are gathered and dealt every 0.5 s through shields. | U-254, U-262. | Claude idea · new |
| [X-132] | Death recap | The fight that killed: walk back while hits are < 10 s apart, max 30 s; rows by (source, what), a bar each; timeline in 0.5 s columns coloured by damage type. | U-262. | Claude idea · new |
| [X-133] | Announcements | First blood, kill, double…penta (next kill within 10 s), sprees 3…8+, shutdown (+150 gold, spree ≥ 3), ace, executed, turret / inhibitor / shrine destroyed, boss slain, minions spawned; sky events client-side. One banner at a time (2.6 s) + voice. | U-263. | Claude idea · new |
| [X-134] | Items | 3 tiers; buying uses carried parts (also parts of parts), you pay the rest; 18 new items (3 basic, 6 epic, 9 legendary). Bots build towards finished items. | U-261. | Claude idea · new |
| [X-135] | Fire fuel | Calm flame: missing fuel paid with HP ×2. Rage: +10 % of damage dealt, +5 % of damage taken, drains 8 / s after 5 s out of combat, a skill paid fully with rage +20 % damage; Storm Daughter uses rage. | U-268. | Claude idea · new |
| [X-136] | Surfaces | Stone roads (lane centre, 3 cells), forest paths (pond → lanes, 1.6 cells), wooden bridges (lane over river). Earthbending only on land, dryland, sand, stone, roads, paths, shallow water. | U-269. | Claude idea · new |
| [X-137] | Avatar | Avatar Kyren: W water wave, E earth pillar, R fire cone, T Avatar State (6 s: fly, +50 % power, +25 % size), P Elemental Cycle (4 elements → next skill +40 %); Q from 3 elements; any bender special. Max one Avatar per team; normal + dark version share a unique group. | U-279, U-07, U-08. | Claude idea · new |
| [X-138] | Magic stances | Non-bender specials Ember / Tide / Stone / Gale Stance: 10 s, cd 60 s, physical skills + autos become that element (sky events apply). | U-264. | Claude idea · new |
| [X-139] | Ranking | Elo MMR (start 1200, K 48 → 32 after 10 games), LP win +20 / loss −18 ±1 per 25 MMR off (10…30), tiers per 400 LP with 4 divisions; snake-draft team balance; pick timer 90 s on dedicated servers. File `server_data/ratings_<port>.txt`. | U-276. | Claude idea · new |
| [X-140] | Light fixes | The light's own cell is always lit (the ray walk went the wrong way); afterglow for fire skills with light_ms; burning trees glow 6 s; magma glows (scanned every 2 s). | U-256. | Claude idea · new |
| [X-141] | Network | enet range-coder compression on every host; lights sent only after a rebuild (16 Hz). | U-280. | Claude idea · new |
| [X-142] | Art pipeline | ComfyUI FLUX.2 klein 4B → `art/raw/` (tools/art/gen_art.py) → pack PNGs (install_art.py). Blender 5.2 primitives → 8-direction sprite sheets + .glb (make_models.py). Piper / espeak-ng announcer + numpy effects (make_sounds.py). | U-257, U-258, U-259. | Claude idea · new |
| [X-143] | Own versions | `versions/<name>/version.txt`: `champion.<Name>.<key> = n` (hp, attack, ep_per_level, defence, elemental_defence, move_speed, attack_range, attack_speed) and `skill.<Name>.<key> = n` (damage, damage_per_rank, ep, ad, range, width, speed, radius, cast_ms, cooldown_ms, cost, duration_ms, cc_ms, cc_value). Any mistake → refused, official numbers stay. Server: `--version <name>`; joining players with the same folder switch automatically. Items are not in versions yet. | U-95, U-285. | Claude idea · new |
| [X-144] | Skin models | A skin may bring `champions/<id>/skins/<skin>/model/sheet.png + sheet.txt`; used for MY champion with my chosen skin (other players: base model). | U-97, U-285. | Claude idea · new |
| [X-145] | Champion select layout | Filters and pick rows wrap inside the middle column; the picks panel scrolls (mouse wheel) and is clipped; the loadout editor is folded ("Loadout ▸"); every button label shrinks / cuts to fit. | U-250, U-282. | Claude idea · new |
| [X-146] | Wet vs fire | In apply_hit (every hit: skills, autoattacks, burn ticks): fire family except lightning on a wet target → ×0.75 and dry. Allies: a fire / water skill touching a wet ally only dries it. | U-286. | Claude idea · new |
| [X-147] | Profiler | `sim_run <min> <seed> --profile`: ms per step part + the slowest ticks. Found: pathfinding spikes 30–42 ms (unreachable goals searched the whole map 4×). | U-287. | Claude idea · new |
| [X-148] | Pathfinding budget | Goal moved to the nearest cell where the body fits (≤ 8 cells); 2 tries (body, thin) instead of 4; ≤ 15 000 explored cells per tick for everybody together — the rest walks straight and asks next tick. Vision: one team per tick. Worst tick 42 ms → 7 ms (budget 7.8 ms). | U-287. | Claude idea · new |
| [X-149] | Walls | HP per wall cell: ice 160, stone 240, metal 480 (stands 1.5 × longer). Effectiveness %: ice — lava 300, fire 250, earth 120, lightning 100, water/ice 50, air 60; stone — earth/lava 150, metal 100, physical 75, fire 60, water/air 50, lightning 40; metal — metalbending 200, lava 150, earth/lightning 60, fire 50, physical 40, water/air 30. Items: Ice Pick, Siege Maul, Lodestone, Quarry Gloves. Source: casting earth / ice / metal skills within 4 cells of a stone / ice / metal wall → +15 % damage, the wall loses 40 HP. Cracks drawn on damaged pieces. Time changes (CellChange.pub) go into every player's map. | U-288. | Claude idea · new |
| [X-150] | Sixth skill | Slot Y between the ultimate T and the specials (code: P Q W E R T Y F G, `kSkillSlots` = 9; the specials keep the names F / G, keys G / H). 11 new Y skills (Claude idea). Settings v3 moves old default keys; shop O; attack range circle = range + body reach. | U-289. | Claude idea · new |
| [X-151] | Fog fade | The fog (10 rebuilds / s) is mixed pixel by pixel from the shown one to the newest over 0.1 s → no stepping edges. | U-290. | Claude idea · new |
| [X-152] | Rank rules | SkillDef: `ultimate`, `unlock_level` (0 = 6 / 1), `rank_step` (0 = 6), `rank_cap` (0 = 2), `passive_desc`. `skill_max_rank()`: ultimate = 0 before unlock, then 1 + (level − unlock) / step; others = cap + level / step. Old ultimates moved from T to Y; last round's extra skills are on T. Files: `<slot>_ultimate / _unlock_level / _rank_step / _rank_cap` in .champ files and `skill.<name>.<same>` in versions. A learned active skill's own passive counts in has_passive(). | U-291. | Claude idea · new |
| [X-153] | Puddles | Heat Draw (radius 220) turns touching steam clouds into Puddle clouds for 8 s (no vision block); units inside the steam at that moment: Wet 5 s (waterbenders 1 s). On shallow water / a puddle, not swimming, not flying: Wet refreshed 0.15 s each tick (longer wet kept). | U-292. | Claude idea · new |
| [X-154] | Condensing | Ice effects (not steam) through steam: the cloud shrinks by the effect radius (≥ 48), a puddle (or ice patch in ice form) radius + 16 appears / is refreshed (8 s); wet 5 s there (not for ice). Waterbender source form (key K, CmdKind::SourceForm): casting a water / ice skill within 4 cells of a puddle / ice patch uses it up, +15 %, the skill becomes water or ice. Ice patch: −20 % move speed while on it; Heat Draw melts it into a puddle. | U-293. | Claude idea · new |
| [X-155] | Ice wall source | Wall skills with ice need, within 6 cells: shallow / deep water, glacier, an ice wall, or a puddle / ice patch (used up). | U-294. | Claude idea · new |
| [X-156] | Sounds v2 | FL Studio has no command line to make sounds → sounds stay generated (scipy): per element recipes (water splash + bubbles, fire roar + crackle, earth thump + gravel, air band sweep, lightning crack + buzz, non-bender steel swish), shape = length / envelope / impact, soft clip, 0.25 s room echo. Lightning has its own set. Own sounds from FL: same file names in an own pack (sounds/README.txt). | U-295. | Claude idea · new |
| [X-157] | Water numbers | Water skills from a source cost no carried water: a chosen source (river / sea / glacier, ice wall, puddle / ice patch) within the skill range + 4 cells, or water within 6 cells of the caster. Ice walls / Iceberg cost 320 (> 300 carried) → need a source. The Avatar has its own pool: 150, 60 per water skill, 2.5 / s (×6 near water, full on glacier). |
| [X-158] | Source first | Water bolts / waves / cones / walls (`source_first`, −1 = this default): click 1 on water picks the source (ring), click 2 the target; the skill starts at the source, +25 % range, +15 % damage, the chosen form (water / ice). Used puddles vanish, ice walls lose 30 HP. Quick cast over water = source picked. |
| [X-159] | Lava + walls | Lava cells: 12 % cool into a 4 s rock, the rest into ground. Every 250 ms lava hurts walls next to it: stone 4, ice 20, metal 3 HP; touching ice halves the lava's rest time once. |
| [X-160] | Wall HP v2 | Ice 90, stone 140, metal 300 (was 160 / 240 / 480). Natural rocks 2 × and break into rubble that grows back after 3 min; the map border never breaks. |
| [X-161] | Rock shapes | Outcrop per grove: 50 % one rock, 20 % small, 10 % long, 10 % thick, 10 % a pair; a 1.6-cell hill ring. Never within 6 cells of the base / pole / sea areas. Cores are Wall::Stone. Earth / lava bolts leave a 1-cell rock for 20 s at the end of their flight (not on units). |
| [X-162] | Paths v2 | Look-ahead: up to 6 waypoints skipped per tick when the straight line keeps the body clear. Cliffs (step > 1) count as blocked in the clearance map. Second A* try = body size − 1. |
| [X-163] | Collision v2 | `kCollUnit` is in the default mask; two units collide only when both have it. Spirits (Wandering Spirit, Spirit Wolf, Cursed Spirit, Knowledge Seeker), `ghost` items and Mist Form clear it. |
| [X-164] | Displacement CC | A knock holds the unit (stun) for distance / 1 200 units per s (min 100 ms) and is logged as "displaced" in the recap. |
| [X-165] | Moon formula | Water power ‰ = 1000 + 5 × moon phase % (0 new … 100 full). Full moon: +25 % range / length / width / radius / speed, water autoattack range +25 %, cooldowns × 0.90 after haste (SkyBoost::cd_pct). Night / new-moon modifiers removed. |
| [X-166] | Jungle v2 | MonsterDef: respawn_ms, first_spawn_ms, +10 % HP / +8 % attack / +5 gold / +10 XP per respawn (default), buff, team reward. Platypus Bear → Ember 90 s (autos burn 1 % max HP over 2 s, slow 15 %). Knowledge Seeker → Wan Shi Tong's Wisdom 90 s (regen +50 %, CD × 0.9). Lion Turtle (6:00, every 6 min): team +4 % power per stack, permanent. Sun Dragons (10:00, every 7 min): 250 gold (+25 per respawn) to every team champion. Epic islands at (170,170) / (341,341) with shallow fords. |
| [X-167] | Portals v2 | Portal cells (10,10) and the mirror — about 300 units in from the corners. Recall / portal channel 6 s. |
| [X-168] | Comet = 2 × full moon | Fire at the comet: +1000 ‰ power (2 × the moon's 500), +50 % ranges / sizes / fire autoattack range, cooldowns × 0.80, XP +50 % (`kCometMoonFactor`). |
| [X-169] | Wall segments | Every wall skill builds segments of 100 units (`raise_segment`): thick 33 / 66 / 99 (`wall_thick` 1–3, default 66), raised 1 or 2 levels (`wall_level`, default 2), HP per segment 300 / ice 100 (`wall_hp`, `+ wall_hp_rank` per rank). A segment's cells share HP and fall together. Lines = n × 100 side by side; rings = segments along the tangent (≥ 4). Segments never stack on other walls. |
| [X-170] | Lifted on walls | A unit whose whole body (all hitbox sample points) lies on a new segment stands on top: the segment cells become walkable at the raised level. Level 1 = walk off; level 2 = trapped without mobility (step > 1). Skills pass over a segment somebody stands on. |
| [X-171] | Wall look | Cracks in 3 states by lost HP: < 1/3 none, 1/3–2/3 two cracks, > 2/3 five cracks + missing chunks. |
| [X-172] | Rock bending | Earth bolts / waves are source-first: click 1 on any stone wall (map rock, bent wall, thrown rock) within range + 4 cells → that rock breaks off and is thrown from there: +25 % range, +20 % damage. Without a rock: thrown from the body as before. |
| [X-173] | Ally redirect | A lightning bolt / wave touching an ALLY (not the caster) with Redirect active is redirected by that ally — ally → ally → enemy chains. |
| [X-174] | Knowledge Seeker | The buff camp spirit is a Knowledge Seeker (Wan Shi Tong's fox), buff "Wan Shi Tong's Wisdom". No koi spirits in fights (Tui / La must never die). |
| [X-175] | Boulder hitbox | Resting and flying boulders (lifted stones) are collected per tick (`rock_bodies_`) and block walking like walls (kCollWall); flying units pass. |
| [X-176] | Buff camps moved | Buff camps at (150,300) / (174,442) + mirror: ≈ 40 cells from any road (checked with a tool against the generated map). |
| [X-177] | Metal walls v2 | Metal: 1 segment (100 × 33) and 600 HP by default; lightning within 192 units turns ≈ 5° per tick towards the nearest metal cell; where it strikes, everything within 33 units of the cell is hit. |
| [X-178] | Thin passes | A* accepts cells one short of the body clearance when the body circle fits there exactly (centre or half a cell aside), +50 % cost. |
| [X-179] | Wall autos | `AutoAttackDef::hits_walls` / `wall_pct`: right click on a wall → OrderKind::AttackWall, hits at the autoattack speed with wall_effect_pct. |
| [X-180] | MoveWater | SkillKind::MoveWater (source first): a puddle source moves; a river / sea cell area dries (shallow → ground, deep → shallow) for the duration; an ice wall melts; the water lies at the target as a puddle (dur_ms). |
| [X-181] | Companions | CompanionKind × 6; recall (default) spawns the animal at the fountain, 1 000 u/s to the rider, then home; `riding` blocks commands except ping / emote / settings; dies → the companion leaves. `MatchConfig::classic_recall` = the old 6 s channel. |
| [X-182] | Bounties | 300 + 100 per kill in a row after the first (max 1 000), −50 per death in a row (min 100); assisters share 50 %; kill XP 140 + 30 × level split between killer, assisters and allies within 1 000. |
| [X-183] | Earth Qs | Lift and Throw (Boulder, casts 2, lift beside the caster), Raise Stone (Boulder, one cast: only raises, one stone per caster), Stone Push (StonePush: the chosen / nearest resting stone flies with this skill). Pure earthbenders get these 3 as their Q options. |
| [X-184] | Mouse control | SkillDef::control 1 = steer (Steer commands 20 / s while the key is held; turn steer_turn per tick), 2 = draw (DrawPoint path ≤ 40 points, segments every 100 units along it, within range). |
| [X-185] | Own champions v2 | Roster cap 127 (int8 on the wire), reserved → stable pointers. `.champ` keys for every skill field, champion element / role / resource, Q options, autoattack shapes; validated; BOM / size limits; server files in custom_champions/_server. |
| [X-186] | Drawing order | Terrain rows are drawn between the units by screen depth; bars / names / CC timers in a second pass; effects + companions on the 100 ms delayed timeline; sprites on whole pixels; fog fades alpha only. |
| [X-187] | Interactions | game/reference/interactions.cpp builds the rule list from the tables; Menu → Interactions; `dump_content --interactions` → docs/INTERACTIONS.md. |
| [X-188] | Rule sets | MatchConfig::ruleset (server option + practice option): Elemental (default) / LoL-like / Dota-like. Classic sets: lifesteal (autos), omnivamp (all damage), grievous wounds (StatusKind::Grievous, −value % healing, 3 s, from `grievous_on_hit`), classic-only items (shop hides them otherwise, buying refused); Dota-like also allows denying own minions under 50 % HP (no gold, enemies near get half XP). |
| [X-189] | Passive procs | SkillDef::proc on the passive: 1 next autoattack hit, 2 a skill hit, 3 every proc_cd_ms, 4 HP under proc_hp_pct; damage proc_dmg + proc_pct_max_hp, heal proc_heal_pct of it + proc_heal_max_hp, shield, CC; own cooldown (Unit::proc_ready_ms). Works in every rule set; `.champ` keys p.proc… |
| [X-190] | Own emotes | custom_emotes/<name>/emote.png (≤ 256², 96 KB) + sound.wav (≤ 256 KB); first 4 in the wheel (slots 6–9); uploaded on lock-in (Msg::EmoteUpload, PNG header checked), passed on at match start only with ServerConfig::allow_own_emotes; shown only with Settings::show_other_emotes. |
| [X-191] | Setups v2 | Setups hold classic runes (9 marks / seals / glyphs + 3 quints), masteries 30/30/30 (5 tiers, 4 points per tier), a build (≤ 12 steps with texts) and a levelling order; editor with tabs; auto-buy / auto-level (client, off by default, Options). |
| [X-192] | FL Studio sounds | `fl-wine.sh FL64.exe /R /Ewav <project>` renders <project>.wav (tested); FL stays open → the prefix is closed after each render; refuses while FL is open. Projects need an instrument (a MIDI file / the empty template render silence). tools/art/fl_sounds.py renders tools/art/fl_sounds/*.flp and (with PyFLP) derives element / shape variants. |
| [X-193] | Maps | MapDef (game/match/maps.hpp): generator, bases, lanes = minion paths, structures (t3 optional), camps, epics (+ island flag), portals, minion waves, team size, own pack (resourcepacks/map_<id>, tried before the default). Elemental World (5 v 5) + Foggy Swamp (3 v 3, two lanes, swamp pools, banyan root ring, Swamp Monster). The Welcome / Lobby messages carry the map; clients rebuild the grid. |
| [X-194] | Lobby | Owner = host (hosted) or first player (dedicated); LobbyConfig (map, systems, mode), TransferOwner, leave, invite link em-join://host:port (LAN address via getifaddrs). Queue mode (dedicated): map + game mode queues, queue_min players pop into random teams, others wait. |
| [X-195] | Systems + modes | One unified game: vamp / healing reduction always on; "LoL-like only" = no denies. Game modes: Classic, Blind Pick (enemy picks hidden per player), All Random. Bots: random free champions. |
| [X-196] | Charges | SkillDef charges / charge_ms / charge_gap_ms; AbilityRt charges_used + charge_next_ms; HUD badge + refill ring. |
| [X-197] | Sensed targets | Unit::sensed_until[team] set by seismic / heat / sound waves (0.6–0.9 s); targeted skills with sensed_target (−1 = with a sense passive) may target them; a click on a fresh wave sends target 0 + the point. |
| [X-198] | Nothing to act on | World::nothing_to_act_on: Heat Draw (lava / steam / ice near), Ice Path (water near), Water Spout, Stone Push (a lifted stone in range), MoveWater (a source or carried water) — refused before cost / cooldown. |
| [X-199] | Ground shaping | 0.3 s self-root (self_root_ms) for pillars / walls / lifts / pushes; pillars skip cells under the caster's team; elevation layers add up with own timers; regrown terrain pushes units out. Own bending shows at once on the own team's map (CellChange.pub bit 2/4). |
| [X-200] | Drawing | Terrain rows between units; wall cracks drawn on the wall; segments sent as NetWallSeg and drawn as rotated blocks; map rocks as boulders; cooldown = clockwise sweep + ready flash; edge panning only with window focus (the camera corner bug). |
| [X-201] | Audio | Soft limiter (knee 0.6), music ducking under many effects, 1 s loop crossfade, music gain 0.55. |
| [X-202] | FL Studio | tools/art/flp.py reads / writes .flp (event stream); fl_make_sounds.py builds a project from FL's Basic template (808 samplers) + 3x Osc channels transplanted from the Electro template, writes note recipes into pattern 1 (pattern ids start at 1), renders with FL64.exe /R /Ewav through ~/Wine/FlStudio/fl-wine.sh and installs the trimmed WAVs. No PyFLP needed. Big templates do not finish rendering (a dialog / plugin load). |
