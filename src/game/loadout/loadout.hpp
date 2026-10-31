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
#include <cstddef>

#include "core/stats.hpp"

namespace em {

// A single stat change: ANY stat (StatKey, core/stats.hpp), flat or "+x %" of the stat [U-483].
// Units follow stats.hpp (hundredths: 100 = 1.00 or 1 %).
struct StatMod {
    StatKey key = StatKey::Health;
    std::int32_t value = 0;
    bool pct = false;
};
inline void apply_mod(Stats& s, const StatMod& m) {
    if (m.key == StatKey::Count || !m.value) return;
    if (m.pct) add_stat_pct(s, m.key, m.value);
    else add_stat(s, m.key, m.value);
}

// ---------------------------------------------------------------- keystones
enum class KeystoneId : std::uint8_t { ElementalSurge, Stoneheart, Tailwind, FuelTheFire, ChiRush, StatOnly };  // StatOnly [U-483]: added by a version
struct KeystoneDef {
    KeystoneId id;
    std::string_view name;
    std::string_view desc;
    std::string_view impact;  // what the post-game screen counts
    StatMod mod{StatKey::Count, 0, false};  // [U-483] + a stat (versions can give any keystone one; added keystones only have this)
};
inline constexpr std::array kDefaultKeystones = {
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
inline constexpr std::array kDefaultSpecTrees = {
    SpecTree{"Ferocity", "damage",
             {{{{{"Sharp Mind", {StatKey::ElementalPower, 1'200}}, {"Heavy Hands", {StatKey::Attack, 800}}}},
               {{{"Quick Hands", {StatKey::AttackSpeed, 800}}, {"Focus", {StatKey::ElementalHaste, 800}}}},
               {{{"Piercing", {StatKey::ElementalPenetration, 800}}, {"Long Reach", {StatKey::SkillRange, 6'000}}}}}}},
    SpecTree{"Resolve", "toughness",
             {{{{{"Thick Skin", {StatKey::Defence, 800}}, {"Spirit Ward", {StatKey::ElementalDefence, 800}}}},
               {{{"Endurance", {StatKey::Health, 500, true}}, {"Unshaken", {StatKey::Tenacity, 1'000}}}},
               {{{"Bulk", {StatKey::Health, 12'000}}, {"Rooted Stance", {StatKey::ModelSize, 500}}}}}}},
    SpecTree{"Insight", "casting",
             {{{{{"Calm Breath", {StatKey::CastSpeed, 1'500}}, {"Clarity", {StatKey::ElementalHaste, 1'000}}}},
               {{{"Far Sight", {StatKey::SkillRange, 8'000}}, {"Inner Power", {StatKey::ElementalPower, 1'500}}}},
               {{{"Flow", {StatKey::CastSpeed, 2'000}}, {"Wisdom", {StatKey::ElementalHaste, 1'200}}}}}}},
    SpecTree{"Harmony", "mobility",
             {{{{{"Light Step", {StatKey::MoveSpeed, 1'500}}, {"Small Frame", {StatKey::ModelSize, -500}}}},
               {{{"Swift Strikes", {StatKey::AttackSpeed, 1'000}}, {"Steady", {StatKey::Tenacity, 800}}}},
               {{{"Wind Runner", {StatKey::MoveSpeed, 2'000}}, {"Balance", {StatKey::Health, 8'000}}}}}}},
};

// ---------------------------------------------------------------- runes (stat shards)
struct RuneDef {
    std::string_view name;
    StatMod mod;
};
// rows = slots (0 offense, 1 flex, 2 defense), columns = the shard picked
inline constexpr std::array<std::array<RuneDef, 3>, 3> kDefaultRunes{{
    {{{"+9 elemental power", {StatKey::ElementalPower, 900}}, {"+6 attack", {StatKey::Attack, 600}},
      {"+10 % attack speed", {StatKey::AttackSpeed, 1'000}}}},
    {{{"+9 elemental power", {StatKey::ElementalPower, 900}}, {"+6 attack", {StatKey::Attack, 600}},
      {"+6 defence", {StatKey::Defence, 600}}}},
    {{{"+65 health", {StatKey::Health, 6'500}}, {"+8 elemental defence", {StatKey::ElementalDefence, 800}},
      {"+10 % tenacity", {StatKey::Tenacity, 1'000}}}},
}};
inline constexpr const char* kRuneSlotNames[] = {"Offense", "Flex", "Defense"};

// ---------------------------------------------------------------- [U-318] classic runes (Claude ideas [X-113])
// A page = 9 marks (offence) + 9 seals (defence) + 9 glyphs (utility): each slot is one of 5 kinds of that
// section, so the page stores HOW MANY of each kind (the sum per section is at most 9). Quintessences:
// 3 slots, few kinds, each strong. All free. The numbers are per piece, in the units of stats.hpp.
inline constexpr int kClassicSections = 3, kClassicKinds = 5, kClassicSlots = 9, kQuintSlots = 3, kQuintKinds = 8;  // [U-483] 6 + 2 free places
inline constexpr const char* kClassicSectionNames[] = {"Marks (offence)", "Seals (defence)", "Glyphs (utility)"};
inline constexpr std::array<std::array<RuneDef, kClassicKinds>, kClassicSections> kDefaultClassicRunes{{
    {{{"Mark of Power", {StatKey::ElementalPower, 100}}, {"Mark of Might", {StatKey::Attack, 80}},
      {"Mark of Rhythm", {StatKey::AttackSpeed, 90}}, {"Mark of Piercing", {StatKey::ElementalPenetration, 60}},
      {"Mark of Focus", {StatKey::ElementalHaste, 80}}}},
    {{{"Seal of Hide", {StatKey::Defence, 90}}, {"Seal of Spirit", {StatKey::ElementalDefence, 90}},
      {"Seal of Vigour", {StatKey::Health, 600}}, {"Seal of Growth", {StatKey::Health, 40, true}},
      {"Seal of Resolve", {StatKey::Tenacity, 70}}}},
    {{{"Glyph of Clarity", {StatKey::ElementalHaste, 80}}, {"Glyph of Flow", {StatKey::CastSpeed, 120}},
      {"Glyph of Wind", {StatKey::MoveSpeed, 100}}, {"Glyph of Sight", {StatKey::SkillRange, 500}},
      {"Glyph of Tempo", {StatKey::AttackSpeed, 60}}}},
}};
inline constexpr std::array<RuneDef, 6> kDefaultQuints{{
    {"Quintessence of Power", {StatKey::ElementalPower, 1'500}}, {"Quintessence of Might", {StatKey::Attack, 1'000}},
    {"Quintessence of Vitality", {StatKey::Health, 12'000}}, {"Quintessence of the Bulwark", {StatKey::Defence, 1'200}},
    {"Quintessence of Warding", {StatKey::ElementalDefence, 1'200}}, {"Quintessence of Swiftness", {StatKey::MoveSpeed, 2'000}},
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
inline constexpr std::array<MasteryTree, kMasteryTrees> kDefaultMasteryTrees{{
    {"Offence", "elemental fury",
     {{{{{"Sparks", 3, {StatKey::ElementalPower, 50}}, {"Sharpened Edge", 3, {StatKey::Attack, 40}}, {"Quick Fingers", 2, {StatKey::AttackSpeed, 100}}}},
       {{{"Focus", 3, {StatKey::ElementalHaste, 100}}, {"Cold Steel", 2, {StatKey::ElementalPenetration, 80}}, {"Kindling", 3, {StatKey::ElementalPower, 50}}}},
       {{{"Far Reach", 2, {StatKey::SkillRange, 1'500}}, {"Burning Core", 3, {StatKey::ElementalPower, 60}}, {"Flurry", 3, {StatKey::AttackSpeed, 100}}}},
       {{{"Piercing Flame", 3, {StatKey::ElementalPenetration, 100}}, {"Dragon Strength", 3, {StatKey::Attack, 50}}, {"Spell Mastery", 2, {StatKey::ElementalPower, 80}}}},
       {{{"Avatar's Fury", 1, {StatKey::ElementalPower, 300}}, {"Warlord", 1, {StatKey::Attack, 200}}, {}}}}}},
    {"Defence", "stone and spirit",
     {{{{{"Tough Hide", 3, {StatKey::Defence, 60}}, {"Spirit Ward", 3, {StatKey::ElementalDefence, 60}}, {"Vigour", 2, {StatKey::Health, 400}}}},
       {{{"Endurance", 3, {StatKey::Health, 50, true}}, {"Resolve", 2, {StatKey::Tenacity, 80}}, {"Iron Will", 3, {StatKey::Defence, 60}}}},
       {{{"Stoneskin", 2, {StatKey::ElementalDefence, 60}}, {"Deep Roots", 3, {StatKey::Health, 500}}, {"Calm", 3, {StatKey::Tenacity, 60}}}},
       {{{"Fortitude", 3, {StatKey::Health, 60, true}}, {"Bulwark", 3, {StatKey::Defence, 80}}, {"Warding", 2, {StatKey::ElementalDefence, 80}}}},
       {{{"Mountain Heart", 1, {StatKey::Health, 3'000}}, {"Unbreakable", 1, {StatKey::Tenacity, 400}}, {}}}}}},
    {"Utility", "wind and flow",
     {{{{{"Light Feet", 3, {StatKey::MoveSpeed, 100}}, {"Clear Mind", 3, {StatKey::CastSpeed, 100}}, {"Wisdom", 2, {StatKey::ElementalHaste, 80}}}},
       {{{"Wanderer", 2, {StatKey::MoveSpeed, 100}}, {"Far Sight", 3, {StatKey::SkillRange, 1'500}}, {"Meditation", 3, {StatKey::ElementalHaste, 80}}}},
       {{{"Tailwind Step", 3, {StatKey::MoveSpeed, 100}}, {"Flow", 3, {StatKey::CastSpeed, 120}}, {"Small Frame", 2, {StatKey::ModelSize, -100}}}},
       {{{"Fleet Foot", 3, {StatKey::MoveSpeed, 120}}, {"Echoing Chi", 3, {StatKey::ElementalHaste, 100}}, {"Sage", 2, {StatKey::ElementalPower, 60}}}},
       {{{"Windwalker", 1, {StatKey::MoveSpeed, 500}}, {"Master of Tempo", 1, {StatKey::ElementalHaste, 400}}, {}}}}}},
}};
[[nodiscard]] constexpr int mastery_index(int tree, int tier, int col) { return tree * kMasteryTreeSize + tier * kMasteryPerTier + col; }

// ---------------------------------------------------------------- [U-483] the tables IN USE (game versions edit them)
// Copies of the defaults above with free places: an entry with an EMPTY name is not there (removed by a version, or a
// free place a version can fill). game/loadout/setup_tables.cpp; restore_setup() = back to the defaults.
inline constexpr int kMaxKeystones = 8, kMaxSpecTrees = 6, kMaxShards = 5;
using KeystoneTable = std::array<KeystoneDef, kMaxKeystones>;
using SpecTreeTable = std::array<SpecTree, kMaxSpecTrees>;
using ShardTable = std::array<std::array<RuneDef, kMaxShards>, 3>;
using ClassicTable = std::array<std::array<RuneDef, kClassicKinds>, kClassicSections>;
using QuintTable = std::array<RuneDef, kQuintKinds>;
using MasteryTable = std::array<MasteryTree, kMasteryTrees>;
const KeystoneTable& keystones();
const SpecTreeTable& spec_trees();
const ShardTable& shards();
const ClassicTable& classic_runes();
const QuintTable& quints();
const MasteryTable& mastery_trees();
KeystoneTable& keystones_mut();
SpecTreeTable& spec_trees_mut();
ShardTable& shards_mut();
ClassicTable& classic_runes_mut();
QuintTable& quints_mut();
MasteryTable& mastery_trees_mut();
void restore_setup();
// names that versions set at run time must live somewhere stable
std::string_view keep_setup_name(std::string_view s);

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
[[nodiscard]] inline Loadout validated(Loadout l) {
    // [U-483] a pick of something a version removed (empty name) is cleared too
    if (l.keystone < -1 || l.keystone >= std::int8_t(kMaxKeystones) || (l.keystone >= 0 && keystones()[std::size_t(l.keystone)].name.empty())) l.keystone = -1;
    if (l.tree < -1 || l.tree >= std::int8_t(kMaxSpecTrees) || (l.tree >= 0 && spec_trees()[std::size_t(l.tree)].name.empty())) l.tree = -1;
    for (std::size_t t = 0; t < 3; ++t) {
        auto& n = l.nodes[t];
        if (l.tree < 0 || n < -1 || n > 1 || (n >= 0 && spec_trees()[std::size_t(l.tree)].tiers[t][std::size_t(n)].name.empty())) n = -1;
    }
    for (std::size_t r = 0; r < 3; ++r)
        if (l.runes[r] < -1 || l.runes[r] >= kMaxShards || (l.runes[r] >= 0 && shards()[r][std::size_t(l.runes[r])].name.empty())) l.runes[r] = -1;
    for (std::size_t sec = 0; sec < kClassicSections; ++sec)
        for (std::size_t k = 0; k < kClassicKinds; ++k)
            if (classic_runes()[sec][k].name.empty()) l.classic[sec][k] = 0;
    for (auto& sec : l.classic) {  // [U-318] at most 9 per section
        int left = kClassicSlots;
        for (auto& n : sec) {
            n = std::uint8_t(n > left ? left : n);
            left -= n;
        }
    }
    for (auto& q : l.quints)
        if (q < -1 || q >= kQuintKinds || (q >= 0 && quints()[std::size_t(q)].name.empty())) q = -1;
    int total = 0;  // [U-319] ranks ≤ max, tier unlocked by points in earlier tiers of the same tree, 30 in all
    for (int t = 0; t < kMasteryTrees; ++t) {
        int spent = 0;
        for (int tier = 0; tier < kMasteryTiers; ++tier) {
            int in_tier = 0;
            for (int c = 0; c < kMasteryPerTier; ++c) {
                auto& r = l.masteries[std::size_t(mastery_index(t, tier, c))];
                const int mx = mastery_trees()[std::size_t(t)].tiers[std::size_t(tier)][std::size_t(c)].max_rank;
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
void for_each_mod(const Loadout& l, F&& f) {
    if (l.keystone >= 0 && l.keystone < kMaxKeystones && keystones()[std::size_t(l.keystone)].mod.key != StatKey::Count)  // [U-483]
        f(ModSource::Specialisation, keystones()[std::size_t(l.keystone)].mod);
    if (l.tree >= 0 && l.tree < kMaxSpecTrees)
        for (std::size_t t = 0; t < 3; ++t)
            if (l.nodes[t] >= 0) f(ModSource::Specialisation, spec_trees()[std::size_t(l.tree)].tiers[t][std::size_t(l.nodes[t])].mod);
    for (std::size_t r = 0; r < 3; ++r)
        if (l.runes[r] >= 0 && l.runes[r] < kMaxShards) f(ModSource::Shards, shards()[r][std::size_t(l.runes[r])].mod);
    for (std::size_t sec = 0; sec < kClassicSections; ++sec)  // [U-318]
        for (std::size_t k = 0; k < kClassicKinds; ++k)
            if (l.classic[sec][k]) {
                StatMod m = classic_runes()[sec][k].mod;
                m.value *= l.classic[sec][k];
                f(ModSource::ClassicRunes, m);
            }
    for (auto q : l.quints)
        if (q >= 0 && q < kQuintKinds) f(ModSource::ClassicRunes, quints()[std::size_t(q)].mod);
    for (std::size_t t = 0; t < kMasteryTrees; ++t)  // [U-319]
        for (std::size_t tier = 0; tier < kMasteryTiers; ++tier)
            for (std::size_t c = 0; c < kMasteryPerTier; ++c) {
                const std::uint8_t r = l.masteries[std::size_t(mastery_index(int(t), int(tier), int(c)))];
                if (!r) continue;
                StatMod m = mastery_trees()[t].tiers[tier][c].mod;
                m.value *= r;
                f(ModSource::Masteries, m);
            }
}

// All stat bonuses of a loadout.
inline void apply_loadout(Stats& s, const Loadout& l) {
    for_each_mod(l, [&](ModSource, const StatMod& m) { apply_mod(s, m); });
}

[[nodiscard]] inline bool has_keystone(const Loadout& l, KeystoneId k) {
    return l.keystone >= 0 && l.keystone < kMaxKeystones && keystones()[std::size_t(l.keystone)].id == k && !keystones()[std::size_t(l.keystone)].name.empty();
}

// (the old compile-time checks of apply_loadout / validated moved: the tables are run-time now [U-483])
static_assert([] {  // every tree can take all 30 points
    for (const auto& t : kDefaultMasteryTrees) {
        int n = 0;
        for (const auto& tier : t.tiers)
            for (const auto& m : tier) n += m.max_rank;
        if (n < kMasteryPoints) return false;
    }
    return true;
}());

}  // namespace em
