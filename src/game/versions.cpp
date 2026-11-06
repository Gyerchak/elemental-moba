// [U-95] See versions.hpp.
#include "game/versions.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <iterator>
#include <fstream>

#include "core/numbers.hpp"
#include "game/content.hpp"
#include "game/loadout/loadout.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mobs/unit_tweaks.hpp"
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

// ---------------------------------------------------------------- [U-535] own items
//   item.<name>.<key> = value    a name not there yet makes a new item (at the end of the table).
//   keys: cost; every number of ItemDef below (points as in the shop: 25 = +25); group = None / Boots / ElementWard / Special;
//   limit = Many / One / OnePerGroup; from = part, part, part; unique, ghost, classic_only, removed = 0 / 1;
//   any other stat by its name (kStatKeyNames), pct_<stat> for "+x % of it".
namespace {
struct ItemNumber { const char* key; std::int32_t ItemDef::*field; int scale; };
constexpr ItemNumber kItemNumbers[] = {
    {"cost", &ItemDef::cost, 1}, {"health", &ItemDef::health, 100}, {"attack", &ItemDef::attack, 100}, {"elemental_power", &ItemDef::elemental_power, 100},
    {"defence", &ItemDef::defence, 100}, {"elemental_defence", &ItemDef::elemental_defence, 100}, {"attack_speed", &ItemDef::attack_speed, 100},
    {"move_speed", &ItemDef::move_speed, 100}, {"elemental_haste", &ItemDef::elemental_haste, 100}, {"non_elemental_haste", &ItemDef::non_elemental_haste, 100},
    {"cast_speed", &ItemDef::cast_speed, 100}, {"skill_speed", &ItemDef::skill_speed, 100}, {"skill_width", &ItemDef::skill_width, 100},
    {"skill_length", &ItemDef::skill_length, 100}, {"skill_range", &ItemDef::skill_range, 100}, {"skill_radius", &ItemDef::skill_radius, 100},
    {"elemental_lethality", &ItemDef::elemental_lethality, 100}, {"elemental_penetration", &ItemDef::elemental_penetration, 100}, {"tenacity", &ItemDef::tenacity, 100},
    {"cc_amplify", &ItemDef::cc_amplify, 100}, {"model_size", &ItemDef::model_size, 100}, {"resist_water", &ItemDef::resist_water, 100},
    {"resist_earth", &ItemDef::resist_earth, 100}, {"resist_fire", &ItemDef::resist_fire, 100}, {"resist_air", &ItemDef::resist_air, 100},
    {"slow_resist", &ItemDef::slow_resist, 100}, {"slow_resist_ice", &ItemDef::slow_resist_ice, 100}, {"move_speed_pct", &ItemDef::move_speed_pct, 100},
    {"wall_damage", &ItemDef::wall_damage, 100}, {"lifesteal", &ItemDef::lifesteal, 100}, {"omnivamp", &ItemDef::omnivamp, 100},
    {"crit_chance", &ItemDef::crit_chance, 100}, {"crit_damage", &ItemDef::crit_damage, 100}, {"spellvamp", &ItemDef::spellvamp, 100},
    {"on_hit_dmg", &ItemDef::on_hit_dmg, 100}, {"on_hit_max_hp", &ItemDef::on_hit_max_hp, 100}, {"active_cd_ms", &ItemDef::active_cd_ms, 1}, {"true_sight", &ItemDef::true_sight, 1},
};
}  // namespace

std::string item_line(const std::string& name, const std::string& key, const std::string& v) {
    if (name.empty()) return "an item needs a name";
    std::vector<ItemDef>& all = item_store();
    auto it = std::ranges::find_if(all, [&](const ItemDef& d) { return d.name == name; });
    if (it == all.end()) {
        if (all.size() >= 2'000) return "too many items";
        all.push_back(ItemDef{.name = keep_setup_name(name), .cost = 100});
        it = all.end() - 1;
    }
    ItemDef& d = *it;
    auto flag = [&](bool& b) { b = v != "0"; };
    if (key == "unique") return flag(d.unique), std::string();
    if (key == "ghost") return flag(d.ghost), std::string();
    if (key == "classic_only") return flag(d.classic_only), std::string();
    if (key == "removed") return d.mod = v != "0" ? std::string_view("removed") : std::string_view(), std::string();
    if (key == "name") return d.name = keep_setup_name(v), std::string();
    if (key == "group") {
        constexpr const char* n[] = {"None", "Boots", "ElementWard", "Special"};
        for (int i = 0; i < 4; ++i)
            if (v == n[i]) return d.group = ItemGroup(i), std::string();
        return std::format("group '{}': None / Boots / ElementWard / Special", v);
    }
    if (key == "limit") {
        constexpr const char* n[] = {"Many", "One", "OnePerGroup"};
        for (int i = 0; i < 3; ++i)
            if (v == n[i]) return d.limit = BuyLimit(i), std::string();
        return std::format("limit '{}': Many / One / OnePerGroup", v);
    }
    if (key == "active") {
        constexpr const char* n[] = {"None", "LotusTile", "SpiritFlask", "Falcon", "Shadow"};
        for (int i = 0; i < 5; ++i)
            if (v == n[i]) return d.active = ItemActive(i), std::string();
        return std::format("active '{}': None / LotusTile / SpiritFlask / Falcon / Shadow", v);
    }
    if (key == "from") {
        d.from = {};
        std::size_t n = 0, start = 0;
        while (start <= v.size() && n < d.from.size()) {
            const std::size_t c = v.find(',', start);
            const std::string part = trim(v.substr(start, c == std::string::npos ? std::string::npos : c - start));
            if (!part.empty()) d.from[n++] = keep_setup_name(part);
            if (c == std::string::npos) break;
            start = c + 1;
        }
        return {};
    }
    const std::optional<NumExpr> e = parse_num_expr(v);
    if (!e) return std::format("'{}' is not a number", v);
    for (const ItemNumber& f : kItemNumbers)
        if (key == f.key) {
            d.*f.field = std::int32_t(std::clamp<std::int64_t>(apply_num_expr(*e, d.*f.field, f.scale), -100'000'000, 100'000'000));
            return {};
        }
    const bool pct = key.starts_with("pct_");
    const std::string stat = pct ? key.substr(4) : key;
    for (int i = 0; i < int(StatKey::Count); ++i)
        if (stat == kStatKeyNames[i]) {
            ItemDef::More* free = nullptr;
            for (ItemDef::More& m : d.more) {
                if (m.key == StatKey(i) && m.pct == pct) free = &m;
                else if (!free && m.key == StatKey::Count) free = &m;
            }
            if (!free) return "at most 8 other stats on an item";
            *free = {StatKey(i), std::int32_t(apply_num_expr(*e, free->key == StatKey(i) ? free->value : 0, 100)), pct};
            if (free->value == 0) *free = {};
            return {};
        }
    return std::format("unknown key '{}' for an item", key);
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

// [U-375] the same files = the same id, whatever the version is called
std::string version_id(const std::string& name) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::string_view b) {
        for (const char c : b) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    };
    if (name == kOfficialVersion) {
        mix(kOfficialVersion);
    } else {
        std::vector<fs::path> files;
        std::error_code ec;
        for (const auto& e : fs::recursive_directory_iterator(fs::path("versions") / name, ec))
            if (e.is_regular_file() && e.path().extension() != ".info") files.push_back(e.path());  // (source.info: where a copy came from)
        std::ranges::sort(files);
        for (const fs::path& f : files) {
            std::ifstream in(f, std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(in)), {});
            mix(fs::relative(f, fs::path("versions") / name, ec).generic_string()), mix(text);
        }
    }
    return std::format("{:08x}", std::uint32_t(h ^ (h >> 32)));
}

// [U-525] the next free name for what came from this server: "Name v1 2026-11-06", "Name v2 2026-11-09" …
std::string next_server_version_name(const std::string& server_name, const std::string& date) {
    std::string clean;
    for (const char c : server_name) clean += std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ? c : ' ';
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (clean.empty()) clean = "server";
    int top = 0;
    const std::string pre = clean + " v";
    for (const std::string& v : list_versions())
        if (v.starts_with(pre)) top = std::max(top, std::atoi(v.c_str() + pre.size()));
    return std::format("{}{} {}", pre, top + 1, date);
}

// [U-374] a version's text files, to give to a player who does not have them
std::vector<std::pair<std::string, std::string>> version_files(const std::string& name) {
    std::vector<std::pair<std::string, std::string>> out;
    std::error_code ec;
    const fs::path root = fs::path("versions") / name;
    for (const auto& e : fs::recursive_directory_iterator(root, ec))
        if (e.is_regular_file() && e.path().extension() != ".info" && e.file_size(ec) < 8'000'000) {
            std::ifstream in(e.path(), std::ios::binary);
            out.push_back({fs::relative(e.path(), root, ec).generic_string(), std::string((std::istreambuf_iterator<char>(in)), {})});
        }
    return out;
}

bool store_version_files(const std::string& name, const std::vector<std::pair<std::string, std::string>>& files) {
    const auto safe = [](std::string_view p) {  // only plain relative paths inside the version's own folder
        if (p.empty() || p.front() == '/' || p.find("..") != std::string_view::npos || p.find('\\') != std::string_view::npos || p.find(':') != std::string_view::npos) return false;
        return p.ends_with(".txt") || p.ends_with(".png") || p.ends_with(".bmp") || p.ends_with(".wav");
    };
    if (name.empty() || name == kOfficialVersion || name.find('/') != std::string::npos || name.find('\\') != std::string::npos || name.find("..") != std::string::npos) return false;
    std::error_code ec;
    for (const auto& [rel, text] : files) {
        if (!safe(rel)) return false;
        const fs::path to = fs::path("versions") / name / rel;
        fs::create_directories(to.parent_path(), ec);
        std::ofstream(to, std::ios::binary) << text;
    }
    return !files.empty();
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

// ---------------------------------------------------------------- [U-546] units set like champions
std::vector<UnitTweak>& unit_tweaks() {
    static std::vector<UnitTweak> v;
    return v;
}
const UnitTweak* tweak_of(const std::string& name) {
    for (const UnitTweak& t : unit_tweaks())
        if (t.name == name) return &t;
    return nullptr;
}
std::vector<std::string> tweakable_units() {
    std::vector<std::string> out{"Melee Minion", "Ranged Minion", "Siege Minion", "Super Minion", "Turret", "Inhibitor", "Nation Shrine"};
    for (const MonsterDef& m : kMonsters) out.emplace_back(m.name);
    return out;
}
std::string apply_unit_line(const std::string& name, const std::string& key, const std::string& v) {
    const auto names = tweakable_units();
    if (std::ranges::find(names, name) == names.end()) return std::format("no unit named '{}'", name);
    auto it = std::ranges::find_if(unit_tweaks(), [&](const UnitTweak& t) { return t.name == name; });
    if (it == unit_tweaks().end()) unit_tweaks().push_back({.name = name}), it = unit_tweaks().end() - 1;
    UnitTweak& t = *it;
    if (key == "body") {
        const BodyDef* b = v == "Humanoid" ? &kHumanoidBody : v == "Slim" ? &kSlimBody : v == "Broad" ? &kBroadBody : v == "Beast" ? &kBeastBody : nullptr;
        if (!b && v != "Usual") return "body: Usual / Humanoid / Slim / Broad / Beast";
        t.body = b;
        return {};
    }
    if (key.starts_with("skill") && key.size() == 6 && key[5] >= '1' && key[5] <= '3') {
        const SkillDef* found = nullptr;
        for (const ChampionDef& c : champions())
            for (const SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y, &c.q_own, &c.f_own, &c.g_own})
                if (!found && s->name == v) found = s;
        for (const SkillDef& s : q_options())
            if (!found && s.name == v) found = &s;
        for (const SkillDef& s : specials())
            if (!found && s.name == v) found = &s;
        if (!found && !v.empty()) return std::format("no skill named '{}'", v);
        t.skills[std::size_t(key[5] - '1')] = found;
        return {};
    }
    const int x = std::clamp(std::atoi(v.c_str()), 1, 2000);
    if (key == "hp") t.hp = x;
    else if (key == "attack") t.attack = x;
    else if (key == "defence") t.defence = x;
    else if (key == "elemental_defence") t.elemental_defence = x;
    else if (key == "move_speed") t.move_speed = x;
    else if (key == "attack_speed") t.attack_speed = x;
    else if (key == "attack_range") t.attack_range = x;
    else if (key == "size") t.size = x;
    else return std::format("unknown key '{}' for a unit", key);
    return {};
}

std::string apply_item_line(const std::string& name, const std::string& key, const std::string& v) { return item_line(name, key, v); }
std::vector<std::string> item_number_keys() {
    std::vector<std::string> out;
    for (const ItemNumber& f : kItemNumbers) out.push_back(f.key);
    return out;
}
std::int32_t item_number(const ItemDef& d, const std::string& key) {
    for (const ItemNumber& f : kItemNumbers)
        if (key == f.key) return d.*f.field / f.scale;
    return 0;
}
std::string apply_setup_text(const std::string& kind, const std::string& name, const std::string& key, const std::string& v) { return apply_setup_line(kind, name, key, v); }

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
            error = std::format("line {}: write champion.<name>.<key>, skill.<name>.<key> or item.<name>.<key>", line_no);
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
        if (kind == "unit") {  // [U-546] unit.<name>.<key>
            if (const std::string e = apply_unit_line(what, key, v); !e.empty()) {
                error = std::format("line {}: {}", line_no, e);
                break;
            }
            ++n;
            continue;
        }
        if (kind == "item") {  // [U-535] item.<name>.<key>
            if (const std::string e = item_line(what, key, v); !e.empty()) {
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
            error = std::format("line {}: unknown '{}' (champion / skill / item / setup)", line_no, kind);
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
