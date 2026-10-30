// [U-101][U-102][U-103][U-212][U-213] Multiplayer like Minecraft: a saved server list with live status,
// Add / Edit / Delete, Direct connect, Refresh; hosting a match or a server in a few clicks.
#include "net/ranking.hpp"
#include <algorithm>
#include <format>

#include "client/app.hpp"
#include "game/match/maps.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

namespace {
std::pair<std::string, std::uint16_t> split_address(const std::string& a) {
    const auto c = a.rfind(':');
    if (c == std::string::npos) return {a, net::kDefaultPort};
    return {a.substr(0, c), std::uint16_t(std::clamp(std::atoi(a.c_str() + c + 1), 1, 65'535))};
}
std::string platforms_text(std::uint8_t p) {
    std::string s;
    for (std::uint8_t b : {net::kLinux, net::kWindows, net::kMacOS})
        if (p & b) s += std::string(s.empty() ? "" : ", ") + net::platform_name(b);
    return s;
}
}  // namespace

void App::multiplayer() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float x = float(w) / 2 - 420;
    text(ren_, x, 24, tr("Multiplayer"), {255, 200, 120}, 3);
    // refresh: ping every server in the background (never blocks the screen)
    auto refresh = [&] {
        pings_.clear();
        infos_.assign(set_.servers.size(), {});
        for (const ServerEntry& s : set_.servers) {
            const auto [host, port] = split_address(s.address);
            pings_.push_back(std::async(std::launch::async, [host, port] { return ping_server(host, port); }));
        }
    };
    if (infos_.size() != set_.servers.size()) refresh();
    for (std::size_t i = 0; i < pings_.size() && i < infos_.size(); ++i)
        if (pings_[i].valid() && pings_[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) infos_[i] = pings_[i].get();

    float y = 70;
    for (std::size_t i = 0; i < set_.servers.size(); ++i, y += 58) {
        const ServerEntry& s = set_.servers[i];
        const ServerInfo& in = infos_[i];
        const bool sel = selected_server_ == int(i);
        fill_rect(ren_, x, y, 840, 54, sel ? Rgba{50, 50, 75, 255} : Rgba{28, 28, 40, 255});
        rect(ren_, x, y, 840, 54, sel ? Rgba{255, 210, 120} : Rgba{80, 80, 100});
        text(ren_, x + 10, y + 6, s.name, {255, 255, 255}, 2);
        text(ren_, x + 10, y + 26, in.ok ? in.motd : s.address, {170, 170, 190});
        const std::string st = !in.done ? tr("pinging…")
                             : in.ok ? std::format("{}/{}  {:.0f} ms  {}  {}", in.players, in.max_players, in.ping_ms, in.version,
                                                   in.in_match ? tr("in a match") : tr("lobby"))
                                     : std::format("{}: {}", tr("offline"), in.error);
        text(ren_, x + 520, y + 8, st, in.ok ? Rgba{140, 230, 140} : Rgba{230, 120, 120});
        if (in.ok) text(ren_, x + 520, y + 26, platforms_text(in.platforms), {150, 150, 170});
        if (hover(ui_, x, y, 840, 54) && ui_.clicked) {
            if (sel) connect_to(s.address);  // double click = join
            selected_server_ = int(i);
            ui_.clicked = false;
        }
    }
    // [U-276] the selected server's ranking (top 10)
    if (selected_server_ >= 0 && selected_server_ < int(infos_.size()) && !infos_[std::size_t(selected_server_)].ranking.empty() && float(w) > x + 1'200) {
        float ry = 70;
        const float rx = x + 860;
        text(ren_, rx, ry, tr("Ranking on this server"), {255, 220, 140}, 1.5f);
        ry += 24;
        int place = 1;
        for (const auto& e : infos_[std::size_t(selected_server_)].ranking) {
            text(ren_, rx, ry, fit_text(std::format("{:>2}. {}  {}  ({}W / {}L)", place++, e.name, net::tier_name(e.lp), e.wins, e.losses), 330),
                 {220, 220, 230});
            ry += 16;
        }
    }
    y = std::max(y + 10, float(h) - 130);
    const float bw = 205;
    if (button(ren_, ui_, x, y, bw, 34, tr("Join server"), false, selected_server_ >= 0)) connect_to(set_.servers[std::size_t(selected_server_)].address);
    if (button(ren_, ui_, x + 211, y, bw, 34, tr("Direct connect"))) edit_index_ = -2, edit_name_.clear(), edit_addr_ = direct_addr_, go(Screen::ServerEdit);
    if (button(ren_, ui_, x + 422, y, bw, 34, tr("Add server"))) edit_index_ = -1, edit_name_ = tr("My server"), edit_addr_ = "127.0.0.1:7777", go(Screen::ServerEdit);
    if (button(ren_, ui_, x + 633, y, bw, 34, tr("Refresh"))) refresh();
    y += 42;
    if (button(ren_, ui_, x, y, bw, 34, tr("Edit"), false, selected_server_ >= 0)) {
        edit_index_ = selected_server_;
        edit_name_ = set_.servers[std::size_t(selected_server_)].name;
        edit_addr_ = set_.servers[std::size_t(selected_server_)].address;
        go(Screen::ServerEdit);
    }
    if (button(ren_, ui_, x + 211, y, bw, 34, tr("Delete"), false, selected_server_ >= 0)) {
        set_.servers.erase(set_.servers.begin() + selected_server_);
        selected_server_ = -1;
        set_.save();
        refresh();
    }
    if (button(ren_, ui_, x + 422, y, bw, 34, tr("Host a match"))) back_ = Screen::Multiplayer, go(Screen::HostMatch);
    if (button(ren_, ui_, x + 633, y, bw, 34, tr("Back"))) go(Screen::Menu);
    if (!status_.empty()) text(ren_, x, y + 44, status_, {255, 160, 120});
}

void App::server_edit() {  // add / edit / direct connect
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float x = float(w) / 2 - 260;
    const bool direct = edit_index_ == -2;
    text(ren_, x, 60, direct ? tr("Direct connect") : edit_index_ < 0 ? tr("Add server") : tr("Edit server"), {255, 200, 120}, 3);
    float y = 140;
    if (!direct) {
        text_field(ren_, ui_, 31, x, y, 520, edit_name_, tr("Server name"));
        y += 60;
    }
    const bool go_now = text_field(ren_, ui_, 32, x, y, 520, edit_addr_, tr("Server address (host:port)"));
    y += 60;
    if (button(ren_, ui_, x, y, 250, 36, direct ? tr("Join server") : tr("Done")) || go_now) {
        if (direct) {
            direct_addr_ = edit_addr_;
            connect_to(edit_addr_);
            return;
        }
        if (edit_index_ < 0) set_.servers.push_back({edit_name_, edit_addr_});
        else set_.servers[std::size_t(edit_index_)] = {edit_name_, edit_addr_};
        set_.save();
        infos_.clear();
        go(Screen::Multiplayer);
    }
    if (button(ren_, ui_, x + 270, y, 250, 36, tr("Cancel"))) go(Screen::Multiplayer);
}

// [U-102][U-103][U-212] hosting: one screen, sensible defaults, "Start" and it runs
void App::host(bool dedicated) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float x = float(w) / 2 - 330;
    float y = 30;
    text(ren_, x, y, dedicated ? tr("Host a server") : tr("Host a match"), {255, 200, 120}, 3);
    y += 70;
    text_field(ren_, ui_, 41, x, y, 420, host_name_, tr("Name in the server list"));
    text_field(ren_, ui_, 42, x + 440, y, 120, port_, tr("UDP port"));
    text_field(ren_, ui_, 43, x + 580, y, 90, tick_, tr("Tick rate (Hz)"));
    y += 54;
    text_field(ren_, ui_, 44, x, y, 670, host_motd_, tr("Message of the day"));
    y += 44;
    auto toggle = [&](std::string_view label, bool& v) {
        if (button(ren_, ui_, x, y, 670, 26, std::format("{}: {}", label, v ? tr("ON") : tr("OFF")), v)) v = !v;
        y += 30;
    };
    toggle(tr("Force server-only textures"), force_tex_);
    if (!dedicated) toggle(tr("Allow duplicate nicknames"), dup_names_);
    else toggle(tr("Unique nicknames with registration + login"), registration_);
    toggle(tr("Ban phase (draft)"), bans_);
    toggle(tr("Soft level cap after 30"), soft_cap_);
    // [U-416] the rule set: this game's own, or one that adds what LoL / Dota have (vamp, healing reduction, denies)
    if (button(ren_, ui_, x, y, 640, 26, std::format("{}: {}", tr("Systems"), tr(kRuleSetNames[std::clamp(set_.ruleset, 0, 2)])), false))
        set_.ruleset = (set_.ruleset + 1) % int(RuleSet::Count), set_.save();
    y += 30;
    toggle(tr("Players' own emotes are passed on (separate from skins)"), own_emotes_);  // [U-414]
    // [U-420] the map (the lobby owner can still change it in the lobby)
    if (button(ren_, ui_, x, y, 640, 26, std::format("{}: {}", tr("Map"), maps()[std::size_t(std::clamp(set_.map, 0, int(maps().size()) - 1))].name), false))
        set_.map = (std::clamp(set_.map, 0, int(maps().size()) - 1) + 1) % int(maps().size()), set_.save();
    y += 30;
    if (dedicated) toggle(tr("Matchmaking queue: players queue for map + mode, random teams"), queue_mode_);  // [U-423]
    toggle(tr("Ranked: MMR + LP after every match"), ranked_);  // [U-276]
    if (ranked_) toggle(tr("Skill-based matchmaking (balanced teams)"), matchmaking_);
    if (dedicated) toggle(tr("Pick timer 90 s (no pick = lobby disbanded)"), pick_timer_);
    // [U-213] systems that may join
    text(ren_, x, y + 6, tr("Allowed systems:"), {200, 200, 210});
    float px = x + 170;
    for (std::uint8_t b : {net::kLinux, net::kWindows, net::kMacOS}) {
        const bool on = platforms_ & b;
        if (button(ren_, ui_, px, y, 120, 26, net::platform_name(b), on)) platforms_ ^= b;
        px += 126;
    }
    y += 40;
    if (button(ren_, ui_, x, y, 300, 40, dedicated ? tr("Start server") : tr("Start hosting"))) {
        net::ServerConfig c;
        c.version = set_.version;  // [U-95]
        c.name = host_name_;
        c.motd = host_motd_;
        c.port = std::uint16_t(std::clamp(std::atoi(port_.c_str()), 1024, 65'535));
        c.tick_hz = std::uint32_t(std::clamp(std::atoi(tick_.c_str()), 16, 256));
        c.force_server_textures = force_tex_;
        c.allow_duplicate_names = dup_names_;
        c.registration = registration_;
        c.soft_cap = soft_cap_;
        c.ruleset = std::uint8_t(std::clamp(set_.ruleset, 0, 2));  // [U-416]
        c.map = std::clamp(set_.map, 0, int(maps().size()) - 1);   // [U-420]
        c.queue_mode = dedicated && queue_mode_;                    // [U-423]
        c.allow_own_emotes = own_emotes_;                          // [U-414]
        c.bans = bans_;
        c.ranked = ranked_;
        c.matchmaking = ranked_ && matchmaking_;
        c.pick_seconds = dedicated && pick_timer_ ? 90 : 0;
        c.allowed_platforms = platforms_ ? platforms_ : net::kAllPlatforms;
        c.host_secret = dedicated ? 0 : secret_;
        std::string err;
        if (!start_server(c, &err)) status_ = err;
        else if (dedicated) go(Screen::ServerRunning);
        else connect_local(c.port);
    }
    if (button(ren_, ui_, x + 320, y, 200, 40, tr("Back"))) go(back_);
    if (!status_.empty()) text(ren_, x, y + 56, status_, {255, 160, 120}, 1.5f);
    text(ren_, x, y + 80, tr("Others join with your IP address and this port (open the UDP port in your router for the internet)."), {150, 150, 170});
    text(ren_, x, y + 98, tr("LAN: players in your local network join with your LAN address (the Invite button in the lobby copies it)."), {150, 150, 170});
}

void App::server_running() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float x = float(w) / 2 - 260;
    text(ren_, x, 60, tr("Server running"), {255, 200, 120}, 3);
    if (server_) {
        text(ren_, x, 130, std::format("{} {}   {} {}   {}", tr("port"), server_->port(), tr("players"), server_->players.load(),
                                       server_->in_match ? tr("in a match") : tr("lobby")), {230, 230, 230}, 2);
        text(ren_, x, 160, std::format("{} {} s   {} {:.0f}%", tr("match time"), server_->match_ms.load() / 1000, tr("tick load"),
                                       server_->tick_load.load() * 100), {200, 200, 200}, 2);
    }
    if (button(ren_, ui_, x, 220, 260, 40, tr("Stop server"))) {
        stop_server();
        go(Screen::Menu);
    }
}

}  // namespace em
