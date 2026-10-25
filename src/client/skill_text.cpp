// Readable skill / champion numbers. See skill_text.hpp.
#include "client/skill_text.hpp"

#include <format>

#include "client/locale.hpp"

namespace em {

namespace {
const char* kind_name(SkillKind k) {
    switch (k) {
        case SkillKind::None: return "nothing";
        case SkillKind::Bolt: return "projectile";
        case SkillKind::Wave: return "wave (pierces)";
        case SkillKind::Burst: return "blast at a point";
        case SkillKind::Field: return "zone that stays";
        case SkillKind::Cone: return "cone";
        case SkillKind::Dash: return "dash";
        case SkillKind::Wall: return "wall";
        case SkillKind::Pillar: return "raise the ground";
        case SkillKind::Tornado: return "moving tornado";
        case SkillKind::Shield: return "shield";
        case SkillKind::Heal: return "heal";
        case SkillKind::Haste: return "speed up";
        case SkillKind::Blink: return "blink";
        case SkillKind::Lantern: return "hanging light";
        case SkillKind::WindSense: return "sense skills";
        case SkillKind::Flight: return "lift";
        case SkillKind::SelfFreeze: return "freeze yourself";
        case SkillKind::Bloodbend: return "control a target";
        case SkillKind::Boulder: return "raise + push a boulder";
        case SkillKind::Passive: return "passive";
        case SkillKind::WaterSpout: return "ride water";
        case SkillKind::IcePath: return "freeze water";
        case SkillKind::Redirect: return "redirect lightning";
    }
    return "?";
}
const char* cc_name(StatusKind k) {
    static const char* n[] = {"stun", "root", "slow", "silence", "mute", "airborne", "frozen", "blind", "burn", "shield",
                              "haste", "flying", "bloodbound", "channel", "swimming", "water ride", "redirect", "wet"};
    return int(k) < int(std::size(n)) ? n[int(k)] : "";
}
}  // namespace

std::vector<std::string> skill_lines(const SkillDef& s) {
    std::vector<std::string> L;
    L.push_back(std::format("{} — {} ({}){}", s.name, tr(kind_name(s.kind)), tr(element_name(base_element(s.sub))),
                            s.ultimate ? "  · " + tr("ULTIMATE") : ""));
    if (!s.desc.empty()) L.push_back(std::string(s.desc));
    // [U-291] its own passive, cast stages, rank rule
    if (!s.passive_desc.empty()) L.push_back(std::format("{}: {}", tr("Passive"), s.passive_desc));
    if (s.casts > 1) L.push_back(std::format("{} {} {:.1f} s", s.casts, tr("casts, the next within"), s.recast_ms / 1000.0));
    if (s.kind != SkillKind::Passive) {
        const int step = s.rank_step ? s.rank_step : 6;
        if (s.ultimate) L.push_back(std::format("{} {}, {} {}", tr("Unlocks at level"), s.unlock_level ? s.unlock_level : 6, tr("a new rank every"), step));
        else L.push_back(std::format("{} {} {} {}, +1 {} {} {}", tr("Max rank"), s.rank_cap ? s.rank_cap : 2, tr("before level"), step, tr("every"), step, tr("levels")));
    }
    if (s.dmg || s.dmg_rank || s.ep || s.ad || s.bonus_ad) {
        std::string f = std::format("{} = {}", tr("Damage"), s.dmg);
        if (s.dmg_rank) f += std::format(" + {} × ({} − 1)", s.dmg_rank, tr("rank"));
        if (s.ep) f += std::format(" + {}% {}", s.ep, tr("elemental power"));
        if (s.ad) f += std::format(" + {}% {}", s.ad, tr("attack"));
        if (s.bonus_ad) f += std::format(" + {}% {}", s.bonus_ad, tr("bonus attack"));
        L.push_back(f + std::format("   × {}", tr("sky power %")));
    }
    L.push_back(std::format("{} {} ms{}   {} {} ms", tr("Cooldown"), s.cd_ms, s.cd_rank ? std::format(" ({:+} {})", s.cd_rank, tr("per rank")) : "",
                            tr("cast time"), s.cast_ms));
    if (s.cost) L.push_back(std::format("{} {}", tr("Cost"), s.cost));
    std::string geo;
    if (s.range) geo += std::format("{} {}  ", tr("range"), s.range);
    if (s.width) geo += std::format("{} {}  ", tr("width"), s.width);
    if (s.radius) geo += std::format("{} {}  ", tr("radius"), s.radius);
    if (s.speed) geo += std::format("{} {}  ", tr("speed"), s.speed);
    if (!geo.empty()) L.push_back(geo);
    if (s.dur_ms) L.push_back(std::format("{} {} ms   {} {} ms", tr("lasts"), s.dur_ms, tr("ticks every"), s.tick_ms));
    if (s.delay_ms) L.push_back(std::format("{} {} ms", tr("delay"), s.delay_ms));
    if (s.cc != StatusKind::Count) L.push_back(std::format("{} {} ms{}", tr(cc_name(s.cc)), s.cc_ms, s.cc_val ? std::format(" ({})", s.cc_val) : ""));
    if (s.knock) L.push_back(std::format("{} {}", tr("knock back"), s.knock));
    if (s.casts > 1) L.push_back(std::format("{} {} / {} ms", tr("recasts"), s.casts, s.recast_ms));
    if (s.pierce) L.push_back(tr("pierces"));
    if (s.group) L.push_back(tr("group bending: allies of the same element can feed it"));
    if (s.targeted) L.push_back(tr("targeted (point and click)"));
    if (s.sweet_pct) L.push_back(std::format("{} +{}%", tr("sweet spot"), s.sweet_pct));
    if (!s.turns_caster) L.push_back(tr("cast without turning"));
    if (s.through_walls) L.push_back(tr("goes through walls"));
    if (s.aim == Aim::Facing) L.push_back(tr("aims where the champion looks"));
    if (s.melee) L.push_back(tr("melee: can't reach units in the air"));
    if (s.ground) L.push_back(tr("ground skill: can't reach units in the air"));
    if (s.reach_height >= 0) L.push_back(std::format("{} {}", tr("reaches height"), s.reach_height));
    L.push_back(std::format("{}: {}", tr("moving while casting"),
                            s.move == CastMove::Free ? tr("free") : s.move == CastMove::Slowed ? tr("slowed") : tr("rooted")));
    return L;
}

std::vector<std::string> champion_lines(const ChampionDef& c) {
    std::vector<std::string> L;
    auto stat = [&](std::string_view n, std::int32_t base, std::int32_t lvl, std::int32_t div = 100) {
        L.push_back(lvl ? std::format("{:<22} {:>7.2f}  (+{:.2f} {})", tr(n), base / double(div), lvl / double(div), tr("per level"))
                        : std::format("{:<22} {:>7.2f}", tr(n), base / double(div)));
    };
    stat("Health", c.hp, c.hp_lvl);
    stat("Attack", c.attack, c.attack_lvl);
    stat("Elemental power", 0, c.ep_lvl);
    stat("Defence", c.def, c.def_lvl);
    stat("Elemental defence", c.edef, c.edef_lvl);
    L.push_back(std::format("{:<22} {:>7.2f}  (+{:.2f}% {})", tr("Attack speed"), c.as / 100.0, c.as_lvl / 100.0, tr("per level")));
    stat("Move speed", c.ms, 0);
    L.push_back(std::format("{:<22} {:>7}  ({})", tr("Attack range"), c.attack_range, tr("edge to edge")));
    if (c.res_max) L.push_back(std::format("{:<22} {:>7}  (+{:.1f}/s)", tr("Resource"), c.res_max / 100, c.res_regen / 100.0));
    L.push_back(std::format("{}: {}{}", tr("Autoattack"), c.range == Range::Ranged ? tr("ranged") : tr("melee"),
                            c.range == Range::Ranged && c.auto_projectile ? std::format(", {}", tr("projectile")) : ""));
    if (c.autos.move_while_attacking) L.push_back(tr("attacks while moving"));
    L.push_back(std::format("{}: {}   {}: {}", tr("Class"), tr(role_name(c.role)), tr("Scales with"), c.mage ? tr("elemental power") : tr("attack")));
    return L;
}

}  // namespace em
