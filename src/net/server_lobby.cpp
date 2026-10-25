// [U-10][U-101][U-181][U-182][U-200][U-212][U-213] Lobby side of the server: who joins, picks and bans,
// chat channels, skins, and the answer to a server-list ping.
#include <enet/enet.h>

#include <algorithm>
#include <format>

#include "net/server.hpp"

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
        case Msg::StartMatch:
            if (world_ || !(p.slot.host || cfg_.host_secret == 0)) break;
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
    const std::uint16_t proto = r.get<std::uint16_t>();
    const std::string version = r.str();
    std::string name = r.str().substr(0, 24);
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
        send(to.peer, w, true);
        send_custom_champions(to);  // [U-277] before any lobby state that names champions
    };
    // [X-22] reconnect: same name, match running, slot free
    if (world_) {
        for (int i = 0; i < kMaxPlayers; ++i) {
            Player& old = players_[std::size_t(i)];
            if (i != pid && !old.connected && old.unit && old.slot.name == name) {
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
    if (r.bad() || bytes[0] != 'B' || bytes[1] != 'M') return;  // BMP only for now
    p.skin = std::move(bytes);
    p.slot.has_skin = true;
    send_lobby();
}

// ---------------------------------------------------------------- lobby state for everyone
void Server::send_lobby() {
    Writer w;
    w.put(Msg::Lobby);
    std::vector<const LobbySlot*> slots;
    for (const Player& p : players_)
        if (p.connected && p.joined) slots.push_back(&p.slot);
    w.put<std::uint8_t>(std::uint8_t(slots.size()));
    for (const LobbySlot* s : slots) {
        w.put(s->id);
        w.str(s->name);
        w.put(s->team);
        w.put(s->champ);
        w.put(s->q);
        w.put(s->f);
        w.put(s->g);
        w.put<std::uint8_t>(s->ready);
        w.put<std::uint8_t>(s->bot);
        w.put<std::uint8_t>(s->host);
        w.put(s->ban);
        w.put(s->platform);
        w.put<std::uint8_t>(s->has_skin);
        w.put<std::int32_t>(s->lp);
    }
    w.put<std::uint8_t>(in_match.load());
    w.put(phase_);
    w.put<std::uint8_t>(cfg_.bans);
    w.put<std::uint16_t>(std::uint16_t(lobby_deadline_ms_ < 0 ? 0 : std::max<std::int64_t>(1, (lobby_deadline_ms_ - wall_ms()) / 1000)));  // [U-276]
    broadcast(w, true);
}

}  // namespace em::net
