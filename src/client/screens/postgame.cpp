// [U-179][U-199] Loading screen with splash arts, and the post-game statistics.
#include "net/ranking.hpp"
#include <algorithm>
#include <format>
#include <map>

#include "client/app.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

// [U-199] every champion's splash art (skin from the network if allowed, else my pack, else a placeholder)
void App::loading(double now_s) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    fill_rect(ren_, 0, 0, W, H, {8, 8, 14, 255});
    // [U-257] a loading picture behind the splashes (one of loading_1…5, chosen by the match start time)
    if (SDL_Texture* bg = res_.texture(std::format("screens/loading_{}.png", 1 + int(std::uint64_t(net_.start_ms / 1000) % 5)))) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(bg, &tw, &th);
        const float sc = std::max(W / tw, H / th);
        const SDL_FRect d{(W - tw * sc) / 2, (H - th * sc) / 2, tw * sc, th * sc};
        SDL_SetTextureColorMod(bg, 90, 90, 100);
        SDL_RenderTexture(ren_, bg, nullptr, &d);
        SDL_SetTextureColorMod(bg, 255, 255, 255);
    }
    std::vector<const RosterEntry*> teams[2];
    for (const auto& [id, e] : net_.roster) teams[std::min<int>(e.team, 1)].push_back(&e);
    for (auto& t : teams) std::ranges::sort(t, {}, [](const RosterEntry* e) { return e->id; });
    for (int team = 0; team < 2; ++team) {
        const float cw = std::min(220.f, (W - 40) / std::max<float>(1, float(teams[team].size())));
        for (std::size_t k = 0; k < teams[team].size(); ++k) {
            const RosterEntry& e = *teams[team][k];
            const float x = 20 + float(k) * cw, y = team == 0 ? H / 2 + 10 : 40, ch = H / 2 - 60;
            SDL_Texture* splash = nullptr;
            if (auto it = net_.skins.find(e.id); it != net_.skins.end() && (set_.show_other_skins || e.id == net_.my_unit))
                splash = res_.texture_from_bmp(e.id, it->second);  // [U-200]
            if (!splash) splash = res_.champion_splash(e.champ, e.id == net_.my_unit ? set_.prefs(champions()[std::size_t(e.champ)].name).skin : "");
            fill_rect(ren_, x, y, cw - 10, ch, {24, 24, 34, 255});
            if (splash) {
                const SDL_FRect d{x, y, cw - 10, ch};
                SDL_RenderTexture(ren_, splash, nullptr, &d);
            } else {
                view_->draw_icon(e.champ, x + (cw - 10) / 2 - 60, y + ch / 2 - 80, 120, false);
            }
            rect(ren_, x, y, cw - 10, ch, e.team == net_.my_team ? Rgba{90, 200, 110} : Rgba{220, 80, 70});
            text(ren_, x + 6, y + ch - 30, e.name, {255, 255, 255});
            text(ren_, x + 6, y + ch - 16, std::string(champions()[std::size_t(e.champ)].name), {200, 200, 220});
        }
    }
    const std::string t = std::format("{}{}", tr("Loading"), std::string(std::size_t(int(now_s * 3) % 4), '.'));
    text(ren_, W / 2 - text_w(t, 2) / 2, H / 2 - 16, t, {255, 220, 140}, 2);
}

// [U-179] statistics of the match + what every keystone did
void App::postgame() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    const bool won = net_.winner == net_.my_team;
    const std::string title = won ? tr("VICTORY") : tr("DEFEAT");
    text(ren_, W / 2 - text_w(title, 4) / 2, 20, title, won ? Rgba{255, 220, 100} : Rgba{255, 90, 90}, 4);
    const float x = 30;
    float y = 80;
    const bool record = (SDL_GetModState() & SDL_KMOD_ALT) != 0;  // [U-520] Alt: the damage record (before defence, stopped by defence)
    const std::string head = record ? std::format("{:<22}{:>10}{:>10}{:>10}{:>10}{:>10}{:>10}{:>9}{:>9}{:>9}{:>9}", tr("Player"), tr("Dealt"), tr("Before def."), tr("Stopped"), tr("Taken"), tr("Before def."), tr("Stopped"), tr("Best hit"), tr("Best auto"), tr("Minions"), tr("Monsters"))
                                    : std::format("{:<22}{:>5}{:>12}{:>6}{:>8}{:>9}{:>9}{:>9}{:>9}   {}", tr("Player"), tr("Lvl"), "K / D / A",
                                         "CS", tr("Gold"), tr("Dmg"), tr("Taken"), tr("Heal"), tr("Shield"), tr("Keystone (impact)"));
    text(ren_, x + 420, 60, tr("Hold Alt: the damage record (before defence, stopped by defence)"), {170, 170, 190});
    text(ren_, x, y, head, {255, 255, 255});
    y += 18;
    for (int team = 0; team < 2; ++team) {
        fill_rect(ren_, x - 6, y - 2, W - 48, 2, team == 0 ? hex(0xd8402a) : hex(0x4f9a35));
        y += 6;
        for (const net::FinalRow& r : net_.final_stats) {
            if (r.team != team) continue;
            const auto e = net_.roster.find(r.unit);
            const std::string who = e != net_.roster.end() ? e->second.name : "?";
            std::string ks = "-";
            if (r.keystone >= 0 && r.keystone < std::int8_t(keystones().size())) {
                const KeystoneDef& k = keystones()[std::size_t(r.keystone)];
                ks = std::format("{} ({} {})", k.name, r.keystone_impact, k.impact);
            }
            const std::string line = record ? std::format("{:<22}{:>10}{:>10}{:>10}{:>10}{:>10}{:>10}{:>9}{:>9}{:>9}{:>9}", who.substr(0, 21), r.dmg_champs, r.dealt_pre, std::max(0, r.dealt_pre - r.dmg_champs),
                                                          r.dmg_taken, r.taken_pre, std::max(0, r.taken_pre - r.dmg_taken), r.highest_hit, r.highest_auto, r.dmg_minions, r.dmg_monsters)
                                            : std::format("{:<22}{:>5}{:>12}{:>6}{:>8}{:>9}{:>9}{:>9}{:>9}   {}", who.substr(0, 21), r.level,
                                                 std::format("{}/{}/{}", r.kills, r.deaths, r.assists), r.cs, r.gold, r.dmg_champs,
                                                 r.dmg_taken, r.healing, r.shielding, ks);
            text(ren_, x, y, line, r.unit == net_.my_unit ? Rgba{255, 230, 120} : Rgba{220, 220, 230});
            // items (icons) [U-257]
            float ix = W - 360;
            for (std::int16_t it : r.items) {
                if (it >= 0) draw_item_icon(ren_, it, ix, y - 2, 16);
                ix += 20;
            }
            if (r.lp_after >= 0)  // [U-276] LP won / lost
                text(ren_, ix + 6, y, std::format("{:+} LP  {}", r.lp_delta, net::tier_name(r.lp_after)), r.lp_delta >= 0 ? Rgba{120, 230, 120} : Rgba{255, 120, 110});
            y += 18;
        }
        y += 12;
    }
    // ---------------------------------------------------------------- [U-371] graphs (one axis; team hue + my line brighter;
    // a legend with the names, crosshair + values under the mouse) and the chat
    const float gx = x + 44, gw = std::min(W - 420.f, 1100.f), gtop = y + 30, gh = std::max(90.f, H - 126 - gtop - 18);
    static constexpr const char* kKinds[] = {"Gold", "XP", "Gold lead", "Damage dealt", "Damage taken", "Healing", "CS"};
    for (int k = 0; k < 7; ++k)
        if (button(ren_, ui_, x + float(k) * 104, y, 100, 22, tr(kKinds[k]), graph_kind_ == k)) graph_kind_ = k;
    if (!net_.timeline.empty() && gh > 60) {
        auto value = [&](const net::NetTimeline& q) -> std::int64_t {
            switch (graph_kind_) {
                case 1: return q.xp;
                case 3: return q.dealt;
                case 4: return q.taken;
                case 5: return q.healing;
                case 6: return q.cs;
                default: return q.gold;
            }
        };
        std::int64_t maxv = 1, minv = 0, end_ms = 1;
        std::map<std::uint32_t, std::vector<const net::NetTimeline*>> lines;
        for (const net::NetTimeline& q : net_.timeline) {
            lines[q.unit].push_back(&q);
            end_ms = std::max<std::int64_t>(end_ms, q.ms);
        }
        std::map<std::int32_t, std::int64_t> lead;  // Gold lead: Fire minus Earth, one line
        if (graph_kind_ == 2) {
            for (const net::NetTimeline& q : net_.timeline) {
                const auto f = std::ranges::find_if(net_.final_stats, [&](const net::FinalRow& r) { return r.unit == q.unit; });
                if (f != net_.final_stats.end()) lead[q.ms] += f->team == 0 ? q.gold : -std::int64_t(q.gold);
            }
            for (const auto& [ms, v] : lead) maxv = std::max(maxv, std::abs(v));
            minv = -maxv;
        } else {
            for (const net::NetTimeline& q : net_.timeline) maxv = std::max(maxv, value(q));
        }
        auto px = [&](std::int64_t ms) { return gx + gw * float(ms) / float(end_ms); };
        auto py = [&](std::int64_t v) { return gtop + gh - gh * float(v - minv) / float(std::max<std::int64_t>(1, maxv - minv)); };
        for (int i = 0; i <= 4; ++i) {  // recessive grid
            const std::int64_t v = minv + (maxv - minv) * i / 4;
            line(ren_, gx, py(v), gx + gw, py(v), {70, 70, 85, 120});
            text(ren_, x - 4, py(v) - 4, std::format("{}", v), {150, 150, 165});
        }
        for (std::int64_t m = 0; m <= end_ms; m += std::max<std::int64_t>(60'000, end_ms / 8 / 60'000 * 60'000))
            text(ren_, px(m) - 8, gtop + gh + 4, std::format("{}'", m / 60'000), {150, 150, 165});
        const Rgba kFire = hex(0xD8622A), kEarth = hex(0x3F8FE0);  // (checked for colour blindness and on the dark surface)
        const float lx = gx + gw + 16;
        std::uint32_t hot = 0;
        if (graph_kind_ != 2) {  // the legend: every name with its line's colour; pointing at one lifts its line
            float ly = gtop;
            for (const net::FinalRow& r : net_.final_stats) {
                const auto e = net_.roster.find(r.unit);
                fill_rect(ren_, lx, ly + 3, 14, 3, r.team == 0 ? kFire : kEarth);
                text(ren_, lx + 20, ly, fit_text(e != net_.roster.end() ? e->second.name : "?", 170), r.unit == net_.my_unit ? Rgba{255, 255, 255} : Rgba{190, 190, 200});
                if (hover(ui_, lx, ly - 2, 190, 15)) hot = r.unit;
                ly += 15;
            }
            for (const auto& [unit, pts] : lines) {
                const auto f = std::ranges::find_if(net_.final_stats, [&, u = unit](const net::FinalRow& r) { return r.unit == u; });
                const bool mine = unit == net_.my_unit, lifted = unit == hot;
                Rgba c = f != net_.final_stats.end() && f->team == 0 ? kFire : kEarth;
                c = with_alpha(c, mine || lifted ? 255 : hot ? 60 : 150);
                for (std::size_t i = 1; i < pts.size(); ++i) {
                    const float x0 = px(pts[i - 1]->ms), y0 = py(value(*pts[i - 1])), x1 = px(pts[i]->ms), y1 = py(value(*pts[i]));
                    line(ren_, x0, y0, x1, y1, c), line(ren_, x0, y0 + 1, x1, y1 + 1, c);
                    if (mine || lifted) line(ren_, x0, y0 - 1, x1, y1 - 1, mixc(c, {255, 255, 255, 255}, 40));
                }
            }
        } else {
            std::int64_t pm = -1, pv = 0;
            line(ren_, gx, py(0), gx + gw, py(0), {170, 170, 180, 200});
            for (const auto& [ms, v] : lead) {
                if (pm >= 0) {
                    const Rgba c = v >= 0 ? kFire : kEarth;
                    line(ren_, px(pm), py(pv), px(ms), py(v), c), line(ren_, px(pm), py(pv) + 1, px(ms), py(v) + 1, c);
                }
                pm = ms, pv = v;
            }
            text(ren_, lx, gtop, tr("Above the middle: Fire leads"), {190, 190, 200});
            text(ren_, lx, gtop + 15, tr("Below the middle: Earth leads"), {190, 190, 200});
        }
        // ---- under the mouse: a crosshair and the values
        if (hover(ui_, gx, gtop, gw, gh)) {
            const std::int64_t ms = std::int64_t((ui_.mx - gx) / gw * float(end_ms));
            const std::int64_t snap = (ms + 7'500) / 15'000 * 15'000;
            line(ren_, px(snap), gtop, px(snap), gtop + gh, {230, 230, 235, 160});
            std::vector<std::pair<std::int64_t, std::string>> rows;
            if (graph_kind_ == 2) {
                if (auto it = lead.find(std::int32_t(snap)); it != lead.end()) rows.push_back({0, std::format("{}", it->second)});
            } else {
                for (const auto& [unit, pts] : lines)
                    for (const net::NetTimeline* q : pts)
                        if (q->ms == snap) {
                            const auto e = net_.roster.find(unit);
                            rows.push_back({value(*q), std::format("{:>7}  {}", value(*q), e != net_.roster.end() ? e->second.name : "?")});
                        }
                std::ranges::sort(rows, std::greater<>{}, [](const auto& a) { return a.first; });
            }
            const float tw = 230, th = 18 + float(rows.size()) * 14;
            const float tx = std::min(px(snap) + 10, gx + gw - tw);
            fill_rect(ren_, tx, gtop + 4, tw, th, {18, 18, 28, 240});
            text(ren_, tx + 6, gtop + 8, std::format("{}:{:02}", snap / 60'000, snap / 1000 % 60), {255, 255, 255});
            float ty = gtop + 22;
            for (const auto& [v, s] : rows) {
                text(ren_, tx + 6, ty, fit_text(s, tw - 12), {220, 220, 230});
                ty += 14;
            }
        }
    }
    // ---- the chat stays open after the match
    {
        const float cx = W - 620, cy = H - 124;
        float ly = cy - 4;
        std::size_t shown = 0;
        for (auto it = net_.chat.rbegin(); it != net_.chat.rend() && shown < 2; ++it, ++shown, ly -= 14)
            text(ren_, 30, H - 84 - float(shown) * 14, fit_text(it->text, W / 2 - 200), {200, 200, 210});
        (void)cx;
        if (text_field(ren_, ui_, 61, 30, H - 58, W / 2 - 200, post_chat_, tr("Chat (Enter sends)")) && !post_chat_.empty()) {
            net_.send_chat(net::Chat::All, post_chat_);
            post_chat_.clear();
        }
    }
    const bool lobby_ready = net_.back_in_lobby || net_.state != NetClient::State::Match;
    if (button(ren_, ui_, W / 2 - 160, H - 70, 320, 44, lobby_ready ? tr("Continue") : tr("Continue (waiting for the lobby…)"), false, lobby_ready)) {
        net_.state = NetClient::State::Lobby;
        net_.winner = -1;
        net_.final_stats.clear();
        ready_ = false;
        go(Screen::ChampSelect);
    }
    if (button(ren_, ui_, W - 230, H - 70, 200, 44, tr("Leave"))) {
        net_.disconnect();
        stop_server();
        go(Screen::Menu);
    }
}

}  // namespace em
