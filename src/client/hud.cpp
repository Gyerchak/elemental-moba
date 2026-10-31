// HUD pieces [U-155..U-170]: LoL-like health bars, champion icons, the bottom panel (icon + name + stats |
// skills with bars under them | items | icon buttons), skill tooltips (numbers / Alt = formulas / + = next
// rank), buffs with timers, ally + enemy portraits, unit tooltips, gold / XP pop-ups.
#include <algorithm>
#include <cmath>
#include <format>

#include "client/game_view.hpp"
#include "client/item_browser.hpp"
#include "client/locale.hpp"
#include "client/modifiers.hpp"
#include "game/match/layout.hpp"

namespace em {

using namespace gfx;

namespace {
const char* kKeys[] = {"P", "Q", "W", "E", "R", "T", "Y", "G", "H"};  // [U-289] specials (F / G in the code) show as G / H
std::string sec1(std::int64_t ms) { return std::format("{:.1f}", std::max<std::int64_t>(0, ms) / 1000.0); }
const char* res_name(ResourceKind k) {
    return k == ResourceKind::Fuel ? "calm flame" : k == ResourceKind::Rage ? "rage" : k == ResourceKind::Water ? "water" : k == ResourceKind::Energy ? "energy" : k == ResourceKind::Mana ? "mana" : "";
}
const char* move_name(CastMove m) { return m == CastMove::Free ? "can move while casting" : m == CastMove::Slowed ? "slowed while casting" : "rooted while casting"; }
const char* cc_name(StatusKind k) { return k == StatusKind::Count ? "" : status_name(int(k)); }
}  // namespace

// ------------------------------------------------------------------ health bar [U-155][U-166]
void GameView::draw_health_bar(float x, float y, float w, float h, std::int32_t hp, std::int32_t hp_max, std::int32_t shield,
                               std::uint8_t shield_sub, Rgba colour, bool ticks) {
    hp = std::max(0, hp);
    hp_max = std::max(1, hp_max);
    const float total = float(std::max(hp_max, hp + shield));  // a shield bigger than the missing HP squeezes the bar (LoL)
    fill_rect(r_, x - 1, y - 1, w + 2, h + 2, {0, 0, 0, 230});
    fill_rect(r_, x, y, w, h, {40, 14, 14, 255});
    const float hw = w * float(hp) / total;
    fill_rect(r_, x, y, hw, h, colour);
    fill_rect(r_, x, y, hw, std::max(1.f, h / 3), mixc(colour, {255, 255, 255, 255}, 30));  // light top edge
    if (shield > 0) {  // shield segment, coloured by its kind [U-155]
        const Element el = base_element(SubElement(shield_sub));
        const Rgba sc = el == Element::None ? Rgba{235, 235, 235} : mixc(hex(element_colour(el)), {255, 255, 255, 255}, 45);
        fill_rect(r_, x + hw, y, w * float(shield) / total, h, sc);
    }
    if (!ticks) return;
    // marks every 100 HP (thin) and every 1 000 HP (thick), like LoL
    const float per100 = w * 100.f / total;
    if (per100 >= 2.5f)
        for (int k = 1; float(k) * 100.f < total; ++k) {
            const float tx = x + float(k) * per100;
            const bool big = k % 10 == 0;
            fill_rect(r_, tx, y, big ? 2.f : 1.f, big ? h : h * 0.55f, {0, 0, 0, std::uint8_t(big ? 230 : 150)});
        }
}

// ------------------------------------------------------------------ champion icon (placeholder art)
// Every champion: own colour, element badge shape, initials. Same look in lobby, HUD and portraits.
void GameView::draw_icon(int champ, float x, float y, float size, bool dim) {
    if (champ < 0 || champ >= int(champions().size())) return;
    if (SDL_Texture* t = res_.champion_icon(champ, "")) {  // [U-257] the pack picture
        const SDL_FRect d{x, y, size, size};
        const std::uint8_t m = dim ? 90 : 255;
        SDL_SetTextureColorMod(t, m, m, m);
        SDL_RenderTexture(r_, t, nullptr, &d);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        return;
    }
    const ChampionDef& c = champions()[std::size_t(champ)];
    Rgba bg = hex(c.colour);
    if (dim) bg = mixc(bg, {40, 40, 40, 255}, 70);
    fill_rect(r_, x, y, size, size, mixc(bg, {0, 0, 0, 255}, 55));
    fill_circle(r_, x + size / 2, y + size / 2, size * 0.42f, bg, 20);
    const Rgba el = hex(element_colour(c.element));
    const float bx = x + size - size * 0.3f, by = y + size - size * 0.3f, bs = size * 0.26f;
    switch (c.element) {  // badge: water drop / earth square / fire triangle / air ring / none diamond
        case Element::Water: fill_circle(r_, bx + bs / 2, by + bs / 2, bs / 2, el, 12); break;
        case Element::Earth: fill_rect(r_, bx, by, bs, bs, el); break;
        case Element::Fire:
            for (int k = 0; k < int(bs); ++k) fill_rect(r_, bx + bs / 2 - float(k) / 2, by + float(k), float(k), 1, el);
            break;
        case Element::Air: circle(r_, bx + bs / 2, by + bs / 2, bs / 2, el, 12); circle(r_, bx + bs / 2, by + bs / 2, bs / 3, el, 12); break;
        case Element::None:
            for (int k = 0; k < int(bs); ++k) {
                const float wdt = float(k < int(bs) / 2 ? k : int(bs) - k) * 2;
                fill_rect(r_, bx + bs / 2 - wdt / 2, by + float(k), wdt, 1, el);
            }
            break;
    }
    std::string ini;
    for (std::size_t i = 0; i < c.name.size(); ++i)
        if (i == 0 || c.name[i - 1] == ' ') ini += c.name[i];
    const float sc = size >= 56 ? 2.f : 1.f;
    text(r_, x + size / 2 - text_w(ini, sc) / 2, y + size / 2 - 4 * sc, ini, dim ? Rgba{150, 150, 150} : Rgba{255, 255, 255}, sc);
    rect(r_, x, y, size, size, dim ? Rgba{80, 80, 80} : Rgba{200, 190, 150});
}

bool GameView::icon_button(Ui& ui, float x, float y, float size, std::string_view label, std::string_view key, bool active) {
    const bool hv = hover(ui, x, y, size, size);
    fill_rect(r_, x, y, size, size, active ? Rgba{110, 80, 30, 255} : hv ? Rgba{70, 70, 95, 255} : Rgba{38, 38, 52, 255});
    rect(r_, x, y, size, size, hv ? Rgba{255, 220, 140} : Rgba{110, 110, 140});
    text(r_, x + size / 2 - text_w(label) / 2, y + size / 2 - 4, label, {235, 235, 235});
    text(r_, x + 2, y + 2, key, {255, 220, 120});  // the key only on the icon [U-169]
    if (hv && ui.clicked) {
        ui.clicked = false;
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ buffs / debuffs [U-167]
void GameView::draw_buffs(NetClient& net, float x, float y, double since) {
    // [U-237][U-255] my modifiers in one row: sky first (gold frame), terrain, then statuses / buffs with timers
    const auto& me = net.cur.me;
    std::vector<ModView> mods;
    const auto it = net.roster.find(net.my_unit);
    const Element el = it != net.roster.end() ? champions()[std::size_t(it->second.champ)].element : Element::None;
    const SkyState sky = WorldClock::at_ms(net.start_ms + net.cur.now_ms + std::int64_t(since * 1000));
    for (ModView& m : env_modifiers(el, sky)) mods.push_back(std::move(m));
    for (const DrawUnit& d : draw_)
        if (d.u.id == net.my_unit) {
            const int cx = int(d.x) / kCellUnits, cy = int(d.y) / kCellUnits;
            if (Grid::in_bounds(cx, cy))
                for (ModView& m : terrain_modifiers(net.seen.at(cx, cy), el)) mods.push_back(std::move(m));
        }
    std::array<bool, 256> present{};
    for (int i = 0; i < me.nstatus && i < int(std::size(me.status)); ++i) {
        const auto& st = me.status[i];
        present[st.kind] = true;
        const std::int64_t left = std::max<std::int64_t>(0, st.left_ms - std::int64_t(since * 1000));
        auto& mx = status_max_[st.kind];
        mx = std::max(mx, st.left_ms);
        ModView v = wire_modifier(st.kind, st.value, left);
        v.value = int(mx);  // full duration, for the curtain
        mods.push_back(std::move(v));
    }
    for (std::size_t k = 0; k < status_max_.size(); ++k)
        if (!present[k]) status_max_[k] = 0;
    const float s = 30;
    float bx = x;
    const ModView* hovered = nullptr;
    float hx = 0;
    for (const ModView& m : mods) {
        fill_rect(r_, bx, y, s, s, {20, 20, 28, 235});
        fill_rect(r_, bx + 2, y + 2, s - 4, s - 4, mixc(m.colour, {0, 0, 0, 255}, m.minor ? 75 : 55));
        if (m.left_ms >= 0) {  // duration: a dark curtain growing from the top + a shrinking bar
            const float f = m.value > 0 ? float(m.left_ms) / float(m.value) : 0;
            fill_rect(r_, bx + 2, y + 2, s - 4, (s - 4) * (1 - f), {0, 0, 0, 120});
            fill_rect(r_, bx, y + s - 3, s * f, 3, m.colour);
            text(r_, bx + 3, y + 16, sec1(m.left_ms), {230, 230, 230});
        }
        rect(r_, bx, y, s, s, m.environment ? Rgba{255, 210, 90} : m.debuff ? Rgba{230, 70, 70} : Rgba{80, 220, 110});
        text(r_, bx + 3, y + 4, m.tag, m.minor ? Rgba{170, 170, 170} : Rgba{255, 255, 255});
        if (ui_ && hover(*ui_, bx, y, s, s)) hovered = &m, hx = bx;
        bx += s + 3;
    }
    if (hovered) {  // tooltip: name, what it does, time left
        const std::string t1 = hovered->name + (hovered->left_ms >= 0 ? std::format("  {} s", sec1(hovered->left_ms)) : "");
        const float tw = std::max(text_w(t1), text_w(hovered->detail)) + 10;
        const float tx = std::min(hx, view_.sw - tw - 4);
        fill_rect(r_, tx, y - 34, tw, 30, {0, 0, 0, 235});
        rect(r_, tx, y - 34, tw, 30, hovered->colour);
        text(r_, tx + 5, y - 30, t1, {255, 255, 255});
        text(r_, tx + 5, y - 17, hovered->detail, {210, 210, 220});
    }
}

// ------------------------------------------------------------------ skill tooltip [U-168]
void GameView::draw_skill_tooltip(NetClient& net, int i, float x, float y, bool alt, bool plus) {
    const SkillDef* d = skill_of(net, i);
    if (!d) return;
    const auto& me = net.cur.me;
    const int rank = std::max<int>(1, me.rank[i]);
    std::vector<std::string> L;
    L.push_back(std::format("{}  [{}]  {} {}{}", d->name, kKeys[i], tr("rank"), me.rank[i], d->ultimate ? "   " + tr("ULTIMATE") : ""));
    L.push_back(std::string(d->desc));
    // [U-291] its own passive, cast stages, rank rule
    if (!d->passive_desc.empty()) L.push_back(std::format("{}: {}", tr("Passive"), d->passive_desc));
    if (d->casts > 1) L.push_back(std::format("{} {} {:.1f} s", d->casts, tr("casts, the next within"), d->recast_ms / 1000.0));
    if (d->kind != SkillKind::Passive && i != int(Slot::P) && i < int(Slot::F)) {
        const int step = d->rank_step ? d->rank_step : 6;
        if (d->ultimate)
            L.push_back(std::format("{} {}, {} {}", tr("Unlocks at level"), d->unlock_level ? d->unlock_level : 6, tr("a new rank every"), step));
        else
            L.push_back(std::format("{} {} {} {}, +1 {} {} {}", tr("Max rank"), d->rank_cap ? d->rank_cap : 2, tr("before level"), step, tr("every"), step,
                                    tr("levels")));
    }
    const auto it = net.roster.find(net.my_unit);
    const ChampionDef* cd = it != net.roster.end() ? &champions()[std::size_t(it->second.champ)] : nullptr;
    // [U-470] every number that grows with rank: its value now (rank-scaled)
    const int rk = rank - 1;
    const std::int32_t cc_ms = d->cc_ms + d->cc_ms_rank * rk, cc_val = d->cc_val + d->cc_val_rank * rk, dur_ms = d->dur_ms + d->dur_ms_rank * rk;
    const std::int32_t knock = d->knock + d->knock_rank * rk;
    auto zone_text = [&](SweetZone z, int size, FxKind k, Geo g) {  // [U-461]
        z = sweet_zone_for(k, g, z);
        const int sz = sweet_size_for(z, size);
        return z == SweetZone::Centre ? std::format("the middle {}%", sz) : z == SweetZone::Edge ? std::format("the outer {}%", sz)
                                                                          : std::format("the last {}% of its flight", sz);
    };
    auto fx_of = [](SkillKind k) {
        return k == SkillKind::Cone ? FxKind::Cone : k == SkillKind::Bolt || k == SkillKind::Dash ? FxKind::Bolt : k == SkillKind::Wave ? FxKind::Wave : FxKind::Burst;
    };
    constexpr const char* kGeoNames[] = {"", "circle", "ring", "line", "rectangle", "cone", "arc"};
    if (plus) {  // what the next rank gives
        L.push_back(std::format("Next rank ({} → {}):", me.rank[i], me.rank[i] + 1));
        if (d->dmg_rank) L.push_back(std::format("  damage +{}", d->dmg_rank));
        if (d->cd_rank) L.push_back(std::format("  cooldown {:+.1f} s", d->cd_rank / 1000.0));
        bool any = d->dmg_rank || d->cd_rank;
        for (auto [name, per, ms] : {std::tuple{"crowd control", d->cc_ms_rank, true}, {"crowd control strength", d->cc_val_rank, false},
                                     {"duration", d->dur_ms_rank, true}, {"range", d->range_rank, false}, {"width", d->width_rank, false},
                                     {"radius", d->radius_rank, false}, {"knock back", d->knock_rank, false}, {"cost", d->cost_rank, false}})
            if (per) {  // [U-470]
                L.push_back(ms ? std::format("  {} {:+.2f} s", name, per / 1000.0) : std::format("  {} {:+}", name, per));
                any = true;
            }
        for (const SkillDef::Learned& l : d->learned)
            if (l.key != StatKey::Count && l.per_rank) {
                L.push_back(std::format("  {} {:+.1f}%", kStatKeyNames[int(l.key)], l.per_rank / 100.0));
                any = true;
            }
        if (!any) L.push_back("  rank counts for scaling only");
        if (d->ultimate) L.push_back(std::format("  {} {}", tr("ultimate: a new rank every"), d->rank_step ? d->rank_step : 6));
    } else if (d->kind == SkillKind::Passive) {
        L.push_back("Passive: always on.");
    } else if (!alt) {  // the important numbers, only the ones this skill has
        const char* what = d->kind == SkillKind::Heal ? "Heal" : d->kind == SkillKind::Shield ? "Shield" : "Damage";
        if (me.s_dmg[i] > 0) L.push_back(std::format("{} {}", what, me.s_dmg[i]));
        L.push_back(std::format("Cooldown {} s   Cast time {}", sec1(me.cd_total_ms[i]), me.s_cast_ms[i] > 0 ? std::format("{} ms", me.s_cast_ms[i]) : "instant"));
        if (me.s_cost[i] > 0 && cd) L.push_back(std::format("Cost {} {}", me.s_cost[i], res_name(cd->resource)));
        std::string geo;
        if (me.s_range[i]) geo += std::format("range {}  ", me.s_range[i]);
        if (me.s_width[i]) geo += std::format("width {}  ", me.s_width[i]);
        if (me.s_radius[i]) geo += std::format("radius {}  ", me.s_radius[i]);
        if (me.s_speed[i] && (d->kind == SkillKind::Bolt || d->kind == SkillKind::Wave || d->kind == SkillKind::Tornado || d->kind == SkillKind::Dash))
            geo += std::format("speed {}", me.s_speed[i]);
        if (!geo.empty()) L.push_back(geo);
        if (d->delay_ms) L.push_back(std::format("Delay {} ms", d->delay_ms));
        if (dur_ms) L.push_back(std::format("Lasts {} s{}", sec1(dur_ms), d->kind == SkillKind::Field ? std::format(", hits every {} ms", d->tick_ms) : ""));
        if (d->cc != StatusKind::Count) L.push_back(std::format("{} {} s{}", cc_name(d->cc), sec1(cc_ms), cc_val ? std::format(" ({})", cc_val) : ""));
        if (knock) L.push_back(std::format("Knock back {}", knock));
        if (d->geo != Geo::Auto) L.push_back(std::format("Shape: {}{}", kGeoNames[int(d->geo)], d->inner ? std::format(" (hole {})", d->inner) : ""));
        if (d->sweet_pct > 0 && !(d->casts > 1 && d->stages[0].kind != SkillKind::None))
            L.push_back(std::format("Sweet spot +{}% in {}", d->sweet_pct, zone_text(d->sweet_zone, d->sweet_size, fx_of(d->kind), d->geo)));
        if (d->casts > 1) L.push_back(std::format("{} casts within {} s", d->casts, sec1(d->recast_ms)));
        for (std::size_t sI = 0; d->casts > 1 && sI < d->stages.size() && sI < std::size_t(d->casts); ++sI) {  // [U-456][U-461] each cast
            const SkillDef::CastStage& st = d->stages[sI];
            if (st.kind == SkillKind::None) continue;
            const Geo g = st.geo != Geo::Auto ? st.geo : d->geo;
            std::string ln = std::format("  cast {}: {}{}{}  {}% dmg", sI + 1, g != Geo::Auto ? kGeoNames[int(g)] : st.kind == SkillKind::Cone ? "cone" : "circle",
                                         st.range ? std::format(" {}", st.range) : "", st.width ? std::format("×{}", st.width) : st.radius ? std::format(" r{}", st.radius) : "", st.dmg_pct);
            const int sp = st.sweet_pct >= 0 ? st.sweet_pct : d->sweet_pct;
            if (sp > 0) ln += std::format(", sweet +{}% in {}", sp, zone_text(st.sweet_zone != SweetZone::Auto ? st.sweet_zone : d->sweet_zone,
                                                                             st.sweet_size ? st.sweet_size : d->sweet_size, fx_of(st.kind), g));
            L.push_back(ln);
        }
        if (d->tether_ms) L.push_back(std::format("Chain {} s: still within {} then = pulled {} + hit again", sec1(d->tether_ms), d->tether_radius, d->pull));
        if (d->kind == SkillKind::Transform)
            L.push_back(std::format("Form: +{}% attack, +{}% healing, +{}% speed", d->buff_attack_pct, d->buff_heal_pct, cc_val));
        for (const SkillDef::Learned& l : d->learned)  // [U-468]
            if (l.key != StatKey::Count)
                L.push_back(std::format("While learned: {} {:.1f}%", kStatKeyNames[int(l.key)], (l.base + l.per_rank * rk) / 100.0));
        if (d->free_cast) L.push_back("Can be used during another cast");
        if (d->pass_units || d->through_walls) L.push_back(d->pass_units && d->through_walls ? "Goes through units and over walls" : d->pass_units ? "Goes through units" : "Goes over walls");
        if (d->aa_reset) L.push_back("Resets your autoattack");
        if (d->cast_ms > 0) L.push_back(move_name(d->move));
        L.push_back("hold Alt: formulas");
    } else {  // Alt: how every number is built
        const double ep = me.ep_raw / 100.0, atk = me.attack_raw / 100.0, bon = me.bonus_attack_raw / 100.0;
        if (d->dmg || d->dmg_rank || d->ep || d->ad || d->bonus_ad) {
            std::string f = std::format("Damage = {}", d->dmg);
            if (d->dmg_rank) f += std::format(" + {}×(rank−1) [{}]", d->dmg_rank, d->dmg_rank * (rank - 1));
            if (d->ep) f += std::format(" + {}% EP [{:.0f}]", d->ep, ep * d->ep / 100);
            if (d->ad) f += std::format(" + {}% ATK [{:.0f}]", d->ad, atk * d->ad / 100);
            if (d->bonus_ad) f += std::format(" + {}% bonus ATK [{:.0f}]", d->bonus_ad, bon * d->bonus_ad / 100);
            L.push_back(f);
            L.push_back(std::format("   × sky power {:.1f}%{} = {}", me.power_pct / 10.0, d->sub == SubElement::Ice || d->sub == SubElement::Steam ? " × 80% (state change)" : "", me.s_dmg[i]));
        }
        L.push_back(std::format("Cooldown = {} ms{} ÷ (1 + haste) = {} ms", d->cd_ms, d->cd_rank ? std::format(" + {}×(rank−1)", d->cd_rank) : "", me.cd_total_ms[i]));
        if (d->cast_ms) L.push_back(std::format("Cast time = {} ms ÷ (1 + cast speed {}) = {} ms", d->cast_ms, me.cast_speed / 100, me.s_cast_ms[i]));
        if (d->range) L.push_back(std::format("Range = {} + items × sky size {}% = {}", d->range, me.size_pct, me.s_range[i]));
        if (d->width) L.push_back(std::format("Width = {} + items × sky size {}% = {}", d->width, me.size_pct, me.s_width[i]));
        if (d->radius) L.push_back(std::format("Radius = {} + items × sky size {}% = {}", d->radius, me.size_pct, me.s_radius[i]));
        if (d->cost) L.push_back(std::format("Cost = {}", d->cost));
        // [U-470] every rank-scaled number: base + growth × (rank − 1) = now
        for (auto [name, base, per, ms] : {std::tuple{cc_name(d->cc), d->cc_ms, d->cc_ms_rank, true}, {"CC strength", d->cc_val, d->cc_val_rank, false},
                                           {"Duration", d->dur_ms, d->dur_ms_rank, true}, {"Knock back", d->knock, d->knock_rank, false}})
            if ((base || per) && !(std::string_view(name).empty()))
                L.push_back(ms ? std::format("{} = {} s{} = {} s", name, sec1(base), per ? std::format(" + {}×(rank−1)", sec1(per)) : "", sec1(base + per * rk))
                               : std::format("{} = {}{} = {}", name, base, per ? std::format(" + {}×(rank−1)", per) : "", base + per * rk));
        // [U-464] what multiplies the skill's damage by its kind
        const bool martial = !d->arcane && d->sub == SubElement::Physical;
        if (d->dmg && (me.skill_power || (d->arcane && me.arcane_power) || (martial && me.martial_power)))
            L.push_back(std::format("   × (1 + skill power {}%{})", me.skill_power / 100,
                                    d->arcane ? std::format(" + arcane {}%", me.arcane_power / 100) : martial ? std::format(" + martial {}%", me.martial_power / 100) : ""));
        for (const SkillDef::Learned& l : d->learned)
            if (l.key != StatKey::Count)
                L.push_back(std::format("{} = {:.1f}%{} = {:.1f}%", kStatKeyNames[int(l.key)], l.base / 100.0,
                                        l.per_rank ? std::format(" + {:.1f}%×(rank−1)", l.per_rank / 100.0) : "", (l.base + l.per_rank * rk) / 100.0));
    }
    float w = 0;
    for (const auto& l : L) w = std::max(w, text_w(l));
    const float h = float(L.size()) * 12 + 8;
    x = std::clamp(x, 4.f, view_.sw - w - 16);
    y -= h;
    fill_rect(r_, x, y, w + 12, h, {8, 8, 14, 240});
    rect(r_, x, y, w + 12, h, {150, 130, 90});
    for (std::size_t k = 0; k < L.size(); ++k) text(r_, x + 6, y + 5 + float(k) * 12, L[k], k == 0 ? Rgba{255, 220, 120} : Rgba{225, 225, 225});
}

// ------------------------------------------------------------------ bottom panel [U-166][U-169][U-170]
void GameView::draw_bottom_panel(NetClient& net, double now_s, Ui& ui) {
    const Snapshot& cur = net.cur;
    const auto& me = cur.me;
    const float W = view_.sw, H = view_.sh;
    const double since = now_s - cur.arrived;
    const net::NetUnit* mu = nullptr;
    for (const auto& u : cur.units)
        if (u.id == net.my_unit) mu = &u;
    const auto it = net.roster.find(net.my_unit);
    if (it == net.roster.end()) return;
    const int champ = it->second.champ;
    const ChampionDef& cd = champions()[std::size_t(champ)];
    const bool alt = (SDL_GetModState() & SDL_KMOD_ALT) != 0;

    // [U-289] items = 4 × 2 square icon slots (44 px), then two columns of icon buttons; on narrow screens the
    // skills move closer (pitch 56 → down to 52) so the whole panel always fits
    constexpr float kItemPitch = 44, kItemsW = 4 * kItemPitch, kButtonsW = 2 * 42 + 6;
    const float sp = std::clamp((W - 8 - 250 - 4 - kItemsW - 6 - kButtonsW - 12) / float(kSkillSlots), 52.f, 56.f);
    const float skills_w = float(kSkillSlots) * sp + 12;
    const float panel_w = 250 + skills_w + 4 + kItemsW + 6 + kButtonsW;
    const float x0 = std::max(4.f, (W - panel_w) / 2), y0 = H - 128;
    fill_rect(r_, x0, y0, panel_w, 128, {10, 10, 18, 225});
    rect(r_, x0, y0, panel_w, 128, {70, 60, 45});
    mouse_on_hud_ = hover(ui, x0, y0 - 70, panel_w, 200) || (set_.minimap && hover(ui, W - 208, H - 358, 208, 208)) ||
                    (show_shop_ && hover(ui, shop_x_, shop_y_ - 24, std::min(W - 80, 1100.f), std::min(H - 260, 620.f) + 24));  // [U-389] the real shop rect

    // left: icon + name + level/XP, stats beside it
    draw_icon(champ, x0 + 8, y0 + 8, 72, mu && !mu->alive);
    if (mu) {
        fill_rect(r_, x0 + 60, y0 + 62, 22, 18, {0, 0, 0, 220});
        text(r_, x0 + 62, y0 + 66, std::format("{}", mu->level), {255, 230, 120}, 1.5f);
    }
    text(r_, x0 + 8, y0 + 84, std::string(cd.name), hex(cd.colour));
    text(r_, x0 + 8, y0 + 96, utf8_cut(it->second.name, 11), {200, 200, 210});
    fill_rect(r_, x0 + 8, y0 + 110, 72, 5, {30, 30, 30, 255});
    fill_rect(r_, x0 + 8, y0 + 110, 72 * float(me.xp_this) / float(std::max<std::int64_t>(1, me.xp_next)), 5, {190, 120, 255});
    if (me.skill_points > 0) text(r_, x0 + 8, y0 + 117, std::format("+{} pts", me.skill_points), {255, 220, 120});
    const float sx0 = x0 + 90;
    // [U-238] basic stats like LoL; C = all of them; the second page = my loadout and what it did
    if (button(r_, ui, sx0, y0 - 20, 70, 18, tr("Stats"), panel_page_ == 0)) panel_page_ = 0;
    if (button(r_, ui, sx0 + 74, y0 - 20, 80, 18, tr("Loadout"), panel_page_ == 1)) panel_page_ = 1;
    if (panel_page_ == 0) {
        std::vector<std::string> stats = {
            std::format("ATK  {}", me.attack), std::format("EP   {}", me.ep), std::format("DEF  {}", me.def),
            std::format("EDEF {}", me.edef), std::format("AS   {:.2f}", me.as100x / 100.0), std::format("MS   {}", me.move_speed),
        };
        if (expand_stats_)
            for (std::string extra : {std::format("RNG  {}", me.attack_range), std::format("HST  {}", me.haste), std::format("TEN  {}%", me.tenacity),
                                      std::format("SIZE {}%", me.model_size), std::format("CAST {}", me.cast_speed / 100)})
                stats.push_back(extra);
        // [U-463][U-464] only what this champion can use: crits (champions that crit), the skill power of its kinds of skills
        if (expand_stats_) {
            if (me.uses & 1) stats.push_back(std::format("CRIT {}%", me.crit_chance / 100)), stats.push_back(std::format("CDMG {}%", me.crit_damage / 100));
            if (expand_stats_) {
                if (me.uses & 2) stats.push_back(std::format("ARC  +{}%", me.arcane_power / 100));
                if (me.uses & 4) stats.push_back(std::format("MAR  +{}%", me.martial_power / 100));
                if (me.skill_power) stats.push_back(std::format("SKP  +{}%", me.skill_power / 100));
                if (me.vamp) stats.push_back(std::format("VAMP {}%", me.vamp / 100));
                // [U-475] LoL: ability power / magic pen — only when this champion has AP skills or the stats
                if ((me.uses & 8) || me.ap) stats.push_back(std::format("AP   {}", me.ap / 100));
                if (me.mpen || me.mpen_pct) stats.push_back(std::format("MPEN {}|{}%", me.mpen / 100, me.mpen_pct / 100));
                // [U-476] Dota: attributes, magic resistance, evasion, regen — only when present
                if (me.str || me.agi || me.intel)
                    stats.push_back(std::format("STR {}", me.str / 100)), stats.push_back(std::format("AGI {}", me.agi / 100)),
                        stats.push_back(std::format("INT {}", me.intel / 100));
                if (me.mres) stats.push_back(std::format("MRES {}%", me.mres / 100));
                if (me.eva) stats.push_back(std::format("EVA  {}%", me.eva / 100));
                if (me.hp_regen) stats.push_back(std::format("HP5  +{:.1f}/s", me.hp_regen / 100.0));
            }
        }
        for (std::size_t k = 0; k < std::min<std::size_t>(stats.size(), 12); ++k)
            text(r_, sx0 + float(k / 6) * 78, y0 + 10 + float(k % 6) * 12, stats[k], {205, 205, 215});
        if (stats.size() > 12) {  // [U-476] more stats than fit beside the skills: a panel above the bar
            const std::size_t more = stats.size() - 12, cols = (more + 5) / 6;
            const float px = sx0, py = y0 - 28 - 6 * 12;
            fill_rect(r_, px - 4, py - 4, float(cols) * 92 + 6, 6 * 12 + 8, {8, 8, 14, 225});
            for (std::size_t k = 0; k < more; ++k) text(r_, px + float(k / 6) * 92, py + float(k % 6) * 12, stats[12 + k], {205, 205, 215});
        }
        if (!expand_stats_) text(r_, sx0 + 78, y0 + 10, tr("C = all"), {130, 130, 150});  // (beside the stats, not on them)
    } else {
        const Loadout& lo = me.loadout;
        float ly = y0 + 8;
        auto put = [&](const std::string& t2, Rgba c) {
            text(r_, sx0, ly, t2, c);
            ly += 12;
        };
        if (lo.keystone >= 0) {
            const KeystoneDef& k = kKeystones[std::size_t(lo.keystone)];
            put(std::string(k.name), {255, 230, 150});
            put(std::format("{} {}", me.keystone_impact, k.impact), {200, 230, 200});
        } else {
            put(tr("no keystone"), {150, 150, 160});
        }
        if (lo.tree >= 0) {
            put(std::string(kSpecTrees[std::size_t(lo.tree)].name), {220, 220, 230});
            for (std::size_t t = 0; t < 3; ++t)
                if (lo.nodes[t] >= 0) put("  " + std::string(kSpecTrees[std::size_t(lo.tree)].tiers[t][std::size_t(lo.nodes[t])].name), {190, 190, 205});
        }
        for (std::size_t r2 = 0; r2 < 3; ++r2)
            if (lo.runes[r2] >= 0 && ly < y0 + 118) put(std::string(kRunes[r2][std::size_t(lo.runes[r2])].name), {190, 190, 205});
    }
    text(r_, sx0, y0 + 88, std::format("{} {}", tr("gold"), me.gold), {255, 215, 80}, 1.5f);
    if (cd.stance) text(r_, sx0, y0 + 106, me.stance_b ? tr("stance: knives") : tr("stance: fists"), {200, 200, 220});

    // middle: skills, then HP + resource UNDER the skills [U-170]
    const float kx = x0 + 250, ky = y0 + 10;
    int hover_slot = -1;
    bool hover_plus = false;
    for (int i = 0; i < kSkillSlots; ++i) {  // P Q W E R T Y | G H (the specials a little apart)
        const float sx = kx + float(i) * sp + (i >= int(Slot::F) ? 12.f : 0.f), sy = ky;
        const SkillDef* d = skill_of(net, i);
        const bool learned = i == 0 || me.rank[i] > 0;
        const std::int64_t cd_left = std::max<std::int64_t>(0, me.cd_left_ms[i] - std::int64_t(since * 1000));
        fill_rect(r_, sx, sy, 50, 50, learned ? Rgba{48, 48, 66, 255} : Rgba{22, 22, 28, 255});
        if (d) {
            if (SDL_Texture* ic = res_.skill_icon(d->name)) {  // [U-257] the ComfyUI icon
                const SDL_FRect dst{sx + 1, sy + 1, 48, 48};
                SDL_SetTextureColorMod(ic, learned ? 255 : 90, learned ? 255 : 90, learned ? 255 : 90);
                SDL_RenderTexture(r_, ic, nullptr, &dst);
                SDL_SetTextureColorMod(ic, 255, 255, 255);
            } else {
                const Rgba ec = hex(element_colour(base_element(d->sub)));
                fill_circle(r_, sx + 25, sy + 25, 16, with_alpha(ec, learned ? 200 : 60), 16);
                text(r_, sx + 25 - text_w(utf8_cut(d->name, 5)) / 2, sy + 21, utf8_cut(d->name, 5), learned ? Rgba{255, 255, 255} : Rgba{110, 110, 110});
            }
        }
        if (cd_left > 0 && learned && me.cd_total_ms[i] > 0) {  // [U-444] a clockwise sweep + the exact time
            const float f = std::min(1.f, float(cd_left) / float(std::max<std::int64_t>(me.cd_total_ms[i], cd_left)));
            cooldown_sweep(r_, sx, sy, 50, f, {0, 0, 0, 185});
            const std::string ts = cd_left < 1'000 ? std::format("{:.1f}", cd_left / 1000.0) : std::format("{}", (cd_left + 999) / 1000);
            text(r_, sx + 26 - text_w(ts, 1.6f) / 2, sy + 19, ts, {0, 0, 0}, 1.6f);  // (a shadow)
            text(r_, sx + 25 - text_w(ts, 1.6f) / 2, sy + 18, ts, {255, 255, 255}, 1.6f);
            cd_was_[std::size_t(i)] = true;
        } else if (cd_was_[std::size_t(i)]) {  // just became ready: a short bright flash
            cd_was_[std::size_t(i)] = false;
            cd_ready_t_[std::size_t(i)] = last_frame_;
        }
        if (const double fl = last_frame_ - cd_ready_t_[std::size_t(i)]; fl >= 0 && fl < 0.35)
            fill_rect(r_, sx, sy, 50, 50, {255, 255, 230, std::uint8_t(160 * (1 - fl / 0.35))});
        if (me.stolen_ms > 0 && me.stolen_slot == i) text(r_, sx + 2, sy + 38, "STOLEN", {255, 80, 255});
        if (me.recast[i] > 0 && d && d->casts > 1) {  // [U-458] a multicast: which cast is next + how long it stays open
            const std::int64_t wl = std::max<std::int64_t>(0, me.recast_left_ms[i] - std::int64_t(since * 1000));
            for (int k = 0; k < std::min(d->casts, 6); ++k) {  // one pip per cast: done = dim, next = bright, later = outline
                const float px = sx + 25 - float(std::min(d->casts, 6) - 1) * 4.f + float(k) * 8.f, py = sy - 6;
                if (k < me.recast[i]) fill_circle(r_, px, py, 3, {120, 110, 60, 255}, 8);
                else if (k == me.recast[i]) fill_circle(r_, px, py, 3.5f, {255, 240, 120, 255}, 8);
                else ring_arc(r_, px, py, 3, 1.f, {200, 200, 160, 200}, 1);
            }
            if (wl > 0 && me.recast_total_ms[i] > 0) {
                ring_arc(r_, sx + 25, sy + 25, 27, float(wl) / float(me.recast_total_ms[i]), {255, 225, 90, 255}, 3);  // the window running out
                const std::string ns = std::format("{}", me.recast[i] + 1);  // the number of the NEXT cast, big
                text(r_, sx + 26 - text_w(ns, 1.6f) / 2, sy + 15, ns, {0, 0, 0}, 1.6f);
                text(r_, sx + 25 - text_w(ns, 1.6f) / 2, sy + 14, ns, {255, 240, 120}, 1.6f);
                const std::string ws = std::format("{:.1f}s", wl / 1000.0);  // seconds before it goes on cooldown
                text(r_, sx + 25 - text_w(ws) / 2, sy + 33, ws, wl < 1'000 ? Rgba{255, 120, 90} : Rgba{255, 240, 180});
            }
        }
        if (me.charges[i] != 255) {  // [U-433][U-444] charges ready + a ring filling up to the next charge
            fill_circle(r_, sx + 41, sy + 40, 9, {20, 20, 30, 235}, 16);
            if (me.charge_total_ms[i] > 0 && me.charge_left_ms[i] > 0)
                ring_arc(r_, sx + 41, sy + 40, 9, 1.f - float(me.charge_left_ms[i]) / float(me.charge_total_ms[i]), {140, 230, 255, 255}, 2);
            const std::string cs = std::format("{}", me.charges[i]);
            text(r_, sx + 41 - text_w(cs) / 2, sy + 36, cs, me.charges[i] > 0 ? Rgba{140, 230, 255} : Rgba{150, 150, 150});
        }
        if (d && me.s_cost[i] > 0 && me.res < me.s_cost[i] * 100 && cd.resource != ResourceKind::Rage)  // no resource
            fill_rect(r_, sx, sy, 50, 50, cd.resource == ResourceKind::Fuel ? Rgba{140, 30, 30, 90} : Rgba{20, 40, 140, 90});  // fuel: HP pays
        rect(r_, sx, sy, 50, 50, pending_ == i ? Rgba{255, 255, 255} : Rgba{95, 95, 125});
        if (ui.clicked && hover(ui, sx, sy, 50, 50) && d) {  // [U-408] click the icon = press its key (aim with the next click)
            ui.clicked = false;
            cast_key(net, i, true);
        }
        text(r_, sx + 2, sy + 2, kKeys[i], {255, 220, 120});
        for (int p = 0; p < std::min<int>(me.rank[i], 6) && i > 0; ++p) fill_rect(r_, sx + 4 + float(p) * 7, sy + 46, 5, 3, {255, 210, 90});
        // [U-297][U-301] a "+" for every rankable skill (Q … Y, the ultimate too); a blocked one is grey and only says why
        if (me.skill_points > 0 && i >= int(Slot::Q) && i <= int(Slot::Y)) {
            const SkillDef* sd = skill_of(net, i);
            if (sd && sd->kind != SkillKind::Passive) {
                const bool can = mu && me.rank[i] < skill_max_rank(*sd, mu->level);  // [U-291] the skill's own rule
                if (hover(ui, sx + 17, sy - 17, 16, 15)) hover_slot = i, hover_plus = true;
                if (button(r_, ui, sx + 17, sy - 17, 16, 15, "+", false, can)) net.send_cmd({.kind = CmdKind::LevelUp, .slot = std::uint8_t(i)});
                else if (!can && ui.clicked && hover(ui, sx + 17, sy - 17, 16, 15)) {
                    ui.clicked = false;
                    hint(std::format("{}: {}", sd->name, tr("cannot level up now — pick another skill")));
                }
            }
        }
        if (hover(ui, sx, sy, 50, 50) && d) hover_slot = i;
    }
    if (mu) {  // HP + resource under the skills
        const float bw = skills_w - 12, bx = kx;
        draw_health_bar(bx, ky + 58, bw, 18, mu->hp / 100, mu->hp_max / 100, mu->shield, mu->shield_sub, {70, 200, 70, 255}, true);
        const std::string hp_t = mu->shield > 0 ? std::format("{} / {}  (+{})", mu->hp / 100, mu->hp_max / 100, mu->shield)
                                                : std::format("{} / {}", mu->hp / 100, mu->hp_max / 100);
        text(r_, bx + bw / 2 - text_w(hp_t) / 2, ky + 63, hp_t, {255, 255, 255});
        fill_rect(r_, bx - 1, ky + 80, bw + 2, 14, {0, 0, 0, 230});
        if (me.res_max > 0) {
            const Rgba rc = resource_colour(std::uint8_t(cd.resource), SDL_GetTicks());  // [U-268] calm flame / chaotic rage
            fill_rect(r_, bx, ky + 81, bw * float(me.res) / float(me.res_max), 12, rc);
            const std::string rt = std::format("{} / {}{}", me.res / 100, me.res_max / 100,
                                               unlimited_fuel(WorldClock::at_ms(net.start_ms + cur.now_ms)) && cd.resource == ResourceKind::Fuel ? "  comet: unlimited" : "");
            text(r_, bx + bw / 2 - text_w(rt) / 2, ky + 83, rt, {255, 255, 255});
        } else {
            const std::string rt = cd.element == Element::Earth ? "no resource: slow casts, long cooldowns" : "no resource";
            text(r_, bx + bw / 2 - text_w(rt) / 2, ky + 83, rt, {150, 150, 160});
        }
    }
    // above the skills: cast / channel bar (small) and buffs [U-167][U-169]
    const float above = y0 - 8;
    if ((me.cast_left_ms > 0 && me.cast_total_ms > 0) || me.channel_left_ms > 0) {
        const bool chan = me.channel_left_ms > 0;
        const std::int64_t left = std::max<std::int64_t>(0, (chan ? me.channel_left_ms : me.cast_left_ms) - std::int64_t(since * 1000));
        const float total = chan ? (left > 4'000 ? 8'000.f : 4'000.f) : float(me.cast_total_ms);
        const float bw = 180, bx = kx + (skills_w - 12) / 2 - bw / 2, by = above - 14;
        fill_rect(r_, bx - 1, by - 1, bw + 2, 9, {0, 0, 0, 220});
        fill_rect(r_, bx, by, bw * (1 - float(left) / total), 7, chan ? Rgba{140, 190, 255} : Rgba{255, 220, 120});
        text(r_, bx + bw + 6, by - 1, chan ? std::format("channel {} s", sec1(left)) : std::format("cast {} s", sec1(left)), {220, 220, 230});
    }
    draw_buffs(net, kx, above - 50, since);
    if (me.wind_sense_ms > 0) text(r_, kx + 300, above - 44, std::format("Wind Sense {} s", sec1(me.wind_sense_ms)), {240, 250, 255});
    if (me.no_cd) text(r_, kx + 300, above - 30, "practice: no cooldowns", {255, 120, 120});

    // right: items (key on the slot), icon buttons (key only on the icon) [U-169]
    const float ix = kx + skills_w + 4;
    for (int i = 0; i < kItemSlots; ++i) {
        // [U-322] a 3 × 3 box (slot 0 = special, key 1 … slot 8, key 9) and the boots slot left of its bottom row
        const bool boots = i == kBootsSlot;
        const float sx = boots ? ix : ix + kItemPitch + float(i % 3) * kItemPitch;
        const float sy = boots ? y0 + 6 + 2 * 41 : y0 + 6 + float(i / 3) * 41;
        fill_rect(r_, sx, sy, 38, 38, boots ? Rgba{34, 40, 34, 255} : i == kSpecialSlot ? Rgba{44, 38, 26, 255} : Rgba{40, 40, 50, 255});
        rect(r_, sx, sy, 38, 38, i == kSpecialSlot ? Rgba{200, 170, 90} : Rgba{90, 90, 110});
        if (!boots) text(r_, sx + 2, sy + 2, std::format("{}", i + 1), {150, 150, 170});
        else if (me.items[i] < 0) text(r_, sx + 4, sy + 14, tr("boots"), {110, 130, 110});
        if (me.items[i] >= 0) {
            const ItemDef& d = kItems[std::size_t(me.items[i])];
            draw_item_icon(r_, me.items[i], sx + 1, sy + 1, 36);  // [U-257] the item picture
            if (d.active != ItemActive::None && me.item_cd_ms[i] > 0) {
                fill_rect(r_, sx + 2, sy + 2, 36, 36, {0, 0, 0, 150});
                text(r_, sx + 6, sy + 14, std::format("{}s", me.item_cd_ms[i] / 1000), {255, 220, 120});
            }
            if (hover(ui, sx, sy, 38, 38)) {
                // [U-209] what this item's effect did this game
                std::string impact;
                if (me.item_impact[i] > 0) {
                    if (d.resist_fire || d.resist_water || d.resist_air || d.resist_earth) impact = std::format(" — {} {}", tr("prevented"), me.item_impact[i]);
                    else if (d.active == ItemActive::SpiritFlask) impact = std::format(" — {} {} HP", tr("healed"), me.item_impact[i]);
                    else if (d.active != ItemActive::None) impact = std::format(" — {} {}×", tr("used"), me.item_impact[i]);
                }
                const std::string t = std::format("{} ({} g){}{}", d.name, d.cost,
                                                  d.active != ItemActive::None ? std::format(" — {} {}", tr("active, key"), i + 1) : "", impact);
                const float tx = std::min(sx, W - text_w(t) - 12);  // stays on the screen
                fill_rect(r_, tx, sy - 18, text_w(t) + 8, 14, {0, 0, 0, 230});
                text(r_, tx + 4, sy - 15, t, {255, 255, 255});
            }
        }
    }
    auto key_name = [&](Action a) { return std::string(SDL_GetScancodeName(set_.keys[std::size_t(a)])); };
    const float bx = ix + kItemsW + 6;  // two columns of buttons, never on the item slots
    if (icon_button(ui, bx, y0 + 6, 38, "SHOP", key_name(Action::Shop), show_shop_)) show_shop_ = !show_shop_;
    if (icon_button(ui, bx, y0 + 46, 38, "HOME", key_name(Action::Recall), me.channel_left_ms > 0)) net.send_cmd({.kind = CmdKind::Recall});
    if (icon_button(ui, bx, y0 + 86, 38, "PORT", key_name(Action::Portal), false)) net.send_cmd({.kind = CmdKind::Portal});
    if (icon_button(ui, bx + 42, y0 + 6, 38, "SCORE", key_name(Action::Score), show_score_)) show_score_ = !show_score_;
    if (icon_button(ui, bx + 42, y0 + 46, 38, set_.auto_attack ? "AA on" : "AA off", key_name(Action::ToggleAutoAttack), set_.auto_attack))
        on_action(Action::ToggleAutoAttack, false, net);
    if (icon_button(ui, bx + 42, y0 + 86, 38, set_.quick_cast_all ? "QC on" : "QC off", key_name(Action::QuickCastAll), set_.quick_cast_all))  // [U-447]
        on_action(Action::QuickCastAll, false, net);

    // [U-478] switchable features (only when this match has them)
    if (net.features & kFeatGlyph) {
        const std::string gl = me.glyph_active_ms > 0 ? std::format("{:.0f}s", me.glyph_active_ms / 1000.0)
                             : me.glyph_left_ms > 0  ? std::format("{}s", (me.glyph_left_ms + 999) / 1000) : "";
        if (icon_button(ui, bx + 84, y0 + 6, 38, "GLYPH", gl, me.glyph_active_ms > 0) && me.glyph_left_ms == 0) net.send_cmd({.kind = CmdKind::Glyph});
    }
    {
        float ly = above - 16;
        if ((net.features & kFeatCourier) && me.courier_n > 0)
            text(r_, ix, ly, std::format("{}: {} {}, {} {} s", tr("Courier"), me.courier_n, tr("items"), tr("next in"), (me.courier_left_ms + 999) / 1000), {200, 230, 255}), ly -= 14;
        if ((net.features & kFeatNeutralItems) && me.neutral >= 0 && me.neutral < int(kNeutralItems.size()))
            text(r_, ix, ly, std::format("{}: {} (T{})", tr("Neutral"), kNeutralItems[std::size_t(me.neutral)].item.name, kNeutralItems[std::size_t(me.neutral)].tier), {190, 230, 160});
    }
    if ((net.features & kFeatTalents)) {  // a talent to choose: two buttons above the skills
        const auto rit = net.roster.find(net.my_unit);
        if (rit != net.roster.end())
            for (int t = 0; t < 4; ++t)
                if (me.level >= 10 + 5 * t && me.talents[t] == 0) {
                    const ChampionDef& cdef = champions()[std::size_t(rit->second.champ)];
                    const float tx = kx, ty = above - 92;
                    fill_rect(r_, tx - 4, ty - 18, 420, 46, {10, 10, 18, 220});
                    text(r_, tx, ty - 14, std::format("{} {}:", tr("Talent at level"), 10 + 5 * t), {255, 220, 120});
                    for (int side = 0; side < 2; ++side) {
                        const SkillDef::Learned& l = talent_of(cdef, t, side);
                        const std::string lab = std::format("+{} {}", l.base / 100, kStatKeyNames[int(l.key)]);
                        if (button(r_, ui, tx + float(side) * 206, ty, 200, 24, lab)) net.send_cmd({.kind = CmdKind::Talent, .slot = std::uint8_t(t * 2 + side)});
                    }
                    break;  // one at a time
                }
    }
    if (hover_slot >= 0) draw_skill_tooltip(net, hover_slot, kx + float(hover_slot) * sp, above - 54, alt, hover_plus);  // [U-238]
}

// ------------------------------------------------------------------ portraits: allies left, enemies right [U-171]
void GameView::draw_portraits(NetClient& net, Ui& ui) {
    const Snapshot& cur = net.cur;
    const float W = view_.sw;
    int na = 0, ne = 0;
    for (const auto& s : cur.scores) {
        const auto r = net.roster.find(s.id);
        if (r == net.roster.end() || s.id == net.my_unit) continue;
        const bool ally = r->second.team == net.my_team;
        const float sz = 44;
        // [U-430] my allies' portraits: under the info panel of a selected unit (they overlapped its stats)
        const float ally_top = selected_ ? std::max(76.f, info_bottom_ + 10) : 76.f;
        const float x = ally ? 8 : W - sz - 74, y = ally ? ally_top + float(na++) * 60 : 180 + float(ne++) * 60;
        const DrawUnit* du = nullptr;
        for (const DrawUnit& d : draw_)
            if (d.u.id == s.id) du = &d;
        fill_rect(r_, x - 2, y - 2, sz + 72, sz + 8, {0, 0, 0, 150});
        draw_icon(r->second.champ, x, y, sz, !s.alive);
        text(r_, x + 2, y + sz - 10, std::format("{}", s.level), {255, 230, 120});
        if (!s.alive) text(r_, x + 10, y + 16, std::format("{}", std::max(0, s.respawn_ms / 1000)), {255, 90, 90}, 1.5f);
        // health (+ resource) when I can see them; allies always
        if (du && du->u.alive) {
            draw_health_bar(x + sz + 4, y + 4, 62, 7, du->u.hp / 100, du->u.hp_max / 100, du->u.shield, du->u.shield_sub,
                            ally ? Rgba{70, 160, 255} : Rgba{230, 60, 50}, false);
            if (du->u.res_max > 0) fill_rect(r_, x + sz + 4, y + 13, 62 * float(du->u.res) / float(du->u.res_max), 3, {120, 160, 255});
        } else if (s.alive) {
            text(r_, x + sz + 4, y + 4, "unseen", {130, 130, 140});
        }
        // ultimate ready? (allies only — enemy cooldowns are hidden) [U-171]
        if (ally) {
            const bool learned = s.rank[5] > 0;
            const bool ready = learned && s.cd_left[5] == 0;
            fill_circle(r_, x + sz + 12, y + 30, 7, !learned ? Rgba{50, 50, 50} : ready ? Rgba{90, 230, 110} : Rgba{110, 110, 120}, 12);
            text(r_, x + sz + 8, y + 26, "T", {0, 0, 0});
            if (learned && !ready) text(r_, x + sz + 24, y + 26, std::format("{}s", s.cd_left[5] / 1000), {200, 200, 200});
        }
        text(r_, x + sz + 4, y + 40, utf8_cut(r->second.name, 9), {200, 200, 210});
        if (hover(ui, x, y, sz, sz) && ui.clicked) {  // click → camera on them
            ui.clicked = false;
            if (du) center_on(du->x, du->y);
            else hint("not visible");
        }
    }
}

// ------------------------------------------------------------------ info panel of the clicked unit [U-161][U-205][U-238]
// Hovering shows nothing; a left click selects; the panel sits top-left: main part → modifiers → items → stats.
void GameView::draw_tooltip(NetClient& net, Ui& ui) {
    (void)ui;
    const DrawUnit* du = nullptr;
    for (const DrawUnit& d : draw_)
        if (d.u.id == selected_) du = &d;
    if (!du) return;
    const auto& u = du->u;
    const UnitKind k = UnitKind(u.kind);
    const bool ally = u.team == net.my_team;
    const char* side = u.team == 0 ? "Fire Nation" : u.team == 1 ? "Earth Nation" : "Neutral";
    const net::NetScore* sc = nullptr;
    for (const auto& s2 : net.cur.scores)
        if (s2.id == u.id) sc = &s2;
    const float x = 8, w = 360;
    float y = 8;
    std::vector<std::string> L;
    std::string title;
    switch (k) {
        case UnitKind::Champion: {
            const auto r = net.roster.find(u.id);
            title = std::format("{} — {}", r != net.roster.end() ? r->second.name : "?", champions()[std::size_t(std::max<int>(0, u.champ))].name);
            break;
        }
        case UnitKind::Minion: {
            static const char* kM[] = {"melee", "ranged", "siege", "super"};
            title = std::format("{} {} ({})", tr(side), tr("minion"), tr(kM[std::min<int>(u.mtype, 3)]));
            break;
        }
        case UnitKind::Turret: title = std::format("{} {}", tr(side), tr("turret — a fixed wall, cannot be bent")); break;
        case UnitKind::Inhibitor: title = std::format("{} {}", tr(side), tr("inhibitor")); break;
        case UnitKind::Nexus: title = std::format("{} {}", tr(side), tr("Nation Shrine")); break;
        case UnitKind::Monster: case UnitKind::Boss: title = tr(net::kMobNames[std::min<int>(u.name_id, int(std::size(net::kMobNames)) - 1)]); break;
    }
    // main part
    const float h_main = 54;
    fill_rect(r_, x, y, w, h_main, {8, 8, 14, 235});
    rect(r_, x, y, w, h_main, ally ? Rgba{80, 150, 255} : u.team == 2 ? Rgba{220, 170, 70} : Rgba{230, 80, 70});
    if (k == UnitKind::Champion && u.champ >= 0) draw_icon(u.champ, x + 4, y + 4, 46, false);
    const float tx = x + (k == UnitKind::Champion ? 56 : 6);
    text(r_, tx, y + 6, std::format("{}  {} {}", title, tr("lvl"), u.level), {255, 230, 150});
    draw_health_bar(tx, y + 22, w - (tx - x) - 8, 10, u.hp / 100, u.hp_max / 100, u.shield, u.shield_sub, ally ? Rgba{70, 160, 255} : Rgba{230, 60, 50}, true);
    text(r_, tx, y + 36, std::format("{} / {}{}", u.hp / 100, u.hp_max / 100, u.shield ? std::format("  (+{})", u.shield) : ""), {220, 220, 230});
    y += h_main + 2;
    // modifiers [U-237][U-255]: sky → terrain → statuses, each with its name and what it does
    {
        std::vector<ModView> mods;
        if (k == UnitKind::Champion && u.champ >= 0) {
            const Element el = champions()[std::size_t(u.champ)].element;
            const SkyState sky = WorldClock::at_ms(net.start_ms + net.cur.now_ms);
            for (ModView& m : env_modifiers(el, sky))
                if (!m.minor) mods.push_back(std::move(m));
            const int cx = int(du->x) / kCellUnits, cy = int(du->y) / kCellUnits;
            if (Grid::in_bounds(cx, cy))
                for (ModView& m : terrain_modifiers(net.seen.at(cx, cy), el)) mods.push_back(std::move(m));
        }
        status_modifiers(u.status, mods);
        if (!mods.empty()) {
            fill_rect(r_, x, y, w, float(mods.size()) * 14 + 4, {8, 8, 14, 225});
            for (const ModView& m : mods) {
                fill_rect(r_, x + 3, y + 3, 26, 11, mixc(m.colour, {0, 0, 0, 255}, 55));
                rect(r_, x + 3, y + 3, 26, 11, m.environment ? Rgba{255, 210, 90} : m.debuff ? Rgba{230, 70, 70} : Rgba{80, 220, 110});
                text(r_, x + 4, y + 5, m.tag, {255, 255, 255});
                text(r_, x + 34, y + 5, fit_text(m.name + ": " + m.detail, w - 40), m.environment ? Rgba{255, 225, 150} : Rgba{225, 225, 235});
                y += 14;
            }
            y += 6;
        }
    }
    // items + stats
    if (k == UnitKind::Champion && sc) {
        float ix = x;
        for (std::int16_t it : sc->items) {
            if (it >= 0) draw_item_icon(r_, it, ix, y, 24);
            else fill_rect(r_, ix, y, 24, 24, {30, 30, 40, 220});
            ix += 27;
        }
        y += 28;
        L.push_back(std::format("ATK {}  EP {}  DEF {}  EDEF {}  AS {:.2f}  MS {}", sc->attack, sc->ep, sc->def, sc->edef, sc->as100x / 100.0, sc->ms));
        if (expand_stats_) {
            L.push_back(std::format("{} {}  {} {}%", tr("range"), sc->range, tr("size"), sc->size));
            const auto r = net.roster.find(u.id);
            const ChampionDef& c = champions()[std::size_t(std::max<int>(0, u.champ))];
            for (int i = 1; i < kSkillSlots; ++i) {
                const int qid = r != net.roster.end() ? r->second.q : -1;
                const int spec = r == net.roster.end() ? -1 : i == int(Slot::F) ? r->second.f : i == int(Slot::G) ? r->second.g : -1;
                const SkillDef* d = i == 1 ? (qid >= 0 ? &q_options()[std::size_t(qid)] : nullptr)
                                  : i == 2 ? &c.w : i == 3 ? &c.e : i == 4 ? &c.r : i == 5 ? &c.t : i == 6 ? &c.y
                                  : spec >= 0 ? &specials()[std::size_t(spec)] : nullptr;
                if (!d) continue;
                std::string line = std::format("{} {:<18}{} {} {}", kKeys[i], d->name, d->ultimate ? " (" + tr("ult") + ")" : "", tr("rank"), sc->rank[i]);
                if (!d->passive_desc.empty()) line += "  +" + tr("passive");
                if (sc->dmg[i] > 0) line += std::format("  {} {}", tr("dmg"), sc->dmg[i]);
                line += std::format("  cd {} s", sec1(sc->cd_total[i]));
                if (ally && sc->cd_left[i] >= 0)  // timers only for allies [U-161]
                    line += sc->rank[i] == 0 ? "" : sc->cd_left[i] > 0 ? std::format("  {} {} s", tr("ready in"), sec1(sc->cd_left[i])) : "  " + tr("READY");
                L.push_back(line);
            }
        } else {
            L.push_back(tr("C = all stats, skills and cooldowns"));
        }
    } else {
        L.push_back(std::format("{} {}  {} {}  DEF {}  EDEF {}", tr("attack"), u.atk, tr("range"), u.range, u.def, u.edef));
    }
    float lw = w;
    for (const auto& l : L) lw = std::max(lw, text_w(l) + 12);
    fill_rect(r_, x, y, lw, float(L.size()) * 12 + 8, {8, 8, 14, 225});
    for (std::size_t i = 0; i < L.size(); ++i) text(r_, x + 6, y + 5 + float(i) * 12, L[i], {225, 225, 235});
    info_bottom_ = y + float(L.size()) * 12 + 8;  // [U-430] the ally portraits go below this
}

// ------------------------------------------------------------------ gold / XP pop-ups [U-159]
void GameView::draw_rewards(NetClient& net, double now_s) {
    if (!set_.show_rewards) return;
    for (const LocalReward& lr : net.rewards) {
        const float t = float(now_s - lr.born);
        const std::uint8_t a = std::uint8_t(std::clamp(170.f * (1.f - t / 1.6f), 0.f, 170.f));  // transparent
        const float x = view_.sx(float(lr.r.x)), y = view_.sy(float(lr.r.y)) - 20 - 30 * t;
        std::string s;
        if (lr.r.xp > 0) s += std::format("+{}xp ", lr.r.xp);
        if (lr.r.gold > 0) s += std::format("+{}g", lr.r.gold);
        text(r_, x - text_w(s) / 2, y, s, {255, 225, 90, a});
    }
}

}  // namespace em
