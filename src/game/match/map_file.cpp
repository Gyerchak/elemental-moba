// [U-518] See map_file.hpp.
#include "game/match/map_file.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>

#include "game/mobs/monsters.hpp"

namespace em {

namespace fs = std::filesystem;

namespace {
std::deque<std::string> g_map_strings;  // MapDef holds string_views
std::vector<std::string> g_blobs;

std::string_view keep(std::string s) {
    auto it = std::ranges::find(g_map_strings, s);
    if (it == g_map_strings.end()) g_map_strings.push_back(std::move(s)), it = std::prev(g_map_strings.end());
    return *it;
}
void line_pt(std::string& t, const char* key, CellPt p) { t += std::format("{} {} {}\n", key, p.first, p.second); }
bool known_monster(std::string_view name) {
    return std::ranges::any_of(kMonsters, [&](const MonsterDef& m) { return m.name == name; });
}
CellPt clamp_pt(int x, int y) {
    if (x < 0 || y < 0) return kNoCell;
    return {std::min(x, kGridSize - 1), std::min(y, kGridSize - 1)};
}

MapDef def_of(const MapDoc& d) {
    MapDef m;
    m.id = keep(d.id), m.name = keep(d.name), m.desc = keep(d.desc);
    m.team_size = d.team_size;
    m.pack = keep(d.pack.empty() ? "map_" + d.id : d.pack);
    m.base = d.base, m.fountain = d.fountain, m.nexus = d.nexus, m.pole = d.pole, m.portal = d.portal, m.spirit_island = d.spirit_island;
    m.lanes = d.lanes, m.lane_structs = d.lane_structs;
    m.nexus_turrets = d.nexus_turrets, m.camps = d.camps, m.aggro_camps = d.aggro_camps, m.buff_camps = d.buff_camps, m.dark_spirits = d.dark_spirits;
    for (const auto& [c, name] : d.epics) m.epics.push_back({c, keep(name)});
    for (const MapDoc::Spot& s : d.spots) m.spots.push_back({s.cell, keep(s.monster), s.mirrored});
    m.epic_islands = d.epic_islands, m.exact_terrain = d.exact_terrain;
    m.minions = d.minions;
    m.features = d.features;
    m.cells = std::make_shared<const std::vector<std::uint32_t>>(d.cells);
    return m;
}
}  // namespace

std::string map_id_of(std::string_view name) {
    std::string id;
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) id += char(std::tolower(c));
        else if ((c == ' ' || c == '_' || c == '-') && !id.empty() && id.back() != '_') id += '_';
    }
    while (!id.empty() && id.back() == '_') id.pop_back();
    return id.empty() ? std::string("map") : id.substr(0, 40);
}

MapDoc doc_of(const MapDef& m) {
    MapDoc d;
    d.id = m.id, d.name = m.name, d.desc = m.desc, d.pack = m.pack;
    d.team_size = m.team_size;
    d.base = m.base, d.fountain = m.fountain, d.nexus = m.nexus, d.pole = m.pole, d.portal = m.portal, d.spirit_island = m.spirit_island;
    d.lanes = m.lanes, d.lane_structs = m.lane_structs;
    d.nexus_turrets = m.nexus_turrets, d.camps = m.camps, d.aggro_camps = m.aggro_camps, d.buff_camps = m.buff_camps, d.dark_spirits = m.dark_spirits;
    for (const EpicSpot& e : m.epics) d.epics.push_back({e.cell, std::string(e.monster)});
    for (const MonsterSpot& s : m.spots) d.spots.push_back({s.cell, std::string(s.monster), s.mirrored});
    d.epic_islands = m.epic_islands, d.exact_terrain = m.exact_terrain;
    d.minions = m.minions;
    d.features = m.features;
    const Grid g = map_grid(m);
    d.cells.resize(std::size_t(kGridSize) * kGridSize);
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x) d.cells[std::size_t(y) * kGridSize + std::size_t(x)] = g.at(x, y).raw();
    return d;
}

std::string doc_to_blob(const MapDoc& d) {
    std::string t = "EMAP1\n";
    auto clean = [](std::string s) {
        std::ranges::replace(s, '\n', ' ');
        return s;
    };
    t += std::format("id {}\nname {}\ndesc {}\npack {}\nteam_size {}\n", d.id, clean(d.name), clean(d.desc), clean(d.pack), d.team_size);
    line_pt(t, "base", d.base), line_pt(t, "fountain", d.fountain), line_pt(t, "nexus", d.nexus);
    line_pt(t, "pole", d.pole), line_pt(t, "portal", d.portal), line_pt(t, "spirit_island", d.spirit_island);
    for (std::size_t i = 0; i < d.lanes.size(); ++i) {
        t += "lane";
        for (const CellPt& p : d.lanes[i]) t += std::format(" {} {}", p.first, p.second);
        t += '\n';
        const LaneStructs s = i < d.lane_structs.size() ? d.lane_structs[i] : LaneStructs{kNoCell, kNoCell, kNoCell, kNoCell};
        t += std::format("structs {} {} {} {} {} {} {} {}\n", s.t1.first, s.t1.second, s.t2.first, s.t2.second, s.t3.first, s.t3.second, s.inhib.first, s.inhib.second);
    }
    for (const CellPt& p : d.nexus_turrets) line_pt(t, "nexus_turret", p);
    for (const CellPt& p : d.camps) line_pt(t, "camp", p);
    for (const CellPt& p : d.aggro_camps) line_pt(t, "aggro_camp", p);
    for (const CellPt& p : d.buff_camps) line_pt(t, "buff_camp", p);
    for (const CellPt& p : d.dark_spirits) line_pt(t, "dark_spirit", p);
    for (const auto& [c, n] : d.epics) t += std::format("epic {} {} {}\n", c.first, c.second, n);
    for (const MapDoc::Spot& s : d.spots) t += std::format("spot {} {} {} {}\n", s.cell.first, s.cell.second, s.mirrored ? 1 : 0, s.monster);
    t += std::format("epic_islands {}\nexact_terrain {}\n", d.epic_islands ? 1 : 0, d.exact_terrain ? 1 : 0);
    t += std::format("melee {}\nranged {}\nsiege_every {}\nfirst_ms {}\nperiod_ms {}\nfeatures {}\n", d.minions.melee, d.minions.ranged, d.minions.siege_every, d.minions.first_ms,
                     d.minions.period_ms, d.features);
    // the cells: pairs of (how many, value), little endian
    std::string bin;
    auto put32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) bin += char((v >> (8 * i)) & 255); };
    for (std::size_t i = 0; i < d.cells.size();) {
        std::size_t j = i;
        while (j < d.cells.size() && d.cells[j] == d.cells[i]) ++j;
        put32(std::uint32_t(j - i)), put32(d.cells[i]);
        i = j;
    }
    t += std::format("cells {}\n", bin.size());
    return t + bin;
}

std::string doc_from_blob(std::string_view blob, MapDoc& out) {
    if (!blob.starts_with("EMAP1\n")) return "not a map file";
    if (blob.size() > 16'000'000) return "the map file is too big";
    MapDoc d;
    d.lanes.clear();
    std::size_t at = 6;
    bool got_cells = false;
    while (at < blob.size() && !got_cells) {
        const std::size_t end = blob.find('\n', at);
        if (end == std::string_view::npos) return "the file ends too early";
        const std::string line(blob.substr(at, end - at));
        at = end + 1;
        const std::size_t sp = line.find(' ');
        const std::string key = line.substr(0, sp), rest = sp == std::string::npos ? "" : line.substr(sp + 1);
        std::istringstream in(rest);
        auto pt = [&](CellPt& p) {
            int x = -1, y = -1;
            in >> x >> y;
            p = clamp_pt(x, y);
        };
        auto pts = [&](std::vector<CellPt>& v) {
            CellPt p;
            pt(p);
            if (p != kNoCell && v.size() < 128) v.push_back(p);
        };
        if (key == "id") d.id = map_id_of(rest);
        else if (key == "name") d.name = rest.substr(0, 40);
        else if (key == "desc") d.desc = rest.substr(0, 300);
        else if (key == "pack") d.pack = rest.substr(0, 60);
        else if (key == "team_size") { in >> d.team_size; d.team_size = std::clamp(d.team_size, 1, 10); }
        else if (key == "base") pt(d.base);
        else if (key == "fountain") pt(d.fountain);
        else if (key == "nexus") pt(d.nexus);
        else if (key == "pole") pt(d.pole);
        else if (key == "portal") pt(d.portal);
        else if (key == "spirit_island") pt(d.spirit_island);
        else if (key == "lane") {
            std::vector<CellPt> l;
            int x = 0, y = 0;
            while (in >> x >> y && l.size() < 64) l.push_back(clamp_pt(x, y));
            std::erase(l, kNoCell);
            if (d.lanes.size() < 8) d.lanes.push_back(std::move(l));
        } else if (key == "structs") {
            LaneStructs s{};
            pt(s.t1), pt(s.t2), pt(s.t3), pt(s.inhib);
            if (d.lane_structs.size() < 8) d.lane_structs.push_back(s);
        }
        else if (key == "nexus_turret") pts(d.nexus_turrets);
        else if (key == "camp") pts(d.camps);
        else if (key == "aggro_camp") pts(d.aggro_camps);
        else if (key == "buff_camp") pts(d.buff_camps);
        else if (key == "dark_spirit") pts(d.dark_spirits);
        else if (key == "epic") {
            CellPt p;
            pt(p);
            std::string n;
            std::getline(in >> std::ws, n);
            if (p != kNoCell && known_monster(n) && d.epics.size() < 32) d.epics.push_back({p, n});
        } else if (key == "spot") {
            CellPt p;
            pt(p);
            int mir = 1;
            in >> mir;
            std::string n;
            std::getline(in >> std::ws, n);
            if (p != kNoCell && known_monster(n) && d.spots.size() < 256) d.spots.push_back({p, n, mir != 0});
        }
        else if (key == "epic_islands") d.epic_islands = rest == "1";
        else if (key == "exact_terrain") d.exact_terrain = rest == "1";
        else if (key == "melee") { in >> d.minions.melee; d.minions.melee = std::clamp(d.minions.melee, 0, 12); }
        else if (key == "ranged") { in >> d.minions.ranged; d.minions.ranged = std::clamp(d.minions.ranged, 0, 12); }
        else if (key == "siege_every") { in >> d.minions.siege_every; d.minions.siege_every = std::clamp(d.minions.siege_every, 0, 20); }
        else if (key == "first_ms") { in >> d.minions.first_ms; d.minions.first_ms = std::clamp<std::int64_t>(d.minions.first_ms, 1'000, 900'000); }
        else if (key == "period_ms") { in >> d.minions.period_ms; d.minions.period_ms = std::clamp<std::int64_t>(d.minions.period_ms, 5'000, 300'000); }
        else if (key == "features") { int f = 0; in >> f; d.features = std::uint16_t(f); }
        else if (key == "cells") {
            std::size_t n = 0;
            in >> n;
            if (n > blob.size() - at || n % 8) return "the cells of the map are damaged";
            const std::size_t want = std::size_t(kGridSize) * kGridSize;
            d.cells.reserve(want);
            const auto* p = reinterpret_cast<const unsigned char*>(blob.data() + at);
            auto get32 = [&](std::size_t o) { return std::uint32_t(p[o]) | std::uint32_t(p[o + 1]) << 8 | std::uint32_t(p[o + 2]) << 16 | std::uint32_t(p[o + 3]) << 24; };
            for (std::size_t o = 0; o < n; o += 8) {
                const std::uint32_t run = get32(o), raw = get32(o + 4);
                if (run > want - d.cells.size()) return "the cells of the map are damaged";
                d.cells.insert(d.cells.end(), run, raw);
            }
            if (d.cells.size() != want) return "the cells of the map are damaged";
            got_cells = true;
        }
    }
    if (!got_cells) return "the map has no cells";
    if (d.lanes.empty()) return "a map needs at least one lane";
    for (const auto& l : d.lanes)
        if (l.size() < 2) return "every lane needs at least two points";
    d.lane_structs.resize(d.lanes.size(), LaneStructs{kNoCell, kNoCell, kNoCell, kNoCell});
    for (const LaneStructs& s : d.lane_structs)
        if (s.t1 == kNoCell || s.t2 == kNoCell || s.inhib == kNoCell) return "every lane needs two turrets and an inhibitor";
    if (d.base == kNoCell || d.fountain == kNoCell || d.nexus == kNoCell) return "a map needs a base, a fountain and a nexus";
    out = std::move(d);
    return {};
}

void set_custom_maps(const std::vector<std::string>& blobs) {
    reset_maps();
    g_blobs.clear();
    for (const std::string& b : blobs) {
        MapDoc d;
        if (!doc_from_blob(b, d).empty()) continue;
        if (add_map(def_of(d))) g_blobs.push_back(b);
    }
}

void load_custom_maps() {
    std::vector<std::pair<std::string, std::string>> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("custom_maps", ec)) {
        if (e.path().extension() != ".emap") continue;
        std::ifstream in(e.path(), std::ios::binary);
        files.push_back({e.path().filename().string(), std::string((std::istreambuf_iterator<char>(in)), {})});
    }
    std::ranges::sort(files);
    std::vector<std::string> blobs;
    for (auto& [f, b] : files) blobs.push_back(std::move(b));
    set_custom_maps(blobs);
}

const std::vector<std::string>& custom_map_blobs() { return g_blobs; }

bool is_builtin_map_id(std::string_view id) { return id == "elemental_world" || id == "foggy_swamp"; }

}  // namespace em
