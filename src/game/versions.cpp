// [U-95] See versions.hpp.
#include "game/versions.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>

#include "core/numbers.hpp"
#include "game/content.hpp"
#include <limits>
#include <optional>

namespace em {

namespace fs = std::filesystem;

namespace {
std::string g_current = kOfficialVersion;
std::vector<std::string> g_mods;  // [U-478]
std::string g_linked_pack;

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
// [U-482] any number may be relative to the official one: +30%, -20%, *1.5, +40 (core/numbers.hpp)
bool set_champion(ChampionDef& c, const std::string& key, const NumExpr& e) {
    auto put = [&](std::int32_t& f, double scale) { f = std::int32_t(std::clamp<std::int64_t>(apply_num_expr(e, f, scale), -1'000'000'000, 1'000'000'000)); };
    if (key == "hp") put(c.hp, 100);
    else if (key == "hp_per_level") put(c.hp_lvl, 100);
    else if (key == "attack") put(c.attack, 100);
    else if (key == "attack_per_level") put(c.attack_lvl, 100);
    else if (key == "ep_per_level") put(c.ep_lvl, 100);
    else if (key == "defence") put(c.def, 100);
    else if (key == "elemental_defence") put(c.edef, 100);
    else if (key == "move_speed") put(c.ms, 100);
    else if (key == "attack_range") put(c.attack_range, 1);
    else if (key == "attack_speed") put(c.as, 1);
    else if (key == "resource_max") put(c.res_max, 100);
    else if (key == "resource_regen") put(c.res_regen, 100);
    else return false;
    return true;
}
bool set_skill(SkillDef& s, const std::string& key, const NumExpr& e) {
    auto put = [&](auto& f) {
        using T = std::remove_reference_t<decltype(f)>;
        f = T(std::clamp<std::int64_t>(apply_num_expr(e, std::int64_t(f), 1), std::numeric_limits<T>::min(), std::numeric_limits<T>::max()));
    };
    if (key == "damage") put(s.dmg);
    else if (key == "damage_per_rank") put(s.dmg_rank);
    else if (key == "ep") put(s.ep);
    else if (key == "ad") put(s.ad);
    else if (key == "ap") put(s.ap);
    else if (key == "range") put(s.range);
    else if (key == "width") put(s.width);
    else if (key == "speed") put(s.speed);
    else if (key == "radius") put(s.radius);
    else if (key == "cast_ms") put(s.cast_ms);
    else if (key == "cooldown_ms") put(s.cd_ms);
    else if (key == "cooldown_per_rank") put(s.cd_rank);
    else if (key == "cost") put(s.cost);
    else if (key == "duration_ms") put(s.dur_ms);
    else if (key == "cc_ms") put(s.cc_ms);
    else if (key == "cc_value") put(s.cc_val);
    else if (key == "knock") put(s.knock);
    else if (key == "sweet_pct") put(s.sweet_pct);
    else if (key == "ultimate") s.ultimate = apply_num_expr(e, s.ultimate, 1) != 0;  // [U-291] rank rules
    else if (key == "unlock_level") put(s.unlock_level);
    else if (key == "rank_step") put(s.rank_step);
    else if (key == "rank_cap") put(s.rank_cap);
    else return false;
    return true;
}
}  // namespace

const std::string& current_version() { return g_current; }
bool mod_enabled(std::string_view mod) { return std::ranges::find(g_mods, mod) != g_mods.end(); }
std::string version_pack_dir() {
    if (g_current == kOfficialVersion) return {};
    const fs::path p = fs::path("versions") / g_current / "pack";
    std::error_code ec;
    return fs::is_directory(p, ec) ? p.string() : std::string{};
}
const std::string& version_linked_pack() { return g_linked_pack; }

std::vector<std::string> list_versions() {
    std::vector<std::string> out{kOfficialVersion};
    std::error_code ec;
    std::vector<std::string> own;
    for (const auto& e : fs::directory_iterator("versions", ec))
        if (e.is_directory() && fs::exists(e.path() / "version.txt")) own.push_back(e.path().filename().string());
    std::ranges::sort(own);
    out.insert(out.end(), own.begin(), own.end());
    return out;
}

std::string version_description(const std::string& name) {
    if (name == kOfficialVersion) return "the official game";
    std::ifstream in(fs::path("versions") / name / "version.txt");
    for (std::string line; std::getline(in, line);) {
        line = trim(line);
        if (line.starts_with("description")) {
            const auto eq = line.find('=');
            if (eq != std::string::npos) return trim(line.substr(eq + 1));
        }
    }
    return {};
}

std::string apply_version(const std::string& name, int* changes) {
    restore_official_numbers();
    g_current = kOfficialVersion;
    g_mods.clear(), g_linked_pack.clear();  // [U-478]
    if (changes) *changes = 0;
    if (name == kOfficialVersion) return {};
    std::ifstream in(fs::path("versions") / name / "version.txt");
    if (!in) return std::format("versions/{}/version.txt not found", name);
    int n = 0, line_no = 0;
    std::string error;
    for (std::string line; std::getline(in, line) && error.empty();) {
        ++line_no;
        if (const auto h = line.find('#'); h != std::string::npos) line.resize(h);
        line = trim(line);
        if (line.empty()) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            error = std::format("line {}: no '='", line_no);
            break;
        }
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (k == "description" || k == "base") continue;
        if (k == "mods") {  // [U-478] mods = lol, other
            std::string w;
            for (char ch : v + " ") {
                if (ch == ' ' || ch == ',') {
                    if (!w.empty()) g_mods.push_back(w), ++n;
                    w.clear();
                } else {
                    w += ch;
                }
            }
            continue;
        }
        if (k == "resource_pack") {
            g_linked_pack = v;
            continue;
        }
        // "champion.<name>.<key>" / "skill.<name>.<key>" (names may contain spaces, not dots)
        const auto d1 = k.find('.'), d2 = k.rfind('.');
        if (d1 == std::string::npos || d1 == d2) {
            error = std::format("line {}: write champion.<name>.<key> or skill.<name>.<key>", line_no);
            break;
        }
        const std::string kind = k.substr(0, d1), what = k.substr(d1 + 1, d2 - d1 - 1), key = k.substr(d2 + 1);
        const std::optional<NumExpr> value = parse_num_expr(v);  // [U-482]
        if (!value) {
            error = std::format("line {}: '{}' is not a number (570, +40, -20%, *1.5 …)", line_no, v);
            break;
        }
        bool found = false, key_ok = true;
        if (kind == "champion") {
            for (ChampionDef& c : champions_mut())
                if (c.name == what) found = true, key_ok = set_champion(c, key, *value);
        } else if (kind == "skill") {
            for (ChampionDef& c : champions_mut())
                for (SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y})
                    if (s->name == what) found = true, key_ok = key_ok && set_skill(*s, key, *value);
            for (SkillDef& s : q_options_mut())
                if (s.name == what) found = true, key_ok = key_ok && set_skill(s, key, *value);
            for (SkillDef& s : specials_mut())
                if (s.name == what) found = true, key_ok = key_ok && set_skill(s, key, *value);
        } else {
            error = std::format("line {}: unknown '{}' (champion / skill)", line_no, kind);
            break;
        }
        if (!found) error = std::format("line {}: no {} named '{}'", line_no, kind, what);
        else if (!key_ok) error = std::format("line {}: unknown key '{}'", line_no, key);
        else ++n;
    }
    if (!error.empty()) {
        restore_official_numbers();
        g_mods.clear(), g_linked_pack.clear();
        return error;
    }
    g_current = name;
    if (changes) *changes = n;
    return {};
}

}  // namespace em
