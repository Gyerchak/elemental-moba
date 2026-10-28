// [U-237][U-253][U-255] See modifiers.hpp.
#include "client/modifiers.hpp"

#include <format>
#include <iterator>

#include "client/locale.hpp"
#include "core/modifiers.hpp"
#include "core/sky_effects.hpp"
#include "core/status.hpp"
#include "core/surface_rules.hpp"

namespace em {

namespace {
struct Look {
    const char* tag;
    const char* name;
    const char* detail;
    Rgba colour;
    bool debuff;
};
constexpr Look kStatus[] = {
    {"STN", "Stun", "cannot move, attack or cast", {255, 220, 60}, true},
    {"ROT", "Root", "cannot move", {120, 200, 80}, true},
    {"SLW", "Slow", "moves slower", {120, 160, 255}, true},
    {"SIL", "Silence", "cannot use abilities", {200, 120, 255}, true},
    {"MUT", "Mute", "no bending: no abilities, no elemental autoattacks", {160, 160, 160}, true},
    {"AIR", "Airborne", "knocked up: cannot act", {230, 240, 255}, true},
    {"FRZ", "Frozen", "frozen in ice: cannot act, no heat", {170, 230, 255}, true},
    {"BLD", "Blind", "sees less, no targeted skills", {90, 90, 90}, true},
    {"BRN", "Burn", "takes fire damage every second", {255, 120, 40}, true},
    {"SHD", "Shield", "absorbs damage", {235, 235, 235}, false},
    {"HST", "Haste", "moves faster", {120, 255, 160}, false},
    {"FLY", "Flying", "in the air: melee and ground skills miss", {200, 230, 255}, false},
    {"BLB", "Bloodbound", "moved by a bloodbender", {200, 30, 50}, true},
    {"CHN", "Channelling", "channelling (recall, portal, group bending)", {200, 220, 255}, false},
    {"SWM", "Swimming", "half speed, no spells except water mobility", {60, 110, 220}, true},
    {"WAT", "Water ride", "raised on water: melee and ground skills miss", {90, 170, 255}, false},
    {"RDR", "Redirection", "lightning redirection window is open", {255, 240, 120}, false},
    {"WET", "Wet", "lightning damage taken +25 %; the next fire hit −25 % (then dry)", {80, 160, 255}, true},
};
static_assert(std::size(kStatus) == std::size_t(StatusKind::Count));

constexpr Look kExtra[] = {
    {"SRG", "Elemental Surge", "hits on the same champion (3rd = bonus damage)", {255, 150, 255}, false},
    {"SHC", "Stoneheart", "cooling down", {170, 140, 100}, false},
    {"CHI", "Chi Rush", "next autoattack +30 % damage", {255, 230, 150}, false},
    {"LOT", "Lotus", "cast speed up", {255, 170, 220}, false},
    {"FLK", "Flask", "cannot die, strong healing", {120, 255, 200}, false},
    {"UNT", "Untargetable", "cannot be targeted", {230, 230, 255}, false},
    {"WND", "Wind Sense", "senses moving units through the wind", {220, 240, 200}, false},
    {"BIG", "Bigger", "model size up", {200, 200, 150}, false},
    {"STL", "Skill stolen", "one skill was stolen", {150, 90, 160}, true},
    {"AVA", "Avatar State", "flying, +50 % power, +25 % skill size", {120, 230, 255}, false},
    {"CYC", "Elemental Cycle", "elements used in a row (4 = next skill +40 %)", {200, 255, 230}, false},
    {"MAG", "Magic stance", "your skills and autoattacks are elemental magic now", {200, 160, 255}, false},
    {"ICE", "Ice form", "water from puddles / steam comes as ice (K switches)", {200, 240, 255}, false},
};
static_assert(std::size(kExtra) == std::size_t(ModKind::Count));

ModView view(const Look& l, int value, std::int64_t left) {
    return {l.tag, tr(l.name), tr(l.detail), l.colour, l.debuff, false, left, value};
}
std::string pct(int v) { return std::format("{:+} %", v); }
}  // namespace

Rgba status_colour(int k) { return k >= 0 && k < int(StatusKind::Count) ? kStatus[k].colour : Rgba{200, 200, 200}; }
const char* status_name(int k) { return k >= 0 && k < int(StatusKind::Count) ? kStatus[k].name : ""; }

std::vector<ModView> env_modifiers(Element e, const SkyState& s) {
    std::vector<ModView> out;
    auto add = [&](const char* tag, const char* name, std::string detail, Rgba c, bool debuff, bool minor) {
        out.push_back({tag, tr(name), std::move(detail), c, debuff, true, -1, 0, minor});
    };
    const SkyBoost b = sky_boost(e, s);
    const int sight = vision_range_pct(s);
    // day / night: sight for everybody, +5 % power for fire by day [X-53]; water follows the moon [U-370]
    if (s.is_day && !s.solar_eclipse)
        add("DAY", "Day", e == Element::Fire ? std::format("{} {}, {} {}", tr("fire power"), pct(5), tr("fuel regen"), pct(fuel_regen_pct(s) - 100))
                                             : std::format("{} {} %", tr("sight"), sight), {255, 220, 120}, false, e != Element::Fire);
    if (!s.is_day)
        add("NGT", "Night", std::format("{} {} %", tr("sight"), sight), {120, 130, 200}, sight < 100, true);
    // [U-370] the moon, all month long: water power +0.5 % per 1 % of moon phase (0 % new … 100 % full)
    add("MOO", "Moon", e == Element::Water ? std::format("{} {} %, {} +{:.1f} %", tr("moon phase"), s.moon_light_pct, tr("water power"),
                                                         moon_power_pm(s.moon_light_pct) / 10.0)
                                           : std::format("{} {} %", tr("moon phase"), s.moon_light_pct),
        {200, 210, 240}, false, e != Element::Water);
    if (s.full_moon)
        add("MON", "Full moon", e == Element::Water ? std::format("{} {}, {} {}, {} −10 %, XP {}, {}", tr("ranges / sizes (also water autoattacks)"), pct(kFullMoonSizePct),
                                                              tr("radius / length"), pct(kFullMoonSizePct), tr("cooldowns"), pct(25), tr("bloodbending allowed"))
                                                    : tr("waterbenders: +50 % power, +25 % ranges, −10 % cooldowns"), {235, 240, 255}, false, e != Element::Water);
    if (s.new_moon) add("NEW", "New moon", e == Element::Water ? std::format("{} +0 %", tr("water power")) : tr("darkest nights"), {70, 70, 110}, false, true);
    if (s.solar_eclipse) {
        const bool fire = e == Element::Fire;
        add("ECL", "Solar eclipse", fire ? std::format("{} {} %{}", tr("fire power"), b.power_pct, b.muted ? std::string(" — ") + tr("FIRE MUTED") : "")
                                         : tr("firebenders lose their power; darkness"), {150, 60, 60}, fire, !fire);
    }
    if (s.comet_landed && unlimited_fuel(s))
        add("CMT", "Comet", e == Element::Fire ? std::format("{} {}, {} {}, XP {}, {}", tr("fire power"), pct(75), tr("skill size"), pct(40), pct(40), tr("unlimited fuel"))
                                               : tr("firebenders +75 % power"), {255, 90, 60}, false, e != Element::Fire);
    if (s.planetary_alignment)
        add("ALN", "Planetary alignment", e != Element::None ? std::format("{}: {} {}, {} {}, {} {}, {} {}, {} {}, {} {}", tr("magic skills"),
                                                                           tr("power"), pct(25), tr("width"), pct(25), tr("radius"), pct(25),
                                                                           tr("speed"), pct(25), tr("duration"), pct(25), tr("DoT duration"), pct(25))
                                                             : tr("all benders +25 % (not non-benders)"), {200, 140, 255}, false, e == Element::None);
    return out;
}

std::vector<ModView> terrain_modifiers(const Cell& c, Element e) {
    std::vector<ModView> out;
    if (c.bush() == Bush::Alive) out.push_back({"BSH", tr("Brush"), tr("hidden from enemies outside the brush"), {60, 150, 60}, false, false, -1, 0});
    const Surface s = c.surface();
    if (e == Element::Earth && !earth_bendable(s))
        out.push_back({"NOE", tr("No earth"), std::format("{}: {}", tr(surface_name(s)), tr("earthbending not possible here")), {150, 110, 70}, true, false, -1, 0});
    if (s == Surface::Glacier || s == Surface::Water)
        if (e == Element::Water) out.push_back({"H2O", tr(surface_name(s)), tr("water all around"), {90, 170, 255}, false, false, -1, 0});
    if (s == Surface::Magma) out.push_back({"MAG", tr("Magma"), tr("burning ground"), {255, 110, 30}, true, false, -1, 0});
    return out;
}

ModView wire_modifier(std::uint8_t kind, int value, std::int64_t left_ms) {
    if (kind < kModWireBase) {
        if (kind >= std::size(kStatus)) return {"?", "?", "", {200, 200, 200}, false, false, left_ms, value};
        ModView v = view(kStatus[kind], value, left_ms);
        if (value != 0 && (kind == std::uint8_t(StatusKind::Slow) || kind == std::uint8_t(StatusKind::Haste)))
            v.detail += std::format(" ({} %)", value);
        else if (value != 0 && kind == std::uint8_t(StatusKind::Burn))
            v.detail += std::format(" ({} / s)", value);
        else if (value != 0 && kind == std::uint8_t(StatusKind::Shield))
            v.detail += std::format(" ({})", value);
        return v;
    }
    const int k = kind - kModWireBase;
    if (k >= int(std::size(kExtra))) return {"?", "?", "", {200, 200, 200}, false, false, left_ms, value};
    ModView v = view(kExtra[k], value, left_ms);
    if (k == int(ModKind::SurgeStacks)) v.detail = std::format("{} / 3 — {}", value, v.detail);
    if (k == int(ModKind::Bigger)) v.detail += std::format(" (+{} %)", value);
    if (k == int(ModKind::SourceIce)) v.left_ms = -1;
    if (k == int(ModKind::Cycle)) {
        v.detail = value >= 4 ? tr("READY: the next skill deals +40 %") : std::format("{} / 4 {}", value, tr("elements used"));
        v.left_ms = -1;
    }
    return v;
}

void status_modifiers(std::uint32_t bits, std::vector<ModView>& out) {
    for (int k = 0; k < int(StatusKind::Count); ++k)
        if ((bits >> k) & 1) out.push_back(view(kStatus[k], 0, -1));
}

NextEvent next_sky_event(std::int64_t ms) {
    // event starts in real time: eclipses = sunrise of game days 12 and 17, full moon = day 15 (and every 30 days),
    // comet 30:00, alignment 60:00
    NextEvent best{"", 0};
    auto consider = [&](const char* name, std::int64_t at) {
        if (at > ms && (best.name.empty() || at - ms < best.in_ms)) best = {tr(name), at - ms};
    };
    for (int d : WorldClock::kEclipseDays) consider("Solar eclipse", std::int64_t(d) * 120'000 + 30'000);
    for (int c = 0; c < 4; ++c) consider("Full moon", (std::int64_t(WorldClock::kFullMoonAge) + c * WorldClock::kMoonCycleDays) * 120'000);
    consider("Comet", WorldClock::kCometMs);
    consider("Planetary alignment", WorldClock::kAlignStartMs);
    return best;
}

}  // namespace em
