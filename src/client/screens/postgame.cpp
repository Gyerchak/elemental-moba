// [U-179][U-199] Loading screen with splash arts, and the post-game statistics.
#include "net/ranking.hpp"
#include <algorithm>
#include <format>

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
    const std::string head = std::format("{:<22}{:>5}{:>12}{:>6}{:>8}{:>9}{:>9}{:>9}{:>9}   {}", tr("Player"), tr("Lvl"), "K / D / A",
                                         "CS", tr("Gold"), tr("Dmg"), tr("Taken"), tr("Heal"), tr("Shield"), tr("Keystone (impact)"));
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
            const std::string line = std::format("{:<22}{:>5}{:>12}{:>6}{:>8}{:>9}{:>9}{:>9}{:>9}   {}", who.substr(0, 21), r.level,
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
