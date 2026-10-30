#pragma once
// ======================================================================================
//  LOADOUTS [U-96][U-177][U-178] — three systems that work differently:
//    KEYSTONE        pick 1     a special effect with a trigger (see the hooks in mechanics/combat.cpp,
//                               mechanics/casting.cpp)
//    SPECIALISATION  pick 1 tree, then 1 node in each of its 3 tiers   (percent-style bonuses)
//    RUNES           3 slots (offense / flex / defense), 1 shard each   (flat stats)
//    CLASSIC RUNES   [U-318]    a page of 9 marks + 9 seals + 9 glyphs + 3 quintessences (free, flat stats)
//    MASTERIES       [U-319]    30 points over 3 trees, tiers unlock by points spent in that tree
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

// ---------------------------------------------------------------- [U-318] classic runes (Claude ideas [X-113])
// A page = 9 marks (offence) + 9 seals (defence) + 9 glyphs (utility): each slot is one of 5 kinds of that
// section, so the page stores HOW MANY of each kind (the sum per section is at most 9). Quintessences:
// 3 slots, few kinds, each strong. All free. The numbers are per piece, in the units of stats.hpp.
inline constexpr int kClassicSections = 3, kClassicKinds = 5, kClassicSlots = 9, kQuintSlots = 3, kQuintKinds = 6;
inline constexpr const char* kClassicSectionNames[] = {"Marks (offence)", "Seals (defence)", "Glyphs (utility)"};
inline constexpr std::array<std::array<RuneDef, kClassicKinds>, kClassicSections> kClassicRunes{{
    {{{"Mark of Power", {StatKind::ElementalPower, 100}}, {"Mark of Might", {StatKind::Attack, 80}},
      {"Mark of Rhythm", {StatKind::AttackSpeedPct, 90}}, {"Mark of Piercing", {StatKind::ElementalPenetration, 60}},
      {"Mark of Focus", {StatKind::ElementalHaste, 80}}}},
    {{{"Seal of Hide", {StatKind::Defence, 90}}, {"Seal of Spirit", {StatKind::ElementalDefence, 90}},
      {"Seal of Vigour", {StatKind::Health, 600}}, {"Seal of Growth", {StatKind::HealthPct, 40}},
      {"Seal of Resolve", {StatKind::Tenacity, 70}}}},
    {{{"Glyph of Clarity", {StatKind::ElementalHaste, 80}}, {"Glyph of Flow", {StatKind::CastSpeed, 120}},
      {"Glyph of Wind", {StatKind::MoveSpeed, 100}}, {"Glyph of Sight", {StatKind::SkillRange, 500}},
      {"Glyph of Tempo", {StatKind::AttackSpeedPct, 60}}}},
}};
inline constexpr std::array<RuneDef, kQuintKinds> kQuints{{
    {"Quintessence of Power", {StatKind::ElementalPower, 1'500}}, {"Quintessence of Might", {StatKind::Attack, 1'000}},
    {"Quintessence of Vitality", {StatKind::Health, 12'000}}, {"Quintessence of the Bulwark", {StatKind::Defence, 1'200}},
    {"Quintessence of Warding", {StatKind::ElementalDefence, 1'200}}, {"Quintessence of Swiftness", {StatKind::MoveSpeed, 2'000}},
}};

// ---------------------------------------------------------------- [U-319] masteries 30 / 30 / 30
// 3 trees (offence / defence / utility), 5 tiers of 3 (the last tier 2). A mastery has 1..max_rank ranks, each
// rank gives `mod`. Tier t opens when 4 × t points are spent in earlier tiers of the SAME tree (like old LoL).
// 30 points in total over all trees. The last tier is the strongest (keystone-like, 1 rank, plain stats).
inline constexpr int kMasteryTrees = 3, kMasteryTiers = 5, kMasteryPerTier = 3, kMasteryTreeSize = kMasteryTiers * kMasteryPerTier;
inline constexpr int kMasteryPoints = 30, kMasteryTierStep = 4;
struct MasteryDef {
    std::string_view name;
    std::uint8_t max_rank = 0;  // 0 = no mastery in this place
    StatMod mod;                // per rank
};
struct MasteryTree {
    std::string_view name;
    std::string_view theme;
    std::array<std::array<MasteryDef, kMasteryPerTier>, kMasteryTiers> tiers;
};
inline constexpr std::array<MasteryTree, kMasteryTrees> kMasteryTrees_{{
    {"Offence", "elemental fury",
     {{{{{"Sparks", 3, {StatKind::ElementalPower, 50}}, {"Sharpened Edge", 3, {StatKind::Attack, 40}}, {"Quick Fingers", 2, {StatKind::AttackSpeedPct, 100}}}},
       {{{"Focus", 3, {StatKind::ElementalHaste, 100}}, {"Cold Steel", 2, {StatKind::ElementalPenetration, 80}}, {"Kindling", 3, {StatKind::ElementalPower, 50}}}},
       {{{"Far Reach", 2, {StatKind::SkillRange, 1'500}}, {"Burning Core", 3, {StatKind::ElementalPower, 60}}, {"Flurry", 3, {StatKind::AttackSpeedPct, 100}}}},
       {{{"Piercing Flame", 3, {StatKind::ElementalPenetration, 100}}, {"Dragon Strength", 3, {StatKind::Attack, 50}}, {"Spell Mastery", 2, {StatKind::ElementalPower, 80}}}},
       {{{"Avatar's Fury", 1, {StatKind::ElementalPower, 300}}, {"Warlord", 1, {StatKind::Attack, 200}}, {}}}}}},
    {"Defence", "stone and spirit",
     {{{{{"Tough Hide", 3, {StatKind::Defence, 60}}, {"Spirit Ward", 3, {StatKind::ElementalDefence, 60}}, {"Vigour", 2, {StatKind::Health, 400}}}},
       {{{"Endurance", 3, {StatKind::HealthPct, 50}}, {"Resolve", 2, {StatKind::Tenacity, 80}}, {"Iron Will", 3, {StatKind::Defence, 60}}}},
       {{{"Stoneskin", 2, {StatKind::ElementalDefence, 60}}, {"Deep Roots", 3, {StatKind::Health, 500}}, {"Calm", 3, {StatKind::Tenacity, 60}}}},
       {{{"Fortitude", 3, {StatKind::HealthPct, 60}}, {"Bulwark", 3, {StatKind::Defence, 80}}, {"Warding", 2, {StatKind::ElementalDefence, 80}}}},
       {{{"Mountain Heart", 1, {StatKind::Health, 3'000}}, {"Unbreakable", 1, {StatKind::Tenacity, 400}}, {}}}}}},
    {"Utility", "wind and flow",
     {{{{{"Light Feet", 3, {StatKind::MoveSpeed, 100}}, {"Clear Mind", 3, {StatKind::CastSpeed, 100}}, {"Wisdom", 2, {StatKind::ElementalHaste, 80}}}},
       {{{"Wanderer", 2, {StatKind::MoveSpeed, 100}}, {"Far Sight", 3, {StatKind::SkillRange, 1'500}}, {"Meditation", 3, {StatKind::ElementalHaste, 80}}}},
       {{{"Tailwind Step", 3, {StatKind::MoveSpeed, 100}}, {"Flow", 3, {StatKind::CastSpeed, 120}}, {"Small Frame", 2, {StatKind::ModelSize, -100}}}},
       {{{"Fleet Foot", 3, {StatKind::MoveSpeed, 120}}, {"Echoing Chi", 3, {StatKind::ElementalHaste, 100}}, {"Sage", 2, {StatKind::ElementalPower, 60}}}},
       {{{"Windwalker", 1, {StatKind::MoveSpeed, 500}}, {"Master of Tempo", 1, {StatKind::ElementalHaste, 400}}, {}}}}}},
}};
[[nodiscard]] constexpr int mastery_index(int tree, int tier, int col) { return tree * kMasteryTreeSize + tier * kMasteryPerTier + col; }

// ---------------------------------------------------------------- the loadout a champion plays with
struct Loadout {
    std::int8_t keystone = -1;               // index into kKeystones
    std::int8_t tree = -1;                   // index into kSpecTrees
    std::array<std::int8_t, 3> nodes{-1, -1, -1};  // 0 / 1 per tier
    std::array<std::int8_t, 3> runes{-1, -1, -1};  // 0..2 per slot
    // [U-318] classic runes: how many of each kind per section (marks / seals / glyphs, sum ≤ 9) + 3 quintessences (−1 = none)
    std::array<std::array<std::uint8_t, kClassicKinds>, kClassicSections> classic{};
    std::array<std::int8_t, kQuintSlots> quints{-1, -1, -1};
    // [U-319] ranks per mastery (see mastery_index); points ≤ 30, tiers unlocked by points in the tree
    std::array<std::uint8_t, kMasteryTrees * kMasteryTreeSize> masteries{};
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
    for (auto& sec : l.classic) {  // [U-318] at most 9 per section
        int left = kClassicSlots;
        for (auto& n : sec) {
            n = std::uint8_t(n > left ? left : n);
            left -= n;
        }
    }
    for (auto& q : l.quints)
        if (q < -1 || q >= kQuintKinds) q = -1;
    int total = 0;  // [U-319] ranks ≤ max, tier unlocked by points in earlier tiers of the same tree, 30 in all
    for (int t = 0; t < kMasteryTrees; ++t) {
        int spent = 0;
        for (int tier = 0; tier < kMasteryTiers; ++tier) {
            int in_tier = 0;
            for (int c = 0; c < kMasteryPerTier; ++c) {
                auto& r = l.masteries[std::size_t(mastery_index(t, tier, c))];
                const int mx = kMasteryTrees_[std::size_t(t)].tiers[std::size_t(tier)][std::size_t(c)].max_rank;
                int v = spent >= tier * kMasteryTierStep ? r : 0;
                v = v > mx ? mx : v;
                v = v > kMasteryPoints - total ? kMasteryPoints - total : v;
                r = std::uint8_t(v);
                total += v;
                in_tier += v;
            }
            spent += in_tier;
        }
    }
    return l;
}
[[nodiscard]] constexpr int mastery_points(const Loadout& l) {
    int n = 0;
    for (auto r : l.masteries) n += r;
    return n;
}
[[nodiscard]] constexpr int mastery_points_in_tree(const Loadout& l, int tree) {
    int n = 0;
    for (int i = 0; i < kMasteryTreeSize; ++i) n += l.masteries[std::size_t(tree * kMasteryTreeSize + i)];
    return n;
}
// points spent in the tiers BELOW `tier` of this tree (what opens the tier)
[[nodiscard]] constexpr int mastery_points_below(const Loadout& l, int tree, int tier) {
    int n = 0;
    for (int i = 0; i < tier * kMasteryPerTier; ++i) n += l.masteries[std::size_t(tree * kMasteryTreeSize + i)];
    return n;
}

// Where a stat bonus comes from (the setup editor sums them up per source [U-318][U-319]).
enum class ModSource : std::uint8_t { Specialisation, Shards, ClassicRunes, Masteries, Count };
// Calls f(source, mod) for every active bonus of the loadout (counts and ranks already multiplied in).
template <class F>
constexpr void for_each_mod(const Loadout& l, F&& f) {
    if (l.tree >= 0)
        for (std::size_t t = 0; t < 3; ++t)
            if (l.nodes[t] >= 0) f(ModSource::Specialisation, kSpecTrees[std::size_t(l.tree)].tiers[t][std::size_t(l.nodes[t])].mod);
    for (std::size_t r = 0; r < 3; ++r)
        if (l.runes[r] >= 0) f(ModSource::Shards, kRunes[r][std::size_t(l.runes[r])].mod);
    for (std::size_t sec = 0; sec < kClassicSections; ++sec)  // [U-318]
        for (std::size_t k = 0; k < kClassicKinds; ++k)
            if (l.classic[sec][k]) {
                StatMod m = kClassicRunes[sec][k].mod;
                m.value *= l.classic[sec][k];
                f(ModSource::ClassicRunes, m);
            }
    for (auto q : l.quints)
        if (q >= 0) f(ModSource::ClassicRunes, kQuints[std::size_t(q)].mod);
    for (std::size_t t = 0; t < kMasteryTrees; ++t)  // [U-319]
        for (std::size_t tier = 0; tier < kMasteryTiers; ++tier)
            for (std::size_t c = 0; c < kMasteryPerTier; ++c) {
                const std::uint8_t r = l.masteries[std::size_t(mastery_index(int(t), int(tier), int(c)))];
                if (!r) continue;
                StatMod m = kMasteryTrees_[t].tiers[tier][c].mod;
                m.value *= r;
                f(ModSource::Masteries, m);
            }
}

// All stat bonuses of a loadout.
constexpr void apply_loadout(Stats& s, const Loadout& l) {
    for_each_mod(l, [&](ModSource, const StatMod& m) { apply_mod(s, m); });
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
// [U-318][U-319] 9 marks of power, a quintessence, 3 ranks of Sparks; a locked tier is cleared
static_assert([] {
    Stats s;
    Loadout l;
    l.classic[0][0] = 12;  // too many → 9
    l.quints[0] = 0;
    l.masteries[std::size_t(mastery_index(0, 0, 0))] = 3;
    l.masteries[std::size_t(mastery_index(0, 4, 0))] = 1;  // tier 5 needs 16 points → cleared
    l = validated(l);
    apply_loadout(s, l);
    return l.classic[0][0] == 9 && l.masteries[std::size_t(mastery_index(0, 4, 0))] == 0 && s.elemental_power.bonus == 900 + 1'500 + 150;
}());
static_assert([] {  // every tree can take all 30 points
    for (const auto& t : kMasteryTrees_) {
        int n = 0;
        for (const auto& tier : t.tiers)
            for (const auto& m : tier) n += m.max_rank;
        if (n < kMasteryPoints) return false;
    }
    return true;
}());
static_assert(validated(Loadout{.keystone = 9, .tree = -1, .nodes = {1, 1, 1}}).keystone == -1);

}  // namespace em
