# Time and sky — exact timings

Code: `src/core/world_clock.hpp`. Print the table: `./build/sky_timeline`.

## Base rule [U-30] [U-31]

- 1 real second = 12 game minutes. 1 real minute = 12 game hours.
- 1 game day = **2:00 real**. [U-515] The game starts at **18:00 game time, the beginning of the night**.
- Real 0:00 = 18:00, 0:30 = midnight, 1:00 = 06:00, 1:30 = noon, 2:00 = 18:00 again.
- Day time = 06:00–18:00 game time → real `1:00–2:00` of every 2-minute cycle.

## Moon [U-32] [U-515]

- Cycle 30 game days = **60:00 real**.
- **The exact new moon is at 25:00 real.** Full moon: 55:00–57:00. The match starts with a waning moon (about 83 %).
- Illumination % (triangle, integer): 0 % at new moon → 100 % at full moon → 0 %.

## Events

| Event | Real time | Source |
|-------|-----------|--------|
| New moon + solar eclipse | 25:00–26:00, darkest at noon 25:30; repeats at every new moon (85:00) | [U-35][U-515] |
| **Comet** | 29:00 | [U-34][U-515] |
| Full moon | 55:00–57:00 | [U-32][U-515] |
| Planetary alignment (3 game days) | **60:00–66:00** | [U-33][U-128] |

## Event effects (code: `src/core/sky_effects.hpp`) [U-122..U-126]

| Event | Who | Power | Sizes | XP | Other |
|-------|-----|-------|-------|----|-------|
| Comet from 29:00 | fire | +75 % | +40 % | +40 % | strongest event |
| Full moon 55:00–57:00 | water | +50 % | +25 % | +25 % | bloodbending allowed |
| Alignment 60:00–66:00 | all benders | +25 % | +10 % | — | |
| Eclipse 25:00–26:00 | fire | 100 → 0 → 100 % (linear V) | — | — | muted 6 s in the middle |
| Day / night | fire / water | +5 % | — | — | |
| New moon | water | −10 % | — | — | |

Eclipse "only during day time" [U-35] → the eclipse covers exactly the day-time
hour window of that game day (it ends at sunset 18:00 game time).

## Fuel for firebenders [U-138][U-139]

- Sun height: fuel regen +0 % at 06:00 → **+30 % at noon** → +0 % at 18:00; nothing at night / in an eclipse [X-76].
- Comet window 25:00–29:00: **unlimited fuel** (skills cost nothing, bar stays full) [X-77].
