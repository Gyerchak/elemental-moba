# Own champions — the `.champ` file

A champion is **only values**. Every mechanic lives in the engine (the unified LoL + Dota + own rules).
Put the file in `custom_champions/<name>.champ`. Check it without starting the game:

    ./build/dump_content --check-champ custom_champions/<name>.champ

With ANY problem (unknown key, bad number) the game ignores the file — the checker prints the reason.

## 0. Writing numbers (everywhere: champion files, game versions)

| Written | Means |
|---|---|
| `570` / `2.8` / `=570` | set it (decimals are scaled to the key's unit) |
| `+40` / `-40` | add / subtract |
| `+30%` / `-20%` | 30 % more / 20 % less than the value before this line (the base champion's, the copied skill's, the official one) |
| `150%` | 150 % of the value before |
| `*1.5` | multiply |

Stat BONUSES (`learned.*`, `talentN.*`, item `more`) are different: `learned.attack = +30%` = **+30 % of that stat**
(added after every flat bonus); `learned.attack = 30` = +30 flat.

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
  `dur_ms delay_ms tick_ms`, `cc cc_ms cc_val` (Stun Root Slow Silence ChiBlock Disarm Subdue Fear Hex Break
  DebuffImmune …), `knock`, `ap` (% of ability power), `dispel` (1 basic, 2 strong),
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
martial_power skill_amp skill_range skill_radius on_hit_damage on_hit_max_hp ability_power magic_pen magic_pen_pct
strength agility intelligence hp_regen mana_regen magic_resist evasion bash_chance bash_ms bash_damage damage_block
block_chance true_strike mana_break heal_amp` — 100 = 1 % for percentages. Items can carry any of them (`ItemDef::more`).

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
| Ability power | skills deal + `ap` % of it |
| Silence-free kit rule | non-mages: every damaging skill scales with attack, bonus attack or ability power (`ad` / `bonus_ad` / `ap`) |
| Healing received | EVERY heal (skills, passives, vamp, items, HP regen, fountain) × (1 + `heal_amp`); a Transform's `buff_heal_pct` adds to it; then grievous wounds |
| Magic penetration | flat / % — lower the target's elemental defence against non-physical damage (with the elemental pens) |

## 5b. LoL-modified (this game's own LoL-style stats, `lol.cpp`)

| Stat | Rule |
|---|---|
| Skill power (`skill_power`) | ability power that only NON-mages use |
| Magic power (`magic_power`) | ability power that only MAGES use |
| Skill amp (`skill_amp`) | % more damage for every skill (was called "skill power" before) |

## 6. This game's own mechanics (engine: `src/game/mechanics/bending_cc.cpp`)

A skill is **bending** when its sub-element belongs to an element (water, earth, fire, air families); a **weapon**
skill has no element and is not `arcane` (arcane = non-elemental magic, never blocked by these).

| Mechanic | Rule |
|---|---|
| Chi Block | bending of the chosen elements is gone: those bending skills AND bending autoattacks; other skills work |
| Disarm | no weapon autoattacks, no weapon skills; bending still works |
| Subdue | both kinds: no bending at all and disarmed (only arcane magic is left) |

## 7. Dota 2 mechanics (engine: `src/game/mechanics/dota.cpp`)

Champion keys: `attr = Strength / Agility / Intelligence / Universal`, `strength agility intelligence` (points at
level 1) and `strength_per_level agility_per_level intelligence_per_level` (decimals allowed: `2.8`), `resource = Mana`.

| Mechanic | Rule |
|---|---|
| Strength | +22 HP and +0.1 HP regen per point |
| Agility | +1/6 defence and +1 % attack speed per point |
| Intelligence | +12 mana, +0.05 mana regen, +0.1 % magic resistance per point |
| Primary attribute | +1 attack per point (Universal: +0.7 per point of all three) |
| Mana | a resource: regenerates; Intelligence + `mana_regen` add to it |
| HP regen | `hp_regen` per second on top of the base regen |
| Magic resistance | non-physical damage × (1 − resistance), max 90 % |
| Evasion / true strike | autoattacks miss by chance; true strike never misses |
| Damage block | physical autoattacks taken − a flat amount (`block_chance`, 0 = always) |
| Bash | on-hit chance: stun + damage |
| Mana break | on-hit: burns the target's mana, half of it as damage |
| Hex | no skills, no autoattacks, crawls slowly |
| Break | passives off (champion passives, procs, skills' own passives) |
| Debuff immunity | no new crowd control / debuffs land |
| Dispel | `dispel = 1` basic / `2` strong: cleans the caster; strips buffs from enemies hit |
| Heal amplification | `heal_amp`: more healing received |

Already in the engine before (shared by both games): denies, lifesteal / spellvamp (Dota "spell lifesteal"),
tenacity (Dota "status resistance"), skill power (Dota "spell amplification"), skill range (cast range), haste
(cooldown reduction), splash autoattacks (cleave), crits, turn rate, day / night vision.
### Switchable features (all OFF by default — a map's `features`, or the lobby owner: Lobby → Features)

| Feature | Rule |
|---|---|
| Buyback | dead: pay 200 + 40 × level gold to come back at once; 5 min cooldown |
| Courier | buy anywhere: the item arrives after the courier's trip from the fountain (600 units / s) |
| Neutral items | jungle monsters may drop one (25 %), tier = 1 + minutes / 10 (max 4); one neutral slot |
| Scepter upgrades | item Sage Scepter upgrades the ultimate: +30 % damage, −20 % cooldown, +15 % size (`scepter_dmg_pct`, `scepter_cd_pct`) |
| Talents | levels 10 / 15 / 20 / 25: pick one of two bonuses (own: `talent1.left.attack = 25` … `talent4.right.<stat>`) |
| Invisibility | item Shade Blade: invisible 10 s (attacking / casting ends it); true sight: turrets (700), Sentry Lantern (900) |
| Glyph | the team's structures cannot be damaged for 5 s; 5 min cooldown |

## 8. Game versions with mods (`versions/<name>/version.txt`)

`mods = lol` switches on content that exists only in that version (items with `.mod = "lol"`: Arcane Codex,
Hollow Wand, Void Lens → version `lol-mod`). The version's own assets live in `versions/<name>/pack/` (a resource
pack used only while the version is active); `resource_pack = <name>` links a pack from `resourcepacks/` too.

## 9. Resource packs in order (Options → Graphics)

Switch packs on and order them (^ / v). Like Minecraft: for the same file, the pack HIGHER in the list wins. After
your packs come the game version's pack, the map's pack and `default`.
