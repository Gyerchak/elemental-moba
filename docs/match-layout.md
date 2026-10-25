# Match layout

Code: `src/game/layout.hpp` (cells, Fire Nation side). Earth Nation = point mirror (511−x, 511−y) [U-83].
Point symmetry maps Fire's **top** lane onto Earth's **bot** lane, so Earth's structures in lane L
are the mirror of Fire's structures in lane 2−L.

| Thing | Fire cell | Tag |
|-------|-----------|-----|
| Base centre (shop zone, r 40 cells) | (40, 471) | [U-06][?-01] |
| Fountain (spawn + heal) | (18, 493) | [X-12] |
| Nation Shrine (nexus) | (46, 465) | [X-07] |
| Nexus turrets | (60, 458), (53, 451) | [X-07] |
| Top: tier 1 / 2 / 3 / inhibitor | (28,175) (28,290) (30,385) (32,420) | [X-07][U-84] |
| Mid | (196,315) (150,361) (104,407) (80,431) | [X-07] |
| Bot | (336,483) (221,483) (126,483) (91,480) | [X-07] |
| North pole = spirit portal (V, 4 s → south pole) | (28, 28) | [U-84][X-54] |
| Spirit island + Face Stealer | centre (256, 256), r 30 | [X-55][X-86] |
| Jungle camps (5 per side) | (110,300) (150,410) (230,430) (80,360) (190,360) | [U-82] |

Structure order: tier 1 → tier 2 → tier 3 → inhibitor → nexus turrets (after any inhibitor) → Shrine.
Destroy the enemy Nation Shrine to win [X-12].
