#pragma once
// ======================================================================================
//  LOADOUTS [U-96][U-177][U-178] — three systems that work differently:
//    KEYSTONE        pick 1     a special effect with a trigger (see the hooks in mechanics/combat.cpp,
//                               mechanics/casting.cpp)
//    SPECIALISATION  pick 1 tree, then 1 node in each of its 3 tiers   (percent-style bonuses)
//    RUNES           3 slots (offense / flex / defense), 1 shard each   (flat stats)
//  Nothing picked (−1) = no bonus [U-178]. All numbers are Claude ideas [X-113] — edit freely.
// ======================================================================================
#include <array>
#include <cstdint>
#include <string_view>

#include "core/stats.hpp"

namespace em {

// A single stat change. Units follow stats.hpp (hundredths: 100 = 1.00 or 1 %).
enum class StatKind : std::uint8_t {
    Health, Attack, ElementalPower, Defence, ElementalDefence, AttackSpeedPct, MoveSpeed, ElementalHaste,
    Tenacity, CastSpeed, SkillRange, ModelSize, ElementalPenetration, HealthPct,
};
struct StatMod {
    StatKind kind = StatKind::Health;
    std::int32_t value = 0;
};
constexpr void apply_mod(Stats& s, const StatMod& m) {
    switch (m.kind) {
        case StatKind::Health: s.health.bonus += m.value; break;
        case StatKind::Attack: s.attack.bonus += m.value; break;
        case StatKind::ElementalPower: s.elemental_power.bonus += m.value; break;
        case StatKind::Defence: s.defence.bonus += m.value; break;
        case StatKind::ElementalDefence: s.elemental_defence.bonus += m.value; break;
        case StatKind::AttackSpeedPct: s.attack_speed.bonus += m.value; break;
        case StatKind::MoveSpeed: s.move_speed.bonus += m.value; break;
        case StatKind::ElementalHaste: s.elemental_haste += m.value; break;
        case StatKind::Tenacity: s.tenacity += m.value; break;
        case StatKind::CastSpeed: s.cast_speed += m.value; break;
        case StatKind::SkillRange: s.skill_range += m.value; break;
        case StatKind::ModelSize: s.model_size += m.value; break;
        case StatKind::ElementalPenetration: s.elemental_penetration += m.value; break;
        case StatKind::HealthPct: s.health.bonus += s.health.base * m.value / 10'000; break;
    }
}

// ---------------------------------------------------------------- keystones
enum class KeystoneId : std::uint8_t { ElementalSurge, Stoneheart, Tailwind, FuelTheFire, ChiRush };
struct KeystoneDef {
    KeystoneId id;
    std::string_view name;
    std::string_view desc;
    std::string_view impact;  // what the post-game screen counts
};
inline constexpr std::array kKeystones = {
    KeystoneDef{KeystoneId::ElementalSurge, "Elemental Surge",
                "3 skill hits on the same champion within 3 s: +40 (+15 % EP) damage", "bonus damage dealt"},
    KeystoneDef{KeystoneId::Stoneheart, "Stoneheart",
                "Falling under 35 % HP: shield 80 (+10 per level) for 3 s. Cooldown 60 s", "shield given"},
    KeystoneDef{KeystoneId::Tailwind, "Tailwind", "After every skill: +20 % move speed for 1.5 s", "times triggered"},
    KeystoneDef{KeystoneId::FuelTheFire, "Fuel the Fire",
                "Skill hits on champions give back 10 % of the cost (no resource: −0.5 s on that cooldown)",
                "resource / cooldown returned"},
    KeystoneDef{KeystoneId::ChiRush, "Chi Rush", "The first autoattack within 3 s after a skill deals +30 % damage",
                "bonus damage dealt"},
};

// ---------------------------------------------------------------- specialisation trees
struct SpecNode {
    std::string_view name;
    StatMod mod;
};
struct SpecTree {
    std::string_view name;
    std::string_view theme;
    std::array<std::array<SpecNode, 2>, 3> tiers;  // pick 1 of 2 in each tier
};
inline constexpr std::array kSpecTrees = {
    SpecTree{"Ferocity", "damage",
             {{{{{"Sharp Mind", {StatKind::ElementalPower, 1'200}}, {"Heavy Hands", {StatKind::Attack, 800}}}},
               {{{"Quick Hands", {StatKind::AttackSpeedPct, 800}}, {"Focus", {StatKind::ElementalHaste, 800}}}},
               {{{"Piercing", {StatKind::ElementalPenetration, 800}}, {"Long Reach", {StatKind::SkillRange, 6'000}}}}}}},
    SpecTree{"Resolve", "toughness",
             {{{{{"Thick Skin", {StatKind::Defence, 800}}, {"Spirit Ward", {StatKind::ElementalDefence, 800}}}},
               {{{"Endurance", {StatKind::HealthPct, 500}}, {"Unshaken", {StatKind::Tenacity, 1'000}}}},
               {{{"Bulk", {StatKind::Health, 12'000}}, {"Rooted Stance", {StatKind::ModelSize, 500}}}}}}},
    SpecTree{"Insight", "casting",
             {{{{{"Calm Breath", {StatKind::CastSpeed, 1'500}}, {"Clarity", {StatKind::ElementalHaste, 1'000}}}},
               {{{"Far Sight", {StatKind::SkillRange, 8'000}}, {"Inner Power", {StatKind::ElementalPower, 1'500}}}},
               {{{"Flow", {StatKind::CastSpeed, 2'000}}, {"Wisdom", {StatKind::ElementalHaste, 1'200}}}}}}},
    SpecTree{"Harmony", "mobility",
             {{{{{"Light Step", {StatKind::MoveSpeed, 1'500}}, {"Small Frame", {StatKind::ModelSize, -500}}}},
               {{{"Swift Strikes", {StatKind::AttackSpeedPct, 1'000}}, {"Steady", {StatKind::Tenacity, 800}}}},
               {{{"Wind Runner", {StatKind::MoveSpeed, 2'000}}, {"Balance", {StatKind::Health, 8'000}}}}}}},
};

// ---------------------------------------------------------------- runes (stat shards)
struct RuneDef {
    std::string_view name;
    StatMod mod;
};
// rows = slots (0 offense, 1 flex, 2 defense), columns = the shard picked
inline constexpr std::array<std::array<RuneDef, 3>, 3> kRunes{{
    {{{"+9 elemental power", {StatKind::ElementalPower, 900}}, {"+6 attack", {StatKind::Attack, 600}},
      {"+10 % attack speed", {StatKind::AttackSpeedPct, 1'000}}}},
    {{{"+9 elemental power", {StatKind::ElementalPower, 900}}, {"+6 attack", {StatKind::Attack, 600}},
      {"+6 defence", {StatKind::Defence, 600}}}},
    {{{"+65 health", {StatKind::Health, 6'500}}, {"+8 elemental defence", {StatKind::ElementalDefence, 800}},
      {"+10 % tenacity", {StatKind::Tenacity, 1'000}}}},
}};
inline constexpr const char* kRuneSlotNames[] = {"Offense", "Flex", "Defense"};

// ---------------------------------------------------------------- the loadout a champion plays with
struct Loadout {
    std::int8_t keystone = -1;               // index into kKeystones
    std::int8_t tree = -1;                   // index into kSpecTrees
    std::array<std::int8_t, 3> nodes{-1, -1, -1};  // 0 / 1 per tier
    std::array<std::int8_t, 3> runes{-1, -1, -1};  // 0..2 per slot
    friend constexpr bool operator==(const Loadout&, const Loadout&) = default;
};

// Clamp anything out of range to "nothing" (server-side check [X-24]).
[[nodiscard]] constexpr Loadout validated(Loadout l) {
    if (l.keystone < -1 || l.keystone >= std::int8_t(kKeystones.size())) l.keystone = -1;
    if (l.tree < -1 || l.tree >= std::int8_t(kSpecTrees.size())) l.tree = -1;
    for (auto& n : l.nodes)
        if (l.tree < 0 || n < -1 || n > 1) n = -1;
    for (auto& r : l.runes)
        if (r < -1 || r > 2) r = -1;
    return l;
}

// All stat bonuses of a loadout.
constexpr void apply_loadout(Stats& s, const Loadout& l) {
    if (l.tree >= 0)
        for (std::size_t t = 0; t < 3; ++t)
            if (l.nodes[t] >= 0) apply_mod(s, kSpecTrees[std::size_t(l.tree)].tiers[t][std::size_t(l.nodes[t])].mod);
    for (std::size_t r = 0; r < 3; ++r)
        if (l.runes[r] >= 0) apply_mod(s, kRunes[r][std::size_t(l.runes[r])].mod);
}

[[nodiscard]] constexpr bool has_keystone(const Loadout& l, KeystoneId k) {
    return l.keystone >= 0 && kKeystones[std::size_t(l.keystone)].id == k;
}

static_assert([] {
    Stats s;
    Loadout l{.keystone = 0, .tree = 0, .nodes = {0, 0, 1}, .runes = {0, -1, 2}};
    apply_loadout(s, l);
    return s.elemental_power.bonus == 1'200 + 900 && s.attack_speed.bonus == 800 && s.skill_range == 6'000 &&
           s.tenacity == 1'000;
}());
static_assert(validated(Loadout{.keystone = 9, .tree = -1, .nodes = {1, 1, 1}}).keystone == -1);

}  // namespace em
