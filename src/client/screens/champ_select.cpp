// [U-10][U-11][U-71][U-177][U-178][U-180][U-181][U-182][U-200] Champion select like LoL (also in practice).
#include "net/ranking.hpp"
#include <algorithm>
#include <cctype>
#include <format>

#include "client/app.hpp"
#include "game/match/maps.hpp"
#if !defined(_WIN32)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif
#include "client/own_emotes.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

namespace {
std::string lower(std::string_view s) {
    std::string o(s);
    for (char& c : o) c = char(std::tolower(static_cast<unsigned char>(c)));
    return o;
}
}  // namespace

// [U-178] the last full setup / Q / F / G used on this champion; first options if never set
void App::apply_champion_defaults(int champ) {
    if (champ < 0) return;
    const ChampionDef& c = champions()[std::size_t(champ)];
    ChampPrefs& p = set_.prefs(c.name);
    pick_setup_ = p.setup >= 0 && p.setup < int(set_.setups.size()) ? p.setup : -1;
    pick_loadout_ = pick_setup_ >= 0 ? set_.setups[std::size_t(pick_setup_)].loadout : Loadout{};
    pick_q_ = q_allowed(champ, p.q) ? p.q : default_q(champ);
    auto first_special = [&](int other) {
        for (int i = 0; i < int(specials().size()); ++i)
            if (special_allowed(i, c) && i != other) return i;
        return -1;
    };
    pick_f_ = special_allowed(p.f, c) ? p.f : first_special(-1);
    pick_g_ = special_allowed(p.g, c) && p.g != pick_f_ ? p.g : first_special(pick_f_);
}

void App::send_pick() {
    net_.send_pick(pick_team_, pick_champ_, pick_q_, pick_f_, pick_g_, ready_, pick_loadout_);
    if (pick_champ_ >= 0) {  // remember for next time [U-178]
        ChampPrefs& p = set_.prefs(champions()[std::size_t(pick_champ_)].name);
        p.setup = pick_setup_;
        p.q = pick_q_;
        p.f = pick_f_;
        p.g = pick_g_;
    }
}

// [U-177] keystone (1) · specialisation tree (1) + a node per tier · runes (1 per slot).
// Every row wraps inside the width w, so nothing runs off the screen [U-250]. Returns the height used.
float App::draw_loadout_editor(float x, float y, Loadout& lo, bool editable, float w) {
    bool changed = false;
    const float y0 = y, left = x + 80, right = x + w;
    // place buttons left to right, next line when the row is full
    float bx = left;
    auto place = [&](float bw) {
        if (bx + bw > right && bx > left) bx = left, y += 26;
        const float at = bx;
        bx += bw + 6;
        return at;
    };
    auto new_row = [&](std::string_view label) {
        y += bx > left ? 26 : 0;
        bx = left;
        text(ren_, x, y + 4, label, {255, 220, 140});
    };
    std::string tip;
    new_row(tr("Keystone"));
    for (int k = 0; k < int(kKeystones.size()); ++k) {
        const float at = place(124);
        if (button(ren_, ui_, at, y, 124, 22, kKeystones[std::size_t(k)].name, lo.keystone == k, editable))
            lo.keystone = std::int8_t(lo.keystone == k ? -1 : k), changed = true;
        if (hover(ui_, at, y, 124, 22)) tip = std::string(kKeystones[std::size_t(k)].desc);
    }
    new_row(tr("Tree"));
    for (int t = 0; t < int(kSpecTrees.size()); ++t) {
        const float at = place(150);
        if (button(ren_, ui_, at, y, 150, 22, fit_text(std::format("{} ({})", kSpecTrees[std::size_t(t)].name, kSpecTrees[std::size_t(t)].theme), 146),
                   lo.tree == t, editable))
            lo.tree = std::int8_t(lo.tree == t ? -1 : t), lo.nodes = {-1, -1, -1}, changed = true;
    }
    if (lo.tree >= 0) {
        new_row(tr("Nodes"));
        for (int tier = 0; tier < 3; ++tier)
            for (int n = 0; n < 2; ++n) {
                const SpecNode& node = kSpecTrees[std::size_t(lo.tree)].tiers[std::size_t(tier)][std::size_t(n)];
                const float at = place(110);
                if (button(ren_, ui_, at, y, 110, 22, fit_text(std::string(node.name), 106), lo.nodes[std::size_t(tier)] == n, editable))
                    lo.nodes[std::size_t(tier)] = std::int8_t(lo.nodes[std::size_t(tier)] == n ? -1 : n), changed = true;
            }
    }
    for (int slot = 0; slot < 3; ++slot) {
        new_row(tr(kRuneSlotNames[slot]));
        for (int r = 0; r < 3; ++r) {
            const float at = place(170);
            if (button(ren_, ui_, at, y, 170, 22, fit_text(std::string(kRunes[std::size_t(slot)][std::size_t(r)].name), 166),
                       lo.runes[std::size_t(slot)] == r, editable))
                lo.runes[std::size_t(slot)] = std::int8_t(lo.runes[std::size_t(slot)] == r ? -1 : r), changed = true;
        }
    }
    y += 26;
    if (!tip.empty()) {  // the hovered keystone, under the editor
        text(ren_, x, y + 2, fit_text(tip, w), {200, 200, 220});
        y += 16;
    }
    if (changed && editable && screen_ == Screen::ChampSelect) send_pick();
    return y - y0;
}

// [U-182] a chat box for one channel (team = champion-select chat, lobby = server chat); "/w name text" = friend
void App::draw_chat_box(float x, float y, float w, float h, net::Chat channel) {
    fill_rect(ren_, x, y, w, h, {10, 10, 16, 220});
    rect(ren_, x, y, w, h, {80, 80, 100});
    float ly = y + h - 46;
    for (auto it = net_.chat.rbegin(); it != net_.chat.rend() && ly > y + 4; ++it) {
        if (it->channel != channel && it->channel != net::Chat::Whisper && it->channel != net::Chat::System) continue;
        const Rgba col = it->channel == net::Chat::Whisper ? Rgba{230, 140, 255} : it->channel == net::Chat::System ? Rgba{255, 220, 120} : Rgba{220, 230, 255};
        text(ren_, x + 6, ly, std::format("{}: {}", it->from, it->text), col);
        ly -= 13;
    }
    if (text_field(ren_, ui_, 60, x + 4, y + h - 32, w - 8, chat_text_, "") && !chat_text_.empty()) {
        if (chat_text_.starts_with("/w ")) {
            const std::string rest = chat_text_.substr(3);
            const auto sp = rest.find(' ');
            if (sp != std::string::npos) net_.send_chat(net::Chat::Whisper, rest.substr(sp + 1), rest.substr(0, sp));
        } else if (chat_text_.starts_with("/friend ")) {
            set_.friends.push_back(chat_text_.substr(8));
            set_.save();
        } else {
            net_.send_chat(channel, chat_text_);
        }
        chat_text_.clear();
    }
}

namespace {
// [U-422] the address others can use: the one I joined, or — when I host on this PC — my LAN address
std::string invite_address(const std::string& joined) {
    if (joined != "127.0.0.1" && joined != "localhost" && joined != "::1" && !joined.empty()) return joined;
#if !defined(_WIN32)
    ifaddrs* all = nullptr;
    std::string best = "127.0.0.1";
    if (getifaddrs(&all) == 0) {
        for (ifaddrs* i = all; i; i = i->ifa_next)
            if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && !(i->ifa_flags & IFF_LOOPBACK)) {
                char buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr, buf, sizeof buf);
                best = buf;
                break;
            }
        freeifaddrs(all);
    }
    return best;
#else
    return joined;
#endif
}
}  // namespace

void App::champ_select() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    if (net_.state == NetClient::State::Connecting) {
        text(ren_, 40, 40, tr("Connecting…"), {230, 230, 230}, 3);
        if (button(ren_, ui_, 40, 100, 200, 36, tr("Cancel"))) net_.disconnect(), stop_server(), go(Screen::Menu);
        return;
    }
    const bool banning = net_.bans_enabled && net_.phase == net::DraftPhase::Banning;
    const auto roster = champions();
    // ---- header
    const std::string title = banning ? tr("BAN PHASE — choose a champion to ban") : tr("CHOOSE YOUR CHAMPION");
    text(ren_, W / 2 - text_w(title, 2.5f) / 2, 12, title, banning ? Rgba{255, 120, 120} : Rgba{255, 210, 130}, 2.5f);
    text(ren_, W / 2 - text_w(net_.server_name) / 2, 40, std::format("{} — {}", net_.server_name, net_.server_motd), {160, 160, 180});

    // [U-276] the pick timer of the server
    if (net_.lobby_timer_s > 0) {
        const int left = std::max(0, net_.lobby_timer_s - int(now_s_ - net_.lobby_timer_at));
        const std::string t = std::format("{} {} s", tr("Pick time left:"), left);
        text(ren_, W / 2 - text_w(t, 2) / 2, 60, t, left <= 10 ? Rgba{255, 110, 90} : Rgba{255, 230, 150}, 2);
    }
    // [U-422] am I the lobby owner? (the "host" flag of my lobby slot; it can be handed over)
    bool i_own = false;
    for (const auto& s : net_.lobby)
        if (s.id == net_.my_id && s.host) i_own = true;
    // ---- teams (mine left, the other right), bans under them
    auto draw_team = [&](int team, float x) {
        text(ren_, x, 60, team == 0 ? tr("FIRE NATION") : tr("EARTH NATION"), team == 0 ? hex(0xd8402a) : hex(0x4f9a35), 2);
        float y = 86;
        for (const auto& s : net_.lobby) {
            if (s.team != team) continue;
            fill_rect(ren_, x, y, 250, 54, s.id == net_.my_id ? Rgba{50, 45, 30, 255} : Rgba{24, 24, 34, 255});
            if (s.champ >= 0) {
                if (SDL_Texture* t = res_.champion_icon(s.champ, "")) {
                    const SDL_FRect d{x + 3, y + 3, 48, 48};
                    SDL_RenderTexture(ren_, t, nullptr, &d);
                } else {
                    view_->draw_icon(s.champ, x + 3, y + 3, 48, false);
                }
            }
            text(ren_, x + 58, y + 8, std::format("{}{}", s.host ? "* " : "", s.name), {235, 235, 240});
            text(ren_, x + 58, y + 22, s.champ >= 0 ? std::string(roster[std::size_t(s.champ)].name) : tr("picking…"), {180, 180, 200});
            if (s.ready) text(ren_, x + 58, y + 36, tr("ready"), {120, 230, 120});
            if (s.ban >= 0) text(ren_, x + 150, y + 36, std::format("{}: {}", tr("ban"), roster[std::size_t(s.ban)].name), {255, 120, 120});
            if (s.lp >= 0) text(ren_, x + 140, y + 8, fit_text(net::tier_name(s.lp), 108), {230, 200, 120});  // [U-276]
            if (i_own && s.id != net_.my_id && !s.bot && button(ren_, ui_, x + 186, y + 32, 60, 18, tr("owner")))  // [U-422] hand over
                net_.send_transfer_owner(s.id);
            if (net_.queue_mode && !s.plays) text(ren_, x + 186, y + 22, s.queued ? tr("queued") : tr("waiting"), {170, 170, 200});
            y += 58;
        }
    };
    draw_team(pick_team_, 16);
    draw_team(1 - pick_team_, W - 266);
    if (button(ren_, ui_, 16, H - 46, 250, 32, tr("Switch team"))) pick_team_ = 1 - pick_team_, send_pick();

    // ---- filters [U-180]
    const float gx = 290, gw = W - 580;
    float y = 64;
    text_field(ren_, ui_, 50, gx, y + 12, 220, search_, tr("Search"));
    // filter buttons wrap inside the middle column (never under the team lists) [U-250]
    float fx = gx + 230, fy = y + 12;
    auto spot = [&](float bw) {
        if (fx + bw > gx + gw && fx > gx) fx = gx, fy += 30;
        const float at = fx;
        fx += bw + 4;
        return at;
    };
    if (button(ren_, ui_, spot(60), fy, 60, 26, tr("All"), filter_role_ < 0)) filter_role_ = -1;
    for (int r = 0; r <= int(Role::Support); ++r)
        if (button(ren_, ui_, spot(86), fy, 86, 26, tr(role_name(Role(r))), filter_role_ == r)) filter_role_ = filter_role_ == r ? -1 : r;
    static const char* kEl[] = {"Water", "Earth", "Fire", "Air", "Non-bender"};
    for (int e = 0; e < 5; ++e)
        if (button(ren_, ui_, spot(100), fy, 100, 24, tr(kEl[e]), filter_element_ == e)) filter_element_ = filter_element_ == e ? -1 : e;
    if (button(ren_, ui_, spot(110), fy, 110, 24, tr("Non-mages"), filter_non_mage_)) filter_non_mage_ = !filter_non_mage_;
    if (button(ren_, ui_, spot(110), fy, 110, 24, tr("Favourites"), filter_fav_)) filter_fav_ = !filter_fav_;
    // [U-07][U-279] Avatars / Dark Avatars (they still count for their native element in the element filters)
    if (button(ren_, ui_, spot(100), fy, 100, 24, tr("Avatars"), filter_avatar_ == 1)) filter_avatar_ = filter_avatar_ == 1 ? 0 : 1;
    if (button(ren_, ui_, spot(120), fy, 120, 24, tr("Dark Avatars"), filter_avatar_ == 2)) filter_avatar_ = filter_avatar_ == 2 ? 0 : 2;
    // [U-305] built-in / own champions, apart from the other filters (a gap before them)
    fx += 12;
    if (button(ren_, ui_, spot(90), fy, 90, 24, tr("Built-in"), filter_origin_ == 1)) filter_origin_ = filter_origin_ == 1 ? 0 : 1;
    if (button(ren_, ui_, spot(140), fy, 140, 24, std::format("{} ({})", tr("Own champions"), int(roster.size()) - builtin_champion_count()),
               filter_origin_ == 2)) filter_origin_ = filter_origin_ == 2 ? 0 : 2;
    y = fy + 34;

    // ---- the grid, alphabetical
    std::vector<int> list;
    for (int i = 0; i < int(roster.size()); ++i) {
        const ChampionDef& c = roster[std::size_t(i)];
        if (!search_.empty() && lower(c.name).find(lower(search_)) == std::string::npos) continue;
        if (filter_role_ >= 0 && int(c.role) != filter_role_) continue;
        if (filter_element_ >= 0 && int(c.element) != filter_element_) continue;
        if (filter_non_mage_ && c.mage) continue;
        if (filter_fav_ && !set_.prefs(c.name).favourite) continue;
        if (filter_avatar_ == 1 && !(c.avatar && !c.dark)) continue;
        if (filter_origin_ == 1 && i >= builtin_champion_count()) continue;
        if (filter_origin_ == 2 && i < builtin_champion_count()) continue;
        if (filter_avatar_ == 2 && !(c.avatar && c.dark)) continue;
        list.push_back(i);
    }
    std::ranges::sort(list, {}, [&](int i) { return roster[std::size_t(i)].name; });
    const float cell = 76;
    const int cols = std::max(1, int(gw / cell));
    for (std::size_t k = 0; k < list.size(); ++k) {
        const int i = list[k];
        const ChampionDef& c = roster[std::size_t(i)];
        const float cx = gx + float(int(k) % cols) * cell, cy = y + float(int(k) / cols) * (cell + 14);
        bool taken = false, banned = false;
        for (const auto& s : net_.lobby) {
            if (s.id != net_.my_id && s.champ == i) taken = true;
            if (s.ban == i) banned = true;
        }
        if (SDL_Texture* t = res_.champion_icon(i, set_.prefs(c.name).skin)) {
            const SDL_FRect d{cx, cy, 68, 68};
            SDL_RenderTexture(ren_, t, nullptr, &d);
            if (taken || banned) fill_rect(ren_, cx, cy, 68, 68, {0, 0, 0, 170});
        } else {
            view_->draw_icon(i, cx, cy, 68, taken || banned);
        }
        if (pick_champ_ == i || ban_pick_ == i) rect(ren_, cx - 2, cy - 2, 72, 72, banning ? Rgba{255, 90, 90} : Rgba{255, 220, 120});
        if (set_.prefs(c.name).favourite) text(ren_, cx + 56, cy + 2, "*", {255, 230, 90}, 2);
        if (banned) text(ren_, cx + 6, cy + 30, tr("BANNED"), {255, 90, 90});
        text(ren_, cx, cy + 70, fit_text(std::string(c.name), cell - 4), {200, 200, 210});
        if (hover(ui_, cx, cy, 68, 68)) {
            if (ui_.rclicked) {  // [U-180] right click = favourite
                set_.prefs(c.name).favourite = !set_.prefs(c.name).favourite;
                set_.save();
                ui_.rclicked = false;
            }
            if (ui_.clicked && !taken && !banned) {
                ui_.clicked = false;
                if (banning) {
                    ban_pick_ = i;
                } else {
                    pick_champ_ = i;
                    apply_champion_defaults(i);
                    send_pick();
                }
            }
        }
    }
    if (banning && button(ren_, ui_, gx, H - 300, 220, 34, tr("Ban this champion"), false, ban_pick_ >= 0)) net_.send_ban(ban_pick_);

    // ---- my picks: Q option [U-11], specials F/G [U-71], skin [U-197], loadout [U-177][U-178]
    // The panel sits under the grid in the middle column. Rows wrap; if it is still taller than the space,
    // it scrolls with the mouse wheel and is clipped to its area → nothing ever leaves the screen [U-250].
    const float grid_rows = float((int(list.size()) + cols - 1) / cols);
    const float px0 = gx, pw = gw, ptop = std::min(y + grid_rows * (cell + 14) + 8, H - 260), pbot = H - 52;
    if (pick_champ_ >= 0 && !banning) {
        const bool in_panel = hover(ui_, px0, ptop, pw, pbot - ptop);
        if (in_panel) pick_scroll_ = std::max(0.f, pick_scroll_ - ui_.wheel * 40);
        // clicks outside the panel must not reach buttons scrolled out of view
        const bool saved_click = ui_.clicked;
        if (!in_panel) ui_.clicked = false;
        const SDL_Rect clip{int(px0), int(ptop), int(pw), int(pbot - ptop)};
        SDL_SetRenderClipRect(ren_, &clip);
        float py = ptop - pick_scroll_;
        const float right = px0 + pw;
        auto wrap = [&](float& bx, float x0, float bw) {
            if (bx + bw > right && bx > x0) bx = x0, py += 26;
            const float at = bx;
            bx += bw + 4;
            return at;
        };
        const ChampionDef& c = roster[std::size_t(pick_champ_)];
        text(ren_, px0, py, fit_text(std::format("{} — {} · {}", c.name, tr(role_name(c.role)), tr(kEl[int(c.element)])), pw, 2), hex(c.colour), 2);
        py += 26;
        std::string tip;
        text(ren_, px0, py + 4, "Q:", {255, 220, 140});
        float qx = px0 + 40;
        for (int q = 0; q < int(q_options().size()); ++q)
            if (q_allowed(pick_champ_, q)) {
                const float at = wrap(qx, px0 + 40, 150);
                if (button(ren_, ui_, at, py, 150, 22, fit_text(std::string(q_options()[std::size_t(q)].name), 146), pick_q_ == q)) pick_q_ = q, send_pick();
                if (hover(ui_, at, py, 150, 22)) tip = std::string(q_options()[std::size_t(q)].desc);
            }
        py += 26;
        text(ren_, px0, py + 4, "G/H:", {255, 220, 140});  // [U-289] the specials' keys
        float sx = px0 + 40;
        for (int i = 0; i < int(specials().size()); ++i) {
            if (!special_allowed(i, c)) continue;
            const bool f = pick_f_ == i, g = pick_g_ == i;
            const float at = wrap(sx, px0 + 40, 168);
            if (button(ren_, ui_, at, py, 168, 22, fit_text(std::format("{}{}", f ? "G: " : g ? "H: " : "", specials()[std::size_t(i)].name), 164), f || g)) {
                if (f) pick_f_ = -1;
                else if (g) pick_g_ = -1;
                else if (pick_f_ < 0) pick_f_ = i;
                else pick_g_ = i;
                send_pick();
            }
            if (hover(ui_, at, py, 168, 22)) tip = std::string(specials()[std::size_t(i)].desc);
        }
        py += 26;
        if (!tip.empty()) {  // the hovered skill, in its own line (never on top of buttons)
            text(ren_, px0, py + 2, fit_text(tip, pw), {190, 190, 210});
            py += 18;
        }
        // skins of this champion in my resource pack [U-197][U-200]
        text(ren_, px0, py + 4, tr("Skin:"), {255, 220, 140});
        ChampPrefs& pr = set_.prefs(c.name);
        float kx = px0 + 50;
        if (button(ren_, ui_, wrap(kx, px0 + 50, 90), py, 90, 22, tr("Base"), pr.skin.empty())) pr.skin.clear(), set_.save();
        for (const std::string& sk : res_.skins_of(pick_champ_)) {
            const float at = wrap(kx, px0 + 50, 120);
            if (button(ren_, ui_, at, py, 120, 22, fit_text(sk, 116), pr.skin == sk)) pr.skin = sk, set_.save();
        }
        py += 30;
        // loadout: a saved setup in one click; the full editor only when opened (folded by default)
        text(ren_, px0, py + 4, tr("Setup:"), {255, 220, 140});
        float ux = px0 + 60;
        if (button(ren_, ui_, wrap(ux, px0 + 60, 70), py, 70, 22, tr("None"), pick_setup_ < 0)) pick_setup_ = -1, pick_loadout_ = {}, send_pick();
        for (int s = 0; s < int(set_.setups.size()); ++s) {
            const float at = wrap(ux, px0 + 60, 150);
            if (button(ren_, ui_, at, py, 150, 22, fit_text(set_.setups[std::size_t(s)].name, 146), pick_setup_ == s))
                pick_setup_ = s, pick_loadout_ = set_.setups[std::size_t(s)].loadout, send_pick();
        }
        py += 30;
        const Loadout& lo = pick_loadout_;
        const std::string summary = std::format("{}: {}  ·  {}: {}", tr("Keystone"),
                                                lo.keystone >= 0 ? std::string(kKeystones[std::size_t(lo.keystone)].name) : tr("none"), tr("Tree"),
                                                lo.tree >= 0 ? std::string(kSpecTrees[std::size_t(lo.tree)].name) : tr("none"));
        if (button(ren_, ui_, px0, py, 150, 22, show_loadout_ ? tr("Loadout ▾") : tr("Loadout ▸"), show_loadout_)) show_loadout_ = !show_loadout_;
        text(ren_, px0 + 160, py + 6, fit_text(summary, pw - 160), {200, 200, 215});
        py += 30;
        if (show_loadout_) py += draw_loadout_editor(px0, py, pick_loadout_, true, pw);
        // how far it can scroll
        const float content = py + pick_scroll_ - ptop;
        pick_scroll_ = std::clamp(pick_scroll_, 0.f, std::max(0.f, content - (pbot - ptop)));
        SDL_SetRenderClipRect(ren_, nullptr);
        if (content > pbot - ptop) {  // scroll bar
            const float vis = (pbot - ptop) / content;
            fill_rect(ren_, right - 5, ptop, 4, pbot - ptop, {40, 40, 55, 255});
            fill_rect(ren_, right - 5, ptop + (pbot - ptop) * (pick_scroll_ / content), 4, (pbot - ptop) * vis, {200, 180, 120, 255});
        }
        ui_.clicked = saved_click && !in_panel ? saved_click : ui_.clicked;
    }

    // ---- chat (team = champion-select chat; lobby = server chat) [U-182]
    const float chat_y = 86 + 58.f * 6;
    if (button(ren_, ui_, 16, chat_y - 26, 120, 22, tr("Team chat"), chat_channel_ == net::Chat::Team)) chat_channel_ = net::Chat::Team;
    if (button(ren_, ui_, 140, chat_y - 26, 126, 22, tr("Lobby chat"), chat_channel_ == net::Chat::Lobby)) chat_channel_ = net::Chat::Lobby;
    draw_chat_box(16, chat_y, 250, H - chat_y - 56, chat_channel_);

    // ---- ready / start / leave
    const float bx = W - 266;
    // [U-420][U-422] the map + rules: the owner changes them (a click = the next one); others see them
    // [U-423] queue mode: each player chooses map + mode for the queue and joins / leaves it
    {
        const MapDef& md = cur_map();
        if (net_.queue_mode) {
            bool queued = false;
            for (const auto& s : net_.lobby)
                if (s.id == net_.my_id) queued = s.queued;
            if (button(ren_, ui_, bx, H - 278, 250, 26, std::format("{}: {}", tr("Queue map"), maps()[std::size_t(queue_map_)].name), false, !queued))
                queue_map_ = (queue_map_ + 1) % int(maps().size());
            if (button(ren_, ui_, bx, H - 248, 250, 26, std::format("{}: {}", tr("Mode"), tr(kRuleSetNames[queue_rules_])), false, !queued))
                queue_rules_ = (queue_rules_ + 1) % int(RuleSet::Count);
            int same = 0;
            for (const auto& s : net_.lobby) same += s.queued && s.q_map == queue_map_ && s.q_rules == queue_rules_;
            if (button(ren_, ui_, bx, H - 218, 250, 30, queued ? std::format("{} ({} {})", tr("Leave the queue"), same, tr("in it"))
                                                              : std::format("{} ({} {})", tr("Find a match"), same, tr("waiting")), queued))
                net_.send_queue(!queued, queue_map_, queue_rules_);
        } else {
            if (button(ren_, ui_, bx, H - 248, 250, 26, std::format("{}: {}", tr("Map"), md.name), false, i_own))
                net_.send_lobby_config((map_index(md.id) + 1) % int(maps().size()), net_.ruleset);
            if (button(ren_, ui_, bx, H - 218, 250, 26, std::format("{}: {}", tr("Rules"), tr(kRuleSetNames[std::min<int>(net_.ruleset, 2)])), false, i_own))
                net_.send_lobby_config(map_index(md.id), (net_.ruleset + 1) % int(RuleSet::Count));
        }
        text(ren_, bx, H - 300, fit_text(std::string(md.desc), 250), {150, 160, 180});
        // [U-422] invite: a join link to the clipboard (paste it into Multiplayer → Direct connect)
        if (button(ren_, ui_, bx, H - 186, 120, 30, tr("Invite"))) {
            const std::string link = std::format("em-join://{}:{}", invite_address(net_.address), net_.port);
            SDL_SetClipboardText(link.c_str());
            status_ = std::format("{} {}", tr("copied:"), link);
        }
        if (!status_.empty()) text(ren_, 290, H - 20, fit_text(status_, W - 580), {200, 220, 160});
    }
    if (button(ren_, ui_, bx, H - 140, 250, 40, ready_ ? tr("Ready!") : tr("Lock in / ready"), ready_, pick_champ_ >= 0)) {
        ready_ = !ready_;
        if (ready_ && pick_champ_ >= 0) {
            const auto skin = res_.skin_upload(pick_champ_, set_.prefs(roster[std::size_t(pick_champ_)].name).skin);
            if (!skin.empty()) net_.send_skin(skin);  // [U-200] Minecraft-like: my skin goes to the server
            const auto mine = load_own_emotes();       // [U-414] my own emotes (separate from the skin)
            for (std::size_t i = 0; i < mine.size(); ++i) net_.send_own_emote(std::uint8_t(i), mine[i].png, mine[i].wav);
        }
        send_pick();
        set_.save();
    }
    if ((i_own || net_.practice) && !net_.queue_mode &&
        button(ren_, ui_, bx, H - 94, 250, 40, banning ? tr("End the ban phase") : tr("Start (bots fill empty slots)"))) {
        if (pick_champ_ < 0 && !banning) pick_champ_ = 0, apply_champion_defaults(0);
        send_pick();
        set_.save();
        net_.send_start();
    }
    if (button(ren_, ui_, bx, H - 48, 250, 36, tr("Leave the lobby"))) {  // [U-422]
        net_.disconnect();
        stop_server();
        go(Screen::Menu);
    }
    // autotest: pick and start at once
    if (opt_.autotest_s > 0 && net_.state == NetClient::State::Lobby) {
        if (!autotest_picked_) {
            pick_champ_ = opt_.champ;
            pick_team_ = opt_.champ % 2;
            apply_champion_defaults(pick_champ_);
            ready_ = true;
            send_pick();
            autotest_picked_ = true;
        }
        if (opt_.shot_select) {  // developer check of this screen's layout: picture after 1.5 s, quit
            show_loadout_ = true;
            if (autotest_select_t_ < 0) autotest_select_t_ = now_s_;
            if (now_s_ - autotest_select_t_ > 1.5 && !opt_.shot.empty())
                if (SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr)) {
                    SDL_SaveBMP(s, opt_.shot.c_str());
                    SDL_DestroySurface(s);
                    quit_ = true;
                }
            return;
        }
        if (!autotest_started_ && int(net_.lobby.size()) >= opt_.wait_players && opt_.starter) {
            net_.send_start();
            autotest_started_ = true;
        }
    }
}

}  // namespace em
