# elemental-moba

Open-source MOBA about elemental bending (Avatar: Aang / Korra / Seven Heavens
inspired). **C++23 only.** Goal: lowest latency, most stable, fully optimised.

## Tag legend (read this first)

Every item in every file has exactly one tag:

| Tag      | Meaning                                                        |
|----------|----------------------------------------------------------------|
| `[U-xx]` | **User plan** — comes from Hubert's files in `zrodla/`.        |
| `[X-xx]` | **Claude idea — outside user plan.** Added because a MOBA engine needs it, or a creative suggestion. Accepted by Hubert (2026-10-23); can still be changed or deleted. |
| `[?-xx]` | **Open question** — the plan is unclear or contradicts itself; the assumption used is a Claude idea. |

In code the same tags appear as comments, e.g. `// [X-07]`.
Search everything outside the plan: `grep -rn "\[X-" .`

## Files

| File                     | What                                                  |
|--------------------------|-------------------------------------------------------|
| `zrodla/`                | Original idea files (copied, never edited).           |
| `SPEC.md`                | User plan, cleaned up and numbered `[U-xx]`.          |
| `ENGINE-EXTRAS.md`       | Everything added outside the plan `[X-xx]`.           |
| `OPEN-QUESTIONS.md`      | Unclear points `[?-xx]` + the assumption used.        |
| `PLAN.md`                | Milestones M0…M22, every task tagged U / X.           |
| `docs/champion-file.md`  | Build own champions: keys, shapes, sweet spots, standard skills, stats, LoL / Dota 2 / own mechanics. |
| `docs/INTERACTIONS.md`   | Every element / terrain interaction (generated).      |
| `docs/map-layout.md`     | Default map: zones, coordinates, symmetry.            |
| `docs/time-and-sky.md`   | Day/night, moon, eclipses, comet — exact timings.     |
| `CMakeLists.txt`, `src/`, `tools/` | C++23 game (SDL3 + enet). Mechanics by origin: `src/game/mechanics/lol.cpp`, `dota.cpp`, `bending_cc.cpp`. |

## Build

Needs (already on this PC): CMake, Ninja, clang++ or g++ (C++23), `sdl3`, `sdl3_ttf`, `enet`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Play

```sh
./build/elemental_moba                      # menu: practice, join, host match, host server, settings
./build/elemental_moba --server --port 7777 # dedicated headless server, 128 Hz, pick timer 90 s, ranked
#   options: --pick-seconds N (0 = off) · --unranked (no MMR / LP) · --no-matchmaking
```

The game may be started from any folder (also from inside `build/`): it finds `resourcepacks/` and `lang/` itself.

- **Practice:** menu → *Single player* → pick champion, Q option, F/G specials → *Start*. Bots fill 5 v 5.
- **LAN / internet:** one player → *Host match* (port 7777, UDP); others → *Multiplayer: join* → IP.

### Controls (all keys can be changed in Options → Controls)

| Key | Action |
|-----|--------|
| Right click | move / attack (range is edge to edge [U-133]) |
| Q W E R T Y, G H | skills (Y = ultimate: level 6 / 12 / 18 …; G H = the two specials): shows the exact shape, left click casts (Options → Quick cast) [U-289] |
| Ctrl + Q…Y, or the **+** buttons | learn / rank up a skill (skills: max rank 2 before level 6, 3 before 12 …; ultimate Y at 6 / 12 / 18 …) |
| Alt + skill / item key | cast on yourself [U-243] |
| A | attack-move to the cursor, **closest champion only** [U-148] |
| D | attack-move to the cursor, anything |
| X (switch) | target champions only (right click + targeted skills) [U-162] |
| J | autoattack on / off [U-158][U-289] |
| K | waterbenders: water from puddles / steam comes as **water** or **ice** (switch) [U-293] |
| ` (hold) | my autoattack range circle · **Ctrl + `** = always on / off [U-289] |
| S | stop moving / attacking (a cast or channel goes on) [U-265] |
| Z | cancel the cast / channel (recall, portal) [U-265] |
| Space | camera on me; **hold** = the camera stays on me · **Ctrl+Space** switch stance [U-231][U-251] |
| F1 … F12 | camera on teammate 1 … 12 |
| U | camera lock · mouse wheel zoom · both mouse buttons = camera on me |
| B | recall (8 s) · **I** shop (buy inside the gold ring at your fountain) · **1–8** item actives |
| V | spirit portal (stand on a pole) |
| C | all statistics (mine + the clicked unit) [U-238] |
| Tab | scoreboard · **events ▾** (top right) = all announcements of the match |
| After dying | **Death recap ▸** button = who hit you with what (graphs) [U-262] |
| Left click on a unit | its info panel (top left) |
| Ctrl + left click, drag | ping wheel: danger, enemy missing, on my way, assist me, bait [U-246] |
| Ctrl + 1 … 6 | emotes: laugh, taunt, dance, joke, wave, cheer |
| Enter | team chat · `/all text` everyone · `/w name text` a friend · `/friend name` add a friend |
| Esc | game menu with live options (the match keeps running) |
| Ctrl+Shift+N / G / L | practice only: no cooldowns / +1000 gold / level up |

Start options: `--windowed` (a window instead of fullscreen), `--server` (dedicated server).
Languages: `lang/<code>.txt` (Polish included). Resource packs: `resourcepacks/<name>/`
(the placeholder default pack is made by `./build/make_default_pack`). Own champions: `custom_champions/*.champ`.

### Art, models and sounds (all placeholders, made on this PC)

```sh
python3 tools/art/gen_art.py --comfy-out <ComfyUI output folder>   # ComfyUI must run; skips existing pictures
python3 tools/art/install_art.py                                    # art/raw → resourcepacks/default (PNG)
python3 tools/art/make_models.py                                    # Blender → resourcepacks/default/models/<id>/sheet.png + model.glb
python3 tools/art/make_sounds.py                                    # effects (numpy) + announcer EN (piper-tts) / PL (espeak-ng)
```

### Own content (tests made for Hubert, 2026-10-25)

- **Own champion:** `custom_champions/sky_monk_tenzo.champ` → Menu → *Own champions* → enable *Sky Monk Tenzo*.
- **Resource pack:** `resourcepacks/hubert-test/` → Options → Graphics → pack *hubert-test*: Tenzo's pictures + model, and an Ember Monk skin *comet* (pictures + model; pick it under *Skin* in champion select).
- **Own version:** `versions/balance-test/version.txt` → Menu → *Version* (click to switch). Practice / hosting then run it.

### Tools

```sh
./build/sim_run 60          # headless bot match, prints per-minute score and the winner
./build/map_preview build/map.ppm && magick build/map.ppm build/map.png
./build/sky_timeline        # day / night / moon / events table
# automated client test (offscreen screenshot):
SDL_VIDEO_DRIVER=offscreen ./build/elemental_moba --autotest 40 --shot shot.bmp --champ 0
```
