// [U-397] Every interaction of the systems in one list — generated from the SAME tables / constants the game uses,
// so it can never be out of date. Shown in the menu (Interactions) and written to docs/INTERACTIONS.md
// (./build/dump_content --interactions).
#include "game/reference/interactions.hpp"

#include <format>

#include "core/combos.hpp"
#include "core/elements.hpp"
#include "core/sky_effects.hpp"
#include "core/wall_rules.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mobs/rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
constexpr const char* kSubNames[] = {"Water", "Ice", "Wood", "Blood", "Mud", "Steam", "Air", "Earth", "Lava", "Metal", "Earth armour",
                                     "Metal armour", "Fire", "Blue fire", "Lightning", "Combustion", "Physical"};
constexpr const char* kElNames[] = {"Water", "Earth", "Fire", "Air", "None"};
const char* hit_name(WallHit h) {
    switch (h) {
        case WallHit::Stopped: return "stopped (damages the wall)";
        case WallHit::Melts: return "melts it (spent on it)";
        case WallHit::PassesOver: return "passes over";
        case WallHit::BreaksThrough: return "breaks through";
    }
    return "?";
}
const char* combo_name(Combo c) {
    switch (c) {
        case Combo::Firestorm: return "Firestorm: the fire skill +50 % size and damage";
        case Combo::Steam: return "Steam: a steam cloud (blocks vision)";
        case Combo::Lava: return "Lava: a lava pool that burns";
        case Combo::IceShards: return "Ice shards: freezes, slows 40 %";
        case Combo::Mud: return "Mud: a mud field, slows 50 %";
        case Combo::Sandstorm: return "Sandstorm: blinds";
        default: return "-";
    }
}
}  // namespace

std::vector<std::string> interaction_lines() {
    std::vector<std::string> L;
    auto h = [&](std::string s) { L.push_back("## " + std::move(s)); };
    auto l = [&](std::string s) { L.push_back(std::move(s)); };

    h("Element combos (a skill of another element flying through a zone)");
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
            l(std::format("{} + {} → {}", kElNames[a], kElNames[b], combo_name(combo_of(Element(a), Element(b)))));
    l(std::format("Group bending: every ally of the same element feeding a zone: +{} % size, +{} % damage, +{} s (max {} feeds)",
                  group_size_pct(1) - 100, group_damage_pct(1) - 100, group_extra_ms(1) / 1000, kMaxGroupFeeds));

    h("Skills against walls (what happens when a skill meets a wall)");
    for (int s = 0; s <= int(SubElement::Physical); ++s) {
        const AttackInfo ai{.sub = SubElement(s)};
        l(std::format("{:<13} vs ice: {:<28} vs stone / metal: {}", kSubNames[s], hit_name(wall_hit(WallKind::Ice, ai)), hit_name(wall_hit(WallKind::Stone, ai))));
    }
    l("Air / fire from above (flying) pass over stone walls; water from above passes over every wall; earth stops water only partly (50 % goes on as mud)");

    h("Wall damage by element (% of the hit's damage; ice / stone / metal)");
    for (int s = 0; s <= int(SubElement::Physical); ++s)
        l(std::format("{:<13} ice {:>3} %   stone {:>3} %   metal {:>3} %", kSubNames[s], wall_effect_pct(Wall::Ice, SubElement(s)),
                      wall_effect_pct(Wall::Stone, SubElement(s)), wall_effect_pct(Wall::Metal, SubElement(s))));

    h("Walls (the standard)");
    l(std::format("Skill walls: segments {} units long, {} / {} / {} thick, 1 or 2 levels high; HP per segment {} (ice {}, metal {}); a segment falls as a whole",
                  kWallSegLen, kWallThickStep, kWallThickStep * 2, kWallThickStep * 3, kWallSegHp, kIceWallSegHp, kMetalSegHp));
    l("A unit whose whole body fits on a rising segment is lifted on top: level 1 = it can walk off, level 2 = trapped without mobility");
    l(std::format("Metal walls: one thin segment by default; lightning within {} units bends towards them and hits everything within {} units of the struck piece",
                  kMetalPull, kMetalSplash));
    l(std::format("Single cells (map rocks, lava rocks, thrown rocks): ice {} / stone {} / metal {} HP; map rocks × {} % and grow back after {} s",
                  wall_max_hp(Wall::Ice), wall_max_hp(Wall::Stone), wall_max_hp(Wall::Metal), elements::kNaturalWallHpPct, elements::kNaturalRockRegrowMs / 1000));
    l(std::format("Lava on the ground melts walls next to it every {} ms: stone {}, ice {}, metal {} HP; touching ice halves the lava's rest time once; {} % of lava cells cool into a rock",
                  elements::kLavaMeltMs, elements::kLavaMeltStone, elements::kLavaMeltIce, elements::kLavaMeltMetal, elements::kLavaRockPct));
    l("Thrown earth / lava bolts leave a small rock where they land; earth bolts / waves can lift a chosen rock first (click the rock, then the target): +25 % range, +20 % damage");
    l("Lifted stones (boulders) block walking like walls; Stone Push throws anyone's lifted stone");
    l("Some autoattacks hit walls (right click on a wall) — per champion");

    h("Water");
    l(std::format("Wet lasts {} s ({} s for waterbenders); a wet unit cannot catch fire; the first fire hit on a wet unit −{} % and dries it",
                  elements::kWetMs / 1000, elements::kWetWaterbenderMs / 1000, elements::kWetVsFirePct));
    l(std::format("Water on lava cools it into stone and makes steam ({} s); Heat Draw turns steam into puddles ({} s) and gives {} fuel",
                  elements::kSteamMs / 1000, elements::kPuddleMs / 1000, elements::kHeatDrawFuel / 100));
    l("Ice through steam condenses it into a puddle (or an ice patch); ice patches slow");
    l(std::format("Water skills from a source (river, sea, glacier, puddle, ice patch, ice wall) cost no carried water; chosen source: +{} % range, +{} % damage",
                  elements::kSourceRangePct - 100, elements::kWaterSourceBonusPct));
    l(std::format("Ice walls cost more water than a bender carries → need a source; an ice wall used as a source loses {} HP; the Avatar carries {} water ({} per skill)",
                  elements::kIceWallMeltPerUse, elements::kAvatarWaterMax / 100, elements::kAvatarWaterCost / 100));
    l("Moving water (Draw Stream…): the water is taken from the source (a river dries for a while, a puddle moves) and lies at the target as a new source");
    l("Swimming: no spells (only water mobility); freezing / slowing CC on a swimmer lasts 50 % longer; ice on a swimmer = an icing stun");

    h("Lightning");
    l("Lightning chains through connected shallow water; a redirect window sends incoming lightning to the cursor — also lightning thrown by an ALLY");
    l("Metal walls attract lightning (see Walls)");

    h("Sky (day, moon, comet, eclipse, alignment)");
    for (int pct : {0, 50, 100})
        l(std::format("Moon phase {:>3} %: water power +{:.1f} %", pct, moon_power_pm(pct) / 10.0));
    l(std::format("Full moon: water ranges / sizes +{} % (water autoattacks too), cooldowns × 0.{}", kFullMoonSizePct, kFullMoonCdPct));
    l(std::format("Comet = {} × full moon for fire: power +{} %, sizes +{} %, cooldowns × 0.{}, unlimited fuel", kCometMoonFactor,
                  kCometMoonFactor * moon_power_pm(100) / 10, kCometMoonFactor * kFullMoonSizePct, 100 - kCometMoonFactor * (100 - kFullMoonCdPct)));
    l("Solar eclipse: fire loses power towards the middle and is muted in the middle; planetary alignment: every bender's magic +25 % power / size / duration");
    l("Day: fire +5 % power, fuel regenerates faster towards noon; night: shorter sight (by the moon's light)");

    h("Crowd control and fights");
    l(std::format("Displacement (knock) = CC: the unit is held for distance / {} units per s", World::kKnockSpeed));
    l("Everybody collides with everybody (allies, minions, monsters); spirits, ghost items and mist form pass through");
    l(std::format("Champion kill: bounty {} gold (+{} per kill in a row, max {}; −{} per death in a row, min {}); assisters share {} %; XP {} + {} × level, shared nearby",
                  rules::kBaseBounty, rules::kBountyPerKill, rules::kMaxBounty, rules::kBountyPerDeath, rules::kMinBounty, rules::kAssistGoldPct,
                  rules::champion_kill_xp(0), rules::champion_kill_xp(1) - rules::champion_kill_xp(0)));
    l("A champion who hit the victim in the last 10 s gets the kill even when a turret / minion / monster lands the last hit");

    h("Jungle");
    for (const MonsterDef& m : kMonsters)
        l(std::format("{:<18} {:>6} HP  {:>4} gold  {:>4} XP  respawn {:>3} s{}{}{}", m.name, m.hp / 100, m.gold, m.xp, m.respawn_ms / 1000,
                      m.spirit ? "  spirit (passes through units)" : "", m.buff == JungleBuff::Ember ? "  → Ember buff" : m.buff == JungleBuff::Wisdom ? "  → Wisdom buff" : "",
                      m.team_reward == TeamReward::LionTurtleStack ? std::format("  → team +{} % power per stack", m.team_value)
                      : m.team_reward == TeamReward::DragonGold    ? std::format("  → {} gold for every team champion", m.team_value)
                      : m.team_reward == TeamReward::FaceSteal     ? std::string("  → steals a skill from a killer")
                                                                   : std::string()));
    l("Every respawn: +hp_growth % HP, +attack_growth % attack, more gold and XP (per monster in monsters.hpp)");

    h("Travel companions (recall)");
    l(std::format("The chosen animal runs from the base at {} units/s, carries the champion home at the same speed; it cannot be stopped or cancelled; the rider can be hit",
                  kCompanionSpeed));
    return L;
}

}  // namespace em
