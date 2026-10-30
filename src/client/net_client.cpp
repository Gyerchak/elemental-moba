// Client networking. See net_client.hpp.
#include "game/champions/custom.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include "client/net_client.hpp"
#include "game/match/maps.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>

namespace em {

using namespace net;

// [U-420] the server's map: the same terrain here (and its resource pack, see Resources::map_pack)
void NetClient::use_map(int index) {
    set_map(index);
    map = cur_map().id;
    grid = cur_map().make();
    prepare_match_grid(grid);
    seen = grid;
    grid_dirty = true;
}

NetClient::NetClient() : grid(make_default_map()) {
    prepare_match_grid(grid);
    seen = grid;
}

NetClient::~NetClient() { disconnect(); }

bool NetClient::connect(const std::string& host, std::uint16_t port, const std::string& name, std::uint64_t secret,
                        std::string version, bool allow_skins) {
    disconnect();
    address = host, this->port = port;  // [U-422] for invites
    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (host_) enet_host_compress_with_range_coder(host_);  // [U-280] must match the server
    if (!host_) return false;
    ENetAddress addr{};
    if (enet_address_set_host(&addr, host.c_str()) != 0) {
        reject_reason = "unknown host";
        state = State::Rejected;
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }
    addr.port = port;
    peer_ = enet_host_connect(host_, &addr, 2, 0);
    if (!peer_) return false;
    name_ = name;
    secret_ = secret;
    allow_skins_ = allow_skins;
    client_version = std::move(version);
    state = State::Connecting;
    grid = cur_map().make();
    prepare_match_grid(grid);
    seen = grid;
    have_snapshot = false;
    pos_hist.clear();  // [U-338]
    roster.clear();
    skins.clear();
    final_stats.clear();
    back_in_lobby = false;
    winner = -1;
    feed.clear();
    announces.clear();
    wall_anims.clear();
    last_break_ms = -1;
    recap = {};
    waves.clear();
    thread_ = std::jthread([this](std::stop_token st) { net_loop(st); });
    return true;
}

void NetClient::disconnect() {
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
    if (peer_) {
        enet_peer_disconnect_now(peer_, 0);
        peer_ = nullptr;
    }
    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
    std::scoped_lock lk(mu_);
    inbox_.clear();
    outbox_.clear();
    state = State::Idle;
    if (custom_from_server >= 0) {  // [U-415] the server's own champions are dropped; the app loads mine again
        custom_from_server = -1;
        reload_own_champions = true;
    }
}

// The network thread: sends what the game queued, services enet with a 1 ms wait, hands packets over.
void NetClient::net_loop(std::stop_token st) {
    while (!st.stop_requested()) {
        {
            std::scoped_lock lk(mu_);
            for (auto& [bytes, reliable] : outbox_) {
                if (!peer_) break;
                ENetPacket* p = enet_packet_create(bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
                enet_peer_send(peer_, reliable ? 0 : 1, p);
            }
            outbox_.clear();
        }
        ENetEvent ev;
        int got = enet_host_service(host_, &ev, 1);
        while (got > 0) {
            Packet pk;
            if (ev.type == ENET_EVENT_TYPE_RECEIVE) {
                pk.data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
            } else if (ev.type == ENET_EVENT_TYPE_CONNECT) {
                pk.event = 1;
            } else if (ev.type == ENET_EVENT_TYPE_DISCONNECT) {
                pk.event = 2;
            }
            {
                std::scoped_lock lk(mu_);
                inbox_.push_back(std::move(pk));
            }
            got = enet_host_check_events(host_, &ev);
        }
        if (peer_) rtt_ = peer_->roundTripTime;
    }
}

void NetClient::send_raw(const Writer& w, bool reliable) {
    std::scoped_lock lk(mu_);
    outbox_.push_back({w.buf, reliable});
}

void NetClient::send_pick(int team, int champ, int q, int f, int g, bool ready, const Loadout& lo) {
    Writer w;
    w.put(Msg::Pick);
    w.put(std::int8_t(team));
    w.put(std::int8_t(champ));
    w.put(std::int8_t(q));
    w.put(std::int8_t(f));
    w.put(std::int8_t(g));
    w.put<std::uint8_t>(ready);
    w.put(lo);
    send_raw(w, true);
}
void NetClient::send_ban(int champ) {
    Writer w;
    w.put(Msg::Ban);
    w.put(std::int8_t(champ));
    send_raw(w, true);
}
void NetClient::send_start() {
    Writer w;
    w.put(Msg::StartMatch);
    send_raw(w, true);
}
void NetClient::send_cmd(const Cmd& c) {
    Writer w;
    w.put(Msg::Command);
    w.put(c);
    send_raw(w, true);  // orders must not get lost; enet keeps them in order
}
void NetClient::send_chat(Chat ch, const std::string& text, const std::string& to) {
    Writer w;
    w.put(Msg::Chat);
    w.put(ch);
    w.str(to);
    w.str(text);
    send_raw(w, true);
}
void NetClient::send_skin(const std::vector<std::uint8_t>& bmp) {
    Writer w;
    w.put(Msg::SkinUpload);
    w.put<std::uint32_t>(std::uint32_t(bmp.size()));
    for (std::uint8_t b : bmp) w.put(b);
    send_raw(w, true);
}

void NetClient::send_own_emote(std::uint8_t idx, const std::vector<std::uint8_t>& png, const std::vector<std::uint8_t>& wav) {
    Writer w;  // [U-414]
    w.put(Msg::EmoteUpload);
    w.put(idx);
    w.put<std::uint32_t>(std::uint32_t(png.size()));
    for (std::uint8_t b : png) w.put(b);
    w.put<std::uint32_t>(std::uint32_t(wav.size()));
    for (std::uint8_t b : wav) w.put(b);
    send_raw(w, true);
}

void NetClient::send_lobby_config(int map_index, int rules) {
    Writer w;
    w.put(Msg::LobbyConfig);
    w.put(std::uint8_t(map_index)), w.put(std::uint8_t(rules));
    send_raw(w, true);
}
void NetClient::send_transfer_owner(std::uint8_t slot_id) {
    Writer w;
    w.put(Msg::TransferOwner);
    w.put(slot_id);
    send_raw(w, true);
}
void NetClient::send_queue(bool on, int map_index, int rules) {
    Writer w;
    w.put(Msg::Queue);
    w.put(std::uint8_t(on)), w.put(std::uint8_t(map_index)), w.put(std::uint8_t(rules));
    send_raw(w, true);
}

void NetClient::poll(double now_s) {
    std::deque<Packet> got;
    {
        std::scoped_lock lk(mu_);
        got.swap(inbox_);
    }
    for (Packet& p : got) {
        if (p.event == 1) {  // connected → say hello
            Writer w;
            w.put(Msg::Hello);
            w.put(kProtocol);
            w.str(client_version);
            w.str(name_);
            w.put(secret_);
            w.put<std::uint8_t>(this_platform());  // [U-213]
            w.put<std::uint8_t>(allow_skins_);     // [U-98]
            send_raw(w, true);
        } else if (p.event == 2) {
            if (state != State::Rejected) state = State::Disconnected;
        } else {
            handle(p.data.data(), p.data.size(), now_s);
        }
    }
    ping_ms = rtt_.load();
    std::erase_if(waves, [&](const LocalWave& w) { return now_s - w.born > 1.6; });
    std::erase_if(feed, [&](const TextLine& t) { return now_s - t.born > 12; });
    std::erase_if(hits, [&](const LocalHit& h) { return now_s - h.born > 0.9; });
    std::erase_if(rewards, [&](const LocalReward& r) { return now_s - r.born > 1.6; });
    if (chat.size() > 200) chat.erase(chat.begin(), chat.begin() + std::ptrdiff_t(chat.size() - 200));
}

void NetClient::handle(const std::uint8_t* data, std::size_t n, double now_s) {
    Reader r({data, n});
    const Msg m = r.get<Msg>();
    switch (m) {
        case Msg::Welcome:
            my_id = r.get<std::uint8_t>();
            server_version = r.str();
            tick_hz = r.get<std::uint32_t>();
            practice = r.get<std::uint8_t>() != 0;
            is_host = r.get<std::uint8_t>() != 0;
            server_name = r.str();
            server_motd = r.str();
            ruleset = r.get<std::uint8_t>();          // [U-416]
            own_emotes_allowed = r.get<std::uint8_t>() != 0;  // [U-414]
            use_map(r.get<std::uint8_t>());                   // [U-420]
            if (state != State::Match) state = State::Lobby;
            break;
        case Msg::Reject:
            reject_reason = r.str();
            server_version = r.str();
            state = State::Rejected;
            break;
        case Msg::Lobby: {
            lobby.clear();
            const std::uint8_t k = r.get<std::uint8_t>();
            for (int i = 0; i < k && !r.bad(); ++i) {
                LobbySlot s;
                s.id = r.get<std::uint8_t>();
                s.name = r.str();
                s.team = r.get<std::int8_t>();
                s.champ = r.get<std::int8_t>();
                if (s.champ >= int(champions().size())) s.champ = -1;  // [U-383]
                s.q = r.get<std::int8_t>();
                s.f = r.get<std::int8_t>();
                s.g = r.get<std::int8_t>();
                s.ready = r.get<std::uint8_t>() != 0;
                s.bot = r.get<std::uint8_t>() != 0;
                s.host = r.get<std::uint8_t>() != 0;
                s.ban = r.get<std::int8_t>();
                s.platform = r.get<std::uint8_t>();
                s.has_skin = r.get<std::uint8_t>() != 0;
                s.lp = r.get<std::int32_t>();
                s.queued = r.get<std::uint8_t>() != 0, s.q_map = r.get<std::uint8_t>(), s.q_rules = r.get<std::uint8_t>();  // [U-423]
                s.plays = r.get<std::uint8_t>() != 0;
                lobby.push_back(std::move(s));
            }
            lobby_in_match = r.get<std::uint8_t>() != 0;
            phase = r.get<DraftPhase>();
            bans_enabled = r.get<std::uint8_t>() != 0;
            lobby_timer_s = r.get<std::uint16_t>();  // [U-276]
            lobby_timer_at = now_s;
            {  // [U-420][U-422][U-423] the lobby's map / rules / queue mode (the owner may change them)
                const std::uint8_t m = r.get<std::uint8_t>();
                ruleset = r.get<std::uint8_t>();
                queue_mode = r.get<std::uint8_t>() != 0;
                if (!r.bad() && state != State::Match && maps()[std::min<std::size_t>(m, maps().size() - 1)].id != map) use_map(m);
            }
            if (state == State::Match && !lobby_in_match && winner >= 0) back_in_lobby = true;
            break;
        }
        case Msg::MatchStart: {
            my_unit = r.get<std::uint32_t>();
            my_team = r.get<std::uint8_t>();
            tick_hz = r.get<std::uint32_t>();
            start_ms = r.get<std::int64_t>();
            practice = r.get<std::uint8_t>() != 0;
            roster.clear();
            announces.clear();
            wall_anims.clear();
            last_break_ms = -1;
            recap = {};
            const std::uint8_t k = r.get<std::uint8_t>();
            for (int i = 0; i < k && !r.bad(); ++i) {
                RosterEntry e;
                e.id = r.get<std::uint32_t>();
                e.team = r.get<std::uint8_t>();
                e.champ = r.get<std::int8_t>();
                if (e.champ >= int(champions().size())) e.champ = 0;  // [U-383] a server can't make me index past the roster
                e.q = r.get<std::int8_t>();
                e.f = r.get<std::int8_t>();
                e.g = r.get<std::int8_t>();
                e.name = r.str();
                roster[e.id] = std::move(e);
            }
            grid = make_default_map();
            prepare_match_grid(grid);
            seen = grid;
            grid_dirty = true;
            have_snapshot = false;
            pos_hist.clear();  // [U-338]
            winner = -1;
            final_stats.clear();
            back_in_lobby = false;
            state = State::Match;
            break;
        }
        case Msg::Cells: {
            const std::uint32_t k = r.get<std::uint32_t>();
            for (std::uint32_t i = 0; i < k && !r.bad(); ++i) {
                const CellChange c = r.get<CellChange>();
                if (!Grid::in_bounds(c.x, c.y)) continue;
                grid.at(c.x, c.y) = Cell(c.raw);
                if (c.pub) seen.at(c.x, c.y) = Cell(c.raw);  // [U-288] changes by time: every map, seen or not
            }
            grid_dirty = true;
            break;
        }
        case Msg::Snapshot: {
            Snapshot s;
            s.tick = r.get<std::uint64_t>();
            if (have_snapshot && s.tick <= cur.tick) return;  // old / out of order (unreliable channel)
            s.now_ms = r.get<std::int64_t>();
            s.kills[0] = r.get<std::int32_t>();
            s.kills[1] = r.get<std::int32_t>();
            s.units.resize(r.get<std::uint16_t>());
            for (auto& u : s.units) {
                u = r.get<NetUnit>();
                // [U-383] never trust indices from the server: champion / body / monster name in range
                if (u.champ >= int(champions().size())) u.champ = -1;
                if (u.name_id >= std::size(net::kMobNames)) u.name_id = 0;
            }
            s.fx.resize(r.get<std::uint16_t>());
            for (auto& f : s.fx) f = r.get<NetFx>();
            if (const std::uint16_t nl = r.get<std::uint16_t>(); nl == 0xffff) {  // [U-280] lights unchanged: keep the last ones
                s.lights = cur.lights;
            } else {
                s.lights.resize(nl);
                for (auto& l : s.lights) l = r.get<NetLight>();
            }
            const std::uint16_t nw = r.get<std::uint16_t>();
            for (int i = 0; i < nw && !r.bad(); ++i) waves.push_back({r.get<NetWave>(), now_s});
            const std::uint16_t nh = r.get<std::uint16_t>();
            for (int i = 0; i < nh && !r.bad(); ++i) hits.push_back({r.get<NetHit>(), now_s});
            s.paths.resize(r.get<std::uint8_t>());
            for (auto& p : s.paths) p = r.get<NetPath>();
            s.falcons.resize(r.get<std::uint8_t>());
            for (auto& f : s.falcons) f = r.get<NetFalcon>();
            s.ghosts.resize(r.get<std::uint8_t>());
            for (auto& g : s.ghosts) g = r.get<NetGhost>();
            s.clouds.resize(r.get<std::uint16_t>());
            for (auto& c : s.clouds) c = r.get<NetCloud>();
            s.walls.resize(r.get<std::uint16_t>());  // [U-288]
            for (auto& wh : s.walls) wh = r.get<NetWallHp>();
            {  // [U-341] new falling wall pieces → local animations (each once)
                const std::uint16_t nb = r.get<std::uint16_t>();
                for (int i = 0; i < nb && !r.bad(); ++i) {
                    const NetWallBreak b = r.get<NetWallBreak>();
                    if (b.ms > last_break_ms) wall_anims.push_back({b, now_s});
                }
                for (const auto& a : wall_anims) last_break_ms = std::max(last_break_ms, a.b.ms);
                std::erase_if(wall_anims, [&](const WallAnim& a) { return now_s - a.born > 1.2; });
            }
            s.pings.resize(r.get<std::uint8_t>());
            for (auto& p : s.pings) p = r.get<NetPing>();
            s.scores.resize(r.get<std::uint8_t>());
            for (auto& sc : s.scores) sc = r.get<NetScore>();
            s.companions.resize(r.get<std::uint8_t>());  // [U-407]
            for (auto& c : s.companions) c = r.get<NetCompanion>();
            s.camps.resize(r.get<std::uint8_t>());  // [U-365]
            for (auto& c : s.camps) c = r.get<NetCamp>();
            s.me = r.get<NetMe>();
            s.me.nstatus = std::min<std::uint8_t>(s.me.nstatus, std::uint8_t(std::size(s.me.status)));  // [U-383]
            const std::uint16_t nr = r.get<std::uint16_t>();
            for (int i = 0; i < nr && !r.bad(); ++i) rewards.push_back({r.get<NetReward>(), now_s});
            if (r.bad()) return;
            s.arrived = now_s;
            prev = have_snapshot ? std::move(cur) : s;
            cur = std::move(s);
            {  // [U-338] position history for the delayed interpolation
                PosFrame f{now_s, {}};
                for (const auto& u : cur.units) f.pos[u.id] = {u.x, u.y};
                for (const auto& e : cur.fx) f.fx[e.id] = {e.x, e.y};               // [U-389] effects and companions
                for (const auto& c : cur.companions) f.comp[c.owner] = {c.x, c.y};  // ride the same delayed timeline
                pos_hist.push_back(std::move(f));
                while (pos_hist.size() > 8) pos_hist.pop_front();
            }
            have_snapshot = true;
            break;
        }
        case Msg::Feed: feed.push_back({r.str(), now_s}); break;
        case Msg::ChatLine: {
            ChatLine c;
            c.channel = r.get<Chat>();
            c.from = r.str();
            c.text = r.str();
            c.born = now_s;
            if (!r.bad()) chat.push_back(std::move(c));
            break;
        }
        case Msg::GameOver: winner = r.get<std::uint8_t>(); break;
        case Msg::Announce: {
            const NetAnnounce a = r.get<NetAnnounce>();
            if (!r.bad()) announces.push_back({a, now_s});
            break;
        }
        case Msg::DeathRecap: {
            DeathRecapView v;
            v.killer = r.get<std::uint32_t>();
            v.hits.resize(std::min<std::size_t>(r.get<std::uint16_t>(), 200));
            for (auto& h : v.hits) h = r.get<NetRecapHit>();
            v.born = now_s;
            if (!r.bad()) recap = std::move(v);
            break;
        }
        case Msg::CustomChampions: {  // [U-277] take the server's own champions (save the files, same order)
            const int n = r.get<std::uint8_t>();
            std::vector<std::pair<std::string, std::string>> files;
            for (int i = 0; i < n && !r.bad(); ++i) {
                std::string f = r.str(), text = r.str();
                std::erase_if(f, [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'); });
                if (f.size() > 6 && f.ends_with(".champ")) files.push_back({f, text});
            }
            if (r.bad()) break;
            std::vector<ChampionDef> defs;
            for (auto& [f, text] : files) {
                ChampionDef d;
                if (parse_champion_text(text, d).empty()) defs.push_back(d);
            }
            bool same = int(champions().size()) - builtin_champion_count() == int(defs.size());
            for (std::size_t i = 0; same && i < defs.size(); ++i) same = champions()[std::size_t(builtin_champion_count()) + i].name == defs[i].name;
            if (same) break;  // already the same list (e.g. I am the host)
            std::error_code ec;
            // the server's files go into their own folder: they never overwrite my own champions (same file name) [U-415]
            std::filesystem::create_directories("custom_champions/_server", ec);
            for (auto& [f, text] : files) std::ofstream(std::filesystem::path("custom_champions/_server") / f, std::ios::binary) << text;
            reset_custom_champions();
            for (const ChampionDef& d : defs) register_champion(d);
            custom_from_server = int(defs.size());
            break;
        }
        case Msg::FinalStats: {
            final_stats.resize(r.get<std::uint8_t>());
            for (auto& f : final_stats) f = r.get<FinalRow>();
            break;
        }
        case Msg::SkinData: {
            const std::uint32_t unit = r.get<std::uint32_t>();
            const std::uint32_t sz = r.get<std::uint32_t>();
            if (sz > 512 * 1024) return;
            std::vector<std::uint8_t> b(sz);
            for (auto& x : b) x = r.get<std::uint8_t>();
            if (!r.bad()) skins[unit] = std::move(b);
            break;
        }
        case Msg::EmoteData: {  // [U-414]
            const std::uint32_t unit = r.get<std::uint32_t>();
            const std::uint8_t idx = r.get<std::uint8_t>();
            const std::uint32_t np = r.get<std::uint32_t>();
            if (idx >= kOwnEmotes || np > kOwnEmotePngMax) return;
            std::vector<std::uint8_t> png(np);
            for (auto& x : png) x = r.get<std::uint8_t>();
            const std::uint32_t nw = r.get<std::uint32_t>();
            if (nw > kOwnEmoteWavMax) return;
            std::vector<std::uint8_t> wav(nw);
            for (auto& x : wav) x = r.get<std::uint8_t>();
            if (!r.bad()) own_emotes[unit][idx] = {std::move(png), std::move(wav)};
            break;
        }
        case Msg::Notice:
            notice = r.str();
            ++notice_seq;
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- server list ping [U-212]
ServerInfo ping_server(const std::string& host, std::uint16_t port, int timeout_ms) {
    ServerInfo info;
    info.done = true;
    ENetHost* h = enet_host_create(nullptr, 1, 2, 0, 0);
    if (h) enet_host_compress_with_range_coder(h);
    if (!h) return info.error = "no socket", info;
    ENetAddress addr{};
    if (enet_address_set_host(&addr, host.c_str()) != 0) {
        enet_host_destroy(h);
        return info.error = "unknown host", info;
    }
    addr.port = port;
    ENetPeer* p = enet_host_connect(h, &addr, 2, 0);
    const auto t0 = std::chrono::steady_clock::now();
    auto left = [&] {
        return std::max<long long>(0, timeout_ms - std::chrono::duration_cast<std::chrono::milliseconds>(
                                                       std::chrono::steady_clock::now() - t0).count());
    };
    ENetEvent ev;
    while (left() > 0 && enet_host_service(h, &ev, std::uint32_t(left())) > 0) {
        if (ev.type == ENET_EVENT_TYPE_CONNECT) {
            Writer w;
            w.put(Msg::StatusQuery);
            enet_peer_send(p, 0, enet_packet_create(w.buf.data(), w.buf.size(), ENET_PACKET_FLAG_RELIABLE));
        } else if (ev.type == ENET_EVENT_TYPE_RECEIVE) {
            Reader r({ev.packet->data, ev.packet->dataLength});
            if (r.get<Msg>() == Msg::StatusReply) {
                info.name = r.str();
                info.motd = r.str();
                info.version = r.str();
                info.players = r.get<std::uint8_t>();
                info.max_players = r.get<std::uint8_t>();
                info.in_match = r.get<std::uint8_t>() != 0;
                info.platforms = r.get<std::uint8_t>();
                info.ok = !r.bad();
                const int nr = r.get<std::uint8_t>();  // [U-276] ranking (older servers: none)
                for (int i = 0; i < nr && !r.bad(); ++i) {
                    ServerInfo::Ranked e;
                    e.name = r.str();
                    e.lp = r.get<std::int32_t>();
                    e.wins = r.get<std::uint16_t>();
                    e.losses = r.get<std::uint16_t>();
                    if (!r.bad()) info.ranking.push_back(e);
                }
                info.ping_ms = double(p->roundTripTime);
            }
            enet_packet_destroy(ev.packet);
            if (info.ok) break;
        }
    }
    if (!info.ok && info.error.empty()) info.error = "no answer";
    enet_peer_disconnect_now(p, 0);
    enet_host_destroy(h);
    return info;
}

}  // namespace em
