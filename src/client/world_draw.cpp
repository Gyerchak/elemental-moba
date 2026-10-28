// Everything drawn IN the world (under the HUD), back to front:
//   map → shop territory → terrain models → clouds → skills → falcon ghosts → units (+ animation cues, bars,
//   modifiers, CC timers, emotes) → paths → pings → sense waves → damage numbers → fog / night → falcons.
#include <algorithm>
#include <cmath>
#include <format>

#include "client/anim.hpp"
#include "client/modifiers.hpp"
#include "client/game_view.hpp"
#include "client/locale.hpp"
#include "game/match/layout.hpp"

namespace em {

using namespace gfx;

namespace {
bool has_status(const net::NetUnit& u, StatusKind k) { return ((u.status >> int(k)) & 1) != 0; }

// [U-223] damage text colour: physical orange, every element its own colour, sweet spot magenta [U-222]
Rgba damage_colour(const net::NetHit& h) {
    if (h.pad & 1) return {255, 80, 220};  // sweet spot (kHitSweet)
    if (h.pad & 2) return {255, 150, 60};  // physical (kHitPhysical)
    switch (SubElement(h.sub)) {
        case SubElement::Lightning: return {255, 240, 90};
        case SubElement::Ice: return {170, 230, 255};
        case SubElement::Lava: return {255, 110, 30};
        case SubElement::Metal: case SubElement::MetalArmor: return {195, 200, 210};
        case SubElement::BlueFire: return {90, 160, 255};
        default: break;
    }
    switch (base_element(SubElement(h.sub))) {
        case Element::Water: return {80, 170, 255};
        case Element::Earth: return {205, 165, 95};
        case Element::Fire: return {255, 95, 45};
        case Element::Air: return {235, 245, 255};
        case Element::None: return {255, 150, 60};
    }
    return {255, 255, 255};
}
void outlined(SDL_Renderer* r, float x, float y, std::string_view s, Rgba c, float scale) {
    const Rgba o{0, 0, 0, c.a};
    for (auto [dx, dy] : {std::pair{-1.f, 0.f}, {1.f, 0.f}, {0.f, -1.f}, {0.f, 1.f}}) text(r, x + dx, y + dy, s, o, scale);
    text(r, x, y, s, c, scale);
}
}  // namespace

// ---------------------------------------------------------------- trees, bushes, walls as simple models [U-202]
void GameView::draw_terrain_models(NetClient& net) {
    const SpriteSheet* tree = view_.hz > 0 ? sprites_.get("tree") : nullptr;  // [U-258]
    const SpriteSheet* bush = view_.hz > 0 ? sprites_.get("bush") : nullptr;
    if (view_.hz <= 0 || !set_.tree_models || view_.zoom < 0.3f) return;
    const int x0 = std::clamp(int(std::min(view_.wx(0), view_.wx(view_.sw)) / kCellUnits) - 2, 0, kGridSize - 1);
    const int x1 = std::clamp(int(std::max(view_.wx(0), view_.wx(view_.sw)) / kCellUnits) + 2, 0, kGridSize - 1);
    const int y0 = std::clamp(int(std::min(view_.wy(0), view_.wy(view_.sh)) / kCellUnits) - 4, 0, kGridSize - 1);
    const int y1 = std::clamp(int(std::max(view_.wy(0), view_.wy(view_.sh)) / kCellUnits) + 2, 0, kGridSize - 1);
    const float z = view_.zoom;
    for (int k = 0; k <= y1 - y0; ++k) {
        const int y = view_.flip ? y1 - k : y0 + k;  // back to front on screen
        for (int x = x0; x <= x1; ++x) {
            const Cell c = net.seen.at(x, y);
            const float wx = float(x * kCellUnits + 16), wy = float(y * kCellUnits + 16);
            const float sx = view_.sx(wx), gy = view_.sy(wy);
            if (c.tree() == Tree::Alive && ((x * 7 + y * 13) % 3 == 0)) {  // a tree on every 3rd tree cell
                if (tree) {  // [U-258] Blender model; size varies a little per cell
                    const float sz = 0.5f + float((x * 31 + y * 17) % 5) * 0.04f;
                    Sprites::draw(r_, *tree, "idle", 0, 0, sx, gy, z * sz, mixc({255, 255, 255, 255}, {200, 230, 190, 255}, (x * 13 + y) % 40));
                } else {
                    fill_rect(r_, sx - 3 * z, view_.sy_h(wy, 50), 6 * z, gy - view_.sy_h(wy, 50), {70, 50, 30, 255});
                    fill_circle(r_, sx, view_.sy_h(wy, 75), 34 * z, {32, 88, 38, 255}, 14);
                    fill_circle(r_, sx - 6 * z, view_.sy_h(wy, 85), 20 * z, {44, 110, 48, 255}, 12);
                }
            } else if (c.bush() == Bush::Alive && ((x + y) % 2 == 0)) {
                if (bush) Sprites::draw(r_, *bush, "idle", 0, 0, sx, gy, z * 0.7f);
                else fill_ellipse(r_, sx, view_.sy_h(wy, 10), 18 * z, 12 * z, {54, 128, 44, 230}, 12);
            } else if (c.walk() == Walk::Blocked && c.surface() == Surface::Stone && c.wall() == Wall::None) {
                // [U-346] a natural rock wall in the jungle: a big rough rock, higher than player walls
                const float top = view_.sy_h(wy, 85 + float((x * 7 + y * 3) % 20));
                fill_rect(r_, sx - 17 * z, top, 34 * z, gy - top, {104, 98, 90, 255});
                fill_rect(r_, sx - 17 * z, top - 7 * z * view_.gy, 34 * z, 14 * z * view_.gy, {146, 140, 128, 255});
                line(r_, sx - 6 * z, top + 6 * z, sx + 4 * z, gy - 6 * z, {80, 76, 70, 200});
            } else if (c.wall() == Wall::Stone) {  // raised block
                const float top = view_.sy_h(wy, 60);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {84, 78, 70, 255});
                fill_rect(r_, sx - 16 * z, top - 6 * z * view_.gy, 32 * z, 12 * z * view_.gy, {120, 112, 100, 255});
            } else if (c.wall() == Wall::Ice) {
                const float top = view_.sy_h(wy, 50);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {190, 235, 250, 170});
            } else if (c.wall() == Wall::Metal) {  // [U-288] steel plates with a shine
                const float top = view_.sy_h(wy, 70);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {120, 126, 138, 255});
                fill_rect(r_, sx - 16 * z, top - 6 * z * view_.gy, 32 * z, 12 * z * view_.gy, {190, 196, 210, 255});
                line(r_, sx - 10 * z, top + 4 * z, sx - 10 * z, gy - 4 * z, {225, 230, 240, 200});
            }
        }
    }
}

// ---------------------------------------------------------------- [U-290] fog fade
// The fog is rebuilt 10× a second; between rebuilds every pixel moves from the old value to the new one, so the
// edge of the fog glides with the champions instead of jumping in steps.
void GameView::update_fog_fade(double now_s) {
    if (fog_fade_from_ < 0 || fog_shown_px_.size() != fog_px_.size()) return;
    const int t = std::clamp(int((now_s - fog_fade_from_) / 0.1 * 256), 0, 256);
    if (t == fog_fade_t_) return;
    fog_fade_t_ = t;
    for (std::size_t i = 0; i < fog_px_.size(); ++i) {
        const std::uint32_t a = fog_from_px_[i], b = fog_px_[i];
        if (a == b) {
            fog_shown_px_[i] = b;
            continue;
        }
        std::uint32_t out = 0;
        for (int sh = 0; sh < 32; sh += 8) {
            const int ca = int((a >> sh) & 255), cb = int((b >> sh) & 255);
            out |= std::uint32_t(ca + (cb - ca) * t / 256) << sh;
        }
        fog_shown_px_[i] = out;
    }
    SDL_UpdateTexture(fog_tex_, nullptr, fog_shown_px_.data(), kGridSize * 4);
}

// ---------------------------------------------------------------- [U-288] cracks on damaged walls
void GameView::draw_wall_damage(NetClient& net) {
    const float z = view_.zoom;
    // [U-341] falling pieces: broken by a hit → chunks fly the hit's way and drop; by time → it crumbles down in place
    for (const auto& a : net.wall_anims) {
        const float t = float(last_frame_ - a.born);
        if (t < 0 || t > 1.1f) continue;
        const float wx = float(a.b.x * kCellUnits + 16), wy = float(a.b.y * kCellUnits + 16);
        const Rgba col = a.b.kind == std::uint8_t(Wall::Ice) ? Rgba{200, 238, 255, 255} : a.b.kind == std::uint8_t(Wall::Metal) ? Rgba{170, 176, 188, 255}
                                                                                       : Rgba{120, 110, 98, 255};
        const std::uint8_t al = std::uint8_t(std::clamp(255.f * (1.1f - t), 0.f, 255.f));
        const bool pushed = a.b.dx != 0 || a.b.dy != 0;
        for (int k = 0; k < 6; ++k) {
            const float spread = float((a.b.x * 13 + a.b.y * 7 + k * 29) % 40 - 20);
            float px = wx, py = wy, h;
            if (pushed) {  // thrown along the hit, a little apart, in an arc
                const float dist = (60.f + float(k) * 12.f) * t;
                px += float(a.b.dx) / 1000.f * dist + float(-a.b.dy) / 1000.f * spread * t;
                py += float(a.b.dy) / 1000.f * dist + float(a.b.dx) / 1000.f * spread * t;
                h = std::max(0.f, 50.f * t * (1.f - t) * 4.f + 30.f * (1.f - t));
            } else {  // crumbling: pieces slide down the wall and spread a bit
                px += spread * 0.6f * t;
                py += float((k * 17) % 20 - 10) * 0.6f * t;
                h = std::max(0.f, 55.f * (1.f - t * 1.4f));
            }
            const float sz = (5.f + float(k % 3) * 2.f) * z;
            fill_rect(r_, view_.sx(px) - sz / 2, view_.sy_h(py, h) - sz / 2, sz, sz, with_alpha(col, al));
        }
        if (!pushed && t < 0.6f)  // dust at the foot
            ground_fill(r_, view_, wx, wy, 20.f + 40.f * t, {150, 140, 125, std::uint8_t(120 * (1.f - t / 0.6f))}, 18);
    }
    for (const auto& wh : net.cur.walls) {
        const float wx = float(wh.x * kCellUnits + 16), wy = float(wh.y * kCellUnits + 16);
        const float sx = view_.sx(wx), gy = view_.sy(wy), top = view_.sy_h(wy, 60);
        const float dmg = float(100 - wh.pct) / 100.f;  // 0 = whole … 1 = about to fall
        fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {20, 20, 20, std::uint8_t(140 * dmg)});
        const int cracks = 1 + int(dmg * 4);
        for (int k = 0; k < cracks; ++k) {  // zig-zag cracks, more with less HP
            const float cx = sx + float((wh.x * 7 + k * 11) % 20 - 10) * z;
            line(r_, cx, top + 2, cx + 5 * z, top + (gy - top) * 0.4f, {10, 10, 10, 230});
            line(r_, cx + 5 * z, top + (gy - top) * 0.4f, cx - 3 * z, gy - 2, {10, 10, 10, 230});
        }
    }
}

// ---------------------------------------------------------------- clouds: steam, smoke, sand, snow [U-241]
void GameView::draw_clouds(NetClient& net) {
    // [U-350] the spirit portals: a column of soft light at both poles, always (like the spirit portals in Korra)
    for (const auto& p : {kPortalCell, mirror(kPortalCell)}) {
        const float wx = float(p.first * kCellUnits), wy = float(p.second * kCellUnits);
        const float sx = view_.sx(wx), gy = view_.sy(wy), top = view_.sy_h(wy, 900);
        const float shimmer = 0.5f + 0.5f * float(std::sin(double(local_tick_) * 0.02));
        fill_rect(r_, sx - 30 * view_.zoom, top, 60 * view_.zoom, gy - top, {170, 210, 255, std::uint8_t(35 + 25 * shimmer)});
        fill_rect(r_, sx - 10 * view_.zoom, top, 20 * view_.zoom, gy - top, {235, 245, 255, std::uint8_t(60 + 40 * shimmer)});
        ground_fill(r_, view_, wx, wy, 120, {200, 230, 255, 50}, 40);
        ground_ring(r_, view_, wx, wy, 120, {220, 240, 255, 160}, 40);
    }
    for (const auto& c : net.cur.clouds) {
        if (c.kind == 6) {  // [U-350] someone is coming through the portal: the beam pulses bright
            const float pulse = 0.5f + 0.5f * float(std::sin(double(local_tick_) * 0.06));
            const float sx = view_.sx(float(c.x)), gy = view_.sy(float(c.y)), top = view_.sy_h(float(c.y), 900);
            fill_rect(r_, sx - 22 * view_.zoom, top, 44 * view_.zoom, gy - top, {200, 230, 255, std::uint8_t(60 + 90 * pulse)});
            ground_ring(r_, view_, float(c.x), float(c.y), 90.f + 20.f * pulse, {230, 245, 255, 220}, 40);
            continue;
        }
        if (c.kind == 5) {  // [U-293] an ice patch: white-blue, with cracks of light
            ground_fill(r_, view_, float(c.x), float(c.y), float(c.radius), {200, 235, 255, 150}, 28);
            ground_ring(r_, view_, float(c.x), float(c.y), float(c.radius), {240, 250, 255, 200}, 28);
            continue;
        }
        if (c.kind == 4) {  // [U-292] a puddle: flat water on the ground with a light shine
            ground_fill(r_, view_, float(c.x), float(c.y), float(c.radius), {70, 140, 210, 110}, 28);
            ground_ring(r_, view_, float(c.x), float(c.y), float(c.radius), {170, 210, 255, 140}, 28);
            ground_fill(r_, view_, float(c.x) - float(c.radius) * 0.3f, float(c.y) - float(c.radius) * 0.2f, float(c.radius) * 0.25f,
                        {220, 240, 255, 70}, 16);
            continue;
        }
        const Rgba col = c.kind == 0 ? Rgba{225, 230, 238, 120} : c.kind == 1 ? Rgba{70, 70, 74, 150}
                       : c.kind == 2 ? Rgba{205, 180, 120, 140} : Rgba{245, 248, 255, 150};
        for (int k = 0; k < 5; ++k) {
            const float a = float(k) * 1.256f + float(local_tick_) * 0.002f;
            ground_fill(r_, view_, float(c.x) + std::cos(a) * float(c.radius) * 0.35f, float(c.y) + std::sin(a) * float(c.radius) * 0.35f,
                        float(c.radius) * 0.7f, col, 18);
        }
    }
}

// ---------------------------------------------------------------- falcon reports: ghosts [U-235]
void GameView::draw_ghosts(NetClient& net) {
    for (const auto& g : net.cur.ghosts) {
        const bool ally = g.team == net.my_team;
        ground_fill(r_, view_, float(g.x), float(g.y), g.kind == std::uint8_t(UnitKind::Minion) ? 14.f : 26.f, {150, 150, 155, 110}, 14);
        if (g.kind == std::uint8_t(UnitKind::Champion) && g.champ >= 0) {
            const float s = 26, sx = view_.sx(float(g.x)) - s / 2, sy = view_.sy_h(float(g.y), 80) - s;
            draw_icon(g.champ, sx, sy, s, true);
            rect(r_, sx - 2, sy - 2, s + 4, s + 4, ally ? Rgba{90, 220, 110} : Rgba{150, 150, 150});  // green ally / grey enemy
        }
    }
}

// ---------------------------------------------------------------- units [U-129][U-130][U-175][U-176][U-226]
// [U-258] which model sprite a unit uses
std::string GameView::model_id(const net::NetUnit& u) const {
    switch (UnitKind(u.kind)) {
        case UnitKind::Champion: return Resources::champion_id(u.champ);
        case UnitKind::Minion: {
            static const char* kM[] = {"minion_melee", "minion_ranged", "minion_siege", "minion_super"};
            return kM[std::min<int>(u.mtype, 3)];
        }
        case UnitKind::Turret: return "turret";
        case UnitKind::Inhibitor: return "inhibitor";
        case UnitKind::Nexus: return "nexus";
        case UnitKind::Monster:
        case UnitKind::Boss: {
            const std::string id = Resources::slug(net::kMobNames[std::min<int>(u.name_id, int(std::size(net::kMobNames)) - 1)]);
            return id == "bandit_archer" ? std::string("bandit") : id;  // (shares the bandit model until it has its own)
        }
    }
    return {};
}

AnimSlot GameView::anim_of(const net::NetUnit& u) const {
    auto has = [&](StatusKind k) { return ((u.status >> int(k)) & 1) != 0; };
    if (!u.alive) return AnimSlot::Death;
    if (u.emote != 255) return AnimSlot::Emote;
    if (has(StatusKind::Frozen)) return AnimSlot::Frozen;
    if (has(StatusKind::Bloodbound)) return AnimSlot::Bloodbent;
    if (has(StatusKind::Stun)) return AnimSlot::Stunned;
    if (has(StatusKind::Airborne)) return AnimSlot::FlyEnemy;
    if (has(StatusKind::Flying)) return AnimSlot::FlySelf;
    if (has(StatusKind::Swimming)) return AnimSlot::Swim;
    if (has(StatusKind::Channel)) return AnimSlot::Recall;
    if (has(StatusKind::Blind)) return AnimSlot::Blinded;
    if (has(StatusKind::Silence) || has(StatusKind::Mute)) return AnimSlot::Silenced;
    if (has(StatusKind::Burn)) return AnimSlot::Burning;
    if (u.casting) return AnimSlot::Cast;
    if (has(StatusKind::Slow)) return AnimSlot::Slowed;
    return AnimSlot::Walk;
}

void GameView::draw_units(NetClient& net, double now_s) {
    const float z = view_.zoom;
    std::vector<const DrawUnit*> order;
    for (const DrawUnit& du : draw_) order.push_back(&du);
    // back to front; equal heights keep a FIXED order (by id) — an unstable sort swapped them every frame (flicker) [U-338]
    std::ranges::sort(order, [&](const DrawUnit* a, const DrawUnit* b) {
        const float ya = view_.sy(a->y), yb = view_.sy(b->y);
        return ya != yb ? ya < yb : a->u.id < b->u.id;
    });
    auto height_of = [](const net::NetUnit& u) {
        switch (UnitKind(u.kind)) {
            case UnitKind::Champion: return 70.f;
            case UnitKind::Minion: return 44.f;
            case UnitKind::Turret: return 190.f;
            case UnitKind::Inhibitor: return 70.f;
            case UnitKind::Nexus: return 150.f;
            case UnitKind::Monster: return u.body == 4 ? 46.f : 60.f;
            case UnitKind::Boss: return 80.f;
        }
        return 50.f;
    };
    const SkyState sky = WorldClock::at_ms(net.start_ms + net.cur.now_ms);
    if (anim_mem_.size() > 3'000) anim_mem_.clear();  // old minions / monsters (ids are never reused)
    for (const LocalHit& h : net.hits)  // [U-258] who just autoattacked → attack animation
        if (h.h.is_auto) {
            AnimMem& am = anim_mem_[h.h.src];
            am.attacked = std::max(am.attacked, h.born);
        }
    for (const DrawUnit* dp : order) {
        const DrawUnit& du = *dp;
        const auto& u = du.u;
        const UnitKind kind = UnitKind(u.kind);
        // [U-258] animation memory for the model sprite (before the "dead" check: the death animation plays 2 s)
        AnimMem& am = anim_mem_[u.id];
        if (u.walk != am.walk) am.walk = u.walk, am.moved = now_s;
        if (am.was_alive && !u.alive) am.died = now_s;
        am.was_alive = u.alive != 0;
        if (u.casting && !am.was_casting) am.cast_from = now_s;
        am.was_casting = u.casting != 0;
        if (!u.alive && kind == UnitKind::Champion && now_s - am.died > 2.0) continue;
        const AnimSlot anim = anim_of(u);
        const Dir face{u.fx, u.fy};
        const Shape body = place_body(body_by_index(u.body), std::int32_t(du.x), std::int32_t(du.y), face, u.walk, u.size_pct);
        Rgba tc = u.alive ? team_colour(u.team) : Rgba{60, 60, 60, 200};
        if (anim == AnimSlot::Frozen) tc = mixc(tc, {200, 240, 255, 255}, 65);
        const Rgba fill = kind == UnitKind::Champion && u.champ >= 0 ? mixc(tc, hex(champions()[std::size_t(u.champ)].colour), 45) : tc;
        float H = height_of(u) * float(u.size_pct) / 100.f;
        float lift = anim == AnimSlot::FlySelf ? 60.f : anim == AnimSlot::FlyEnemy ? 40.f : 0.f;
        if (anim == AnimSlot::Swim) H *= 0.45f;                                                        // half under water
        if (anim == AnimSlot::Emote && u.emote == 2) lift += 8.f * float(std::abs(std::sin(now_s * 9)));  // dance bob
        const Rgba edge = kind == UnitKind::Champion ? (u.id == net.my_unit ? Rgba{255, 230, 80} : Rgba{20, 20, 20}) : Rgba{30, 30, 30, 200};
        const SpriteSheet* sheet = nullptr;
        if (view_.hz > 0) {
            // [U-97][U-258] my champion with my chosen skin: that skin's own model if the pack has one
            if (u.id == net.my_unit && u.champ >= 0) {
                const std::string& skin = set_.prefs(champions()[std::size_t(u.champ)].name).skin;
                if (!skin.empty()) sheet = sprites_.get_dir("champions/" + Resources::champion_id(u.champ) + "/skins/" + skin + "/model");
            }
            if (!sheet) sheet = sprites_.get(model_id(u));
        }
        if (view_.hz > 0) {
            // footprint = the real hitbox on the ground: a grey shadow at the chosen opacity [U-176]
            const std::uint8_t ha = std::uint8_t(std::clamp(set_.hitbox_opacity, 0, 100) * 255 / 100);
            for (int i = 0; i < body.n; ++i) ground_fill(r_, view_, float(body.c[i].x), float(body.c[i].y), float(body.c[i].r), {110, 110, 110, ha}, 18);
            if (u.id == net.my_unit) outline_shape(r_, view_, body, {255, 230, 80, std::uint8_t(std::max<int>(ha, 60))}, 1.f);
            if (anim == AnimSlot::Swim) ground_ring(r_, view_, du.x, du.y, 50 + 10 * float(std::sin(now_s * 4)), {200, 230, 255, 160}, 24);
            if (sheet) {  // [U-258] the Blender model, as a sprite
                std::string an = "idle";
                double t = now_s;
                bool hold = false;
                if (!u.alive) an = "death", t = now_s - am.died, hold = true;
                else if (anim == AnimSlot::Frozen || anim == AnimSlot::Stunned || anim == AnimSlot::Bloodbent) t = 0;  // frozen still
                else if (u.casting) an = "cast", t = now_s - am.cast_from, hold = true;
                else if (now_s - am.attacked < 0.32) an = "attack", t = now_s - am.attacked, hold = true;
                else if (now_s - am.moved < 0.15) {
                    an = "walk";
                    if (auto w = sheet->anims.find("walk"); w != sheet->anims.end())  // frames follow the real steps
                        t = double(u.walk % 1024) / 1024.0 * w->second.n / std::max(1, w->second.fps);
                }
                // [U-260] the look of what happens to it: frozen = iced skin, burning = glowing red, wet / slowed = bluish
                Rgba tint{255, 255, 255, 255};
                if (kind == UnitKind::Minion) tint = mixc(tint, tc, 55);
                if (has_status(u, StatusKind::Wet) || has_status(u, StatusKind::Slow)) tint = mixc(tint, {170, 200, 255, 255}, 35);
                if (anim == AnimSlot::Burning || has_status(u, StatusKind::Burn))
                    tint = mixc(tint, {255, 110, 40, 255}, int(35 + 20 * std::sin(now_s * 14)));
                if (anim == AnimSlot::Frozen) tint = mixc(tint, {150, 215, 255, 255}, 70);
                if (!u.alive) tint = {150, 150, 150, 220};
                float gy = view_.sy_h(du.y, lift);
                if (anim == AnimSlot::Swim) gy += 14 * z;  // half in the water
                Sprites::draw(r_, *sheet, an, t, sprite_dir(float(u.fx), float(u.fy), view_.flip), view_.sx(du.x), gy,
                              z * float(u.size_pct) / 100.f, tint, hold);
                // [U-260] flames on a burning unit
                if (u.alive && has_status(u, StatusKind::Burn))
                    for (int k = 0; k < 7; ++k) {
                        const float ph = float(std::fmod(now_s * 1.6 + k * 0.143, 1.0));
                        const float fx2 = view_.sx(du.x) + float((k * 37) % 21 - 10) * z * 1.6f;
                        const float fy2 = gy - ph * H * z * view_.hz * 1.1f;
                        fill_circle(r_, fx2, fy2, (6 - 4 * ph) * z + 1, {255, std::uint8_t(200 - 150 * ph), 40, std::uint8_t(230 * (1 - ph))}, 8);
                    }
            } else {
                for (int i = 0; i < body.n; ++i) {  // the standing body (old placeholder: cylinders)
                    const float bx = view_.sx(float(body.c[i].x)), gy = view_.sy_h(float(body.c[i].y), lift);
                    const float rr = view_.rx(float(body.c[i].r)), top = gy - H * z * view_.hz;
                    fill_rect(r_, bx - rr, top, rr * 2, gy - top, mixc(fill, {0, 0, 0, 255}, 25));
                    fill_ellipse(r_, bx, gy, rr, view_.ry(float(body.c[i].r)), mixc(fill, {0, 0, 0, 255}, 25), 16);
                    fill_ellipse(r_, bx, top, rr, view_.ry(float(body.c[i].r)), fill, 16);
                }
            }
            if (kind == UnitKind::Champion || kind == UnitKind::Minion || kind == UnitKind::Monster || kind == UnitKind::Boss) {
                const float hr = (kind == UnitKind::Minion ? 9.f : 12.f) * float(u.size_pct) / 100.f;
                const float fwd = (kind == UnitKind::Boss || u.body == 4 ? 40.f : 12.f) * float(u.size_pct) / 100.f;
                const float hx = du.x + float(face.x) * fwd / kOne, hy = du.y + float(face.y) * fwd / kOne;
                const float hy_s = view_.sy_h(hy, lift + H + hr);
                if (!sheet) fill_circle(r_, view_.sx(hx), hy_s, hr * z, mixc(fill, {255, 255, 255, 255}, 35), 14);
                // ---- placeholder animation cues, one per slot [U-226]
                const float hx_s = view_.sx(hx);
                switch (anim) {
                    case AnimSlot::Cast:  // raised hand with a glow in the element's colour
                        fill_circle(r_, hx_s + 14 * z, hy_s - 10 * z, 6 * z, with_alpha(hex(element_colour(champions()[std::size_t(std::max<int>(0, u.champ))].element)), 220), 10);
                        break;
                    case AnimSlot::Stunned:  // stars around the head
                        for (int k = 0; k < 3; ++k) {
                            const float a = float(now_s * 5) + float(k) * 2.09f;
                            fill_circle(r_, hx_s + std::cos(a) * 16 * z, hy_s - 14 * z + std::sin(a) * 4 * z, 3 * z + 1, {255, 240, 90, 255}, 6);
                        }
                        break;
                    case AnimSlot::Frozen:  // ice block
                        fill_rect(r_, hx_s - 24 * z, hy_s - 10 * z, 48 * z, H * z * view_.hz + 20 * z, {190, 235, 255, 110});
                        break;
                    case AnimSlot::Blinded:  // hands over the eyes
                        fill_rect(r_, hx_s - 12 * z, hy_s - 3 * z, 24 * z, 5 * z, {40, 40, 40, 230});
                        break;
                    case AnimSlot::Bloodbent:  // red strings pulling
                        for (int k = -1; k <= 1; ++k) line(r_, hx_s + float(k) * 8 * z, hy_s - 30 * z, hx_s + float(k) * 4 * z, hy_s, {200, 20, 40, 220});
                        break;
                    case AnimSlot::Recall:  // blue column
                        fill_rect(r_, hx_s - 18 * z, hy_s - 80 * z, 36 * z, H * z * view_.hz + 80 * z, {120, 170, 255, 70});
                        break;
                    case AnimSlot::Burning:
                        fill_circle(r_, hx_s - 8 * z, hy_s + 10 * z, 5 * z + 1, {255, 120, 30, 220}, 8);
                        break;
                    case AnimSlot::Slowed:  // heavy feet
                        ground_ring(r_, view_, du.x, du.y, 30, {120, 160, 255, 160}, 16);
                        break;
                    case AnimSlot::Silenced:
                        text(r_, hx_s - 4, hy_s - 26 * z, "...", {200, 120, 255});
                        break;
                    case AnimSlot::Emote:
                        if (u.emote < std::size(kEmoteNames)) {
                            const std::string e = tr(kEmoteNames[u.emote]);
                            fill_rect(r_, hx_s - text_w(e) / 2 - 4, hy_s - 40 * z - 14, text_w(e) + 8, 14, {255, 255, 255, 220});
                            text(r_, hx_s - text_w(e) / 2, hy_s - 40 * z - 11, e, {20, 20, 20});
                        }
                        break;
                    default: break;
                }
            }
        } else {  // developer view: flat circles, full hitboxes [U-198]
            if (lift > 0)
                for (int i = 0; i < body.n; ++i) fill_circle(r_, view_.sx(float(body.c[i].x)) + 6, view_.sy(float(body.c[i].y)) + 10, float(body.c[i].r) * z, {0, 0, 0, 70});
            for (int i = 0; i < body.n; ++i) fill_circle(r_, view_.sx(float(body.c[i].x)), view_.sy(float(body.c[i].y)) - (lift > 0 ? 8 : 0), float(body.c[i].r) * z, fill);
            outline_shape(r_, view_, body, edge, kind == UnitKind::Champion ? 2.f : 1.f);
            if (kind == UnitKind::Champion || kind == UnitKind::Minion || kind == UnitKind::Monster || kind == UnitKind::Boss) {
                const float hr = (kind == UnitKind::Minion ? 6.f : 8.f) * float(u.size_pct) / 100.f;
                const float fwd = (kind == UnitKind::Boss || u.body == 4 ? 40.f : 20.f) * float(u.size_pct) / 100.f;
                fill_circle(r_, view_.sx(du.x + float(face.x) * fwd / kOne), view_.sy(du.y + float(face.y) * fwd / kOne), hr * z, with_alpha(mixc(fill, {255, 255, 255, 255}, 35), 150), 12);
            }
            text(r_, view_.sx(du.x) + 12, view_.sy(du.y) - 4, anim_name(anim), {200, 255, 200, 160});  // dev: the slot name
        }
        if (!u.alive) continue;
        // ---- bars [U-155][U-166]
        const bool is_struct = u.kind >= std::uint8_t(UnitKind::Turret) && u.kind <= std::uint8_t(UnitKind::Nexus);
        const float bw = kind == UnitKind::Champion ? 96 : is_struct ? 110 : kind == UnitKind::Boss ? 120 : 40;
        const float bh = kind == UnitKind::Champion ? 10 : is_struct ? 8 : 5;
        const float bx = view_.sx(du.x) - bw / 2,
                    by = view_.sy_h(du.y, lift + H + (view_.hz > 0 ? 30.f : 0.f)) -
                         float(body_reach(body_by_index(u.body), u.size_pct)) * z * (view_.hz > 0 ? 0.4f : 1.f) - 18;
        const Rgba hc = u.team == net.my_team ? (u.id == net.my_unit ? Rgba{90, 220, 70} : Rgba{70, 160, 255})
                        : u.team == 2 ? Rgba{230, 170, 60} : Rgba{230, 60, 50};
        draw_health_bar(bx, by, bw, bh, u.hp / 100, u.hp_max / 100, u.shield, u.shield_sub, hc, kind == UnitKind::Champion || is_struct || kind == UnitKind::Boss);
        if (kind == UnitKind::Boss || (kind == UnitKind::Monster && u.size_pct > 100)) {  // [U-364] big / epic monsters: the exact HP
            const std::string hs = std::format("{} / {}", u.hp / 100, u.hp_max / 100);
            text(r_, bx + (bw - text_w(hs, 0.8f)) / 2, by - 12, hs, {255, 255, 255}, 0.8f);
        }
        float below = by + bh + 2;
        if (kind == UnitKind::Champion && u.res_max > 0 && u.champ >= 0) {  // resource bar for every champion
            const ResourceKind rk = champions()[std::size_t(u.champ)].resource;
            const Rgba rc = resource_colour(std::uint8_t(rk), SDL_GetTicks());  // [U-268] calm flame / chaotic rage
            fill_rect(r_, bx - 1, below - 1, bw + 2, 6, {0, 0, 0, 220});
            fill_rect(r_, bx, below, bw * float(std::clamp(u.res, 0, u.res_max)) / float(u.res_max), 4, rc);
            below += 6;
        }
        // [U-237][U-255] modifiers under the bars: sky first (gold frame), then terrain, then statuses (red frame =
        // debuff). Tags of 3 letters; what does not fit → "+N". Minions: statuses only.
        {
            std::vector<ModView> mods;
            if (kind == UnitKind::Champion && u.champ >= 0) {
                const Element el = champions()[std::size_t(u.champ)].element;
                for (ModView& m : env_modifiers(el, sky))
                    if (!m.minor) mods.push_back(std::move(m));
                const int cx = int(du.x) / kCellUnits, cy = int(du.y) / kCellUnits;
                if (Grid::in_bounds(cx, cy))
                    for (ModView& m : terrain_modifiers(net.seen.at(cx, cy), el)) mods.push_back(std::move(m));
            }
            status_modifiers(u.status, mods);
            float mx = bx;
            const float chip = 26;
            for (std::size_t i = 0; i < mods.size(); ++i) {
                if (mx + chip > bx + std::max(bw, 3 * chip)) {
                    text(r_, mx + 1, below + 3, std::format("+{}", mods.size() - i), {255, 255, 255});
                    break;
                }
                const ModView& m = mods[i];
                fill_rect(r_, mx, below + 1, chip - 2, 11, mixc(m.colour, {0, 0, 0, 255}, 60));
                rect(r_, mx, below + 1, chip - 2, 11, m.environment ? Rgba{255, 210, 90} : m.debuff ? Rgba{230, 70, 70} : Rgba{80, 220, 110});
                text(r_, mx + 1, below + 3, m.tag, {255, 255, 255});
                mx += chip;
            }
        }
        // [U-193] crowd-control timer above the bar, like LoL
        if (set_.cc_timers && u.cc_kind != 255 && u.cc_left_cs > 0) {
            auto& mx = cc_max_[u.id];
            mx = std::max(mx, u.cc_left_cs);
            const float f = float(u.cc_left_cs) / float(std::max<std::uint16_t>(1, mx));
            fill_rect(r_, bx, by - 9, bw, 5, {0, 0, 0, 220});
            fill_rect(r_, bx, by - 9, bw * f, 5, status_colour(u.cc_kind));
        } else {
            cc_max_.erase(u.id);
        }
        if (kind == UnitKind::Champion) {
            const auto it = net.roster.find(u.id);
            if (set_.show_names && it != net.roster.end()) text(r_, bx, by - 22, std::format("{} {}", u.level, it->second.name), {240, 240, 240});
        }
    }
}

// ---------------------------------------------------------------- paths of me + my allies [U-203]
void GameView::draw_paths(NetClient& net) {
    if (!set_.show_paths) return;
    for (const auto& p : net.cur.paths) {
        const DrawUnit* du = nullptr;
        for (const DrawUnit& d : draw_)
            if (d.u.id == p.unit) du = &d;
        if (!du) continue;
        const Rgba c = p.unit == net.my_unit ? Rgba{255, 255, 255, 130} : Rgba{110, 220, 120, 110};
        float px = du->x, py = du->y;
        for (int i = 0; i < p.n; ++i) {
            const float nx = float(p.pts[i][0] * kCellUnits + 16), ny = float(p.pts[i][1] * kCellUnits + 16);
            line(r_, view_.sx(px), view_.sy(py), view_.sx(nx), view_.sy(ny), c);
            px = nx;
            py = ny;
        }
        ground_ring(r_, view_, px, py, 14, c, 12);
    }
}

// ---------------------------------------------------------------- pings [U-246]
// [U-365] a dead camp shows its respawn time on the ground where it comes back
void GameView::draw_camp_timers(NetClient& net) {
    for (const auto& c : net.cur.camps) {
        if (c.respawn_s == 0) continue;
        const float wx = float(c.x * kCellUnits + 16), wy = float(c.y * kCellUnits + 16);
        const float sx = view_.sx(wx), sy = view_.sy(wy);
        if (sx < -50 || sy < -50 || sx > view_.sw + 50 || sy > view_.sh + 50) continue;
        const std::string t = std::format("{}:{:02}", c.respawn_s / 60, c.respawn_s % 60);
        text(r_, sx - text_w(t, 1.2f) / 2, sy - 8, t, c.epic ? Rgba{255, 210, 80} : Rgba{220, 220, 220}, 1.2f);
    }
}

void GameView::draw_pings(NetClient& net) {
    const std::int64_t now_ms = net.cur.now_ms;
    for (const auto& p : net.cur.pings) {
        const float age = float(now_ms - p.ms) / 1000.f;
        if (age > 4.5f) continue;
        static constexpr Rgba kPingCol[] = {{120, 220, 255}, {255, 70, 70}, {255, 220, 60}, {90, 220, 120}, {255, 150, 60}, {200, 120, 255}};
        const Rgba c = kPingCol[std::min<int>(p.kind, 5)];
        const std::uint8_t a = std::uint8_t(std::clamp(255.f * (1 - age / 4.5f), 0.f, 255.f));
        ground_ring(r_, view_, float(p.x), float(p.y), 30 + 40 * std::fmod(age, 1.f), with_alpha(c, a), 24);
        const std::string name = tr(kPingNames[std::min<int>(p.kind, 5)]);
        outlined(r_, view_.sx(float(p.x)) - text_w(name) / 2, view_.sy(float(p.y)) - 30, name, with_alpha(c, a), 1);
        // sound + chat line once [U-201]
        const auto key = std::pair{p.from, p.ms};
        if (std::ranges::find(pings_seen_, key) == pings_seen_.end()) {
            pings_seen_.push_back(key);
            if (pings_seen_.size() > 64) pings_seen_.erase(pings_seen_.begin());
            audio_.play(res_.sound(std::format("ping_{}", p.kind)), SoundCat::Ui, 0.9f);
        }
    }
}

// ---------------------------------------------------------------- sense waves [U-141..U-143][U-151]
void GameView::draw_waves(NetClient& net, double now_s) {
    const float z = view_.zoom;
    for (const LocalWave& lw : net.waves) {
        const float t = float(now_s - lw.born);
        const float sx = view_.sx(float(lw.w.x)), sy = view_.sy(float(lw.w.y));
        const std::uint8_t a = std::uint8_t(std::clamp(220.f * (1.f - t / 1.5f), 0.f, 220.f));
        switch (WaveKind(lw.w.kind)) {
            case WaveKind::Seismic:
                for (int k = 0; k < 3; ++k) ground_ring(r_, view_, float(lw.w.x), float(lw.w.y), 20 + 90 * t + 18 * float(k), {200, 150, 80, a}, 28);
                break;
            case WaveKind::SeismicBending: {
                const float s = (20 + 70 * t) * z;
                rect(r_, sx - s, sy - s * view_.gy, 2 * s, 2 * s * view_.gy, {230, 200, 110, a});
                break;
            }
            case WaveKind::Heat:
                for (int k = 0; k < 2; ++k) ground_ring(r_, view_, float(lw.w.x), float(lw.w.y), 10 + 70 * t + 22 * float(k), {255, 120, 40, a}, 24);
                break;
            case WaveKind::Sound:
                for (int k = 0; k < 2; ++k) ground_ring(r_, view_, float(lw.w.x), float(lw.w.y), 12 + 50 * t + 14 * float(k), {220, 220, 230, std::uint8_t(a * 2 / 3)}, 20);
                break;
            case WaveKind::SoundLoud:
                for (int k = 0; k < 3; ++k) ground_ring(r_, view_, float(lw.w.x), float(lw.w.y), 20 + 110 * t + 20 * float(k), {255, 250, 200, a}, 28);
                break;
            case WaveKind::Wind: {
                const float fl = view_.flip ? -1.f : 1.f;
                const float dx = fl * float(lw.w.dx) / kOne, dy = fl * view_.gy * float(lw.w.dy) / kOne;
                for (int k = -3; k <= 3; ++k) {
                    const float ang = std::atan2(dy, dx) + float(k) * 0.25f;
                    const float rr = (30 + 60 * t) * z;
                    line(r_, sx + std::cos(ang) * rr, sy + std::sin(ang) * rr, sx + std::cos(ang) * (rr + 8), sy + std::sin(ang) * (rr + 8), {240, 250, 255, a});
                }
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- damage numbers [U-192][U-221..U-223]
void GameView::draw_hits(NetClient& net, double now_s) {
    if (!set_.damage_numbers) return;
    for (const LocalHit& lh : net.hits) {
        const float t = float(now_s - lh.born);
        const DrawUnit* dst = nullptr;
        const DrawUnit* src = nullptr;
        for (const DrawUnit& du : draw_) {
            if (du.u.id == lh.h.dst) dst = &du;
            if (du.u.id == lh.h.src) src = &du;
        }
        if (!dst) continue;
        const float dx = view_.sx(dst->x), dy = view_.sy_h(dst->y, 70);
        if (lh.h.is_auto && src && t < 0.18f) {  // autoattack slash
            const float sx2 = view_.sx(src->x), sy2 = view_.sy_h(src->y, 50);
            for (int k = -1; k <= 1; ++k) line(r_, sx2 + float(k), sy2, dx + float(k), dy, {255, 255, 255, 220});
            fill_circle(r_, dx, dy, 10.f * (1 - t * 4), {255, 255, 255, 180}, 12);
        }
        const bool mine = lh.h.src == net.my_unit, on_me = lh.h.dst == net.my_unit;
        if (!mine && !on_me && dst->u.kind == std::uint8_t(UnitKind::Minion)) continue;  // keep the screen clean
        // big hits are flashier (bigger, bounce) — NOT crits [U-221]
        const bool big = lh.h.amount * 4 >= dst->u.hp_max / 100 || lh.h.amount >= 300;
        const bool sweet = (lh.h.pad & 1) != 0;
        Rgba c = damage_colour(lh.h);
        if (on_me) c = mixc(c, {255, 60, 60, 255}, 40);
        const std::uint8_t al = std::uint8_t(std::clamp(255.f * (1.f - t / 0.9f), 0.f, 255.f));
        const bool dot = (lh.h.pad & 4) != 0;  // [U-254] damage over time (burn): small, drifts right, no bounce
        const float scale = dot ? (mine || on_me ? 1.4f : 1.1f) : (big ? 3.f : mine || on_me ? 2.f : 1.5f) * (t < 0.08f ? 1.3f : 1.f);
        const float side = dot ? 26.f + 30.f * t : float(int(lh.h.dst + lh.h.amount) % 9 - 4) * 4 * t * 10;
        const std::string s = sweet ? std::format("{}!", lh.h.amount) : std::format("{}", lh.h.amount);
        outlined(r_, dx - text_w(s, scale) / 2 + side, dy - 30 - 60 * t + 40 * t * t, s, with_alpha(c, al), scale);
    }
}

// ---------------------------------------------------------------- the whole world
void GameView::draw_world(NetClient& net, double now_s) {
    const Snapshot& cur = net.cur;
    const float z = view_.zoom;
    // map + fog: the visible world rectangle (from the 4 screen corners), clamped to the map
    fill_rect(r_, 0, 0, view_.sw, view_.sh, {12, 30, 70, 255});
    const float wx0 = std::clamp(std::min(view_.wx(0), view_.wx(view_.sw)), 0.f, float(kMapUnits));
    const float wx1 = std::clamp(std::max(view_.wx(0), view_.wx(view_.sw)), 0.f, float(kMapUnits));
    const float wy0 = std::clamp(std::min(view_.wy(0), view_.wy(view_.sh)), 0.f, float(kMapUnits));
    const float wy1 = std::clamp(std::max(view_.wy(0), view_.wy(view_.sh)), 0.f, float(kMapUnits));
    const SDL_FRect src{wx0 / kCellUnits, wy0 / kCellUnits, (wx1 - wx0) / kCellUnits, (wy1 - wy0) / kCellUnits};
    const float dx0 = std::min(view_.sx(wx0), view_.sx(wx1)), dy0 = std::min(view_.sy(wy0), view_.sy(wy1));
    const SDL_FRect d{dx0, dy0, std::abs(view_.sx(wx1) - view_.sx(wx0)), std::abs(view_.sy(wy1) - view_.sy(wy0))};
    auto draw_layer = [&](SDL_Texture* t, float px_per_cell) {
        if (src.w <= 0 || src.h <= 0) return;
        const SDL_FRect s{src.x * px_per_cell, src.y * px_per_cell, src.w * px_per_cell, src.h * px_per_cell};
        if (view_.flip) SDL_RenderTextureRotated(r_, t, &s, &d, 180.0, nullptr, SDL_FLIP_NONE);  // [U-174]
        else SDL_RenderTexture(r_, t, &s, &d);
    };
    draw_layer(map_tex_, float(kGroundPx));
    {  // [U-149] shop territory at my fountain
        const auto [fx_, fy_] = net.my_team == 0 ? kFountainCell : mirror(kFountainCell);
        const float wxs = float(fx_ * kCellUnits + 16), wys = float(fy_ * kCellUnits + 16);
        ground_fill(r_, view_, wxs, wys, float(kShopRange), {255, 215, 90, 28}, 96);
        ground_ring(r_, view_, wxs, wys, float(kShopRange), {255, 215, 90, 170}, 96);
        fill_rect(r_, view_.sx(wxs) - 14, view_.sy(wys) - 14, 28, 28, {200, 160, 60, 255});
        text(r_, view_.sx(wxs) - 16, view_.sy(wys) - 30, tr("SHOP"), {255, 230, 140});
    }
    for (const DrawUnit& du : draw_)  // enemy turret ranges, for my safety
        if (du.u.alive && du.u.kind == std::uint8_t(UnitKind::Turret) && du.u.team != net.my_team)
            ground_ring(r_, view_, du.x, du.y, 680 + 75, {255, 80, 60, 70}, 64);
    draw_terrain_models(net);
    draw_wall_damage(net);  // [U-288]
    draw_clouds(net);
    // skills [U-131]: fluid body + exact edge
    for (const auto& f : cur.fx) {
        const Shape s = fx_shape(FxKind(f.kind), f.x, f.y, f.radius, f.width, f.length, {f.dx, f.dy}, Flow(f.flow), f.seed, local_tick_);
        const SubElement sub = SubElement(f.sub);
        Rgba col = hex(element_colour(base_element(sub)));
        if (sub == SubElement::Lightning) col = hex(0xfff070);
        if (sub == SubElement::Lava) col = hex(0xff4a10);
        if (sub == SubElement::Ice) col = hex(0xb8f0ff);
        if (sub == SubElement::Steam) col = hex(0xe0e8f0);
        if (sub == SubElement::BlueFire) col = hex(0x40a0ff);
        if (sub == SubElement::Metal) col = hex(0xb8c0c8);
        if (f.combo == std::uint8_t(Combo::Firestorm)) col = hex(0xff7a20);
        const FxKind k = FxKind(f.kind);
        if (k == FxKind::Steam) continue;
        if (k == FxKind::Burst && !f.armed) {  // telegraph: exact edge only
            outline_shape(r_, view_, s, with_alpha(col, 230), 2);
            continue;
        }
        const std::uint8_t a = k == FxKind::Field ? 70 : k == FxKind::Tornado ? 90 : k == FxKind::Heal ? 80 : 150;
        fill_shape(r_, view_, s, with_alpha(col, a));
        outline_shape(r_, view_, s, mixc(col, {255, 255, 255, 255}, 40), 1.5f);
        if (f.feeds) text(r_, view_.sx(float(f.x)) - 12, view_.sy(float(f.y)) - 4, std::format("x{}", f.feeds + 1), {255, 255, 255}, 2);
        if (k == FxKind::Lantern) fill_circle(r_, view_.sx(float(f.x)), view_.sy_h(float(f.y), 140), 10 * z + 3, {255, 220, 120, 255});
        if (view_.hz > 0 && (k == FxKind::Bolt || k == FxKind::Auto || k == FxKind::TowerShot || k == FxKind::Boulder))
            fill_circle(r_, view_.sx(float(f.x)), view_.sy_h(float(f.y), k == FxKind::TowerShot ? 110 : 40),
                        std::max(3.f, float(f.radius) * z * 0.7f), with_alpha(mixc(col, {255, 255, 255, 255}, 25), 230), 14);
    }
    draw_ghosts(net);
    draw_units(net, now_s);
    draw_paths(net);
    draw_camp_timers(net);
    draw_pings(net);
    draw_waves(net, now_s);
    draw_hits(net, now_s);
    // fog + night on top [U-75][U-108][U-115][U-117]
    update_fog_fade(now_s);  // [U-290]
    draw_layer(fog_tex_, 1.f);
    draw_rewards(net, now_s);
    for (const auto& f : cur.falcons)  // my team's falcons [U-235]
        fill_circle(r_, view_.sx(float(f.x)), view_.sy_h(float(f.y), 160), 6, {240, 230, 200, 255}, 8),
            text(r_, view_.sx(float(f.x)) + 8, view_.sy_h(float(f.y), 160) - 4, tr("falcon"), {240, 230, 200});
    if (now_s - click_t_ < 0.5) ground_ring(r_, view_, click_x_, click_y_, float(18 * (1 - (now_s - click_t_) * 2)) + 4, {120, 255, 120, 220});
    if (ping_wheel_ && ui_) {  // [U-246] the wheel while dragging
        const float cx = ping_x_, cy = ping_y_;
        circle(r_, cx, cy, 70, {255, 255, 255, 160}, 40);
        const struct { const char* n; float dx, dy; } opts[] = {{"danger", 0, -80}, {"on my way", 80, 0}, {"assist me", 0, 80},
                                                                {"enemy missing", -80, 0}, {"bait", -60, -60}};
        for (const auto& o : opts) outlined(r_, cx + o.dx - text_w(tr(o.n)) / 2, cy + o.dy, tr(o.n), {255, 255, 255, 255}, 1);
        line(r_, cx, cy, ui_->mx, ui_->my, {255, 255, 255, 200});
    }
}

}  // namespace em
