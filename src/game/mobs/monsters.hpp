#pragma once
// [U-308] Every jungle monster in ONE table — change a number here to balance it.
//   hp / attack / defence / elemental_defence in hundredths (100 = 1), move_speed in units/s × 100,
//   attack_speed = attacks per second × 100, ranges in game units (edge to edge),
//   gold / xp = the kill reward, patience = how far from its camp it chases (LoL "leash"),
//   give_up_ms = not hit for this long while out of patience → it walks home,
//   reset_heal_pct = % of max HP healed per second while walking home (and when the fight ends).
// Claude idea numbers (placeholders).
#include <array>
#include <cstdint>
#include <string_view>

#include "core/hitbox.hpp"
#include "core/stats.hpp"

namespace em {

enum class UnitCategory : std::uint8_t;  // world.hpp

// [U-366] jungle buffs (Claude ideas, Avatar themed) — what the killer gets
//   Ember (Platypus Bear): autoattacks burn (1 % max HP over 2 s) and slow 15 % for 1 s
//   Tui's Blessing (Moon Koi): resource regen +50 %, cooldowns × 0.90
enum class JungleBuff : std::uint8_t { None, Ember, TuiBlessing };
inline constexpr int kTuiCdPct = 90, kTuiRegenPct = 150, kEmberSlowPct = 15;
inline constexpr int kLionTurtlePctPerStack = 4;  // [U-364] every Lion Turtle stack: +4 % power for the team
// [U-364] what an epic monster gives the killer's team
enum class TeamReward : std::uint8_t { None, LionTurtleStack, DragonGold, FaceSteal };

struct MonsterDef {
    std::string_view name;
    const BodyDef* body = &kHumanoidBody;
    bool boss = false;           // an EPIC monster (objective)
    bool aggressive = false;     // attacks on sight [X-97]
    bool body_blocks = false;    // [X-96] (old switch: every unit blocks now [U-367])
    bool spirit = false;         // [U-367] a spirit: walks through units and units walk through it
    std::int32_t hp = 140'000, attack = 3'500, defence = 2'000, elemental_defence = 2'000;
    std::int32_t move_speed = 30'000, attack_speed = 70, attack_range = 40;
    Range range = Range::Melee;
    std::int32_t model_size = 0;  // [U-129] +size (100 = 1 %) — the model follows the hitbox [U-304]
    std::int32_t sight = 600;
    std::int32_t gold = 70, xp = 140;
    std::int32_t patience = 1'100;
    std::int64_t give_up_ms = 2'500;
    std::int32_t reset_heal_pct = 5;
    // [U-365] timers and growth — every respawn makes it stronger and worth more (LoL-like scaling)
    std::int64_t respawn_ms = 120'000;   // after death
    std::int64_t first_spawn_ms = 0;     // 0 = at the start; else it appears this late
    std::int32_t hp_growth_pct = 10, attack_growth_pct = 8;  // + this % per respawn (of the base)
    std::int32_t gold_growth = 5, xp_growth = 10;            // + this much per respawn
    // [U-366][U-364] rewards
    JungleBuff buff = JungleBuff::None;
    std::int64_t buff_ms = 0;
    TeamReward team_reward = TeamReward::None;
    std::int32_t team_value = 0;  // DragonGold: gold for every champion of the team; LionTurtleStack: % power per stack
};

inline constexpr std::array kMonsters = {
    MonsterDef{.name = "Wandering Spirit", .spirit = true},
    MonsterDef{.name = "Spirit Wolf", .body = &kBeastBody, .spirit = true},
    MonsterDef{.name = "Cursed Spirit", .body = &kSlimBody, .aggressive = true, .spirit = true, .hp = 120'000, .attack = 5'000, .defence = 2'500,
               .elemental_defence = 2'500, .move_speed = 31'000, .attack_speed = 60, .attack_range = 380, .range = Range::Ranged,
               .sight = 700, .gold = 85, .xp = 160},
    MonsterDef{.name = "Angry Beast", .body = &kBeastBody, .aggressive = true, .body_blocks = true, .hp = 220'000, .attack = 6'500,
               .defence = 2'500, .elemental_defence = 2'500, .move_speed = 31'000, .attack_speed = 65, .sight = 700, .gold = 110, .xp = 200},
    MonsterDef{.name = "Bandit", .aggressive = true, .hp = 90'000, .attack = 4'000, .defence = 2'500, .elemental_defence = 2'500,
               .move_speed = 31'000, .attack_speed = 75, .sight = 700, .gold = 60, .xp = 110},
    MonsterDef{.name = "Bandit Archer", .aggressive = true, .hp = 80'000, .attack = 4'000, .defence = 2'500, .elemental_defence = 2'500,
               .move_speed = 31'000, .attack_speed = 75, .attack_range = 420, .range = Range::Ranged, .sight = 700, .gold = 60, .xp = 110},
    MonsterDef{.name = "Face Stealer", .body = &kBeastBody, .boss = true, .body_blocks = true, .hp = 600'000, .attack = 12'000,
               .defence = 6'000, .elemental_defence = 6'000, .move_speed = 32'000, .attack_speed = 60, .attack_range = 60,
               .model_size = 6'000, .sight = 700, .gold = 300, .xp = 600, .patience = 1'400, .give_up_ms = 3'000, .reset_heal_pct = 8,
               .respawn_ms = 300'000, .team_reward = TeamReward::FaceSteal},
    // [U-366] buff camps (Claude ideas)
    MonsterDef{.name = "Platypus Bear", .body = &kBeastBody, .aggressive = true, .hp = 260'000, .attack = 7'000, .defence = 3'000,
               .elemental_defence = 2'500, .attack_speed = 60, .model_size = 1'500, .sight = 700, .gold = 100, .xp = 220,
               .respawn_ms = 150'000, .buff = JungleBuff::Ember, .buff_ms = 90'000},
    MonsterDef{.name = "Moon Koi", .spirit = true, .hp = 220'000, .attack = 5'000, .defence = 2'500, .elemental_defence = 3'500,
               .attack_speed = 60, .attack_range = 350, .range = Range::Ranged, .sight = 700, .gold = 100, .xp = 220,
               .respawn_ms = 150'000, .buff = JungleBuff::TuiBlessing, .buff_ms = 90'000},
    // [U-364] epic monsters (Claude ideas): a stacking team buff and a gold one
    MonsterDef{.name = "Lion Turtle", .body = &kBeastBody, .boss = true, .hp = 900'000, .attack = 10'000, .defence = 7'000,
               .elemental_defence = 7'000, .move_speed = 20'000, .attack_speed = 45, .attack_range = 120, .model_size = 9'000,
               .sight = 800, .gold = 200, .xp = 500, .patience = 900, .give_up_ms = 3'000, .reset_heal_pct = 8,
               .respawn_ms = 360'000, .first_spawn_ms = 360'000, .hp_growth_pct = 15, .attack_growth_pct = 10,
               .team_reward = TeamReward::LionTurtleStack, .team_value = 4},
    MonsterDef{.name = "Sun Dragons", .body = &kBeastBody, .boss = true, .hp = 750'000, .attack = 11'000, .defence = 5'000,
               .elemental_defence = 6'000, .move_speed = 30'000, .attack_speed = 55, .attack_range = 300, .range = Range::Ranged,
               .model_size = 6'000, .sight = 800, .gold = 150, .xp = 450, .patience = 1'000, .give_up_ms = 3'000, .reset_heal_pct = 8,
               .respawn_ms = 420'000, .first_spawn_ms = 600'000, .hp_growth_pct = 12, .attack_growth_pct = 10, .gold_growth = 25,
               .team_reward = TeamReward::DragonGold, .team_value = 250},
};

[[nodiscard]] constexpr int monster_index(std::string_view name) {
    for (std::size_t i = 0; i < kMonsters.size(); ++i)
        if (kMonsters[i].name == name) return int(i);
    return -1;
}

}  // namespace em
