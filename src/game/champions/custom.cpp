// [U-210][U-277] Own champions from `.champ` text. Shared by the client (files on disk) and the server
// (which sends its files to every player who joins). See client/custom_champions.hpp for the file format.
#include "game/champions/custom.hpp"

#include <algorithm>
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
    std::istringstream in(text);
    std::string line, name;
    bool has_base = false;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (const auto h = line.find('#'); h != std::string::npos) line.resize(h);
        line = trim(line);
        if (line.empty()) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) return std::format("line {}: no '='", n);
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        auto number = [&](std::int32_t& dst, std::int32_t scale) -> bool {
            int x = 0;
            const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
            if (ec != std::errc{} || p != v.data() + v.size()) return false;
            dst = x * scale;
            return true;
        };
        bool ok = true;
        if (k == "base") {
            const auto it = std::ranges::find_if(champions(), [&](const ChampionDef& c) { return c.name == v; });
            if (it == champions().end()) return std::format("line {}: unknown base champion '{}'", n, v);
            out = *it;
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
