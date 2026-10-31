// [U-10][U-101][U-181][U-182][U-200][U-212][U-213] Lobby side of the server: who joins, picks and bans,
// chat channels, skins, and the answer to a server-list ping.
#include <random>
#include <enet/enet.h>

#include <algorithm>
#include <format>

#include "net/server.hpp"
#include "game/match/maps.hpp"

namespace em::net {

// ---------------------------------------------------------------- message dispatch
void Server::receive(int pid, Reader& r) {
    Player& p = players_[std::size_t(pid)];
    const Msg m = r.get<Msg>();
    if (m == Msg::StatusQuery) return on_status_query(p);
    if (m == Msg::Hello) return on_hello(pid, r);
    if (!p.joined) return;  // everything else needs a Hello first
    switch (m) {
        case Msg::Pick: on_pick(p, r); break;
        case Msg::Ban: on_ban(p, r); break;
        case Msg::Chat: on_chat(p, r); break;
        case Msg::SkinUpload: on_skin(p, r); break;
        case Msg::EmoteUpload: on_emote(p, r); break;  // [U-414]
        case Msg::LobbyConfig: {  // [U-422] the owner chooses the map + rule set (before the match; not in queue mode)
            const std::uint8_t m = r.get<std::uint8_t>(), rs = r.get<std::uint8_t>(), md = r.get<std::uint8_t>();
            const std::uint16_t ft = r.get<std::uint16_t>();  // [U-478]
            if (r.bad() || world_ || cfg_.queue_mode || pid != owner_) break;
            const bool new_map = std::clamp<int>(m, 0, int(maps().size()) - 1) != cfg_.map;
            cfg_.map = std::clamp<int>(m, 0, int(maps().size()) - 1);
            cfg_.ruleset = std::uint8_t(std::min<int>(rs, 2));
            cfg_.mode = std::uint8_t(std::min<int>(md, int(GameMode::Count) - 1));  // [U-454]
            set_map(cfg_.map);
            cfg_.features = new_map ? cur_map().features : std::uint16_t(ft & ((1u << kFeatureCount) - 1));  // a new map: its defaults
            std::string fs;
            for (int f = 0; f < kFeatureCount; ++f)
                if (cfg_.features & (1u << f)) fs += (fs.empty() ? "" : ", ") + std::string(kFeatureNames[f]);
            send_chat(Chat::System, "server", std::format("Map: {} — mode: {} — systems: {} — features: {}", cur_map().name, kGameModeNames[cfg_.mode],
                                                          kRuleSetNames[cfg_.ruleset], fs.empty() ? "none" : fs), nullptr, nullptr);
            send_lobby();
            break;
        }
        case Msg::TransferOwner: {  // [U-422]
            const std::uint8_t to = r.get<std::uint8_t>();
            if (r.bad() || pid != owner_ || to >= players_.size()) break;
            const Player& t = players_[to];
            if (!t.connected || !t.joined) break;
            owner_ = to;
            send_chat(Chat::System, "server", std::format("{} owns the lobby now", t.slot.name), nullptr, nullptr);
            send_lobby();
            break;
        }
        case Msg::Queue: {  // [U-423] join / leave the queue for a map + mode
            const std::uint8_t on = r.get<std::uint8_t>(), m = r.get<std::uint8_t>(), rs = r.get<std::uint8_t>();
            if (r.bad() || !cfg_.queue_mode || world_ || (p.slot.plays && lobby_deadline_ms_ >= 0)) break;
            p.slot.queued = on != 0;
            p.slot.q_map = std::uint8_t(std::min<int>(m, int(maps().size()) - 1));
            p.slot.q_rules = std::uint8_t(std::min<int>(rs, int(GameMode::Count) - 1));  // (the game mode)
            try_pop_queue();
            send_lobby();
            break;
        }
        case Msg::StartMatch:
            // [U-383] only the host starts; a server without a host secret starts when every player in the lobby is ready to
            // pick (at least 2 players, or practice)
            if (world_ || !(p.slot.host || (cfg_.host_secret == 0 && (cfg_.practice || players >= 2)))) break;
            if (phase_ == DraftPhase::Banning) {  // [U-181] bans done → picks
                phase_ = DraftPhase::Picking;
                send_chat(Chat::System, "server", "Ban phase over — pick your champions", nullptr, nullptr);
                send_lobby();
                break;
            }
            start_match();
            break;
        case Msg::Command: {
            if (!world_ || !p.unit) break;
            const Cmd c = r.get<Cmd>();
            if (r.bad()) break;
            if (!cfg_.practice && (c.kind == CmdKind::CheatGold || c.kind == CmdKind::CheatLevel || c.kind == CmdKind::CheatNoCooldowns))
                break;  // [X-24]
            world_->command(p.unit, c);
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------- server list ping [U-212]
void Server::on_status_query(Player& p) {
    int n = 0;
    for (const Player& o : players_) n += o.connected && o.joined;
    Writer w;
    w.put(Msg::StatusReply);
    w.str(cfg_.name);
    w.str(cfg_.motd);
    w.str(cfg_.version);
    w.put<std::uint8_t>(std::uint8_t(n));
    w.put<std::uint8_t>(std::uint8_t(kMaxPlayers));
    w.put<std::uint8_t>(in_match.load());
    w.put<std::uint8_t>(cfg_.allowed_platforms);
    // [U-276] the server's ranking: top 10 (name, LP, wins, losses)
    const auto top = cfg_.ranked ? ratings_.top(10) : std::vector<std::pair<std::string, Rating>>{};
    w.put<std::uint8_t>(std::uint8_t(top.size()));
    for (const auto& [n, r] : top) {
        w.str(n);
        w.put<std::int32_t>(r.lp);
        w.put<std::uint16_t>(std::uint16_t(r.wins));
        w.put<std::uint16_t>(std::uint16_t(r.losses));
    }
    send(p.peer, w, true);
    enet_peer_disconnect_later(p.peer, 0);
}

// ---------------------------------------------------------------- joining
void Server::on_hello(int pid, Reader& r) {
    Player& p = players_[std::size_t(pid)];
    if (p.joined) return;  // [U-383] a second Hello never joins twice
    const std::uint16_t proto = r.get<std::uint16_t>();
    const std::string version = r.str();
    std::string name = r.str().substr(0, 24);
    // [U-383] nicknames: no control characters or the separators of the ranking file / settings; no cut UTF-8 sequence
    std::erase_if(name, [](char c) { return static_cast<unsigned char>(c) < 32 || c == '|' || c == '=' || c == '#'; });
    while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) name.pop_back();
    if (!name.empty() && static_cast<unsigned char>(name.back()) >= 0xC0) name.pop_back();
    const std::uint64_t secret = r.get<std::uint64_t>();
    const std::uint8_t platform = r.get<std::uint8_t>();
    const bool allow_skins = r.get<std::uint8_t>() != 0;
    auto reject = [&](std::string_view why) {
        Writer w;
        w.put(Msg::Reject);
        w.str(why);
        w.str(cfg_.version);
        send(p.peer, w, true);
        enet_peer_disconnect_later(p.peer, 0);
    };
    if (proto != kProtocol) return reject("protocol mismatch — update the game");
    // [U-95] the client falls back to the server's version itself; we only refuse unknown ones
    if (version != cfg_.version) return reject(std::format("server runs version {}", cfg_.version));
    if (!(cfg_.allowed_platforms & platform))  // [U-213]
        return reject(std::format("this server does not allow {} players", platform_name(platform)));
    if (name.empty()) name = std::format("Player{}", pid);
    auto welcome = [&](Player& to, int id) {
        Writer w;
        w.put(Msg::Welcome);
        w.put<std::uint8_t>(std::uint8_t(id));
        w.str(cfg_.version);
        w.put<std::uint32_t>(cfg_.tick_hz);
        w.put<std::uint8_t>(cfg_.practice);
        w.put<std::uint8_t>(to.slot.host);
        w.str(cfg_.name);
        w.str(cfg_.motd);
        w.put<std::uint8_t>(cfg_.ruleset);                  // [U-416]
        w.put<std::uint8_t>(cfg_.allow_own_emotes ? 1 : 0); // [U-414]
        w.put<std::uint8_t>(std::uint8_t(cfg_.map));        // [U-420]
        send(to.peer, w, true);
        send_custom_champions(to);  // [U-277] before any lobby state that names champions
    };
    // [X-22] reconnect: same name, match running, slot free
    if (world_) {
        for (int i = 0; i < kMaxPlayers; ++i) {
            Player& old = players_[std::size_t(i)];
            if (i != pid && !old.connected && old.unit && old.slot.name == name && old.secret == secret) {  // [U-383] the same player only
                old.peer = p.peer;
                old.connected = true;
                old.joined = true;
                old.peer->data = reinterpret_cast<void*>(std::intptr_t(i));
                p = Player{};
                if (Unit* u = world_->find(old.unit)) u->bot = false;
                welcome(old, i);
                send_match_start(old);
                world_->add_feed(std::format("{} reconnected", name));
                return;
            }
        }
        return reject("match already running");
    }
    if (!cfg_.allow_duplicate_names)  // [U-102]
        for (const Player& o : players_)
            if (o.connected && o.joined && &o != &p && o.slot.name == name) return reject("nickname already used");
    p.joined = true;
    p.allow_skins = allow_skins;
    p.secret = secret;
    if (cfg_.ranked && !cfg_.practice)  // [U-276]
        if (const Rating* rt = ratings_.get(name, secret)) p.slot.lp = rt->lp;
    p.slot.id = std::uint8_t(pid);
    p.slot.name = name;
    p.slot.platform = platform;
    p.slot.host = cfg_.host_secret != 0 && secret == cfg_.host_secret;
    // [U-422] owner: the host of a hosted lobby; on a dedicated server the first player (until handed over / left)
    if (p.slot.host || owner_ < 0 || !players_[std::size_t(owner_)].joined) owner_ = pid;
    p.slot.plays = !cfg_.queue_mode;  // [U-423] in queue mode nobody plays until their queue pops
    int n[2] = {};
    for (const Player& o : players_)
        if (o.connected && o.joined && &o != &p) ++n[std::clamp<int>(o.slot.team, 0, 1)];
    p.slot.team = std::int8_t(n[0] <= n[1] ? 0 : 1);
    p.slot.champ = -1;
    ++players;
    welcome(p, pid);
    send_chat(Chat::System, "server", std::format("{} joined", name), nullptr, nullptr);
    send_lobby();
}

bool Server::champion_banned(int champ) const {
    for (const Player& o : players_)
        if (o.connected && o.joined && o.slot.ban == champ) return true;
    return false;
}

// ---------------------------------------------------------------- picks and bans [U-10][U-181]
void Server::on_pick(Player& p, Reader& r) {
    if (world_) return;
    const std::int8_t team = r.get<std::int8_t>(), champ = r.get<std::int8_t>(), q = r.get<std::int8_t>(),
                      f = r.get<std::int8_t>(), g = r.get<std::int8_t>();
    const bool ready = r.get<std::uint8_t>() != 0;
    const Loadout lo = r.get<Loadout>();
    if (r.bad()) return;
    p.slot.team = std::int8_t(std::clamp<int>(team, 0, 1));
    // [U-08] unique: nobody else may hold this champion; [U-181] banned ones are out
    bool free = champ >= 0 && champ < std::int8_t(champions().size()) && phase_ == DraftPhase::Picking && !champion_banned(champ);
    for (const Player& o : players_)
        if (o.connected && o.joined && &o != &p && o.slot.champ == champ) free = false;
    // [U-07] max one Avatar per team; [U-08] a normal + dark version (same unique group) only once per game
    if (free && champ >= 0) {
        const ChampionDef& c = champions()[std::size_t(champ)];
        for (const Player& o : players_) {
            if (!o.connected || !o.joined || &o == &p || o.slot.champ < 0) continue;
            const ChampionDef& oc = champions()[std::size_t(o.slot.champ)];
            if (c.avatar && oc.avatar && c.dark == oc.dark && o.slot.team == p.slot.team) free = false;  // one Avatar + one Dark Avatar per team
            if (c.unique_group && oc.unique_group == c.unique_group) free = false;
        }
    }
    if (free || champ < 0) p.slot.champ = champ;
    p.slot.q = q;  // checked against the champion when the match starts [U-178]
    p.slot.f = f;
    p.slot.g = g;
    p.slot.ready = ready;
    p.slot.loadout = validated(lo);  // [U-177][X-24]
    send_lobby();
}

void Server::on_ban(Player& p, Reader& r) {
    const std::int8_t champ = r.get<std::int8_t>();
    if (r.bad() || world_ || phase_ != DraftPhase::Banning) return;
    if (champ >= -1 && champ < std::int8_t(champions().size())) p.slot.ban = champ;
    send_lobby();
}

// ---------------------------------------------------------------- chat channels [U-182]
void Server::send_chat(Chat ch, std::string_view from, std::string_view text, const Player* team_of, const Player* to) {
    Writer w;
    w.put(Msg::ChatLine);
    w.put(ch);
    w.str(from);
    w.str(text);
    for (Player& o : players_) {
        if (!o.connected || !o.joined) continue;
        if (team_of && o.slot.team != team_of->slot.team) continue;
        if (to && &o != to) continue;
        send(o.peer, w, true);
    }
}

void Server::on_chat(Player& p, Reader& r) {
    const Chat ch = r.get<Chat>();
    const std::string target = r.str().substr(0, 24);
    const std::string text = r.str().substr(0, 200);
    if (r.bad() || text.empty()) return;
    switch (ch) {
        case Chat::Team: send_chat(ch, p.slot.name, text, &p, nullptr); break;
        case Chat::Whisper: {  // friends chat: to one player by name, echo to me
            Player* to = nullptr;
            for (Player& o : players_)
                if (o.connected && o.joined && o.slot.name == target) to = &o;
            if (!to) {
                send_chat(Chat::System, "server", std::format("{} is not online here", target), nullptr, &p);
                return;
            }
            send_chat(ch, p.slot.name, text, nullptr, to);
            if (to != &p) send_chat(ch, std::format("to {}", target), text, nullptr, &p);
            break;
        }
        case Chat::All:
        case Chat::Lobby:
        default: send_chat(ch, p.slot.name, text, nullptr, nullptr); break;
    }
}

// ---------------------------------------------------------------- skins (Minecraft-like) [U-200]
void Server::on_skin(Player& p, Reader& r) {
    const std::uint32_t n = r.get<std::uint32_t>();
    constexpr std::uint32_t kMaxSkin = 512 * 1024;
    if (r.bad() || n < 2 || n > kMaxSkin) return;
    std::vector<std::uint8_t> bytes(n);
    for (auto& b : bytes) b = r.get<std::uint8_t>();
    if (r.bad() || bytes[0] != 'B' || bytes[1] != 'M' || n < 54) return;  // BMP only for now
    {  // [U-383] a small picture only: width / height ≤ 512, no compression tricks (a 500-byte "30 000 × 30 000" bomb)
        auto i32 = [&](std::size_t o) { return std::int32_t(bytes[o] | bytes[o + 1] << 8 | bytes[o + 2] << 16 | std::uint32_t(bytes[o + 3]) << 24); };
        const std::int32_t w = i32(18), h = i32(22), comp = i32(30);
        if (w <= 0 || w > 512 || h == 0 || h > 512 || h < -512 || (comp != 0 && comp != 3)) return;
    }
    p.skin = std::move(bytes);
    p.slot.has_skin = true;
    send_lobby();
}

// ---------------------------------------------------------------- own emotes [U-414] (separate from skins)
// A small PNG (≤ 256 × 256, checked in the header) + an optional WAV (RIFF). Kept even when the server does not pass
// them on; send_own_emotes() decides at the match start.
void Server::on_emote(Player& p, Reader& r) {
    const std::uint8_t idx = r.get<std::uint8_t>();
    const std::uint32_t np = r.get<std::uint32_t>();
    if (r.bad() || idx >= kOwnEmotes || np < 33 || np > kOwnEmotePngMax) return;
    std::vector<std::uint8_t> png(np);
    for (auto& b : png) b = r.get<std::uint8_t>();
    const std::uint32_t nw = r.get<std::uint32_t>();
    if (r.bad() || nw > kOwnEmoteWavMax) return;
    std::vector<std::uint8_t> wav(nw);
    for (auto& b : wav) b = r.get<std::uint8_t>();
    if (r.bad() || png[0] != 0x89 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G') return;
    auto be32 = [&](std::size_t o) { return std::uint32_t(png[o]) << 24 | std::uint32_t(png[o + 1]) << 16 | std::uint32_t(png[o + 2]) << 8 | png[o + 3]; };
    const std::uint32_t w = be32(16), h = be32(20);  // IHDR
    if (w == 0 || h == 0 || w > 256 || h > 256) return;
    if (!wav.empty() && (wav.size() < 44 || wav[0] != 'R' || wav[1] != 'I' || wav[2] != 'F' || wav[3] != 'F')) wav.clear();
    p.emotes[idx] = {std::move(png), std::move(wav)};
}

// [U-422] the owner left: the first player still in the lobby owns it
void Server::pick_owner() {
    owner_ = -1;
    for (std::size_t i = 0; i < players_.size(); ++i)
        if (players_[i].connected && players_[i].joined) {
            owner_ = int(i);
            send_chat(Chat::System, "server", std::format("{} owns the lobby now", players_[i].slot.name), nullptr, nullptr);
            return;
        }
}

// [U-423] matchmaking: when queue_min players wait in the SAME queue (map + mode), they get that map / mode and random
// teams, then pick their champions (pick timer); everyone else keeps waiting in the lobby
void Server::try_pop_queue() {
    if (!cfg_.queue_mode || world_ || lobby_deadline_ms_ >= 0) return;
    for (const Player& a : players_) {
        if (!a.connected || !a.joined || !a.slot.queued) continue;
        std::vector<Player*> same;
        for (Player& b : players_)
            if (b.connected && b.joined && b.slot.queued && b.slot.q_map == a.slot.q_map && b.slot.q_rules == a.slot.q_rules) same.push_back(&b);
        const int team_cap = 2 * maps()[a.slot.q_map].team_size;
        if (int(same.size()) < std::min(cfg_.queue_min, team_cap)) continue;
        if (int(same.size()) > team_cap) same.resize(std::size_t(team_cap));
        cfg_.map = a.slot.q_map;
        cfg_.mode = std::uint8_t(std::min<int>(a.slot.q_rules, int(GameMode::Count) - 1));  // [U-454] the queue is map + game mode
        set_map(cfg_.map);
        cfg_.features = cur_map().features;  // [U-478] a queue match: the map's default features
        std::shuffle(same.begin(), same.end(), std::mt19937{std::random_device{}()});  // random teams
        for (Player& p : players_) p.slot.plays = false;
        for (std::size_t i = 0; i < same.size(); ++i) {
            same[i]->slot.plays = true;
            same[i]->slot.queued = false;
            same[i]->slot.team = std::int8_t(i % 2);
            same[i]->slot.champ = -1;
        }
        lobby_deadline_ms_ = wall_ms() + std::int64_t(std::max(cfg_.pick_seconds, 60)) * 1000;
        send_chat(Chat::System, "server", std::format("Match found: {} — {} ({} players). Pick your champions!", cur_map().name,
                                                      kGameModeNames[cfg_.mode], same.size()), nullptr, nullptr);
        return;
    }
}

// ---------------------------------------------------------------- lobby state for everyone
void Server::send_lobby() {
    std::vector<const LobbySlot*> slots;
    for (std::size_t i = 0; i < players_.size(); ++i) {
        Player& p = players_[i];
        p.slot.host = int(i) == owner_;  // [U-422] the owner flag is the "host" flag of the lobby
        if (p.connected && p.joined) slots.push_back(&p.slot);
    }
    // [U-454] Blind Pick: before the match nobody sees the other team's picks → one message per player
    const bool blind = cfg_.mode == std::uint8_t(GameMode::BlindPick) && !world_;
    auto build = [&](int viewer_team) {
        Writer w;
        w.put(Msg::Lobby);
        w.put<std::uint8_t>(std::uint8_t(slots.size()));
        for (const LobbySlot* s : slots) {
            const bool hide = blind && viewer_team >= 0 && s->team != viewer_team;
            w.put(s->id);
            w.str(s->name);
            w.put(s->team);
            w.put(hide ? std::int8_t(-1) : s->champ);
            w.put(s->q);
            w.put(s->f);
            w.put(s->g);
            w.put<std::uint8_t>(s->ready);
            w.put<std::uint8_t>(s->bot);
            w.put<std::uint8_t>(s->host);
            w.put(hide ? std::int8_t(-1) : s->ban);
            w.put(s->platform);
            w.put<std::uint8_t>(s->has_skin);
            w.put<std::int32_t>(s->lp);
            w.put<std::uint8_t>(s->queued), w.put(s->q_map), w.put(s->q_rules), w.put<std::uint8_t>(s->plays);  // [U-423]
        }
        w.put<std::uint8_t>(in_match.load());
        w.put(phase_);
        w.put<std::uint8_t>(cfg_.bans);
        w.put<std::uint16_t>(std::uint16_t(lobby_deadline_ms_ < 0 ? 0 : std::max<std::int64_t>(1, (lobby_deadline_ms_ - wall_ms()) / 1000)));  // [U-276]
        w.put<std::uint8_t>(std::uint8_t(cfg_.map)), w.put(cfg_.ruleset), w.put<std::uint8_t>(cfg_.queue_mode);  // [U-420][U-422][U-423]
        w.put(cfg_.mode);  // [U-454]
        w.put<std::uint16_t>(cfg_.features);  // [U-478]
        return w;
    };
    if (!blind) {
        Writer w = build(-1);
        broadcast(w, true);
        return;
    }
    for (Player& p : players_)
        if (p.connected && p.joined) {
            Writer w = build(p.slot.team);
            send(p.peer, w, true);
        }
}

}  // namespace em::net
