// [X-01][U-102][U-103] Server core: binding the port, sending, the fixed-tick loop, disconnects.
// Lobby / draft / chat: server_lobby.cpp. Match / snapshots / statistics: server_match.cpp.
#include "game/versions.hpp"
#include "game/champions/custom.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include "net/server.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <thread>

namespace em::net {

Server::Server(ServerConfig cfg) : cfg_(cfg) { players_.resize(kMaxPlayers); }

Server::~Server() {
    if (host_) enet_host_destroy(host_);
}

// [U-277] every valid custom_champions/*.champ. A champion already in the roster (the host loaded it) keeps its
// place; the others are registered now (dedicated server). The list follows the roster order.
void Server::load_custom_champions() {
    namespace fs = std::filesystem;
    std::vector<std::pair<std::string, std::string>> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("custom_champions", ec)) {
        if (e.path().extension() != ".champ") continue;
        std::ifstream in(e.path(), std::ios::binary);
        files.push_back({e.path().filename().string(), std::string((std::istreambuf_iterator<char>(in)), {})});
    }
    std::ranges::sort(files);
    std::erase_if(files, [](const auto& f) { return f.second.size() > 4'000; });  // [U-383] a cut file would differ on clients
    auto name_of = [](const std::string& text) {
        ChampionDef d;
        return parse_champion_text(text, d).empty() ? std::string(d.name) : std::string();
    };
    if (cfg_.host_secret == 0)  // dedicated server: every valid file. Hosted / practice: only what the host enabled.
        for (auto& [file, text] : files) {
            ChampionDef d;
            if (parse_champion_text(text, d).empty()) register_champion(d);
        }
    custom_files_.clear();
    for (int c = builtin_champion_count(); c < int(champions().size()); ++c)
        for (auto& [file, text] : files)
            if (champions()[std::size_t(c)].name == name_of(text)) {
                custom_files_.push_back({file, text});
                break;
            }
}

std::int64_t Server::wall_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// [U-276] the pick timer (servers with pick_seconds > 0): it starts when 2+ players are in the lobby. When it ends:
// everybody picked → the match starts; somebody did not → the lobby is disbanded: they are sent away, picks reset.
void Server::lobby_timer() {
    if (cfg_.pick_seconds <= 0) return;
    int humans = 0;
    for (const Player& p : players_) humans += p.connected && p.joined;
    if (humans < 2) {
        if (lobby_deadline_ms_ >= 0) {
            lobby_deadline_ms_ = -1;
            send_lobby();
        }
        return;
    }
    if (lobby_deadline_ms_ < 0) {
        lobby_deadline_ms_ = wall_ms() + std::int64_t(cfg_.pick_seconds) * 1000;
        send_chat(Chat::System, "server", std::format("Pick your champion: {} s", cfg_.pick_seconds), nullptr, nullptr);
        send_lobby();
        return;
    }
    if (wall_ms() < lobby_deadline_ms_) return;
    lobby_deadline_ms_ = -1;
    bool all = true;
    for (const Player& p : players_)
        if (p.connected && p.joined && p.slot.champ < 0) all = false;
    if (all) {
        start_match();
        return;
    }
    for (Player& p : players_) {
        if (!p.connected || !p.joined) continue;
        if (p.slot.champ < 0) {  // did not pick → sent away
            Writer w;
            w.put(Msg::Reject);
            w.str("you did not pick a champion in time — the lobby was disbanded");
            w.str(cfg_.version);
            send(p.peer, w, true);
            enet_peer_disconnect_later(p.peer, 0);
        }
        p.slot.champ = -1;
        p.slot.ready = false;
    }
    send_chat(Chat::System, "server", "Somebody did not pick — the lobby was disbanded. Pick again.", nullptr, nullptr);
    send_lobby();
}

void Server::send_custom_champions(Player& p) {
    Writer w;
    w.put(Msg::CustomChampions);
    // the same cap as the roster (127 champions in total) → server and clients number the champions the same way
    const std::size_t cap = std::size_t(std::max(0, 127 - builtin_champion_count()));
    w.put<std::uint8_t>(std::uint8_t(std::min(custom_files_.size(), cap)));
    for (std::size_t i = 0; i < custom_files_.size() && i < cap; ++i) {
        w.str(custom_files_[i].first);
        w.str(custom_files_[i].second);
    }
    send(p.peer, w, true);
}

bool Server::start(std::string* error) {
    if (cfg_.version != current_version()) {  // [U-95] own game version (the hosting client has usually applied it already)
        if (const std::string e = apply_version(cfg_.version); !e.empty()) {
            if (error) *error = std::format("version {}: {}", cfg_.version, e);
            return false;
        }
    }
    if (cfg_.allow_custom_champions) load_custom_champions();  // [U-277]
    if (cfg_.ranked && !cfg_.practice) ratings_.load(std::format("server_data/ratings_{}.txt", cfg_.port));  // [U-276]
    ENetAddress addr{};
    addr.host = cfg_.loopback_only ? ENET_HOST_ANY : ENET_HOST_ANY;
    if (cfg_.loopback_only) enet_address_set_host(&addr, "127.0.0.1");
    addr.port = cfg_.port;
    host_ = enet_host_create(&addr, kMaxPlayers + 4, 2, 0, 0);
    if (host_) enet_host_compress_with_range_coder(host_);  // [U-280] smaller packets → less lag (clients do the same)
    if (!host_) {
        if (error) *error = std::format("cannot bind UDP port {}", cfg_.port);
        return false;
    }
    return true;
}

void Server::send(ENetPeer* p, const Writer& w, bool reliable) {
    if (!p) return;
    // [X-105] snapshots are bigger than one UDP packet: without UNRELIABLE_FRAGMENT enet sends their
    // fragments RELIABLY → the reliable queue backs up 64× a second and the ping keeps growing.
    const enet_uint32 flags = reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
    ENetPacket* pk = enet_packet_create(w.buf.data(), w.buf.size(), flags);
    enet_peer_send(p, reliable ? 0 : 1, pk);
}

void Server::broadcast(const Writer& w, bool reliable) {
    for (Player& p : players_)
        if (p.connected) send(p.peer, w, reliable);
}


void Server::drop(int pid) {
    Player& p = players_[std::size_t(pid)];
    p.connected = false;
    p.peer = nullptr;
    if (p.joined) --players;
    p.joined = false;
    if (world_ && p.unit) {  // [X-22] keep the champion: a bot plays it until the player reconnects
        if (Unit* u = world_->find(p.unit)) {
            u->bot = true;
            world_->add_feed(std::format("{} disconnected (bot takes over)", p.slot.name));
        }
    }
    if (!world_) p = Player{};
    send_lobby();
}

void Server::run(std::stop_token stop) {
    using clock = std::chrono::steady_clock;
    const auto tick_len = std::chrono::nanoseconds(1'000'000'000LL / cfg_.tick_hz);
    auto next = clock::now() + tick_len;
    while (!stop.stop_requested()) {
        // network until the next tick is due
        ENetEvent ev;
        for (;;) {
            const auto now = clock::now();
            const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
            if (enet_host_service(host_, &ev, std::uint32_t(std::max<long long>(0, wait_ms))) <= 0) {
                if (clock::now() >= next) break;
                continue;
            }
            switch (ev.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    int pid = -1;
                    for (int i = 0; i < kMaxPlayers; ++i)
                        if (!players_[std::size_t(i)].connected && !players_[std::size_t(i)].unit) {
                            pid = i;
                            break;
                        }
                    if (pid < 0) {
                        enet_peer_disconnect(ev.peer, 0);
                        break;
                    }
                    players_[std::size_t(pid)] = Player{};
                    players_[std::size_t(pid)].peer = ev.peer;
                    players_[std::size_t(pid)].connected = true;
                    ev.peer->data = reinterpret_cast<void*>(std::intptr_t(pid));
                    enet_peer_timeout(ev.peer, 0, 5'000, 10'000);
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    const int pid = int(reinterpret_cast<std::intptr_t>(ev.peer->data));
                    if (pid >= 0 && pid < kMaxPlayers && players_[std::size_t(pid)].peer == ev.peer) {
                        Reader r({ev.packet->data, ev.packet->dataLength});
                        receive(pid, r);
                    }
                    enet_packet_destroy(ev.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    const int pid = int(reinterpret_cast<std::intptr_t>(ev.peer->data));
                    if (pid >= 0 && pid < kMaxPlayers && players_[std::size_t(pid)].peer == ev.peer) drop(pid);
                    break;
                }
                default: break;
            }
        }
        if (!world_) lobby_timer();  // [U-276]
        // fixed ticks; catch up at most 4 behind
        int steps = 0;
        const auto t0 = clock::now();
        while (clock::now() >= next && steps < 4) {
            next += tick_len;
            ++steps;
            if (!world_) continue;
            world_->step();
            match_ms = world_->now_ms;
            // terrain changes (reliable)
            if (!world_->cell_changes.empty()) {
                Writer w;
                w.put(Msg::Cells);
                w.put<std::uint32_t>(std::uint32_t(world_->cell_changes.size()));
                for (const CellChange& c : world_->cell_changes) w.put(c);
                broadcast(w, true);
                world_->cell_changes.clear();
            }
            for (const Announce& an : world_->announces) {  // [U-263] everybody hears them
                Writer w;
                w.put(Msg::Announce);
                w.put(NetAnnounce{std::uint8_t(an.kind), std::uint8_t(an.team), an.a_kind, an.a_extra, an.a, an.b});
                broadcast(w, true);
            }
            world_->announces.clear();
            for (const DeathRecap& rc : world_->recaps)  // [U-262] only the player who died
                for (Player& p : players_)
                    if (p.connected && p.unit == rc.victim) {
                        Writer w;
                        w.put(Msg::DeathRecap);
                        w.put<std::uint32_t>(rc.killer);
                        const std::size_t n = std::min<std::size_t>(rc.hits.size(), 200);
                        w.put<std::uint16_t>(std::uint16_t(n));
                        for (std::size_t i = rc.hits.size() - n; i < rc.hits.size(); ++i) {
                            const TakenHit& h = rc.hits[i];
                            w.put(NetRecapHit{std::int32_t(rc.ms - h.ms), h.src, std::uint8_t(h.tag.from), h.tag.slot, h.sub,
                                              std::uint8_t(h.physical), h.tag.item, h.src_kind, h.src_extra, h.amount, h.shielded, h.cc, 0,
                                              std::uint16_t(std::clamp(h.cc_ms / 10, 0, 65'535))});
                        }
                        send(p.peer, w, true);
                    }
            world_->recaps.clear();
            for (; feed_sent_ < world_->feed.size(); ++feed_sent_) {
                Writer w;
                w.put(Msg::Feed);
                w.str(world_->feed[feed_sent_].text);
                broadcast(w, true);
            }
            if (world_->tick % std::uint64_t(std::max(1, cfg_.snap_div)) == 0)
                for (Player& p : players_)
                    if (p.connected && p.unit) send_snapshot(p);
            if (world_->over) {  // [U-179] statistics now, back to the lobby a few seconds later
                finish_match();
                break;
            }
        }
        if (clock::now() > next + tick_len * 8) next = clock::now() + tick_len;  // too far behind: drop time
        if (steps) {
            const double used = std::chrono::duration<double>(clock::now() - t0).count();
            tick_load = used / (steps * std::chrono::duration<double>(tick_len).count());
        }
        enet_host_flush(host_);
    }
}

}  // namespace em::net
