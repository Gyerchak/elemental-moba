# PLAN — elemental-moba

One milestone at a time. Every task has its tag: `[U-xx]` = user plan,
`[X-xx]` = Claude idea (see `ENGINE-EXTRAS.md`).
Status: `[x]` done · `[~]` started / simplified · `[ ]` todo.

**State 2026-10-24: a full match is playable** — practice vs 9 bots, host / join over the
network, dedicated server, 128 Hz. Placeholder graphics (coloured circles). Built and run with
clang 22 and GCC 16 (`README.md` → Build / Play).

## M0 — Core data + placeholder map ✔

- [x] Project folder, sources in `zrodla/`, SPEC / EXTRAS / QUESTIONS.
- [x] 32-bit cell + compile-time checks [U-94]; bush field [X-30]; 512×512 grid, step rule [U-90][U-93].
- [x] World clock, moon, eclipses, comet, alignment at 60:00 [U-30..U-35][U-128].
- [x] Night vision, new moon ≈ 0 [U-108][U-115]; light layer + Sky Lantern [U-116..U-118][X-60..X-62].
- [x] Sky buffs, eclipse V-curve + 6 s fire mute [U-107][U-122..U-126]; tenacity / CC amplify [U-127][X-68].
- [x] Element rules, stats, damage, resources, slots, scaling, kit check, XP curve [U-15..U-28][U-39..U-64][U-104..U-121].
- [x] Default map generator + preview [U-83..U-89].
- [x] First real compile (2026-10-24): fixed a clang constexpr order problem in `Cell` and an `isqrt` loop.

## M1 — Simulation loop ✔

- [x] Fixed 128 Hz tick, integer fixed-point positions (Q24.8) [U-102][X-04][X-05].
- [x] Own minimal ECS: unit + effect arrays [X-06][X-83].
- [x] Movement with turn rate [U-74][X-88], A* + string pulling, re-path when walls appear [X-09].
- [x] Status system for all CC through `cc_duration_ms` [U-36][U-59][U-127][X-27]; eclipse mute [U-125].
- [x] **Hitboxes from the 2026-10-23 notes:** model size stat [U-129], body-shaped hitbox moving with the walk [U-130][X-69], fluid skill shapes [U-131][X-70], one touch = hit [U-132][X-71], edge-to-edge autoattack range [U-133].

## M2 — Placeholder renderer ✔

- [x] SDL3 window, top-down camera, coloured circle bodies [U-03][X-17] (SDL 2D renderer, not raw GL).
- [x] Grid map, fog of war per team [U-75][X-10], night darkness + light layer [U-25][U-117].
- [x] Exact skill edges in effects and indicators [U-131]; sense waves drawn [U-141..U-143].
- [x] Debug: tick rate, ping, fps on the HUD; **Alt** = cell bits under the cursor [X-25].

## M3 — Combat ✔

- [x] Right click attack / move, elemental autoattacks [U-41][U-55][U-56][U-73].
- [x] Projectiles, skillshots, 1 point-click (bloodbending), multi-hit fields, multicast [U-37][U-106][U-109].
- [x] Projectile blocking: water counters fire, earth stops air / fire, partly water [U-17][U-20][U-38].
- [x] Cast time, rooted / free / slowed casting, S = stop + cancel [U-42][U-53][?-04].
- [~] Space = switch stance [U-72] — built for the non-bender (fists ↔ knives); other champions have no stance yet.
- [x] Elemental combos + group bending [U-134][U-135][X-72][X-73].

## M4 — Terrain bending ✔ (mostly)

- [x] Stone / ice walls with collapse time, melting [U-15][U-16][U-18][U-24][X-38].
- [x] Elevation change (pillars, 2 levels = unwalkable) [U-45].
- [x] Burning trees / bushes + regrowth [U-14][X-91]; lava → stone wall [X-56]; steam fog [X-57]; lightning on water [X-58].
- [x] Flying (air Flight, fire jets) [U-51][U-77]; seismic sense can't feel flyers [U-142].
- [x] Heavy stone: raise → push, a 2nd earthbender can push it [U-27].
- [~] Water resource from the environment (counts water cells, ice = unlimited) [U-26][U-60][X-89]; no temperature layer [X-26].
- [ ] Underground travel [U-50]; water / ice elevation [U-49]; icy surface sliding [U-52].

## M5 — Match structure ✔

- [x] 3 lanes, turrets 3 tiers, inhibitors, nexus turrets, Nation Shrine [X-07]; waves from 1:05 every 30 s, siege, super minions [X-08].
- [x] Earth minions clear walls (1.5 s), fire minions blow them up (2.5 s), never stuck > 3 s [U-12][U-13].
- [x] Gold, XP, levels (no cap), sky XP boosts, per-level stats, respawn, win [U-111][U-122][X-11][X-12]. Measured pace: level ~10 at 15:00, ~18 at 33:00 (target X-64).
- [x] Jungle spirits / animals [U-82]; Face Stealer boss on the spirit island [X-55][X-86]; spirit portals at the poles [X-54].

## M6 — Champions and items

- [~] Data-driven content: champions / skills / items are data tables in C++ (`game/content.cpp`, `core/items.hpp`), not text files yet [X-13]. Roster of 10 [X-59].
- [x] Q picked in champion select [U-11]; F/G special skills per element incl. sandbending for air [U-71][U-76..U-79][U-137].
- [x] Items, element-defensive items, size / speed / cast items, Spiritual Water Flask [U-80][U-81][U-113][U-136][X-46][X-74].
- [x] Skill points in the HUD (+ buttons, Ctrl+key), T at 9 / 18 / 27 [U-111][U-120].
- [x] Fuel by sun height, unlimited fuel in the comet [U-138][U-139].
- [ ] Avatar / Dark Avatar / unique character variants [U-07]; unique champion per game is done [U-08].

## M7 — Network ✔ (prediction later)

- [x] enet UDP, server-authoritative, snapshots filtered by team vision, interpolation [X-01][X-02][X-24][X-93].
- [ ] Client-side prediction of own movement, lag compensation [X-02][X-03].
- [x] Host match / host server, tick rate setting (default 128 Hz) [U-102][U-103]; headless `--server`.
- [~] Lobby + champion select (teams, unique picks, ready, host starts, bots fill) [U-10]; no bans / timer [X-29][?-29].
- [x] Up to 32 slots [U-09] (tested with 2 clients + 8 bots); reconnect by nickname, bot plays meanwhile [X-22].

## M8 — Menu and versions

- [~] Version picker + auto-fallback on join [U-95][X-14] — one built-in version (`official-0.1`); custom snapshots not yet.
- [ ] Per-version sets: runes, builds, skins, special abilities [U-96].
- [~] Resource packs + skin display rules [U-97][U-98] — the settings toggles exist, no skins yet.
- [x] Settings (nickname, quick cast, nicknames, colourblind, minimap, chat) [U-99]; sound / voice not built.
- [x] Single player practice tool + bots; F9 no cooldowns, F10 +1000 gold, F11 level up [U-100][X-21].

## M10 — Hubert's test round 1 (2026-10-24) — built, waiting for Hubert's test

- [x] Keyboard: text input only while typing (an IME swallowed every key); keys by keycode or position; on-screen key counter.
- [x] Reason shown when an order does nothing; floating damage numbers + autoattack slash [X-103]; auto-acquire [X-99].
- [x] Collision layers on body borders: terrain, trees, walls, structures, units [U-144..U-146][X-94..X-96].
- [x] Aggressive jungle camps [U-147][X-97]; A / D attack-moves [U-148][X-98]; shop range [U-149][X-100].
- [x] Vision from the eyes (210° cone, 160 behind) [U-150][X-101]; seeing into bushes needs the eyes inside [U-152][X-104].
- [x] Sound waves from outside vision [U-151][X-102]. Camera free by default, Y lock, F1 centre.
- [ ] Not run by Claude (Hubert tests himself).

## M11 — Test round 2 (2026-10-24) — built, waiting for Hubert's test

- [x] Ping fix: snapshots as unreliable fragments [X-105]; client network thread [X-125].
- [x] Code in folders: champions / skills / mobs / mechanics / match / ai / loadout / reference [U-172][U-215].
- [x] Smooth day light [U-153][X-106]; monster leash [U-154][X-107]; shields on bars [U-155]; calmer minions [U-156]; 8 slots [U-157].
- [x] Autoattack switch H [U-158]; gold / XP pop-ups [U-159]; blind rules [U-160]; info panel [U-161]; X champions only [U-162].
- [x] Quick cast per skill [U-163]; both buttons = camera [U-164]; key bindings saved in settings.cfg [U-165].
- [x] LoL-like bars with 100 / 1000 marks [U-166]; buffs with timers [U-167]; skill tooltips normal / Alt / + [U-168]; HUD icons [U-169][U-170]; portraits [U-171].
- [x] Your team always bottom-left [U-174]; tilted game view + developer view [U-175]; hitbox shadow 10 % [U-176].

## M12 — Test round 3 (2026-10-24) — built, waiting for Hubert's test

- [x] Runes, specialisation trees, keystones + saved setups + per-champion defaults [U-177][U-178][X-113].
- [x] Post-game statistics with keystone impact [U-179]; LoL-like champion select with filters / search / favourites [U-180].
- [x] Ban phase (off by default) [U-181]; chats: lobby, team, all, whisper / friends [U-182]. [~] Friend list = names you add with /friend; no online list yet.
- [x] Q options and specials in skill pools [U-183]; skill switches: turning, walls, aim [U-184]; lightning redirection [U-185]; projectile or not [U-186].
- [x] Champion wiki screen [U-187]; water: swimming, Ice Path, Water Spout, swim weakness, height rules [U-188..U-191][X-115][X-116].
- [x] Damage numbers [U-192]; CC timers [U-193]; LoL-2016 minions [U-194][X-114]; XP rules [U-195]; minion pathing around structures [U-196].
- [~] Resource packs: folders, default pack generator, icons / splashes / sounds / music used [U-197]. Ground and model textures not used yet.
- [~] Placeholder 3D models (cylinders) + animation cues for every slot; developer view [U-198][U-226]. Real models: with the 3D renderer.
- [x] Loading screen [U-199]; Minecraft-like skin upload [U-200]; volumes + positional sounds in hearing range [U-201].
- [x] Tree / bush / wall models [U-202]; path lines [U-203]; smooth fog [U-204]; info panel top-left on click [U-205]; time top-right [U-206].
- [x] True fullscreen + mouse kept inside + keyboard grab in a fullscreen match; Esc = live game menu [U-207][U-233].
- [x] Options: 7 pages [U-208]; item impact in the item tooltip [U-209].
- [~] Own champions: files, validation, practice roster [U-210]; servers: every player needs the same files [?-33].
- [x] Server sends numbers only [U-211]; Minecraft-like server list / direct connect / easy hosting [U-212]; platform restriction [U-213].
- [ ] Windows / macOS builds: the code is portable (SDL3, enet, C++23) but not built or tested on them here [U-213].
- [~] Vulkan: SDL3's Vulkan renderer backend [U-214][?-34]; a real 3D renderer later.
- [x] Height reach [U-216]; autoattack shapes + attack while moving (mechanic only) [U-217]; slow resist [U-218]; stacking rules [U-219].
- [x] LoL-like shop with icons, categories, sorting, tooltip after click, price at the bottom; menu item browser + own builds; no recommended items [U-220][U-236][U-245].
- [x] No crits; flashy big hits; sweet spots; damage colours per element [U-221..U-223][X-117].

## M13 — Test round 4 (2026-10-24) — built, waiting for Hubert's test

- [x] Jet Propulsion / Updraft names, Sky Ascension (rare free flight with height) [U-224]; air use rules per skill [U-249].
- [~] Environment bending: Earthshape, Iceberg, Heat Draw, air blows clouds away [U-225]. Changes are temporary (15–20 s).
- [x] Trees regrow after 1 game day; lava burns trees; "destroys trees" / "burns ground" per skill [U-227][U-244].
- [x] Fire in Hand, lanterns for everyone else, fire light while casting [U-228][U-244][X-120].
- [x] Teleports only on purpose; blinks are very fast dashes [U-229].
- [x] LoL_ / Dota_ reference mechanics (unused) [U-230].
- [x] Space / F1–F12 / Ctrl+Space [U-231]; Alt self-cast [U-243]; pings + bait + wheel, emotes Ctrl+1–6 [U-246][X-123].
- [x] Sight table for every unit kind [U-234]; falcons with ghost replays [U-235][X-121].
- [x] Comet red / moon white light; modifiers under bars and in the info panel [U-237]; C = all stats, loadout page [U-238].
- [~] Languages: English + Polish for the interface [U-239]. Champion / skill / item names are still English.
- [x] Cosmic timing checked → matches the plan [U-240][?-31].
- [x] Wet + lightning bonuses, water cools lava → steam, smoke, air empowers fire, fire clashes [U-241][X-119][X-124].
- [~] Stacking framework (rules + text) [U-242]; each effect uses its own rule, not yet chosen per effect by data.
- [x] Penetration order like LoL (no reduction stats in the game yet) [U-248]; Snow Squall [U-247] — [ ] air bending snow clouds from the map.
- [ ] Not tested by Claude (Hubert tests himself).

## M14 — Test round 5 (2026-10-25) — built (clang + GCC), waiting for Hubert's test

- [x] Space centres (hold = stays), old settings migrated; Z cancels casts [U-251][U-265].
- [x] Pathfinding uses the body size (shadowed variable bug) [U-252][X-127].
- [x] Real font text, Polish letters, cut long text [U-250][X-128]; game finds its data folder [X-129].
- [x] Modifiers under bars / panel / my row with names and numbers; sky box with chips + next event; sky banners [U-253][U-255][X-130].
- [x] Burns through shields with numbers; damage tags [U-254][X-131]; death recap window with graphs [U-262][X-132].
- [x] Announcements + events window + announcer voice EN (Piper) / PL (espeak) [U-263][X-133].
- [x] Item build paths, 18 new items, tiers in the shop, bots build items [U-261][X-134].
- [x] ComfyUI art: 220 icons / splashes / textures / screens; textured ground [U-257].
- [x] Blender placeholder models → animated sprite sheets (8 directions), trees, bushes, structures [U-258].
- [x] Burning (red glow + flames) and frozen (iced) looks [U-260]; placeholder sounds per skill shape + element [U-259].
- [x] Light bugs [U-256][X-140]; comet 30:00, alignment rules [U-264][U-266]; surfaces + bending [U-269].
- [x] Calm flame / rage [U-268]; 3 Q per champion [U-270]; ult step per champion [U-271]; Wind Sense passive [U-274].
- [x] Avatar Kyren + pick rules [U-279]; non-bender magic stances [U-264].
- [x] Pick timer, MMR / LP / tiers / ranking / matchmaking [U-276]; custom champions sent by the server [U-277].
- [x] Compression + lighter light updates [U-280].
- [ ] Dark Avatar champion [?-37]. [ ] A real 3D renderer (the .glb files are ready). [ ] Not run by Claude (Hubert tests).

## M15 — Round 5 fixes + test content (2026-10-25)

- [x] Crash at match start fixed [U-281]; champion select never overflows [U-282]; Avatar filters [U-283]; no menu notices [U-284].
- [x] Own game versions work (versions/<name>/version.txt) [U-95][X-143]; test version `balance-test`.
- [x] Test own champion `Sky Monk Tenzo` + test pack `hubert-test` (his pictures + model, Ember Monk "comet" skin with model) [U-285][X-144].

## M16 — Performance, walls, sixth skill (2026-10-25)

- [x] Wet vs fire [U-286]. Profiler + pathfinding budget: worst tick 42 → 7 ms [U-287].
- [x] Terrain changes by time public; walls with HP, effectiveness, wall items, metal walls, walls as sources, cracks [U-288].
- [x] Skills Q W E R T Y + specials G H, keys moved, shop O, attack range on `, X switch [U-289].
- [x] Fog fade (no stepping edges); champion select / HUD fit 1024 px [U-290].
- [ ] Moving whole walls by bending [?-41].

## M17 — Ultimate on Y, rank rules per skill (2026-10-25)

- [x] Y = ultimate (6 / 12 / 18 / every 6), others 2 / 3 / 4 …; all per skill + per champion; 0–2 ultimates; skill passives; tooltips + ally info show it [U-291].
- [x] Shop on I; settings v4.
- [x] Heat Draw turns steam into puddles; wet while standing on ground water [U-292].
- [x] Ice through steam condenses it; source form water / ice (K); ice patches; ice walls need a big water source [U-293][U-294].

## M9 — Extras

- [x] Text chat (Enter) [X-23].
- [ ] Replays, spectator [X-19][X-20]; audio + sound fog [X-18]; ping wheel / voice [X-23]; PL/EN [X-28].
