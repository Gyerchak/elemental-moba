// Headless bot-vs-bot match: proves the sim runs to a winner [X-21]. Usage: sim_run [minutes] [seed]
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <format>
#include <print>
#include <string_view>
#include <vector>

#include "game/world.hpp"
#include "game/match/maps.hpp"
#include "game/champions/custom.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>

int main(int argc, char** argv) {
    using namespace em;
    const int minutes = argc > 1 ? std::atoi(argv[1]) : 60;
    MatchConfig cfg;
    cfg.seed = argc > 2 ? std::uint32_t(std::atoi(argv[2])) : 1;
    if (const char* rs = std::getenv("EM_RULESET")) cfg.ruleset = std::uint8_t(std::atoi(rs));  // [U-416] 1 LoL-like, 2 Dota-like
    if (const char* mp = std::getenv("EM_MAP")) cfg.map = std::uint8_t(std::max(0, map_index(mp)));  // [U-420] e.g. twisted_grove
    World w(cfg);
    const bool profile = argc > 3 && std::string_view(argv[3]) == "--profile";  // [U-287]
    w.profile = profile;
    struct Slow {
        double ms;
        std::uint64_t tick;
        std::array<double, World::kProfParts> parts;
    };
    std::vector<Slow> slow;
    std::array<double, World::kProfParts> total{};
    const int ts = cur_map().team_size;  // [U-420] 5 v 5, 3 v 3 …
    // [U-469] EM_CUSTOM=1: the own champions (custom_champions/*.champ) play too — the first players of each team
    std::vector<int> own;
    if (std::getenv("EM_CUSTOM")) {
        std::error_code ec;
        for (const auto& f : std::filesystem::directory_iterator("custom_champions", ec)) {
            if (f.path().extension() != ".champ") continue;
            std::ifstream in(f.path(), std::ios::binary);
            static std::vector<ChampionDef> keep;  // (register_champion keeps a pointer-stable copy? keep ours alive anyway)
            keep.reserve(64);
            keep.emplace_back();
            if (parse_champion_text(std::string((std::istreambuf_iterator<char>(in)), {}), keep.back()).empty()) own.push_back(register_champion(keep.back()));
        }
        std::println("own champions in this match: {}", own.size());
    }
    for (int i = 0; i < 2 * ts; ++i) {
        const int slot = i % ts;
        const int champ = slot < int(own.size()) ? own[std::size_t(slot)] : i;
        w.add_champion(i < ts ? Team::FireNation : Team::EarthNation, champ, int(i % 2), -1, -1, true, -1, "");
    }
    const auto t0 = std::chrono::steady_clock::now();
    std::size_t feed_seen = 0;
    while (!w.over && w.now_ms < std::int64_t(minutes) * 60'000) {
        const auto s0 = std::chrono::steady_clock::now();
        w.step();
        if (profile) {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
            for (int k = 0; k < World::kProfParts; ++k) total[std::size_t(k)] += w.prof_ms[std::size_t(k)];
            slow.push_back({ms, w.tick, w.prof_ms});
            if (slow.size() > 4'000) {  // keep the 20 slowest
                std::ranges::sort(slow, std::greater{}, &Slow::ms);
                slow.resize(20);
            }
        }
        for (; feed_seen < w.feed.size(); ++feed_seen) {}
        if (w.tick % (128 * 60) == 0) {
            int st[2] = {}, lvl[2] = {}, units = 0;
            for (const Unit& u : w.units) {
                ++units;
                if (u.alive && (u.kind == UnitKind::Turret || u.kind == UnitKind::Inhibitor)) ++st[int(u.team)];
                if (u.kind == UnitKind::Champion) lvl[int(u.team)] += u.level;
            }
            std::println("{:>3} min  kills {:>2}-{:<2}  structures {:>2}-{:<2}  avg lvl {:.1f}-{:.1f}  units {:>3}  fx {:>3}",
                         w.now_ms / 60'000, w.team_kills[0], w.team_kills[1], st[0], st[1], lvl[0] / 5.0, lvl[1] / 5.0,
                         units, w.fx.size());
        }
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::println("ticks {}  real {:.1f} s  → {:.0f} ticks/s ({}x real time at 128 Hz)", w.tick, secs, w.tick / secs,
                 int(w.tick / secs / 128));
    if (profile) {  // [U-287] where the time goes, and the worst ticks (budget at 128 Hz = 7.8 ms)
        std::ranges::sort(slow, std::greater{}, &Slow::ms);
        std::println("average ms per tick by part:");
        for (int k = 0; k < World::kProfParts; ++k) std::println("  {:<10} {:.4f}", World::kProfNames[k], total[std::size_t(k)] / double(w.tick));
        std::println("slowest ticks:");
        for (std::size_t i = 0; i < slow.size() && i < 12; ++i) {
            std::string parts;
            for (int k = 0; k < World::kProfParts; ++k)
                if (slow[i].parts[std::size_t(k)] > 0.3) parts += std::format(" {} {:.1f}", World::kProfNames[k], slow[i].parts[std::size_t(k)]);
            std::println("  tick {:>7} ({:>5.1f} s)  {:6.2f} ms:{}", slow[i].tick, double(slow[i].tick) / 128.0, slow[i].ms, parts);
        }
    }
    std::println("result: {}", !w.over ? "no winner yet" : w.winner == Team::FireNation ? "FIRE NATION WINS" : "EARTH NATION WINS");
    for (const Unit& u : w.units)
        if (u.kind == UnitKind::Champion)
            std::println("  {:<15} {} lvl {:>2}  {}/{}/{}  cs {:>3}  gold {:>5}  items {}", u.name,
                         u.team == Team::FireNation ? "Fire " : "Earth", u.level, u.kills, u.deaths, u.assists, u.cs, u.gold,
                         std::count_if(u.items.begin(), u.items.end(), [](auto i) { return i >= 0; }));
    int n = 0;
    for (auto it = w.feed.rbegin(); it != w.feed.rend() && n < 12; ++it, ++n) std::println("  feed {:>5}s {}", it->ms / 1000, it->text);
}
