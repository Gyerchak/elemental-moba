// [U-210][U-277] Own champions from `.champ` text. Shared by the client (files on disk) and the server
// (which sends its files to every player who joins). See client/custom_champions.hpp for the file format.
#include "game/champions/custom.hpp"

#include <algorithm>
#include <cctype>
#include <type_traits>
#include <charconv>
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
                                       "Passive", "WaterSpout", "IcePath", "Redirect", "AvatarState", "MagicStance", "StonePush", "MoveWater"};
constexpr std::string_view kSubs[] = {"Water", "Ice", "Wood", "Blood", "Mud", "Steam", "Air", "Earth", "Lava", "Metal", "EarthArmor",
                                      "MetalArmor", "Fire", "BlueFire", "Lightning", "Combustion", "Physical"};
constexpr std::string_view kStatuses[] = {"Stun", "Root", "Slow", "Silence", "Mute", "Airborne", "Frozen", "Blind", "Burn", "Shield",
                                          "Haste", "Flying", "Bloodbound", "Channel", "Swimming", "WaterRide", "Redirect", "Wet", "Grievous", "None"};
constexpr std::string_view kElements[] = {"Water", "Earth", "Fire", "Air", "None"};
constexpr std::string_view kResources[] = {"Water", "Fuel", "None", "Ammo", "Energy", "Rage"};
constexpr std::string_view kRoles[] = {"Mage", "Marksman", "Fighter", "Tank", "Assassin", "Support"};
constexpr std::string_view kShapes[] = {"Single", "Splash", "Line", "Chain"};
template <std::size_t N>
int enum_of(const std::string_view (&names)[N], std::string_view v) {
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == v) return int(i);
    return -1;
}
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

const SkillDef* find_skill(std::string_view name) {
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
        } else if (k == "resource_max") ok = number(out.res_max, 100);
        else if (k == "resource_regen") ok = number(out.res_regen, 100);
        else if (k == "hp_per_level") ok = number(out.hp_lvl, 100);
        else if (k == "attack_per_level") ok = number(out.attack_lvl, 100);
        else if (k == "q1" || k == "q2" || k == "q3") {  // the 3 Q options (names of existing Q skills)
            const bool known = std::ranges::any_of(q_options(), [&](const SkillDef& q) { return q.name == v; });
            if (!known) return std::format("line {}: '{}' is not a Q option", n, v);
            out.q_pool[std::size_t(k[1] - '1')] = keep(v);
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
                const int e = enum_of(kStatuses, v);
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
                const int e2 = enum_of(kStatuses, v);
                if (e2 < 0) return std::format("line {}: unknown crowd control '{}'", n, v);
                sk.proc_cc = v == "None" ? StatusKind::Count : StatusKind(e2);
            }
            else if (f == "steer_turn") ok = num(sk.steer_turn, 0, 16'000);
            else if (f == "source_first") ok = num(sk.source_first, -1, 1);
            else if (f == "pierce" || f == "targeted" || f == "ally" || f == "group" || f == "melee" || f == "ground" || f == "through_walls" ||
                     f == "turns_caster" || f == "teleport" || f == "env_bend" || f == "full_moon_only" || f == "lift_beside") {
                if (!number(x, 1)) return std::format("line {}: '{}' must be 0 or 1", n, v);
                bool& b = f == "pierce" ? sk.pierce : f == "targeted" ? sk.targeted : f == "ally" ? sk.ally : f == "group" ? sk.group
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
