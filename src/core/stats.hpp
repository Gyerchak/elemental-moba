#pragma once
// [U-39, U-40, U-53..U-58, U-60..U-64, U-127] Stats, resources, damage.
// All values are integers in hundredths (100 = 1.00) — deterministic [X-04].
#include <algorithm>
#include <cstdint>

namespace em {

struct Stat {
    std::int32_t base = 0;
    std::int32_t bonus = 0;
    [[nodiscard]] constexpr std::int32_t total() const { return base + bonus; }
};

enum class Range : std::uint8_t { Melee, Ranged };  // [U-39]

// [U-60..U-64]
enum class ResourceKind : std::uint8_t {
    Water,   // env-based regen + collection range [U-60]
    Fuel,    // [U-268] fire "calm flame": breath (regenerates) + life (when empty, the rest is paid with HP ×2)
    None,    // earth: slow casts, long CDs [U-62]; air: free, low CD [U-63]
    Ammo,    // non-benders [U-64]
    Energy,  // non-benders [U-64]
    Rage,    // [U-268][?-10] fire: chaotic flame — filled by dealing / taking damage, drains out of combat
};
// [U-268] fire fuel numbers (Claude idea placeholders)
inline constexpr std::int32_t kLifePerFuel = 2;        // calm flame: 1 missing fuel = 2 HP
inline constexpr std::int32_t kRageBoostPct = 20;      // a skill paid fully with rage deals +20 %
inline constexpr std::int32_t kRageDealtPct = 10;      // rage gained = 10 % of damage dealt …
inline constexpr std::int32_t kRageTakenPct = 5;       // … and 5 % of damage taken
inline constexpr std::int64_t kRageCombatMs = 5'000;   // drains after 5 s without fighting

struct Stats {
    Stat health, defence, elemental_defence;
    Stat attack;               // base + bonus [U-39]
    Stat elemental_power;      // base + bonus, replaces AP [U-39]
    Stat attack_speed;         // [U-39]
    Stat move_speed;           // [U-39]
    Stat turn_rate;            // [U-74]
    std::int32_t elemental_lethality = 0;    // flat elemental pen [U-40]
    std::int32_t elemental_penetration = 0;  // % elemental pen, 100 = 1 % [U-40]
    std::int32_t elemental_haste = 0;        // [U-57]
    std::int32_t non_elemental_haste = 0;    // [U-57]
    // Items [U-113] and timed effects [U-114] add to the fields below.
    std::int32_t cast_speed = 0;             // shortens cast time [U-53][U-114]
    std::int32_t skill_speed = 0;            // projectile speed [U-54]
    std::int32_t skill_width = 0;            // [U-54]
    std::int32_t skill_length = 0;           // [U-54]
    std::int32_t skill_range = 0;            // [U-113]
    std::int32_t skill_radius = 0;           // [U-113] on top of radius_bonus()
    std::int32_t water_collect_range = 0;    // [U-60]
    std::int32_t tenacity = 0;               // [U-127] shorter CC on me, 100 = 1 %
    std::int32_t cc_amplify = 0;             // [U-127] longer CC I apply, 100 = 1 %
    std::int32_t model_size = 0;             // [U-129] bonus model + hitbox size, 100 = 1 % (can be < 0)
    // [U-136] Less damage taken from one element, 100 = 1 %. [X-74] capped at 60 %.
    std::int32_t resist_water = 0, resist_earth = 0, resist_fire = 0, resist_air = 0;
    std::int32_t slow_resist = 0, slow_resist_ice = 0;  // [U-218] 100 = 1 %, non-ice / ice slows, cap 80 %
    std::int32_t move_speed_pct = 0;                    // [U-219] bonus % move speed, 100 = 1 %
    // [U-288] extra damage against player-made walls, 100 = 1 %: all walls / only one kind (they add up)
    std::int32_t wall_damage = 0, wall_damage_stone = 0, wall_damage_ice = 0, wall_damage_metal = 0;
    // [U-416] classic rule sets only (LoL- / Dota-like; the elemental rules ignore them [U-316]), 100 = 1 %
    std::int32_t lifesteal = 0;       // heal by this % of autoattack damage dealt
    std::int32_t omnivamp = 0;        // heal by this % of ALL damage dealt
    std::int32_t grievous_on_hit = 0; // hits put "grievous wounds" on the target: its healing −this % for 3 s
    std::int32_t champion_vamp = 0;   // [U-468] heal by this % of damage dealt to CHAMPIONS (e.g. a dash skill's passive)
    std::int32_t spellvamp = 0;       // [U-473] heal by this % of SKILL damage dealt
    std::int32_t on_hit_dmg = 0;      // [U-473] on-hit: + flat damage on every autoattack hit (hundredths)
    std::int32_t on_hit_max_hp = 0;   // [U-473] on-hit: + this % of the target's max HP (100 = 1 %)
    // [U-463] critical strikes (autoattacks of champions that can crit: ChampionDef::crits), 100 = 1 %
    std::int32_t crit_chance = 0, crit_damage = 17'500;  // 0 % / 175 % of the hit
    // [U-464] skill power by kind of skill, % more skill damage (100 = 1 %), on top of elemental power:
    //   arcane = non-elemental magic skills, martial = non-magic (physical) skills, skill = every skill
    std::int32_t arcane_power = 0, martial_power = 0, skill_power = 0;
    Range range = Range::Melee;
    ResourceKind resource = ResourceKind::None;
};

// [U-462][U-468] One name per adjustable number: skills' "while learned" bonuses (SkillDef::learned), .champ keys,
// tooltips. Values in the stat's own unit (hundredths for health / attack / power / %, units for ranges).
enum class StatKey : std::uint8_t {
    Health, Attack, ElementalPower, Defence, ElementalDefence, AttackSpeed, MoveSpeed, MoveSpeedPct, ElementalHaste,
    NonElementalHaste, CastSpeed, Tenacity, Lifesteal, Omnivamp, ChampionVamp, CritChance, CritDamage, ArcanePower,
    MartialPower, SkillPower, SkillRange, SkillRadius, Spellvamp, OnHitDamage, OnHitMaxHp, Count
};
inline constexpr const char* kStatKeyNames[] = {
    "health", "attack", "elemental_power", "defence", "elemental_defence", "attack_speed", "move_speed", "move_speed_pct",
    "elemental_haste", "non_elemental_haste", "cast_speed", "tenacity", "lifesteal", "omnivamp", "champion_vamp", "crit_chance",
    "crit_damage", "arcane_power", "martial_power", "skill_power", "skill_range", "skill_radius", "spellvamp", "on_hit_damage",
    "on_hit_max_hp"};
static_assert(std::size(kStatKeyNames) == std::size_t(StatKey::Count));
constexpr void add_stat(Stats& s, StatKey k, std::int32_t v) {
    switch (k) {
        case StatKey::Health: s.health.bonus += v; break;
        case StatKey::Attack: s.attack.bonus += v; break;
        case StatKey::ElementalPower: s.elemental_power.bonus += v; break;
        case StatKey::Defence: s.defence.bonus += v; break;
        case StatKey::ElementalDefence: s.elemental_defence.bonus += v; break;
        case StatKey::AttackSpeed: s.attack_speed.bonus += v; break;
        case StatKey::MoveSpeed: s.move_speed.bonus += v; break;
        case StatKey::MoveSpeedPct: s.move_speed_pct += v; break;
        case StatKey::ElementalHaste: s.elemental_haste += v; break;
        case StatKey::NonElementalHaste: s.non_elemental_haste += v; break;
        case StatKey::CastSpeed: s.cast_speed += v; break;
        case StatKey::Tenacity: s.tenacity += v; break;
        case StatKey::Lifesteal: s.lifesteal += v; break;
        case StatKey::Omnivamp: s.omnivamp += v; break;
        case StatKey::ChampionVamp: s.champion_vamp += v; break;
        case StatKey::CritChance: s.crit_chance += v; break;
        case StatKey::CritDamage: s.crit_damage += v; break;
        case StatKey::ArcanePower: s.arcane_power += v; break;
        case StatKey::MartialPower: s.martial_power += v; break;
        case StatKey::SkillPower: s.skill_power += v; break;
        case StatKey::SkillRange: s.skill_range += v; break;
        case StatKey::SkillRadius: s.skill_radius += v; break;
        case StatKey::Spellvamp: s.spellvamp += v; break;
        case StatKey::OnHitDamage: s.on_hit_dmg += v; break;
        case StatKey::OnHitMaxHp: s.on_hit_max_hp += v; break;
        case StatKey::Count: break;
    }
}

// [U-54] Radius-only skills get width/length stats at 1/3 value.
[[nodiscard]] constexpr std::int32_t radius_bonus(const Stats& s) {
    return (s.skill_width + s.skill_length) / 2 / 3;
}

// [U-55] Elemental autoattack speed = mainly AS + a little elemental haste.
// [U-56] Elemental autoattack damage = mainly attack + a little elemental power.
// [X-34] The ratios below are placeholders.
inline constexpr int kEAutoHastePct = 10, kEAutoPowerPct = 15;
[[nodiscard]] constexpr std::int32_t elemental_auto_speed(const Stats& s) {
    return s.attack_speed.total() + s.elemental_haste * kEAutoHastePct / 100;
}
[[nodiscard]] constexpr std::int32_t elemental_auto_damage(const Stats& s) {
    return s.attack.total() + s.elemental_power.total() * kEAutoPowerPct / 100;
}

// [U-58] Melee have more effective defence. [X-33] +15 % placeholder.
inline constexpr int kMeleeDefenceBonusPct = 15;

// [U-127] CC duration = base × (100 % + attacker CC amplify) × (100 % − target tenacity).
// [X-68] Tenacity capped at 80 %; airborne / bloodbending-type CC can ignore tenacity.
inline constexpr std::int32_t kMaxTenacity = 8'000;
[[nodiscard]] constexpr std::int64_t cc_duration_ms(std::int64_t base_ms, const Stats& attacker, const Stats& target,
                                                    bool ignores_tenacity = false) {
    std::int64_t ms = base_ms * (10'000 + std::max(attacker.cc_amplify, 0)) / 10'000;
    if (!ignores_tenacity) ms = ms * (10'000 - std::clamp(target.tenacity, 0, kMaxTenacity)) / 10'000;
    return ms;
}

static_assert([] {
    Stats a, t;
    a.cc_amplify = 2'500;  // +25 %
    t.tenacity = 4'000;    // 40 %
    return cc_duration_ms(2'000, a, t) == 1'500 && cc_duration_ms(2'000, a, t, true) == 2'500 &&
           cc_duration_ms(1'000, Stats{}, Stats{.tenacity = 9'900}) == 200;
}());

// [U-129] Model size in % (100 = normal). [X-82] never below 50 %.
[[nodiscard]] constexpr std::int32_t model_size_pct(const Stats& s) { return std::max(50, 100 + s.model_size / 100); }

// [X-15] Elemental damage after defence. Order: % pen, then flat lethality.
[[nodiscard]] constexpr std::int32_t elemental_damage(std::int32_t raw, const Stats& attacker, const Stats& target) {
    std::int64_t def = target.elemental_defence.total();
    if (target.range == Range::Melee) def = def * (100 + kMeleeDefenceBonusPct) / 100;
    def = def * (10'000 - attacker.elemental_penetration) / 10'000;
    def = std::max<std::int64_t>(0, def - attacker.elemental_lethality);
    return std::int32_t(std::int64_t(raw) * 10'000 / (10'000 + def));  // def in hundredths
}

}  // namespace em
