# Time and sky — exact timings

Code: `src/core/world_clock.hpp`. Print the table: `./build/sky_timeline`.

## Base rule [U-30] [U-31]

- 1 real second = 12 game minutes. 1 real minute = 12 game hours.
- 1 game day = **2:00 real**. Game starts at **00:00 game time (midnight)** at **new moon**.
- Day time = 06:00–18:00 game time → real `0:30–1:30` of every 2-minute cycle.
- Night = the rest. So the match starts with 30 s of night.

## Moon [U-32]

- Cycle 30 game days = **60:00 real**. Moon age (days) = `game day mod 30`.
- New moon: day 0 (00:00–2:00). **Full moon: day 15 (30:00–32:00).**
- Illumination % (triangle, integer): 0 % at new moon → 100 % at full moon → 0 %.

## One-time events

| Event | Real time | Game day | Source |
|-------|-----------|----------|--------|
| Solar eclipse #1 | 24:30–25:30 (day time of day 12) | 12 | [U-35] |
| **Comet** impact | 25:00 (noon of day 12) | 12 | [U-34], buff idea [X-50] |
| Full moon | 30:00–32:00 | 15 | [U-32], buff idea [X-51] |
| Solar eclipse #2 | 34:30–35:30 (day time of day 17) | 17 | [U-35] |
| Planetary alignment (3 game days) | **60:00–66:00** (starts at the 2nd new moon, night) | 30–32 | [U-33][U-128] |

## Event effects (code: `src/core/sky_effects.hpp`) [U-122..U-126]

| Event | Who | Power | Sizes | XP | Other |
|-------|-----|-------|-------|----|-------|
| Comet 25:00–29:00 | fire | +75 % | +40 % | +40 % | strongest event |
| Full moon 30:00–32:00 | water | +50 % | +25 % | +25 % | bloodbending allowed |
| Alignment 60:00–66:00 | all benders | +25 % | +10 % | — | |
| Eclipse 24:30–25:30, 34:30–35:30 | fire | 100 → 0 → 100 % (linear V) | — | — | muted 6 s in the middle |
| Day / night | fire / water | +5 % | — | — | |
| New moon | water | −10 % | — | — | |

Eclipse "only during day time" [U-35] → the eclipse covers exactly the day-time
hour window of that game day (it ends at sunset 18:00 game time).

## Fuel for firebenders [U-138][U-139]

- Sun height: fuel regen +0 % at 06:00 → **+30 % at noon** → +0 % at 18:00; nothing at night / in an eclipse [X-76].
- Comet window 25:00–29:00: **unlimited fuel** (skills cost nothing, bar stays full) [X-77].
