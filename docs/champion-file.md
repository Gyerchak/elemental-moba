# Own champions — the `.champ` file

A champion is **only values**. Every mechanic lives in the engine (the unified LoL + Dota + own rules).
Put the file in `custom_champions/<name>.champ`. Check it without starting the game:

    ./build/dump_content --check-champ custom_champions/<name>.champ

With ANY problem (unknown key, bad number) the game ignores the file — the checker prints the reason.

## 1. The champion

    base = Ember Monk        # copy everything from a champion first (required)
    name = Ash Monk          # new, unique
    hp = 560                 # also: attack, defence, elemental_defence, move_speed, hp_per_level, attack_per_level
    attack_range = 55        # edge to edge          attack_speed = 65   (× 100)
    colour = ff9933
    crits = 1                # [U-463] its autoattacks can crit (crit chance 0 %, crit damage 175 % — items add chance)

Model (made by `tools/art/make_models.py`): `model.body = humanoid / slim / broad`, `model.height = 2.1`,
`model.weapon = sword / staff / greatsword`, `model.extras = wings horns pauldrons glow_eyes`, `model.accent = ff2020`.

## 2. Skills: start from a standard skill, change the numbers

    w = @circle_blast        # a standard skill …
    w.name = Cinder Rain     # … then only what differs
    w.dmg = 120

| Template | What it is |
|---|---|
| `@bolt` | projectile, stops on the first enemy, the end of its flight hits harder |
| `@line` | instant straight cut from the caster |
| `@cone` | instant fan, the tip hits harder |
| `@arc` | a fan with a hole next to the caster |
| `@circle_blast` | delayed blast at a point, the centre hits harder |
| `@ring_blast` | delayed ring: the middle is safe, the outer edge hits harder |
| `@rect_slam` | long slam at a point along the aim |
| `@field` | zone that hits every 0.5 s |
| `@dash` / `@ghost_dash` | rush (ghost: through units, over walls, usable during another cast) |
| `@blink` `@shield` `@heal` `@haste` | the basics |
| `@chain` | projectile that chains: still close after 1.5 s = pulled back + hit again |
| `@transform` | a stronger form (attack %, healing %, speed, size) |
| `@three_strikes` | 3 casts in 4 s, each its own shape (line → cone → circle) |

Any existing skill works too: `w = Fire Whip`.

## 3. Every skill field: `<slot>.<field> = value` (slots p w e r t y)

- **What it does** — `kind` (Bolt Wave Burst Field Cone Dash Wall Pillar Tornado Shield Heal Haste Blink … Transform),
  `sub` (element), `dmg dmg_rank ep ad bonus_ad`, `range width radius speed`, `cast_ms cd_ms cd_rank cost`,
  `dur_ms delay_ms tick_ms`, `cc cc_ms cc_val` (Stun Root Slow Silence ChiBlock Disarm Subdue Fear …), `knock`,
  `hits = Minion SiegeMinion`. ChiBlock: `cc_val` = the elements (1 water, 2 earth, 4 fire, 8 air, add them; 0 = all).
- **Shape** [U-462] — `geo = Circle / Ring / Line / Rect / Cone / Arc` (Auto = the kind's usual shape), `inner` = the
  hole of a Ring / Arc.
- **Sweet spot** [U-461] — `sweet_pct` (+% damage), `sweet_zone = Centre / Edge / Tip`, `sweet_size` (% of the shape).
  Shown in magenta: in the preview before the cast and on the skill after it.
- **Growth per rank** [U-470] — `dmg_rank cd_rank cc_ms_rank cc_val_rank dur_ms_rank range_rank width_rank radius_rank
  knock_rank cost_rank`. The tooltip (hold Alt) shows each as base + growth × (rank − 1).
- **Multicast** — `casts recast_ms`, each cast its own shape: `stage1.kind / range / width / radius / dmg_pct /
  sweet_pct / geo / inner / sweet_zone / sweet_size` (stage1 … stage3).
- **Chains** — `tether_ms tether_radius pull`, then `cc2 cc2_ms`.
- **Switches (0 / 1)** — `free_cast` (usable during another cast), `pass_units`, `through_walls`, `aa_reset`,
  `arcane` (non-elemental magic), `melee pierce targeted ally ground`.
- **Transform** — `buff_attack_pct buff_heal_pct`, speed = `cc_val`, size = `size_bonus`; `cc` + `radius` + `hits`
  = crowd control around the caster when the form starts.
- **While learned** [U-468] — `learned.<stat> = value`, `learned_rank.<stat> = growth` (max 3 per skill).
- **Passive procs** — `proc proc_cd_ms proc_dmg proc_pct_max_hp proc_heal_pct …` (its cooldown shows on the P icon).
- **Look** [U-459] — `vis.colour vis.fill vis.glow vis.trail vis.flash vis.particles vis.mark_ms`.
- **Rank rules** — `<slot>_ultimate <slot>_unlock_level <slot>_rank_step <slot>_rank_cap`.

## 4. Stat names (`learned.*`, items)

`health attack elemental_power defence elemental_defence attack_speed move_speed move_speed_pct elemental_haste
non_elemental_haste cast_speed tenacity lifesteal omnivamp champion_vamp spellvamp crit_chance crit_damage arcane_power
martial_power skill_power skill_range skill_radius on_hit_damage on_hit_max_hp` — 100 = 1 % for percentages.

## 5. League of Legends mechanics (engine: `src/game/mechanics/lol.cpp`)

| Mechanic | Rule |
|---|---|
| Lifesteal | heal % of autoattack damage |
| Spellvamp | heal % of skill damage |
| Omnivamp | heal % of all damage |
| Champion vamp | heal % of damage to champions |
| Grievous wounds | hits cut the target's healing for 3 s |
| Critical strike | champions with `crits = 1`: crit chance → × crit damage (175 % default) |
| On-hit | every autoattack that lands: + flat damage + % of the target's max HP |
| Fear | cannot act, runs straight away from the one who scared it |
| Silence | no abilities at all (bending or not); autoattacks still work |

## 6. This game's own mechanics (engine: `src/game/mechanics/bending_cc.cpp`)

A skill is **bending** when its sub-element belongs to an element (water, earth, fire, air families); a **weapon**
skill has no element and is not `arcane` (arcane = non-elemental magic, never blocked by these).

| Mechanic | Rule |
|---|---|
| Chi Block | bending of the chosen elements is gone: those bending skills AND bending autoattacks; other skills work |
| Disarm | no weapon autoattacks, no weapon skills; bending still works |
| Subdue | both kinds: no bending at all and disarmed (only arcane magic is left) |
