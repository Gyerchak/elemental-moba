// [X-19][X-20] Replays and watching. A replay is small: the match settings, who played what, the random seed and every command
// with its tick. The world is deterministic, so playing it back means running the match again; a checksum every 10 s tells when
// that stopped being true (another build or other own champions). replays/<date>_<map>.emr; the same build plays them.
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

#include "game/match/maps.hpp"
#include "net/server.hpp"

namespace em::net {

namespace {
constexpr std::uint32_t kReplayMagic = 0x31524d45;  // "EMR1"
}

std::uint64_t Server::world_checksum() const {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (const Unit& u : world_->units) mix(std::uint64_t(std::uint32_t(u.x)) << 32 | std::uint32_t(u.y)), mix(std::uint32_t(u.hp)), mix(std::uint32_t(u.gold));
    mix(world_->rng);
    return h;
}

void Server::replay_note_champion(const ReplayChampion& c) { replay_.champions.push_back(c); }

void Server::replay_note(std::uint8_t type, std::uint32_t unit, const Cmd& c, std::uint64_t value) {
    if (replaying_ || !world_) return;
    replay_.events.push_back({world_->tick, type, unit, c, value});
}

void Server::save_replay() {
    if (replaying_ || replay_saved_ || replay_.champions.empty() || !cfg_.record_replays) return;
    replay_saved_ = true;
    Writer w;
    w.put<std::uint32_t>(kReplayMagic);
    w.put<std::uint16_t>(kProtocol);
    w.str(cfg_.version);
    w.str(std::string(cur_map().id));
    w.put(replay_.config);
    w.put<std::uint32_t>(replay_.rng);
    w.put<std::uint16_t>(std::uint16_t(replay_.champions.size()));
    for (const ReplayChampion& c : replay_.champions) {
        w.put(c.team), w.put(c.champ), w.put(c.q), w.put(c.f), w.put(c.g), w.put(c.bot), w.put(c.slot_id);
        w.str(c.name);
        w.put(c.loadout);
    }
    w.put<std::uint32_t>(std::uint32_t(replay_.events.size()));
    for (const ReplayEvent& e : replay_.events) {
        w.put(e.tick), w.put(e.type), w.put(e.unit), w.put(e.value);
        if (e.type == 0) w.put(e.cmd);
    }
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char stamp[32] = {};
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    std::error_code ec;
    std::filesystem::create_directories("replays", ec);
    std::ofstream out(std::filesystem::path("replays") / std::format("{}_{}.emr", stamp, cur_map().id), std::ios::binary);
    out.write(reinterpret_cast<const char*>(w.buf.data()), std::streamsize(w.buf.size()));
}

bool Server::load_replay(std::string* error) {
    std::ifstream in(cfg_.replay_file, std::ios::binary);
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), {});
    Reader r({data.data(), data.size()});
    auto fail = [&](std::string_view why) {
        if (error) *error = std::string(why);
        return false;
    };
    if (data.empty() || r.get<std::uint32_t>() != kReplayMagic) return fail("this is not a replay file");
    if (r.get<std::uint16_t>() != kProtocol) return fail("this replay was recorded with another build of the game");
    const std::string version = r.str(), map_id = r.str();
    if (version != cfg_.version) cfg_.version = version;
    const int mi = map_index(map_id);
    if (mi < 0) return fail("this replay's map is not here");
    replay_.config = r.get<MatchConfig>();
    replay_.config.map = std::uint8_t(mi);
    replay_.rng = r.get<std::uint32_t>();
    replay_.champions.resize(r.get<std::uint16_t>());
    for (ReplayChampion& c : replay_.champions) {
        c.team = r.get<std::uint8_t>(), c.champ = r.get<std::int32_t>(), c.q = r.get<std::int32_t>(), c.f = r.get<std::int32_t>(), c.g = r.get<std::int32_t>();
        c.bot = r.get<std::uint8_t>(), c.slot_id = r.get<std::int32_t>();
        c.name = r.str();
        c.loadout = r.get<Loadout>();
    }
    replay_.events.resize(std::min<std::uint32_t>(r.get<std::uint32_t>(), 20'000'000));
    for (ReplayEvent& e : replay_.events) {
        e.tick = r.get<std::uint64_t>(), e.type = r.get<std::uint8_t>(), e.unit = r.get<std::uint32_t>(), e.value = r.get<std::uint64_t>();
        if (e.type == 0) e.cmd = r.get<Cmd>();
    }
    if (r.bad()) return fail("the replay file is damaged");
    cfg_.map = mi;
    replaying_ = true;
    return true;
}

void Server::start_replay_match() {
    set_map(cfg_.map);
    world_ = std::make_unique<World>(replay_.config);
    for (const ReplayChampion& c : replay_.champions) {
        if (c.champ < 0 || c.champ >= int(champions().size())) {
            send_chat(Chat::System, "server", "This replay uses own champions that are not here.", nullptr, nullptr);
            world_.reset();
            return;
        }
        world_->add_champion(Team(c.team), c.champ, std::int8_t(c.q), std::int8_t(c.f), std::int8_t(c.g), c.bot != 0, c.slot_id, c.name, c.loadout);
    }
    world_->rng = replay_.rng;
    replay_next_ = 0;
    in_match = true;
    feed_sent_ = 0;
    over_ms_ = -1;
    for (Player& p : players_)
        if (p.connected && p.joined) send_match_start(p);
}

// Before each step: what happened at this tick.
void Server::replay_step() {
    while (replay_next_ < replay_.events.size() && replay_.events[replay_next_].tick <= world_->tick) {
        const ReplayEvent& e = replay_.events[replay_next_++];
        switch (e.type) {
            case 0: world_->command(e.unit, e.cmd); break;
            case 1:
            case 2:
                if (Unit* u = world_->find(e.unit)) u->bot = e.type == 1;
                break;
            case 3:
                if (e.value != world_checksum() && !replay_off_) {
                    replay_off_ = true;
                    send_chat(Chat::System, "server", "The replay no longer matches this build: from here on it shows something else.", nullptr, nullptr);
                    std::fprintf(stderr, "replay out of step at tick %llu\n", static_cast<unsigned long long>(e.tick));
                }
                break;
            default: break;
        }
    }
}

}  // namespace em::net
