# Default map layout

Code: `src/map/default_map.cpp`. Image: `./build/map_preview build/map.ppm`.

## Grid [U-90]

- 512 × 512 cells, 1 cell = 32 game units, map = 16 384 units.
- Coordinates: `x` → right, `y` → down. Cell `(0,0)` = top-left.

## Symmetry [U-83]

- **Point symmetry** around the centre: cell `(x, y)` = cell `(511−x, 511−y)`.
- Code builds only the Fire half and mirrors it → both halves are always identical.
- No elevation anywhere (all cells level 0).

## Zones

| Zone | Where (Fire-side numbers) | Surface | Tag |
|------|---------------------------|---------|-----|
| Fire Nation base (Team A) | circle around (40, 471), bottom-left | stone r<38 · dryland r<66 · sand (desert) r<92 | [U-06][U-87] |
| Earth Nation base (Team B) | mirror, top-right | same | [U-06][U-87] |
| Sea | band along the diagonal `x = y` (top-left ↔ bottom-right), half-width 40 | deep water, not walkable | [U-85][?-08] |
| Beaches | 5 cells around the sea | sand | [X-42] |
| Mid land bridge | mid lane crossing the sea in the centre | land (tropical) | [U-86] |
| North pole | circle r=66 around (28, 28) — top lane corner | glacier | [U-84][?-07] |
| South pole | mirror, bottom-right — bot lane corner | glacier | [U-84] |
| Lanes | top: left edge + top edge · mid: diagonal base→base · bot: bottom edge + right edge; half-width 11 | land | [X-07] |
| Jungle | everything else | land + tree groves + bushes | [U-89] |
| Water sources | 3 ponds per side, r=8 | shallow water | [U-89] |
| Rivers | jungle pond → base, jungle pond → sea | shallow water | [U-87][U-88][?-14] |

## Why the poles sit on the lane corners

The sea runs along the diagonal, so it reaches the top-left and bottom-right
corners — exactly where top and bot lanes turn. The sea is **frozen** there
(pole = glacier), so the lane crosses the sea on ice. Mid crosses it on the
land bridge. Each lane has its own way over the sea.

```
 (0,0) N-pole ─────── top lane ──────── EARTH
   │ ❄  ~~                              BASE
   │       ~~  sea         jungle         │
 top          ~~                          bot
 lane   jungle   ~~  ═mid bridge═         lane
   │               ~~                     │
 FIRE     jungle       ~~   jungle        │
 BASE                     ~~           ❄  │
   └─────────── bot lane ─────────── S-pole (511,511)
```
