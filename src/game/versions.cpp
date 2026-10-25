// [U-95] See versions.hpp.
#include "game/versions.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>

#include "game/content.hpp"

namespace em {

namespace fs = std::filesystem;

namespace {
std::string g_current = kOfficialVersion;

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
bool number(const std::string& v, std::int32_t& out) {
    const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{} && p == v.data() + v.size();
}
// champion stats: key → field + scale (the champion file keys of own champions)
bool set_champion(ChampionDef& c, const std::string& key, std::int32_t v) {
    if (key == "hp") c.hp = v * 100;
    else if (key == "attack") c.attack = v * 100;
    else if (key == "ep_per_level") c.ep_lvl = v * 100;
    else if (key == "defence") c.def = v * 100;
    else if (key == "elemental_defence") c.edef = v * 100;
    else if (key == "move_speed") c.ms = v * 100;
    else if (key == "attack_range") c.attack_range = v;
    else if (key == "attack_speed") c.as = v;
    else return false;
    return true;
}
bool set_skill(SkillDef& s, const std::string& key, std::int32_t v) {
    if (key == "damage") s.dmg = v;
    else if (key == "damage_per_rank") s.dmg_rank = v;
    else if (key == "ep") s.ep = v;
    else if (key == "ad") s.ad = v;
    else if (key == "range") s.range = v;
    else if (key == "width") s.width = v;
    else if (key == "speed") s.speed = v;
    else if (key == "radius") s.radius = v;
    else if (key == "cast_ms") s.cast_ms = v;
    else if (key == "cooldown_ms") s.cd_ms = v;
    else if (key == "cost") s.cost = v;
    else if (key == "duration_ms") s.dur_ms = v;
    else if (key == "cc_ms") s.cc_ms = v;
    else if (key == "cc_value") s.cc_val = v;
    else if (key == "ultimate") s.ultimate = v != 0;  // [U-291] rank rules
    else if (key == "unlock_level") s.unlock_level = std::uint8_t(std::clamp(v, 0, 255));
    else if (key == "rank_step") s.rank_step = std::uint8_t(std::clamp(v, 0, 255));
    else if (key == "rank_cap") s.rank_cap = std::uint8_t(std::clamp(v, 0, 255));
    else return false;
    return true;
}
}  // namespace

const std::string& current_version() { return g_current; }

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
        // "champion.<name>.<key>" / "skill.<name>.<key>" (names may contain spaces, not dots)
        const auto d1 = k.find('.'), d2 = k.rfind('.');
        if (d1 == std::string::npos || d1 == d2) {
            error = std::format("line {}: write champion.<name>.<key> or skill.<name>.<key>", line_no);
            break;
        }
        const std::string kind = k.substr(0, d1), what = k.substr(d1 + 1, d2 - d1 - 1), key = k.substr(d2 + 1);
        std::int32_t value = 0;
        if (!number(v, value)) {
            error = std::format("line {}: '{}' is not a whole number", line_no, v);
            break;
        }
        bool found = false, key_ok = true;
        if (kind == "champion") {
            for (ChampionDef& c : champions_mut())
                if (c.name == what) found = true, key_ok = set_champion(c, key, value);
        } else if (kind == "skill") {
            for (ChampionDef& c : champions_mut())
                for (SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y})
                    if (s->name == what) found = true, key_ok = key_ok && set_skill(*s, key, value);
            for (SkillDef& s : q_options_mut())
                if (s.name == what) found = true, key_ok = key_ok && set_skill(s, key, value);
            for (SkillDef& s : specials_mut())
                if (s.name == what) found = true, key_ok = key_ok && set_skill(s, key, value);
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
        return error;
    }
    g_current = name;
    if (changes) *changes = n;
    return {};
}

}  // namespace em
