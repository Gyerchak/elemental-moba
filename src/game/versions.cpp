// [U-95] See versions.hpp.
#include "game/versions.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>

#include "core/numbers.hpp"
#include "game/content.hpp"
#include "game/loadout/loadout.hpp"
#include <span>
#include <vector>
#include <cstdlib>
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
// ---------------------------------------------------------------- [U-483] the setup system (runes, masteries, keystones, trees)
//   setup.<kind>.<name>.<key> = value     kind: keystone tree node shard rune quint mastery
//   keys: name / desc (keystone) / theme (tree) = text; stat = a stat name (kStatKeyNames); value = a number or formula
//   (points: 9 = 9, +20%, *1.5 …); pct = 1 → the value is "% of the stat"; max_rank (mastery); removed = 1;
//   place = where a NEW one goes (a free place): keystone / tree / quint: new; node: <tree> <tier 1-3> <1-2>;
//   shard: Offense / Flex / Defense; rune: Marks / Seals / Glyphs; mastery: <tree> <tier 1-5>
std::string apply_setup_line(const std::string& kind, const std::string& name, const std::string& key, const std::string& v) {
    auto words = [](const std::string& s) {
        std::vector<std::string> w;
        std::string cur;
        for (char c : s + " ")
            if (c == ' ') {
                if (!cur.empty()) w.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        return w;
    };
    auto stat_of = [](const std::string& s) {
        for (int i = 0; i < int(StatKey::Count); ++i)
            if (s == kStatKeyNames[i]) return StatKey(i);
        return StatKey::Count;
    };
    // the entry: its name + its stat change (+ extras per kind)
    // EVERY entry with this name (the same shard can be in two rows)
    struct Hit {
        std::string_view* nm = nullptr;
        StatMod* mod = nullptr;
        std::string_view* text2 = nullptr;  // desc / theme
        std::uint8_t* max_rank = nullptr;
    };
    std::vector<Hit> hits;
    // a NEW entry goes to a spare place first (after the defaults), so saved setups never point at a different thing
    auto free_place = [](auto& span, std::size_t defaults, auto&& empty) -> decltype(&span[0]) {
        for (std::size_t i = defaults; i < span.size(); ++i)
            if (empty(span[i])) return &span[i];
        for (std::size_t i = 0; i < std::min(defaults, span.size()); ++i)
            if (empty(span[i])) return &span[i];
        return nullptr;
    };
    if (kind == "keystone") {
        for (KeystoneDef& k : keystones_mut())
            if (!k.name.empty() && k.name == name) hits.push_back({&k.name, &k.mod, &k.desc, nullptr});
        if (hits.empty() && key == "place") {
            if (v != "new") return "keystone place = new";
            if (KeystoneDef* k = free_place(keystones_mut(), kDefaultKeystones.size(), [](const KeystoneDef& x) { return x.name.empty(); })) {
                *k = KeystoneDef{KeystoneId::StatOnly, keep_setup_name(name), "", "times picked", {StatKey::Count, 0, false}};
                return {};
            }
            return "no free keystone place (max 8)";
        }
    } else if (kind == "tree") {
        for (SpecTree& t : spec_trees_mut())
            if (!t.name.empty() && t.name == name) hits.push_back({&t.name, nullptr, &t.theme, nullptr});
        if (hits.empty() && key == "place") {
            if (SpecTree* t = free_place(spec_trees_mut(), kDefaultSpecTrees.size(), [](const SpecTree& x) { return x.name.empty(); })) {
                *t = SpecTree{};
                t->name = keep_setup_name(name);
                return {};
            }
            return "no free tree place (max 6)";
        }
    } else if (kind == "node") {
        for (SpecTree& t : spec_trees_mut())
            for (auto& tier : t.tiers)
                for (SpecNode& n : tier)
                    if (!n.name.empty() && n.name == name) hits.push_back({&n.name, &n.mod, nullptr, nullptr});
        if (hits.empty() && key == "place") {  // <tree> <tier> <col>
            const auto w = words(v);
            if (w.size() < 3) return "node place = <tree> <tier 1-3> <1-2>";
            std::string tree;
            for (std::size_t i = 0; i + 2 < w.size(); ++i) tree += (tree.empty() ? "" : " ") + w[i];
            const int tier = std::atoi(w[w.size() - 2].c_str()) - 1, col = std::atoi(w.back().c_str()) - 1;
            for (SpecTree& t : spec_trees_mut())
                if (!t.name.empty() && t.name == tree && tier >= 0 && tier < 3 && col >= 0 && col < 2) {
                    SpecNode& n = t.tiers[std::size_t(tier)][std::size_t(col)];
                    if (!n.name.empty()) return std::format("that place is taken by '{}' (remove it first)", n.name);
                    n = SpecNode{keep_setup_name(name), {StatKey::Count, 0, false}};
                    return {};
                }
            return std::format("no tree '{}' / bad tier or column", tree);
        }
    } else if (kind == "shard" || kind == "rune" || kind == "quint") {
        auto scan = [&](RuneDef& r) {
            if (!r.name.empty() && r.name == name) hits.push_back({&r.name, &r.mod, nullptr, nullptr});
        };
        if (kind == "shard")
            for (auto& row : shards_mut())
                for (RuneDef& r : row) scan(r);
        if (kind == "rune")
            for (auto& row : classic_runes_mut())
                for (RuneDef& r : row) scan(r);
        if (kind == "quint")
            for (RuneDef& r : quints_mut()) scan(r);
        if (hits.empty() && key == "place") {
            std::span<RuneDef> row;
            std::size_t defaults = 0;
            if (kind == "quint") row = quints_mut(), defaults = kDefaultQuints.size();
            else if (kind == "shard") {
                for (int i = 0; i < 3; ++i)
                    if (v == kRuneSlotNames[i]) row = shards_mut()[std::size_t(i)], defaults = 3;
            } else {
                constexpr const char* kSec[] = {"Marks", "Seals", "Glyphs"};
                for (int i = 0; i < 3; ++i)
                    if (v == kSec[i]) row = classic_runes_mut()[std::size_t(i)], defaults = std::size_t(kClassicKinds);
            }
            if (row.empty()) return std::format("{} place: {}", kind, kind == "shard" ? "Offense / Flex / Defense" : kind == "rune" ? "Marks / Seals / Glyphs" : "new");
            if (RuneDef* r = free_place(row, defaults, [](const RuneDef& x) { return x.name.empty(); })) {
                *r = RuneDef{keep_setup_name(name), {StatKey::Count, 0, false}};
                return {};
            }
            return std::format("no free {} place there", kind);
        }
    } else if (kind == "mastery") {
        for (MasteryTree& t : mastery_trees_mut())
            for (auto& tier : t.tiers)
                for (MasteryDef& m : tier)
                    if (m.max_rank && m.name == name) hits.push_back({&m.name, &m.mod, nullptr, &m.max_rank});
        if (hits.empty() && key == "place") {  // <tree> <tier>
            const auto w = words(v);
            if (w.size() < 2) return "mastery place = <tree> <tier 1-5>";
            const int tier = std::atoi(w.back().c_str()) - 1;
            for (MasteryTree& t : mastery_trees_mut())
                if (t.name == w[0] && tier >= 0 && tier < kMasteryTiers) {
                    for (MasteryDef& m : t.tiers[std::size_t(tier)])
                        if (m.max_rank == 0) {
                            m = MasteryDef{keep_setup_name(name), 1, {StatKey::Count, 0, false}};
                            return {};
                        }
                    return "no free place in that tier (3 per tier; remove one first)";
                }
            return std::format("no mastery tree '{}' or bad tier", w[0]);
        }
    } else {
        return std::format("unknown setup kind '{}' (keystone tree node shard rune quint mastery)", kind);
    }
    if (hits.empty()) return std::format("no {} named '{}' (to add one: setup.{}.{}.place = …)", kind, name, kind, name);
    if (key == "place") return {};  // (already there)
    const std::string_view new_text = keep_setup_name(v);
    for (const Hit& h : hits) {
        if (key == "name") *h.nm = new_text;
        else if ((key == "desc" || key == "theme") && h.text2) *h.text2 = new_text;
        else if (key == "removed") {
            if (v != "0") {
                *h.nm = {};
                if (h.max_rank) *h.max_rank = 0;
            }
        } else if (key == "stat" && h.mod) {
            const StatKey sk = stat_of(v);
            if (sk == StatKey::Count) return std::format("unknown stat '{}'", v);
            h.mod->key = sk;
        } else if (key == "pct" && h.mod) h.mod->pct = v != "0";
        else if (key == "value" && h.mod) {
            const std::optional<NumExpr> e = parse_num_expr(v);
            if (!e) return std::format("'{}' is not a number", v);
            h.mod->value = std::int32_t(std::clamp<std::int64_t>(apply_num_expr(*e, h.mod->value, 100), -10'000'000, 10'000'000));
        } else if (key == "max_rank" && h.max_rank) {
            const std::optional<NumExpr> e = parse_num_expr(v);
            if (!e) return std::format("'{}' is not a number", v);
            *h.max_rank = std::uint8_t(std::clamp<std::int64_t>(apply_num_expr(*e, *h.max_rank, 1), 1, 9));
        } else {
            return std::format("unknown key '{}' for a {}", key, kind);
        }
    }
    return {};
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
        if (kind == "setup") {  // [U-483] setup.<kind>.<name>.<key>
            const auto d3 = what.find('.');
            if (d3 == std::string::npos) {
                error = std::format("line {}: write setup.<kind>.<name>.<key>", line_no);
                break;
            }
            if (const std::string e = apply_setup_line(what.substr(0, d3), what.substr(d3 + 1), key, v); !e.empty()) {
                error = std::format("line {}: {}", line_no, e);
                break;
            }
            ++n;
            continue;
        }
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
