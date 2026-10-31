// [U-210][U-277] Own champions from `.champ` text. Shared by the client (files on disk) and the server
// (which sends its files to every player who joins). See client/custom_champions.hpp for the file format.
#include "game/champions/custom.hpp"

#include <algorithm>
#include <cctype>
#include <type_traits>
#include <charconv>
#include <cmath>
#include <deque>
#include <format>
#include <sstream>

#include "game/content.hpp"

namespace em {

namespace {
std::deque<std::string> g_strings;  // keeps names alive (ChampionDef stores string_view)

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// [U-415] full own champions: any skill field by name. Enum values by their names (the order of the enums).
constexpr std::string_view kKinds[] = {"None", "Bolt", "Wave", "Burst", "Field", "Cone", "Dash", "Wall", "Pillar", "Tornado", "Shield",
                                       "Heal", "Haste", "Blink", "Lantern", "WindSense", "Flight", "SelfFreeze", "Bloodbend", "Boulder",
                                       "Passive", "WaterSpout", "IcePath", "Redirect", "AvatarState", "MagicStance", "StonePush", "Transform", "MoveWater"};
constexpr std::string_view kGeos[] = {"Auto", "Circle", "Ring", "Line", "Rect", "Cone", "Arc"};  // [U-462]
constexpr std::string_view kZones[] = {"Auto", "Centre", "Edge", "Tip"};                       // [U-461]
constexpr std::string_view kSubs[] = {"Water", "Ice", "Wood", "Blood", "Mud", "Steam", "Air", "Earth", "Lava", "Metal", "EarthArmor",
                                      "MetalArmor", "Fire", "BlueFire", "Lightning", "Combustion", "Physical"};
constexpr std::string_view kStatuses[] = {"Stun", "Root", "Slow", "Silence", "Mute", "Airborne", "Frozen", "Blind", "Burn", "Shield",
                                          "Haste", "Flying", "Bloodbound", "Channel", "Swimming", "WaterRide", "Redirect", "Wet", "Grievous", "Fear", "Disarm", "Subdue", "Hex", "Break", "DebuffImmune", "None"};
// [U-474] "ChiBlock" = the chi block status (its wire / enum name is Mute)
int status_index(std::string_view v);
constexpr std::string_view kElements[] = {"Water", "Earth", "Fire", "Air", "None"};
constexpr std::string_view kResources[] = {"Water", "Fuel", "None", "Ammo", "Energy", "Rage", "Mana"};
constexpr std::string_view kRoles[] = {"Mage", "Marksman", "Fighter", "Tank", "Assassin", "Support"};
constexpr std::string_view kShapes[] = {"Single", "Splash", "Line", "Chain"};
template <std::size_t N>
int enum_of(const std::string_view (&names)[N], std::string_view v) {
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == v) return int(i);
    return -1;
}
int status_index(std::string_view v) { return v == "ChiBlock" ? int(StatusKind::Mute) : enum_of(kStatuses, v); }  // [U-474]
std::string_view keep(const std::string& s) {  // string_view fields need a stable copy
    auto it = std::ranges::find(g_strings, s);
    if (it == g_strings.end()) g_strings.push_back(s.substr(0, 400)), it = std::prev(g_strings.end());
    return *it;
}
// [U-415] names: letters, digits, space, ' _ - only (used in file names / settings lines), 1 … 24 characters
bool good_name(std::string_view n) {
    return !n.empty() && n.size() <= 24 && std::ranges::all_of(n, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '\'' || c == '_' || c == '-';
    });
}

// [U-462] STANDARD SKILLS for building champions: `w = @circle_blast`, then change only the numbers (w.dmg = …).
// Every one is just values of the core mechanics (SkillKind = what it does, Geo = its shape, SweetZone …).
const std::vector<SkillDef>& skill_templates() {
    static const std::vector<SkillDef> t = [] {
        std::vector<SkillDef> v;
        auto add = [&](std::string_view name, std::string_view desc, auto&& set) {
            SkillDef s;
            s.name = name, s.desc = desc, s.sub = SubElement::Physical, s.dmg = 80, s.dmg_rank = 40, s.ad = 60, s.cast_ms = 250,
            s.cd_ms = 8'000, s.cost = 30;
            set(s);
            v.push_back(s);
        };
        add("@bolt", "A projectile that stops on the first enemy; the end of its flight hits harder",
            [](SkillDef& s) { s.kind = SkillKind::Bolt, s.range = 1'000, s.width = 70, s.speed = 1'600, s.sweet_pct = 25; });
        add("@line", "An instant straight cut from you", [](SkillDef& s) { s.kind = SkillKind::Cone, s.geo = Geo::Line, s.range = 600, s.width = 120, s.melee = true; });
        add("@cone", "An instant fan in front of you; the tip hits harder",
            [](SkillDef& s) { s.kind = SkillKind::Cone, s.range = 450, s.width = 350, s.sweet_pct = 30; });
        add("@arc", "A fan that misses what is right next to you",
            [](SkillDef& s) { s.kind = SkillKind::Cone, s.geo = Geo::Arc, s.range = 500, s.width = 400, s.inner = 150; });
        add("@circle_blast", "A delayed blast at a point; the centre hits harder",
            [](SkillDef& s) { s.kind = SkillKind::Burst, s.range = 800, s.radius = 200, s.delay_ms = 600, s.sweet_pct = 30; });
        add("@ring_blast", "A delayed ring: the middle is safe, the outer edge hits harder", [](SkillDef& s) {
            s.kind = SkillKind::Burst, s.geo = Geo::Ring, s.range = 800, s.radius = 320, s.inner = 160, s.delay_ms = 700, s.sweet_pct = 30,
            s.sweet_zone = SweetZone::Edge;
        });
        add("@rect_slam", "A long slam at a point, along your aim", [](SkillDef& s) {
            s.kind = SkillKind::Burst, s.geo = Geo::Rect, s.range = 400, s.radius = 260, s.width = 120, s.delay_ms = 300;
        });
        add("@field", "A zone that hurts every half second",
            [](SkillDef& s) { s.kind = SkillKind::Field, s.range = 800, s.radius = 230, s.dmg = 20, s.dmg_rank = 10, s.dur_ms = 3'000, s.tick_ms = 500; });
        add("@dash", "Rush to a point, hitting on the way", [](SkillDef& s) { s.kind = SkillKind::Dash, s.range = 400, s.width = 100, s.speed = 1'400, s.cast_ms = 0; });
        add("@ghost_dash", "A dash through units and over walls", [](SkillDef& s) {
            s.kind = SkillKind::Dash, s.dmg = 0, s.dmg_rank = 0, s.range = 350, s.width = 100, s.speed = 1'200, s.cast_ms = 0, s.pass_units = true,
            s.through_walls = true, s.free_cast = true;
        });
        add("@blink", "Teleport a short distance", [](SkillDef& s) { s.kind = SkillKind::Blink, s.dmg = 0, s.dmg_rank = 0, s.range = 400, s.cast_ms = 0; });
        add("@chain", "A projectile that chains its target: still close after 1.5 s = pulled back and hit again", [](SkillDef& s) {
            s.kind = SkillKind::Bolt, s.range = 825, s.width = 120, s.speed = 1'800, s.tether_ms = 1'500, s.tether_radius = 400, s.pull = 400;
        });
        add("@shield", "A shield on yourself", [](SkillDef& s) { s.kind = SkillKind::Shield, s.dmg = 100, s.cast_ms = 0, s.dur_ms = 3'000, s.cd_ms = 14'000; });
        add("@heal", "Heal yourself", [](SkillDef& s) { s.kind = SkillKind::Heal, s.dmg = 90, s.ad = 0, s.ep = 40, s.cd_ms = 14'000; });
        add("@haste", "Run faster for a while", [](SkillDef& s) { s.kind = SkillKind::Haste, s.dmg = 0, s.cast_ms = 0, s.dur_ms = 3'000, s.cc_val = 35; });
        add("@transform", "A stronger form: attack, healing, speed, size", [](SkillDef& s) {
            s.kind = SkillKind::Transform, s.dmg = 0, s.dmg_rank = 0, s.cd_ms = 100'000, s.dur_ms = 10'000, s.cc_val = 40, s.size_bonus = 1'500,
            s.buff_attack_pct = 20, s.buff_heal_pct = 30, s.ultimate = true;
        });
        add("@three_strikes", "Three casts in 4 s: a line, a fan, a slam (each its own shape)", [](SkillDef& s) {
            s.kind = SkillKind::Cone, s.range = 600, s.width = 180, s.cast_ms = 500, s.casts = 3, s.recast_ms = 4'000, s.sweet_pct = 50, s.melee = true;
            s.stages[0] = {.kind = SkillKind::Cone, .range = 600, .width = 160, .dmg_pct = 100, .geo = Geo::Line};
            s.stages[1] = {.kind = SkillKind::Cone, .range = 450, .width = 340, .dmg_pct = 125, .geo = Geo::Cone};
            s.stages[2] = {.kind = SkillKind::Burst, .range = 220, .radius = 200, .dmg_pct = 150, .geo = Geo::Circle, .sweet_zone = SweetZone::Centre};
        });
        return v;
    }();
    return t;
}

const SkillDef* find_skill(std::string_view name) {
    if (name.starts_with("@")) {  // [U-462] a standard skill
        for (const SkillDef& s : skill_templates())
            if (s.name == name) return &s;
        return nullptr;
    }
    for (const ChampionDef& c : champions())
        for (const SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y})
            if (s->name == name) return s;
    for (const SkillDef& s : q_options())
        if (s.name == name) return &s;
    for (const SkillDef& s : specials())
        if (s.name == name) return &s;
    return nullptr;
}

// Parses one file into `out`; returns "" when everything is fine, else the reason.
std::string parse_text(const std::string& text, ChampionDef& out) {
    if (text.size() > 16'000) return "the file is too big (max 16 000 bytes)";
    std::istringstream in(text.starts_with("\xEF\xBB\xBF") ? text.substr(3) : text);  // a UTF-8 BOM from an editor
    std::string line, name;
    bool has_base = false;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (const auto h = line.find(" #"); h != std::string::npos) line.resize(h);  // " # comment" (a '#' inside a name stays)
        if (!line.empty() && line[0] == '#') continue;
        line = trim(line);
        if (line.empty()) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) return std::format("line {}: no '='", n);
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        auto number = [&](std::int32_t& dst, std::int32_t scale) -> bool {
            long long x = 0;
            const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
            if (ec != std::errc{} || p != v.data() + v.size()) return false;  // whole numbers only (no 580.5)
            dst = std::int32_t(std::clamp<long long>(x * scale, -1'000'000'000LL, 1'000'000'000LL));
            return true;
        };
        auto decimal = [&](std::int32_t& dst) -> bool {  // [U-476] 2.8 → 280 (hundredths), for Dota-style gains
            double x = 0;
            const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
            if (ec != std::errc{} || p != v.data() + v.size() || !(x > -1e6 && x < 1e6)) return false;
            dst = std::int32_t(std::llround(x * 100));
            return true;
        };
        bool ok = true;
        if (k == "base") {
            const auto it = std::ranges::find_if(champions(), [&](const ChampionDef& c) { return c.name == v; });
            if (it == champions().end()) return std::format("line {}: unknown base champion '{}'", n, v);
            out = *it;
            out.unique_group = 0, out.avatar = false, out.dark = false;  // an own champion is never "the same one" as its base
            has_base = true;
        } else if (!has_base) {
            return std::format("line {}: 'base = …' must come first", n);
        } else if (k == "name") {
            name = v;
        } else if (k == "hp") ok = number(out.hp, 100);
        else if (k == "attack") ok = number(out.attack, 100);
        else if (k == "ep_per_level") ok = number(out.ep_lvl, 100);
        else if (k == "defence") ok = number(out.def, 100);
        else if (k == "elemental_defence") ok = number(out.edef, 100);
        else if (k == "move_speed") ok = number(out.ms, 100);
        else if (k == "attack_range") ok = number(out.attack_range, 1);
        else if (k == "attack_speed") ok = number(out.as, 1);
        else if (k == "colour") {
            unsigned x = 0;
            const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x, 16);
            ok = ec == std::errc{} && p == v.data() + v.size();
            out.colour = x;
        } else if (k == "element") {  // [U-415] the champion itself
            const int e = enum_of(kElements, v);
            if (e < 0) return std::format("line {}: element must be Water / Earth / Fire / Air / None", n);
            out.element = Element(e);
        } else if (k == "sub") {
            const int e = enum_of(kSubs, v);
            if (e < 0) return std::format("line {}: unknown sub-element '{}'", n, v);
            out.sub = SubElement(e);
        } else if (k == "role") {
            const int e = enum_of(kRoles, v);
            if (e < 0) return std::format("line {}: unknown role '{}'", n, v);
            out.role = Role(e);
        } else if (k == "resource") {
            const int e = enum_of(kResources, v);
            if (e < 0) return std::format("line {}: unknown resource '{}'", n, v);
            out.resource = ResourceKind(e);
        } else if (k == "range") {
            out.range = v == "Ranged" ? Range::Ranged : Range::Melee;
        } else if (k == "mage") {
            out.mage = v == "1" || v == "yes";
        } else if (k == "attr") {  // [U-476] Dota 2 primary attribute
            constexpr std::string_view kAttrs[] = {"None", "Strength", "Agility", "Intelligence", "Universal"};
            const int e = enum_of(kAttrs, v);
            if (e < 0) return std::format("line {}: attr = None / Strength / Agility / Intelligence / Universal", n);
            out.attr = std::uint8_t(e);
        } else if (k == "strength") ok = decimal(out.str);  // [U-476] points (2.8 per level → strength_per_level = 2.8)
        else if (k == "agility") ok = decimal(out.agi);
        else if (k == "intelligence") ok = decimal(out.intel);
        else if (k == "strength_per_level") ok = decimal(out.str_lvl);
        else if (k == "agility_per_level") ok = decimal(out.agi_lvl);
        else if (k == "intelligence_per_level") ok = decimal(out.int_lvl);
        else if (k == "crits") {  // [U-463] its autoattacks can crit (crit stats shown)
            out.crits = v == "1" || v == "yes";
        } else if (k == "resource_max") ok = number(out.res_max, 100);
        else if (k == "resource_regen") ok = number(out.res_regen, 100);
        else if (k == "hp_per_level") ok = number(out.hp_lvl, 100);
        else if (k == "attack_per_level") ok = number(out.attack_lvl, 100);
        else if (k.size() == 2 && k[0] == 'q' && k[1] >= '1' && k[1] <= '6') {  // the Q options q1 … q6 (names of existing Q skills)
            const bool known = std::ranges::any_of(q_options(), [&](const SkillDef& q) { return q.name == v; });
            if (!known) return std::format("line {}: '{}' is not a Q option", n, v);
            out.q_pool[std::size_t(k[1] - '1')] = keep(v);
        } else if (k.starts_with("model.")) {  // [U-460] read by tools/art/make_models.py (the game uses the made sprite sheet)
        } else if (k.starts_with("autos.")) {  // [U-217][U-393] autoattack shape
            const std::string f = k.substr(6);
            std::int32_t x = 0;
            if (f == "shape") {
                const int e = enum_of(kShapes, v);
                if (e < 0) return std::format("line {}: shape must be Single / Splash / Line / Chain", n);
                out.autos.shape = AutoShape(e);
            } else if (!number(x, 1)) return std::format("line {}: '{}' is not a valid number", n, v);
            else if (f == "move_while_attacking") out.autos.move_while_attacking = x != 0;
            else if (f == "hits_walls") out.autos.hits_walls = x != 0;
            else if (f == "wall_pct") out.autos.wall_pct = std::int16_t(std::clamp(x, 0, 1000));
            else if (f == "splash_radius") out.autos.splash_radius = std::int16_t(std::clamp(x, 0, 1000));
            else if (f == "splash_pct") out.autos.splash_pct = std::int16_t(std::clamp(x, 0, 100));
            else if (f == "line_length") out.autos.line_length = std::int16_t(std::clamp(x, 0, 2000));
            else if (f == "chain_count") out.autos.chain_count = std::int8_t(std::clamp(x, 0, 10));
            else if (f == "chain_range") out.autos.chain_range = std::int16_t(std::clamp(x, 0, 2000));
            else if (f == "chain_pct") out.autos.chain_pct = std::int16_t(std::clamp(x, 0, 100));
            else return std::format("line {}: unknown key '{}'", n, k);
        } else if (k.size() > 2 && k[1] == '.' && std::string("pwerty").find(k[0]) != std::string::npos) {
            // [U-415] ANY field of a skill: w.dmg = 80, w.kind = Cone, w.cc = Stun, w.desc = …, w.casts = 3 …
            SkillDef& sk = k[0] == 'p' ? out.p : k[0] == 'w' ? out.w : k[0] == 'e' ? out.e : k[0] == 'r' ? out.r : k[0] == 't' ? out.t : out.y;
            const std::string f = k.substr(2);
            std::int32_t x = 0;
            auto num = [&](auto& field, long long lo, long long hi) {
                if (!number(x, 1)) return false;
                field = static_cast<std::remove_reference_t<decltype(field)>>(std::clamp<long long>(x, lo, hi));
                return true;
            };
            if (f == "name") {
                if (!good_name(v)) return std::format("line {}: skill names: letters, digits, space ' _ - (max 24)", n);
                sk.name = keep(v);
            } else if (f == "desc") sk.desc = keep(v);
            else if (f == "passive_desc") sk.passive_desc = keep(v);
            else if (f == "kind") {
                const int e = enum_of(kKinds, v);
                if (e < 0) return std::format("line {}: unknown skill kind '{}'", n, v);
                sk.kind = SkillKind(e);
            } else if (f == "sub") {
                const int e = enum_of(kSubs, v);
                if (e < 0) return std::format("line {}: unknown sub-element '{}'", n, v);
                sk.sub = SubElement(e);
            } else if (f == "cc") {
                const int e = status_index(v);
                if (e < 0) return std::format("line {}: unknown crowd control '{}'", n, v);
                sk.cc = v == "None" ? StatusKind::Count : StatusKind(e);
            } else if (f == "move") sk.move = v == "Free" ? CastMove::Free : v == "Slowed" ? CastMove::Slowed : CastMove::Rooted;
            else if (f == "aim") sk.aim = v == "Facing" ? Aim::Facing : Aim::Cursor;
            else if (f == "dmg") ok = num(sk.dmg, 0, 100'000);
            else if (f == "dmg_rank") ok = num(sk.dmg_rank, 0, 100'000);
            else if (f == "ep") ok = num(sk.ep, 0, 1'000);
            else if (f == "ad") ok = num(sk.ad, 0, 1'000);
            else if (f == "bonus_ad") ok = num(sk.bonus_ad, 0, 1'000);
            else if (f == "range") ok = num(sk.range, 0, 5'000);
            else if (f == "width") ok = num(sk.width, 0, 3'000);
            else if (f == "speed") ok = num(sk.speed, 0, 10'000);
            else if (f == "radius") ok = num(sk.radius, 0, 2'000);
            else if (f == "cast_ms") ok = num(sk.cast_ms, 0, 10'000);
            else if (f == "cd_ms") ok = num(sk.cd_ms, 0, 600'000);
            else if (f == "cd_rank") ok = num(sk.cd_rank, -100'000, 100'000);
            else if (f == "cost") ok = num(sk.cost, 0, 10'000);
            else if (f == "dur_ms") ok = num(sk.dur_ms, 0, 120'000);
            else if (f == "delay_ms") ok = num(sk.delay_ms, 0, 10'000);
            else if (f == "tick_ms") ok = num(sk.tick_ms, 50, 10'000);
            else if (f == "cc_ms") ok = num(sk.cc_ms, 0, 20'000);
            else if (f == "cc_val") ok = num(sk.cc_val, 0, 100'000);
            else if (f == "knock") ok = num(sk.knock, 0, 2'000);
            else if (f == "casts") ok = num(sk.casts, 1, 10);
            else if (f == "recast_ms") ok = num(sk.recast_ms, 0, 60'000);
            else if (f == "light") ok = num(sk.light, 0, 2'000);
            else if (f == "size_bonus") ok = num(sk.size_bonus, 0, 20'000);
            else if (f == "sweet_pct") ok = num(sk.sweet_pct, 0, 300);
            else if (f == "pct_current_hp") ok = num(sk.pct_current_hp, 0, 10'000);
            else if (f == "pct_max_hp") ok = num(sk.pct_max_hp, 0, 10'000);
            else if (f == "pct_missing_hp") ok = num(sk.pct_missing_hp, 0, 10'000);
            else if (f == "wall_thick") ok = num(sk.wall_thick, 0, 3);
            else if (f == "wall_level") ok = num(sk.wall_level, 0, 2);
            else if (f == "wall_hp") ok = num(sk.wall_hp, 0, 30'000);
            else if (f == "wall_hp_rank") ok = num(sk.wall_hp_rank, 0, 30'000);
            else if (f == "wall_segs") ok = num(sk.wall_segs, 0, 20);
            else if (f == "control") ok = num(sk.control, 0, 2);
            else if (f == "sensed_target") ok = num(sk.sensed_target, -1, 1);  // [U-431]
            // [U-456] cast stages of a multicast: stage1.kind = Cone, stage2.width = 120, stage3.sweet_pct = 70 …
            else if (f.size() > 7 && f.starts_with("stage") && f[5] >= '1' && f[5] <= '3' && f[6] == '.') {
                SkillDef::CastStage& st = sk.stages[std::size_t(f[5] - '1')];
                const std::string g = f.substr(7);
                if (g == "kind") {
                    const int e = enum_of(kKinds, v);
                    if (e < 0) return std::format("line {}: unknown skill kind '{}'", n, v);
                    st.kind = SkillKind(e);
                } else if (g == "range") ok = num(st.range, 0, 5'000);
                else if (g == "width") ok = num(st.width, 0, 3'000);
                else if (g == "radius") ok = num(st.radius, 0, 2'000);
                else if (g == "dmg_pct") ok = num(st.dmg_pct, 0, 1'000);
                else if (g == "sweet_pct") ok = num(st.sweet_pct, -1, 300);
                else if (g == "geo" || g == "sweet_zone") {  // [U-461][U-462]
                    const std::string_view* names = g == "geo" ? kGeos : kZones;
                    const int cnt = g == "geo" ? int(std::size(kGeos)) : int(std::size(kZones));
                    int e = -1;
                    for (int i = 0; i < cnt; ++i)
                        if (names[i] == v) e = i;
                    if (e < 0) return std::format("line {}: unknown {} '{}'", n, g, v);
                    if (g == "geo") st.geo = Geo(e);
                    else st.sweet_zone = SweetZone(e);
                }
                else if (g == "inner") ok = num(st.inner, -1, 2'000);
                else if (g == "sweet_size") ok = num(st.sweet_size, 0, 100);
                else return std::format("line {}: unknown key '{}'", n, k);
            }
            // [U-470] growth per rank of any number
            else if (f == "cc_ms_rank") ok = num(sk.cc_ms_rank, -20'000, 20'000);
            else if (f == "cc_val_rank") ok = num(sk.cc_val_rank, -100'000, 100'000);
            else if (f == "dur_ms_rank") ok = num(sk.dur_ms_rank, -120'000, 120'000);
            else if (f == "range_rank") ok = num(sk.range_rank, -5'000, 5'000);
            else if (f == "width_rank") ok = num(sk.width_rank, -3'000, 3'000);
            else if (f == "radius_rank") ok = num(sk.radius_rank, -2'000, 2'000);
            else if (f == "knock_rank") ok = num(sk.knock_rank, -2'000, 2'000);
            else if (f == "cost_rank") ok = num(sk.cost_rank, -10'000, 10'000);
            // [U-468] while learned: e.learned.omnivamp = 1600, e.learned_rank.omnivamp = 200 (names: kStatKeyNames)
            else if (f.starts_with("learned.") || f.starts_with("learned_rank.")) {
                const bool per = f.starts_with("learned_rank.");
                const std::string stat = f.substr(per ? 13 : 8);
                int key = -1;
                for (int i = 0; i < int(StatKey::Count); ++i)
                    if (stat == kStatKeyNames[i]) key = i;
                if (key < 0) return std::format("line {}: unknown stat '{}'", n, stat);
                SkillDef::Learned* slot = nullptr;
                for (auto& l : sk.learned)
                    if (l.key == StatKey(key) || (!slot && l.key == StatKey::Count)) slot = l.key == StatKey(key) ? &l : slot ? slot : &l;
                for (auto& l : sk.learned)
                    if (l.key == StatKey(key)) slot = &l;
                if (!slot) return std::format("line {}: max 3 learned stats per skill", n);
                slot->key = StatKey(key);
                ok = per ? num(slot->per_rank, -100'000, 100'000) : num(slot->base, -100'000, 100'000);
            }
            else if (f == "geo" || f == "sweet_zone") {  // [U-461][U-462] shape rule / where the sweet spot is
                const std::string_view* names = f == "geo" ? kGeos : kZones;
                const int cnt = f == "geo" ? int(std::size(kGeos)) : int(std::size(kZones));
                int e = -1;
                for (int i = 0; i < cnt; ++i)
                    if (names[i] == v) e = i;
                if (e < 0) return std::format("line {}: unknown {} '{}'", n, f, v);
                if (f == "geo") sk.geo = Geo(e);
                else sk.sweet_zone = SweetZone(e);
            }
            else if (f == "hits") {  // [U-311][U-472] which units it can hit: hits = Minion SiegeMinion SuperMinion (empty = all)
                constexpr std::string_view kCats[] = {"Champion", "Minion", "SiegeMinion", "SuperMinion", "Monster", "EpicMonster", "Turret", "Inhibitor", "Shrine"};
                sk.hits = 0;
                std::string word;
                std::istringstream ws(std::string{v});
                while (ws >> word) {
                    if (!word.empty() && word.back() == ',') word.pop_back();
                    int e = -1;
                    for (int i = 0; i < int(std::size(kCats)); ++i)
                        if (kCats[i] == word) e = i;
                    if (e < 0) return std::format("line {}: unknown unit kind '{}' (Champion Minion SiegeMinion SuperMinion Monster EpicMonster Turret Inhibitor Shrine)", n, word);
                    sk.hits |= std::uint16_t(1u << e);
                }
            }
            else if (f == "inner") ok = num(sk.inner, 0, 2'000);
            else if (f == "ap") ok = num(sk.ap, 0, 1'000);        // [U-475] % of ability power
            else if (f == "dispel") ok = num(sk.dispel, 0, 2);   // [U-476] 1 basic, 2 strong
            else if (f == "sweet_size") ok = num(sk.sweet_size, 0, 100);
            else if (f == "tether_ms") ok = num(sk.tether_ms, 0, 10'000);  // [U-456] chains
            else if (f == "tether_radius") ok = num(sk.tether_radius, 0, 3'000);
            else if (f == "pull") ok = num(sk.pull, 0, 3'000);
            else if (f == "cc2") {
                const int e = status_index(v);
                if (e < 0) return std::format("line {}: unknown crowd control '{}'", n, v);
                sk.cc2 = v == "None" ? StatusKind::Count : StatusKind(e);
            }
            else if (f == "cc2_ms") ok = num(sk.cc2_ms, 0, 20'000);
            else if (f == "free_cast") { std::int32_t b = 0; ok = num(b, 0, 1); sk.free_cast = b != 0; }
            else if (f == "buff_attack_pct") ok = num(sk.buff_attack_pct, 0, 500);  // [U-456] Transform
            else if (f == "buff_heal_pct") ok = num(sk.buff_heal_pct, 0, 500);
            // [U-459] the look on the exact hitbox: vis.colour = ff3020, vis.glow / fill / particles 0..255, trail 0..11 …
            else if (f == "vis.colour") {
                std::uint32_t c = 0;
                const auto res = std::from_chars(v.data(), v.data() + v.size(), c, 16);
                if (res.ec != std::errc{} || c > 0xffffff) return std::format("line {}: a colour is 6 hex digits (ff3020)", n);
                sk.vis.colour = c;
            }
            else if (f == "vis.fill") ok = num(sk.vis.fill, 0, 255);
            else if (f == "vis.glow") ok = num(sk.vis.glow, 0, 255);
            else if (f == "vis.trail") ok = num(sk.vis.trail, 0, 11);
            else if (f == "vis.flash") ok = num(sk.vis.flash, 0, 1);
            else if (f == "vis.particles") ok = num(sk.vis.particles, 0, 40);
            else if (f == "vis.mark_ms") ok = num(sk.vis.mark_ms, 0, 30'000);
            else if (f == "self_root_ms") ok = num(sk.self_root_ms, -1, 5'000);  // [U-434]
            else if (f == "charges") ok = num(sk.charges, 0, 9);               // [U-433]
            else if (f == "charge_ms") ok = num(sk.charge_ms, 0, 600'000);
            else if (f == "charge_gap_ms") ok = num(sk.charge_gap_ms, 0, 10'000);
            // [U-416] passive procs with their own cooldown
            else if (f == "proc") ok = num(sk.proc, 0, 4);
            else if (f == "proc_cd_ms") ok = num(sk.proc_cd_ms, 0, 600'000);
            else if (f == "proc_dmg") ok = num(sk.proc_dmg, 0, 100'000);
            else if (f == "proc_pct_max_hp") ok = num(sk.proc_pct_max_hp, 0, 10'000);
            else if (f == "proc_heal_pct") ok = num(sk.proc_heal_pct, 0, 1'000);
            else if (f == "proc_heal_max_hp") ok = num(sk.proc_heal_max_hp, 0, 10'000);
            else if (f == "proc_hp_pct") ok = num(sk.proc_hp_pct, 1, 100);
            else if (f == "proc_shield") ok = num(sk.proc_shield, 0, 100'000);
            else if (f == "proc_cc_ms") ok = num(sk.proc_cc_ms, 0, 20'000);
            else if (f == "proc_cc") {
                const int e2 = status_index(v);
                if (e2 < 0) return std::format("line {}: unknown crowd control '{}'", n, v);
                sk.proc_cc = v == "None" ? StatusKind::Count : StatusKind(e2);
            }
            else if (f == "steer_turn") ok = num(sk.steer_turn, 0, 16'000);
            else if (f == "source_first") ok = num(sk.source_first, -1, 1);
            else if (f == "pierce" || f == "targeted" || f == "ally" || f == "group" || f == "melee" || f == "ground" || f == "through_walls" ||
                     f == "turns_caster" || f == "teleport" || f == "env_bend" || f == "full_moon_only" || f == "lift_beside" ||
                     f == "arcane" || f == "aa_reset" || f == "pass_units") {
                if (!number(x, 1)) return std::format("line {}: '{}' must be 0 or 1", n, v);
                bool& b = f == "arcane" ? sk.arcane : f == "aa_reset" ? sk.aa_reset : f == "pass_units" ? sk.pass_units  // [U-464][U-465][U-471]
                        : f == "pierce" ? sk.pierce : f == "targeted" ? sk.targeted : f == "ally" ? sk.ally : f == "group" ? sk.group
                        : f == "melee" ? sk.melee : f == "ground" ? sk.ground : f == "through_walls" ? sk.through_walls
                        : f == "turns_caster" ? sk.turns_caster : f == "teleport" ? sk.teleport : f == "env_bend" ? sk.env_bend
                        : f == "full_moon_only" ? sk.full_moon_only : sk.lift_beside;
                b = x != 0;
            } else return std::format("line {}: unknown skill field '{}'", n, f);
        } else if (k.size() > 2 && k[1] == '_' && std::string("pwerty").find(k[0]) != std::string::npos) {
            // [U-291] rank rule of one skill: t_ultimate = 1, y_unlock_level = 6, w_rank_step = 6, w_rank_cap = 2
            SkillDef& sk = k[0] == 'p' ? out.p : k[0] == 'w' ? out.w : k[0] == 'e' ? out.e : k[0] == 'r' ? out.r : k[0] == 't' ? out.t : out.y;
            std::int32_t x = 0;
            if (!number(x, 1)) return std::format("line {}: '{}' is not a valid number", n, v);
            const std::string what = k.substr(2);
            if (what == "ultimate") sk.ultimate = x != 0;
            else if (what == "unlock_level") sk.unlock_level = std::uint8_t(std::clamp(x, 0, 255));
            else if (what == "rank_step") sk.rank_step = std::uint8_t(std::clamp(x, 0, 255));
            else if (what == "rank_cap") sk.rank_cap = std::uint8_t(std::clamp(x, 0, 255));
            else return std::format("line {}: unknown key '{}'", n, k);
        } else if (k == "p" || k == "w" || k == "e" || k == "r" || k == "t" || k == "y") {
            const SkillDef* s = find_skill(v);
            if (!s) return std::format("line {}: unknown skill '{}'", n, v);
            (k == "p" ? out.p : k == "w" ? out.w : k == "e" ? out.e : k == "r" ? out.r : k == "y" ? out.y : out.t) = *s;
        } else {
            return std::format("line {}: unknown key '{}'", n, k);
        }
        if (!ok) return std::format("line {}: '{}' is not a valid number", n, v);
    }
    if (!has_base) return "no 'base = …'";
    if (name.empty()) return "no 'name = …'";
    if (!good_name(name)) return "names: letters, digits, space ' _ - only, at most 24 characters";
    // sane numbers (a file can't break the game) [U-415]
    out.hp = std::clamp(out.hp, 10'000, 1'000'000), out.attack = std::clamp(out.attack, 0, 100'000);
    out.ms = std::clamp(out.ms, 10'000, 100'000), out.as = std::clamp(out.as, 1, 500), out.attack_range = std::clamp(out.attack_range, 0, 2'000);
    out.def = std::clamp(out.def, 0, 100'000), out.edef = std::clamp(out.edef, 0, 100'000);
    // only BUILT-IN names count here: an own champion that is already loaded must still read as valid
    // (two own files with the same name: register_champion() refuses the second) [U-353]
    for (int i = 0; i < builtin_champion_count(); ++i)
        if (champions()[std::size_t(i)].name == name) return std::format("the name '{}' is already used", name);
    auto kept = std::ranges::find(g_strings, name);  // one stored copy per name (files are re-read often)
    if (kept == g_strings.end()) g_strings.push_back(name), kept = std::prev(g_strings.end());
    out.name = *kept;
    return {};
}
}  // namespace

std::string parse_champion_text(const std::string& text, ChampionDef& out) { return parse_text(text, out); }

}  // namespace em
