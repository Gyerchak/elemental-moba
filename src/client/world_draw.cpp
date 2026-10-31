// Everything drawn IN the world (under the HUD), back to front:
//   map → shop territory → terrain models → clouds → skills → falcon ghosts → units (+ animation cues, bars,
//   modifiers, CC timers, emotes) → paths → pings → sense waves → damage numbers → fog / night → falcons.
#include <algorithm>
#include <functional>
#include <cmath>
#include <format>

#include "client/anim.hpp"
#include "client/modifiers.hpp"
#include "client/game_view.hpp"
#include "client/locale.hpp"
#include "game/match/layout.hpp"
#include "game/match/maps.hpp"

namespace em {

using namespace gfx;

namespace {
bool has_status(const net::NetUnit& u, StatusKind k) { return ((u.status >> int(k)) & 1) != 0; }

// [U-223] damage text colour: physical orange, every element its own colour, sweet spot magenta [U-222]
Rgba damage_colour(const net::NetHit& h) {
    if (h.pad & 8) return {255, 60, 40};   // critical strike (kHitCrit) [U-463]
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
// [U-389] terrain models (trees, bushes, rocks, walls) are drawn ROW BY ROW interleaved with the units (see
// draw_units): a unit standing behind a tree / wall is now hidden by it, one in front covers it. Rows reach further
// on the side where tall models stand below the screen edge (their tops still show) → no pop-in.
void GameView::terrain_begin(NetClient& net) {
    wall_pct_.clear();  // [U-438] damaged wall cells this frame
    for (const auto& wh : net.cur.walls) wall_pct_[std::uint32_t(wh.y) * kGridSize + std::uint32_t(wh.x)] = wh.pct;
    // [U-443] wall segments: which cells they cover (those are not drawn one by one) and in which row each is drawn
    seg_cells_.clear();
    seg_rows_.clear();
    for (std::size_t i = 0; i < net.cur.segs.size(); ++i) {
        const auto& s = net.cur.segs[i];
        const float ax = float(s.dx) / kOne, ay = float(s.dy) / kOne, hl = float(s.len) / 2 + 16, ht = float(s.thick) / 2 + 16;
        const int r = int((hl + ht) / kCellUnits) + 1, ccx = s.x / kCellUnits, ccy = s.y / kCellUnits;
        for (int y = ccy - r; y <= ccy + r; ++y)
            for (int x = ccx - r; x <= ccx + r; ++x) {
                const float px = float(x * kCellUnits + 16 - s.x), py = float(y * kCellUnits + 16 - s.y);
                if (std::abs(px * ax + py * ay) <= hl && std::abs(-px * ay + py * ax) <= ht) seg_cells_.insert(std::uint32_t(y) * kGridSize + std::uint32_t(x));
            }
        seg_rows_[ccy].push_back(i);
    }
    terr_on_ = view_.hz > 0 && set_.tree_models && view_.zoom >= 0.2f;
    if (!terr_on_) return;
    const int up = int(std::ceil(300.f * view_.hz / std::max(0.1f, view_.gy) / kCellUnits)) + 1;  // tall models
    const int lo_y = int(std::min(view_.wy(0), view_.wy(view_.sh)) / kCellUnits), hi_y = int(std::max(view_.wy(0), view_.wy(view_.sh)) / kCellUnits);
    terr_x0_ = std::clamp(int(std::min(view_.wx(0), view_.wx(view_.sw)) / kCellUnits) - 3, 0, kGridSize - 1);
    terr_x1_ = std::clamp(int(std::max(view_.wx(0), view_.wx(view_.sw)) / kCellUnits) + 3, 0, kGridSize - 1);
    // the screen bottom is high world y (normal) or low world y (flipped): give that side the big margin
    terr_y0_ = std::clamp(lo_y - (view_.flip ? up : 2), 0, kGridSize - 1);
    terr_y1_ = std::clamp(hi_y + (view_.flip ? 2 : up), 0, kGridSize - 1);
    terr_k_ = 0;
}

// [U-443] one wall segment as a rotated block: four sides (far ones first) and the top, at its real angle
void GameView::draw_segment(const net::NetWallSeg& s) {
    const float ax = float(s.dx) / kOne, ay = float(s.dy) / kOne, sx_ = -ay, sy_ = ax;
    const float hl = float(s.len) / 2, ht = float(s.thick) / 2;
    const float h = s.kind == std::uint8_t(Wall::Ice) ? 30.f + 18.f * s.level : s.kind == std::uint8_t(Wall::Metal) ? 36.f + 20.f * s.level
                                                                                                                      : 32.f + 22.f * s.level;
    const float gx[4] = {float(s.x) - ax * hl - sx_ * ht, float(s.x) + ax * hl - sx_ * ht, float(s.x) + ax * hl + sx_ * ht, float(s.x) - ax * hl + sx_ * ht};
    const float gyw[4] = {float(s.y) - ay * hl - sy_ * ht, float(s.y) + ay * hl - sy_ * ht, float(s.y) + ay * hl + sy_ * ht, float(s.y) - ay * hl + sy_ * ht};
    Rgba side{84, 78, 70, 255}, top{122, 114, 102, 255};
    if (s.kind == std::uint8_t(Wall::Ice)) side = {150, 205, 235, 190}, top = {205, 238, 252, 210};
    if (s.kind == std::uint8_t(Wall::Metal)) side = {110, 116, 128, 255}, top = {185, 191, 205, 255};
    auto quad = [&](SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FPoint d, Rgba col) {
        for (const SDL_FPoint& q : {a, b, c, d})  // [U-467] no screen-wide smear from a bad corner
            if (!std::isfinite(q.x) || !std::isfinite(q.y) || std::abs(q.x) > 60'000.f || std::abs(q.y) > 60'000.f) return;
        const SDL_FColor fc{col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f};
        const SDL_Vertex v[4] = {{a, fc, {}}, {b, fc, {}}, {c, fc, {}}, {d, fc, {}}};
        const int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(r_, nullptr, v, 4, idx, 6);
    };
    SDL_FPoint g[4], t[4];
    for (int k = 0; k < 4; ++k) g[k] = {view_.sx(gx[k]), view_.sy(gyw[k])}, t[k] = {view_.sx(gx[k]), view_.sy_h(gyw[k], h)};
    int order[4] = {0, 1, 2, 3};  // sides sorted far → near (by their screen depth)
    std::sort(order, order + 4, [&](int a, int b) { return g[a].y + g[(a + 1) % 4].y < g[b].y + g[(b + 1) % 4].y; });
    for (int k : order) {
        const int n = (k + 1) % 4;
        const float shade = 0.75f + 0.25f * std::abs(g[n].x - g[k].x) / std::max(1.f, std::hypot(g[n].x - g[k].x, g[n].y - g[k].y));
        quad(g[k], g[n], t[n], t[k], {std::uint8_t(side.r * shade), std::uint8_t(side.g * shade), std::uint8_t(side.b * shade), side.a});
    }
    quad(t[0], t[1], t[2], t[3], top);
    // [U-438] its HP state: cracks on the top and the near side
    const int lost = 100 - s.pct, state = lost * 3 < 100 ? 0 : lost * 3 < 200 ? 1 : 2;
    for (int k = 0; k < (state == 0 ? 0 : state == 1 ? 2 : 5); ++k) {
        const float f = 0.15f + 0.17f * float(k);
        const SDL_FPoint p0{t[0].x + (t[1].x - t[0].x) * f, t[0].y + (t[1].y - t[0].y) * f}, p1{t[3].x + (t[2].x - t[3].x) * (f + 0.06f), t[3].y + (t[2].y - t[3].y) * (f + 0.06f)};
        line(r_, p0.x, p0.y, (p0.x + p1.x) / 2 + 3, (p0.y + p1.y) / 2, {15, 15, 15, 230});
        line(r_, (p0.x + p1.x) / 2 + 3, (p0.y + p1.y) / 2, p1.x, p1.y, {15, 15, 15, 230});
    }
}

void GameView::terrain_until(NetClient& net, float screen_y) {
    if (!terr_on_) return;
    for (; terr_k_ <= terr_y1_ - terr_y0_; ++terr_k_) {
        const int y = view_.flip ? terr_y1_ - terr_k_ : terr_y0_ + terr_k_;  // back to front on screen
        if (view_.sy(float(y * kCellUnits + 16)) > screen_y) return;
        draw_terrain_row(net, y);
        if (const auto it = seg_rows_.find(y); it != seg_rows_.end())  // [U-443] the segments centred on this row
            for (std::size_t i : it->second) draw_segment(net.cur.segs[i]);
    }
}

void GameView::draw_terrain_row(NetClient& net, int y) {
    const SpriteSheet* tree = sprites_.get("tree");  // [U-258]
    const SpriteSheet* bush = sprites_.get("bush");
    const float z = view_.zoom;
    const int x0 = terr_x0_, x1 = terr_x1_;
    const bool trees = z >= 0.3f;  // far out: only the rocks / walls (no sudden switch for everything) [U-389]
        for (int x = x0; x <= x1; ++x) {
            const Cell c = net.seen.at(x, y);
            const float wx = float(x * kCellUnits + 16), wy = float(y * kCellUnits + 16);
            const float sx = view_.sx(wx), gy = view_.sy(wy);
            if (trees && c.tree() == Tree::Alive && ((x * 7 + y * 13) % 3 == 0)) {  // a tree on every 3rd tree cell
                if (tree) {  // [U-258] Blender model; size varies a little per cell
                    const float sz = 0.5f + float((x * 31 + y * 17) % 5) * 0.04f;
                    Sprites::draw(r_, *tree, "idle", 0, 0, sx, gy, z * sz, mixc({255, 255, 255, 255}, {200, 230, 190, 255}, (x * 13 + y) % 40));
                } else {
                    fill_rect(r_, sx - 3 * z, view_.sy_h(wy, 50), 6 * z, gy - view_.sy_h(wy, 50), {70, 50, 30, 255});
                    fill_circle(r_, sx, view_.sy_h(wy, 75), 34 * z, {32, 88, 38, 255}, 14);
                    fill_circle(r_, sx - 6 * z, view_.sy_h(wy, 85), 20 * z, {44, 110, 48, 255}, 12);
                }
            } else if (trees && c.bush() == Bush::Alive && ((x + y) % 2 == 0)) {
                if (bush) Sprites::draw(r_, *bush, "idle", 0, 0, sx, gy, z * 0.7f);
                else fill_ellipse(r_, sx, view_.sy_h(wy, 10), 18 * z, 12 * z, {54, 128, 44, 230}, 12);
            } else if (c.wall() != Wall::None && seg_cells_.contains(std::uint32_t(y) * kGridSize + std::uint32_t(x))) {
                // [U-443] part of a wall segment: drawn as one rotated block (draw_segment), not cell by cell
            } else if (c.walk() == Walk::Blocked && c.surface() == Surface::Stone && c.elevation() >= 1) {
                // [U-443][U-346] a natural map rock: a rounded boulder of a few lumps (size / shade vary per cell),
                // so outcrops read as natural rock, not as square blocks
                const std::uint32_t hsh = std::uint32_t(x) * 73'856'093u ^ std::uint32_t(y) * 19'349'663u;
                const float hgt = 70 + float(hsh % 30), rr = (17 + float(hsh / 7 % 6)) * z;
                const float top = view_.sy_h(wy, hgt);
                fill_ellipse(r_, sx, gy - 2 * z, rr * 1.15f, rr * 0.55f * view_.gy, {58, 54, 50, 200}, 14);           // foot shadow
                fill_ellipse(r_, sx, (top + gy) / 2, rr * 1.05f, (gy - top) / 2 + 3 * z, {98, 92, 84, 255}, 16);       // body
                fill_ellipse(r_, sx - rr * 0.25f, top + (gy - top) * 0.3f, rr * 0.7f, (gy - top) * 0.3f, {126, 119, 108, 255}, 14);  // light face
                fill_ellipse(r_, sx + rr * 0.35f, top + (gy - top) * 0.55f, rr * 0.45f, (gy - top) * 0.25f, {84, 79, 72, 255}, 12);   // dark lump
                fill_ellipse(r_, sx - rr * 0.35f, top + 4 * z, rr * 0.35f, 4 * z * view_.gy + 1, {156, 150, 138, 255}, 10);           // highlight
                draw_wall_cracks(x, y, top);
            } else if (c.wall() == Wall::Stone) {  // raised block
                const float top = view_.sy_h(wy, 60);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {84, 78, 70, 255});
                fill_rect(r_, sx - 16 * z, top - 6 * z * view_.gy, 32 * z, 12 * z * view_.gy, {120, 112, 100, 255});
                draw_wall_cracks(x, y, top);
            } else if (c.wall() == Wall::Ice) {
                const float top = view_.sy_h(wy, 50);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {190, 235, 250, 170});
                draw_wall_cracks(x, y, top);
            } else if (c.wall() == Wall::Metal) {  // [U-288] steel plates with a shine
                const float top = view_.sy_h(wy, 70);
                fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {120, 126, 138, 255});
                fill_rect(r_, sx - 16 * z, top - 6 * z * view_.gy, 32 * z, 12 * z * view_.gy, {190, 196, 210, 255});
                line(r_, sx - 10 * z, top + 4 * z, sx - 10 * z, gy - 4 * z, {225, 230, 240, 200});
                draw_wall_cracks(x, y, top);
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
        // [U-389] only the alpha fades; the colour is the visible side's (a fade from "clear" 0,0,0,0 used to pass
        // through a dark wrong colour → a faint dark halo at the fog edge)
        const int aa = int(a >> 24), ab = int(b >> 24);
        const std::uint32_t rgb = (ab > 0 ? b : a) & 0x00ffffffu;
        fog_shown_px_[i] = rgb | std::uint32_t(aa + (ab - aa) * t / 256) << 24;
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
}

// [U-379][U-438] the crack look of one damaged wall cell, drawn right ON the wall (inside its terrain row: drawn
// before, the wall itself covered it). Three states: untouched (< 1/3 lost) · mildly cracked · almost torn apart.
void GameView::draw_wall_cracks(int x, int y, float top) {
    const auto it = wall_pct_.find(std::uint32_t(y) * kGridSize + std::uint32_t(x));
    if (it == wall_pct_.end()) return;
    const float z = view_.zoom;
    const float sx = view_.sx(float(x * kCellUnits + 16)), gy = view_.sy(float(y * kCellUnits + 16));
    const int lost = 100 - it->second;
    const int state = lost * 3 < 100 ? 0 : lost * 3 < 200 ? 1 : 2;
    if (state == 0) return;
    fill_rect(r_, sx - 16 * z, top, 32 * z, gy - top, {20, 20, 20, std::uint8_t(state == 1 ? 40 : 120)});
    const int cracks = state == 1 ? 2 : 5;
    for (int k = 0; k < cracks; ++k) {
        const float cx = sx + float((x * 7 + k * 11) % 20 - 10) * z;
        line(r_, cx, top + 2, cx + 5 * z, top + (gy - top) * 0.4f, {10, 10, 10, 230});
        line(r_, cx + 5 * z, top + (gy - top) * 0.4f, cx - 3 * z, gy - 2, {10, 10, 10, 230});
    }
    if (state == 2)  // chunks already missing at the top edge
        for (int k = 0; k < 3; ++k) fill_rect(r_, sx + float((y * 5 + k * 9) % 24 - 12) * z, top - 1, 6 * z, 7 * z, {0, 0, 0, 160});
}

// ---------------------------------------------------------------- clouds: steam, smoke, sand, snow [U-241]
void GameView::draw_clouds(NetClient& net) {
    // [U-350] the spirit portals: a column of soft light at both poles, always (like the spirit portals in Korra)
    for (const auto& p : {cur_map().portal, mirror(cur_map().portal)}) {
        if (cur_map().portal.first < 0) break;  // [U-420] a map without portals
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
    if (has(StatusKind::Silence) || has(StatusKind::Mute) || has(StatusKind::Disarm) || has(StatusKind::Subdue)) return AnimSlot::Silenced;
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
    if (fx_born_.size() > 2'000) fx_born_.clear();  // [U-399]
    for (const LocalHit& h : net.hits)  // [U-258] who just autoattacked → attack animation
        if (h.h.is_auto) {
            AnimMem& am = anim_mem_[h.h.src];
            if (h.born > am.attacked) ++am.attack_n, am.crit = (h.h.pad & 8) != 0;  // [U-486] a new swing
            am.attacked = std::max(am.attacked, h.born);
        }
    std::vector<std::function<void()>> deferred;  // [U-389]
    for (const DrawUnit* dp : order) {
        const DrawUnit& du = *dp;
        terrain_until(net, view_.sy(du.y));  // [U-389] every terrain row behind this unit first
        const auto& u = du.u;
        const UnitKind kind = UnitKind(u.kind);
        // [U-258] animation memory for the model sprite (before the "dead" check: the death animation plays 2 s)
        AnimMem& am = anim_mem_[u.id];
        if (u.walk != am.walk) am.walk = u.walk, am.moved = now_s;
        if (am.was_alive && !u.alive) am.died = now_s;
        am.was_alive = u.alive != 0;
        if (u.casting && !am.was_casting) am.cast_from = now_s;
        if (auto& es = emote_seen_[u.id]; u.emote != es) {  // [U-414] a new emote: its own sound (sounds/emote_<name>.wav)
            es = u.emote;
            if (u.emote < std::size(kEmoteNames))
                audio_.play(res_.sound("emote_" + std::string(kEmoteNames[u.emote])), SoundCat::Ui, 0.8f);  // sounds/emote_<name>.wav
            else if (u.emote >= net::kOwnEmoteSlot0 && u.emote < net::kOwnEmoteSlot0 + net::kOwnEmotes) {  // [U-414] its own sound
                const int oi = u.emote - net::kOwnEmoteSlot0;
                const Sound* snd = nullptr;
                if (u.id == net.my_unit) {
                    if (oi < int(my_emotes_.size())) snd = res_.sound_from_wav(0xE0000000u + std::uint32_t(6 + oi), my_emotes_[std::size_t(oi)].wav);
                } else if (set_.show_other_emotes) {
                    if (auto it = net.own_emotes.find(u.id); it != net.own_emotes.end())
                        snd = res_.sound_from_wav(u.id * 8u + std::uint32_t(oi), it->second[std::size_t(oi)].second);
                }
                if (snd) audio_.play(snd, SoundCat::Ui, 0.8f);
            }
        }
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
        if (u.riding && std::ranges::any_of(comp_draw_, [&](const net::NetCompanion& c) { return c.owner == u.id && c.carrying; }))
            lift += 45.f;  // [U-407] on the companion's back (not while only waiting for it [U-429])
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
                // [U-486] the animation KEY for what the unit does (client/anim.hpp); the sheet falls back / remaps it
                std::string an = "idle";
                double t = now_s;
                bool hold = false;
                const bool form = (u.anim_flags & 1) != 0, dashing = (u.anim_flags & 2) != 0, hasted = (u.anim_flags & 4) != 0;
                if (!u.alive) an = "death", t = now_s - am.died, hold = true;
                else if (u.emote != 255 && u.emote < std::size(kEmoteNames)) an = "emote_" + std::string(kEmoteNames[u.emote]);
                else if (anim == AnimSlot::Frozen || anim == AnimSlot::Stunned || anim == AnimSlot::Bloodbent) {
                    an = anim == AnimSlot::Frozen ? "frozen" : anim == AnimSlot::Stunned ? "stunned" : "bloodbent";
                    t = 0;  // frozen still
                } else if (anim == AnimSlot::FlyEnemy) an = "airborne";
                else if (anim == AnimSlot::Recall) an = "recall", t = now_s - am.cast_from;
                else if (u.casting) {
                    constexpr const char* kSlotKey[] = {"p", "q", "w", "e", "r", "t", "y", "f", "g"};
                    const int sl = std::min<int>(u.casting - 1, int(std::size(kSlotKey)) - 1);
                    an = std::string("cast_") + kSlotKey[std::max(0, sl)];
                    const ChampionDef* cdp = u.champ >= 0 && u.champ < int(champions().size()) ? &champions()[std::size_t(u.champ)] : nullptr;
                    const SkillDef* sd = cdp && sl >= 0 ? (sl == 0 ? &cdp->p : sl == 2 ? &cdp->w : sl == 3 ? &cdp->e : sl == 4 ? &cdp->r : sl == 5 ? &cdp->t : sl == 6 ? &cdp->y : nullptr) : nullptr;
                    if (sd && sd->casts > 1) an += std::to_string(u.cast_stage + 1);  // cast_w1 / cast_w2 / cast_w3
                    t = now_s - am.cast_from, hold = true;
                } else if (now_s - am.attacked < 0.32) {
                    an = am.crit ? "crit" : am.attack_n % 2 ? "attack2" : "attack";  // alternating swings, crits their own
                    t = now_s - am.attacked, hold = true;
                } else if (dashing) an = "dash";
                else if (now_s - am.moved < 0.15) {
                    an = hasted ? "run" : "walk";
                    if (const SpriteSheet::Anim* w = sheet->find(form ? "form_" + an : an))  // frames follow the real steps
                        t = double(u.walk % 1024) / 1024.0 * w->n / std::max(1, w->fps);
                } else if (anim == AnimSlot::Silenced) an = "silenced";
                else if (anim == AnimSlot::Burning) an = "burning";
                else if (anim == AnimSlot::Swim) an = "swim";
                if (form && u.alive) an = "form_" + an;  // transformed: form_walk, form_attack … (falls back to the normal one)
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
            if (u.alive) {  // [U-396] status animations (every status that changes the body gets a visible cue)
                const float sx0 = view_.sx(du.x), gy0 = view_.sy(du.y);
                const float rb = float(body_reach(body_by_index(u.body), u.size_pct));
                const float ph = float(std::fmod(now_s, 1.0));
                if (has_status(u, StatusKind::WaterRide)) {  // a column of water carries the rider: rising rings + spray
                    const float top = view_.sy_h(du.y, lift);
                    fill_rect(r_, sx0 - 16 * z, top, 32 * z, gy0 - top, {70, 150, 240, 150});
                    for (int k = 0; k < 4; ++k) {
                        const float f = float(std::fmod(now_s * 1.5 + k * 0.25, 1.0));
                        fill_ellipse(r_, sx0, gy0 - (gy0 - top) * f, 18 * z * (1.2f - f * 0.4f), 5 * z, {190, 230, 255, std::uint8_t(200 * (1 - f))}, 14);
                    }
                    ground_ring(r_, view_, du.x, du.y, rb + 18 + 10 * ph, {150, 210, 255, std::uint8_t(200 * (1 - ph))}, 24);
                }
                if (has_status(u, StatusKind::Shield)) {  // a shimmering bubble
                    const std::uint8_t a = std::uint8_t(70 + 40 * std::sin(now_s * 4));
                    fill_ellipse(r_, sx0, view_.sy_h(du.y, lift + H * 0.5f), (rb + 14) * z, (H * 0.5f + 16) * z * view_.hz + 6 * z, {200, 230, 255, a}, 24);
                }
                if (has_status(u, StatusKind::Haste))  // wind streaks behind the body
                    for (int k = 0; k < 3; ++k) {
                        const float bx = sx0 - float(u.fx) / kOne * (20 + 12 * float(k)) * z, by = view_.sy_h(du.y - float(u.fy) / kOne * (20 + 12 * float(k)), lift + 10 + 12 * float(k));
                        line(r_, bx, by, bx - float(u.fx) / kOne * 22 * z, by - float(u.fy) / kOne * 6 * z, {230, 240, 255, std::uint8_t(180 - 50 * k)});
                    }
                if (has_status(u, StatusKind::Root))  // stone / vines grip the feet
                    for (int k = 0; k < 6; ++k) {
                        const float a = float(k) * 1.047f;
                        const float px = du.x + std::cos(a) * rb, py = du.y + std::sin(a) * rb;
                        line(r_, view_.sx(px), view_.sy(py), view_.sx(px), view_.sy_h(py, 18), {110, 140, 70, 230});
                    }
                if (has_status(u, StatusKind::Redirect))  // crackling sparks around the hands
                    for (int k = 0; k < 4; ++k) {
                        const float a = float(now_s * 9) + float(k) * 1.57f;
                        line(r_, sx0, view_.sy_h(du.y, lift + H * 0.6f), sx0 + std::cos(a) * 22 * z, view_.sy_h(du.y, lift + H * 0.6f) + std::sin(a) * 10 * z,
                             {170, 210, 255, 230});
                    }
                if (has_status(u, StatusKind::Wet))  // drops falling off
                    for (int k = 0; k < 3; ++k) {
                        const float f = float(std::fmod(now_s * 1.2 + k * 0.33, 1.0));
                        fill_circle(r_, sx0 + float(k * 9 - 9) * z, view_.sy_h(du.y, lift + H * (1 - f)), 2 * z + 1, {120, 180, 255, std::uint8_t(220 * (1 - f))}, 6);
                    }
                if (has_status(u, StatusKind::Airborne))  // dust where it was thrown up
                    ground_fill(r_, view_, du.x, du.y, rb + 10, {150, 130, 100, 70}, 18);
                // [U-399] an earthbender lifts the rock out of the ground first: it rises in front during the cast
                if (u.casting && kind == UnitKind::Champion && u.champ >= 0 &&
                    champions()[std::size_t(u.champ)].element == Element::Earth) {
                    const SubElement csub = champions()[std::size_t(u.champ)].sub;
                    const float t = float(now_s - am.cast_from), up = std::min(1.f, t / 0.35f);
                    const float rx = du.x + float(u.fx) / kOne * (rb + 22), ry = du.y + float(u.fy) / kOne * (rb + 22);
                    const Rgba rc = csub == SubElement::Lava ? Rgba{230, 110, 40, 255} : csub == SubElement::Metal ? Rgba{165, 170, 182, 255} : Rgba{125, 105, 85, 255};
                    if (t < 0.45f) ground_fill(r_, view_, rx, ry, 14 + 26 * up, {120, 100, 80, std::uint8_t(140 * (1 - up))}, 14);  // the ground breaks
                    const float rr = 10 * z * (0.6f + 0.4f * up);
                    const float ry_s = view_.sy_h(ry, -10 + (H * 0.55f + 20) * up);
                    fill_circle(r_, view_.sx(rx), ry_s, rr, rc, 10);
                    fill_circle(r_, view_.sx(rx) - rr * 0.3f, ry_s - rr * 0.3f, rr * 0.45f, mixc(rc, {255, 255, 255, 255}, 25), 8);
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
                        if (u.emote >= net::kOwnEmoteSlot0 && u.emote < net::kOwnEmoteSlot0 + net::kOwnEmotes) {  // [U-414] an own emote
                            const int oi = u.emote - net::kOwnEmoteSlot0;
                            SDL_Texture* pic = nullptr;
                            if (u.id == net.my_unit) {
                                if (oi < int(my_emotes_.size())) pic = res_.texture_from_bmp(0xE0000000u + std::uint32_t(6 + oi), my_emotes_[std::size_t(oi)].png);
                            } else if (set_.show_other_emotes) {
                                if (auto it = net.own_emotes.find(u.id); it != net.own_emotes.end() && !it->second[std::size_t(oi)].first.empty())
                                    pic = res_.texture_from_bmp(u.id * 8u + std::uint32_t(oi), it->second[std::size_t(oi)].first);
                            }
                            if (pic) {
                                const float s2 = 56 * std::max(0.6f, z);
                                const SDL_FRect d{std::round(hx_s - s2 / 2), std::round(hy_s - 40 * z - s2), s2, s2};
                                SDL_RenderTexture(r_, pic, nullptr, &d);
                            }
                        } else if (u.emote < std::size(kEmoteNames)) {
                            // [U-414] the emote's picture from the resource pack (emotes/<name>/emote.png; a pack can reskin it),
                            // else its name in a bubble
                            const std::string id(kEmoteNames[u.emote]);
                            if (SDL_Texture* pic = res_.texture("emotes/" + id + "/emote.png")) {
                                const float s2 = 56 * std::max(0.6f, z);
                                const SDL_FRect d{std::round(hx_s - s2 / 2), std::round(hy_s - 40 * z - s2), s2, s2};
                                SDL_RenderTexture(r_, pic, nullptr, &d);
                            } else {
                                const std::string e = tr(id);
                                fill_rect(r_, hx_s - text_w(e) / 2 - 4, hy_s - 40 * z - 14, text_w(e) + 8, 14, {255, 255, 255, 220});
                                text(r_, hx_s - text_w(e) / 2, hy_s - 40 * z - 11, e, {20, 20, 20});
                            }
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
        deferred.push_back([=, this, &net] {  // [U-389] bars / names / CC timers after ALL sprites and terrain rows
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
            text(r_, bx + (bw - text_w(hs, 0.7f)) / 2, by + std::max(0.f, (bh - 7) / 2), hs, {255, 255, 255}, 0.7f);  // inside the bar [U-389]
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
            auto& last = cc_last_[u.id];  // [U-389] a new CC (other kind / more time) restarts the bar
            if (u.cc_kind != last.first || u.cc_left_cs > last.second) mx = u.cc_left_cs;
            last = {u.cc_kind, u.cc_left_cs};
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
        });
    }
    terrain_until(net, 1e9f);  // the rows in front of every unit
    for (auto& f : deferred) f();
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
// [U-407] travel companions: a simple drawn animal per kind (placeholder until the models are made); the rider
// stands on its back while it carries them home
void GameView::draw_companions(NetClient& net, double now_s) {
    const float z = view_.zoom;
    struct KeepPrev {  // the owners seen this frame become "seen before" for the next frame
        GameView& g;
        ~KeepPrev() {
            g.comp_prev_.clear();
            for (const auto& c : g.comp_draw_) g.comp_prev_.push_back(c.owner);
        }
    } keep{*this};
    for (const auto& c : comp_draw_) {
        const float wx = float(c.x), wy = float(c.y);
        const float sx = view_.sx(wx), gy = view_.sy(wy);
        if (sx < -200 || gy < -200 || sx > view_.sw + 200 || gy > view_.sh + 200) continue;
        const float bob = float(std::sin(now_s * 10)) * 3 * z;
        const CompanionKind k = CompanionKind(std::min<int>(c.kind, int(CompanionKind::Count) - 1));
        if (std::ranges::find(comp_prev_, c.owner) == comp_prev_.end())  // [U-407] its call when it shows up (sounds/companion_<kind>.wav)
            audio_.play(res_.sound(std::format("companion_{}", int(k))), SoundCat::Effects, 0.8f);
        const bool flies = k == CompanionKind::SkyBison || k == CompanionKind::Dragon;
        const float h = flies ? 70.f : 0.f, top = view_.sy_h(wy, h) + bob;
        ground_fill(r_, view_, wx, wy, flies ? 60.f : 40.f, {0, 0, 0, 70}, 18);  // shadow
        // the model (tools/art/make_models.py: companion_<name>) — the drawn shapes below are the fallback
        static const char* kModel[] = {"companion_sky_bison", "companion_polar_bear_dog", "companion_monkey_cat", "companion_shirshu",
                                       "companion_badgermole", "companion_dragon"};
        if (const SpriteSheet* sh = view_.hz > 0 ? sprites_.get(kModel[std::size_t(k)]) : nullptr) {
            // [U-389] the facing changes only when it really moved (it stood still = keep the last direction)
            auto [lx, fresh] = companion_last_x_.try_emplace(c.owner, float(c.x));
            auto [ly, fresh2] = companion_last_y_.try_emplace(c.owner, float(c.y));
            const float ddx = float(c.x) - lx->second, ddy = float(c.y) - ly->second;
            int& dir = companion_dir_[c.owner];
            if (ddx * ddx + ddy * ddy > 0.25f) dir = sprite_dir(ddx, ddy, view_.flip), lx->second = float(c.x), ly->second = float(c.y);
            Sprites::draw(r_, *sh, c.carrying || ddx * ddx + ddy * ddy > 0.25f ? "walk" : "idle", now_s, dir, sx, view_.sy_h(wy, h), z,
                          {255, 255, 255, 255}, false);
            continue;
        }
        switch (k) {
            case CompanionKind::SkyBison:  // big white body, brown arrow, six legs
                fill_ellipse(r_, sx, top, 58 * z, 30 * z, {245, 240, 230, 255}, 24);
                fill_ellipse(r_, sx + 46 * z, top - 8 * z, 20 * z, 16 * z, {240, 235, 225, 255}, 16);
                fill_rect(r_, sx - 30 * z, top - 26 * z, 70 * z, 6 * z, {150, 100, 60, 255});
                for (int l = 0; l < 3; ++l) fill_rect(r_, sx - 30 * z + float(l) * 26 * z, top + 20 * z, 8 * z, 12 * z, {230, 225, 215, 255});
                break;
            case CompanionKind::PolarBearDog:
                fill_ellipse(r_, sx, top - 22 * z, 40 * z, 22 * z, {250, 250, 252, 255}, 20);
                fill_ellipse(r_, sx + 34 * z, top - 34 * z, 15 * z, 13 * z, {250, 250, 252, 255}, 14);
                for (int l = 0; l < 4; ++l) fill_rect(r_, sx - 28 * z + float(l) * 17 * z, top - 6 * z, 7 * z, 10 * z, {235, 235, 240, 255});
                break;
            case CompanionKind::MonkeyCat:
                fill_ellipse(r_, sx, top - 16 * z, 22 * z, 14 * z, {200, 140, 70, 255}, 16);
                fill_circle(r_, sx + 18 * z, top - 26 * z, 10 * z, {210, 150, 80, 255}, 12);
                line(r_, sx - 20 * z, top - 18 * z, sx - 36 * z, top - 36 * z + bob, {190, 130, 60, 255});
                break;
            case CompanionKind::Shirshu:  // long low body, no eyes, a long tongue
                fill_ellipse(r_, sx, top - 16 * z, 46 * z, 15 * z, {120, 105, 140, 255}, 20);
                fill_ellipse(r_, sx + 42 * z, top - 18 * z, 13 * z, 10 * z, {135, 120, 150, 255}, 14);
                line(r_, sx + 54 * z, top - 16 * z, sx + 68 * z, top - 12 * z + bob, {220, 90, 120, 255});
                break;
            case CompanionKind::Badgermole:  // digs: a mound of earth moving with it
                fill_ellipse(r_, sx, top - 10 * z, 40 * z, 18 * z, {110, 90, 70, 255}, 20);
                fill_ellipse(r_, sx + 30 * z, top - 18 * z, 14 * z, 11 * z, {90, 75, 60, 255}, 14);
                for (int d = 0; d < 4; ++d) fill_circle(r_, sx - 30 * z - float(d) * 10 * z, gy - float(d % 2) * 4 * z, 4 * z, {130, 110, 85, 200}, 6);
                break;
            default:  // Dragon: long red body with gold, wings
                fill_ellipse(r_, sx, top, 54 * z, 14 * z, {190, 40, 30, 255}, 22);
                fill_ellipse(r_, sx + 50 * z, top - 6 * z, 15 * z, 11 * z, {200, 50, 35, 255}, 14);
                line(r_, sx - 10 * z, top, sx - 30 * z, top - 40 * z + bob * 3, {230, 180, 60, 255});
                line(r_, sx + 10 * z, top, sx - 4 * z, top - 42 * z + bob * 3, {230, 180, 60, 255});
                break;
        }
        if (!c.carrying) {  // coming: the name over it
            const std::string n = tr(kCompanionNames[std::size_t(k)]);
            text(r_, sx - text_w(n, 0.8f) / 2, top - 50 * z, n, {230, 230, 230}, 0.8f);
        }
    }
}

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
        const bool crit = (lh.h.pad & 8) != 0;  // [U-463] crits: bigger, "!!"
        const std::string s = crit ? std::format("{}!!", lh.h.amount) : sweet ? std::format("{}!", lh.h.amount) : std::format("{}", lh.h.amount);
        if (crit) {
            outlined(r_, dx - text_w(s, scale * 1.3f) / 2 + side, dy - 30 - 60 * t + 40 * t * t, s, with_alpha(c, al), scale * 1.3f);
            continue;
        }
        outlined(r_, dx - text_w(s, scale) / 2 + side, dy - 30 - 60 * t + 40 * t * t, s, with_alpha(c, al), scale);
    }
}

// ---------------------------------------------------------------- [U-459] the programmable skill look
// Every skill effect is drawn on its EXACT hitbox (fx_shape = the server's shape); SkillDef::vis adds steps on top of
// it: glow (soft edge rings), trail (where it was), particles (sparks inside the hitbox), flash (a ring at the start
// and where it ends) and mark (a ground mark it leaves for mark_ms). 0 everywhere = the default for its shape.
void GameView::draw_fx_look(const net::NetFx& f, const Shape& s, std::uint32_t col, double now_s) {
    const FxKind k = FxKind(f.kind);
    const Rgba c = hex(col);
    const float rad = float(std::max<int>({f.radius, f.width / 2, 8}));
    const std::uint8_t trail = f.vtrail ? f.vtrail : k == FxKind::Bolt || k == FxKind::Boulder ? 5 : 0;
    const std::uint8_t flash = f.vflash ? f.vflash : k == FxKind::Burst || k == FxKind::Bolt ? 1 : 0;
    auto [it, fresh] = fx_track_.try_emplace(f.id);
    FxTrack& t = it->second;
    if (fresh && flash) fx_marks_.push_back({float(f.x), float(f.y), rad * 0.8f, now_s, 0.25, col, true});
    if (fresh || std::hypot(float(f.x) - t.x, float(f.y) - t.y) > 6) {  // a new trail point every few units of travel
        std::move_backward(t.hist.begin(), t.hist.end() - 1, t.hist.end());
        t.hist[0] = {float(f.x), float(f.y)};
        t.nh = std::uint8_t(std::min<int>(t.nh + 1, int(t.hist.size())));
    }
    t.x = float(f.x), t.y = float(f.y), t.r = rad, t.col = col, t.flash = flash, t.mark_ms = f.vmark_ms, t.seen = now_s;
    for (int i = 1; i < std::min<int>(t.nh, trail + 1); ++i) {  // the trail: shrinking, fading copies behind it
        const float q = 1.f - float(i) / float(trail + 1);
        ground_fill(r_, view_, t.hist[std::size_t(i)].first, t.hist[std::size_t(i)].second, rad * (0.4f + 0.5f * q), with_alpha(c, std::uint8_t(110 * q)), 14);
    }
    if (f.vglow)
        for (int g = 1; g <= 3; ++g)
            outline_shape(r_, view_, s, with_alpha(mixc(c, {255, 255, 255, 255}, 30), std::uint8_t(f.vglow / (g + 1))), 2.f + float(g * f.vglow) / 60.f);
    for (int i = 0; i < f.vpart; ++i) {  // sparks: deterministic per effect + time (no state), inside the hitbox circle
        const std::uint32_t h = (f.seed * 2654435761u) ^ std::uint32_t(i * 40503) ^ std::uint32_t(now_s * 12) * 97u;
        const float ang = float(h % 6283) / 1000.f, d = rad * float((h >> 13) % 100) / 100.f;
        const float life = float(std::fmod(now_s * 12, 1.0));
        fill_circle(r_, view_.sx(float(f.x) + std::cos(ang) * d), view_.sy_h(float(f.y) + std::sin(ang) * d, 10 + 30 * life), 2.f + view_.zoom,
                    with_alpha(mixc(c, {255, 255, 255, 255}, 50), std::uint8_t(230 * (1 - life))), 6);
    }
}

void GameView::draw_fx_marks(double now_s) {
    std::erase_if(fx_marks_, [&](const FxMark& m) { return now_s - m.t0 > m.dur; });
    if (fx_marks_.size() > 400) fx_marks_.erase(fx_marks_.begin(), fx_marks_.end() - 400);
    for (const FxMark& m : fx_marks_) {
        const float q = float((now_s - m.t0) / m.dur);
        const Rgba c = hex(m.col);
        if (m.flash) {
            ground_ring(r_, view_, m.x, m.y, m.r * (0.6f + 0.9f * q), with_alpha(mixc(c, {255, 255, 255, 255}, 45), std::uint8_t(230 * (1 - q))), 32);
            ground_fill(r_, view_, m.x, m.y, m.r * (0.5f + 0.5f * q), with_alpha(c, std::uint8_t(90 * (1 - q))), 20);
        } else {
            ground_fill(r_, view_, m.x, m.y, m.r, with_alpha(mixc(c, {0, 0, 0, 255}, 55), std::uint8_t(120 * (1 - q))), 20);
        }
    }
}

// [U-456] a chain: a line from the caster to the target, the circle it must leave, and how long it has left
void GameView::draw_tethers(NetClient& net, double /*now_s*/) {
    for (const net::NetTether& t : net.cur.tethers) {
        const Rgba c = t.team == net.my_team ? Rgba{120, 190, 255, 255} : Rgba{255, 90, 70, 255};
        const float q = t.total_ms > 0 ? std::clamp(float(t.left_ms) / float(t.total_ms), 0.f, 1.f) : 0.f;
        ground_ring(r_, view_, float(t.ax), float(t.ay), float(t.radius), with_alpha(c, 200), 64);
        ground_fill(r_, view_, float(t.ax), float(t.ay), float(t.radius) * (1 - q), with_alpha(c, 45), 48);  // fills up until it pulls
        const float x0 = view_.sx(float(t.sx)), y0 = view_.sy_h(float(t.sy), 40), x1 = view_.sx(float(t.dx)), y1 = view_.sy_h(float(t.dy), 40);
        for (int i = 0; i <= 10; ++i) {  // chain links
            const float u = float(i) / 10.f;
            fill_circle(r_, x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, 2.5f + view_.zoom, with_alpha(mixc(c, {40, 40, 40, 255}, i % 2 ? 40 : 0), 230), 6);
        }
        const std::string s = std::format("{:.1f}", std::max(0, t.left_ms) / 1000.0);
        outlined(r_, x1 - text_w(s) / 2, y1 - 30, s, c, 1);
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
        if (!std::isfinite(d.x) || !std::isfinite(d.y) || !std::isfinite(d.w) || !std::isfinite(d.h)) return;  // [U-467]
        const SDL_FRect s{src.x * px_per_cell, src.y * px_per_cell, src.w * px_per_cell, src.h * px_per_cell};
        if (view_.flip) SDL_RenderTextureRotated(r_, t, &s, &d, 180.0, nullptr, SDL_FLIP_NONE);  // [U-174]
        else SDL_RenderTexture(r_, t, &s, &d);
    };
    draw_layer(map_tex_, float(kGroundPx));
    {  // [U-149] shop territory at my fountain
        const auto [fx_, fy_] = net.my_team == 0 ? cur_map().fountain : mirror(cur_map().fountain);
        const float wxs = float(fx_ * kCellUnits + 16), wys = float(fy_ * kCellUnits + 16);
        ground_fill(r_, view_, wxs, wys, float(kShopRange), {255, 215, 90, 28}, 96);
        ground_ring(r_, view_, wxs, wys, float(kShopRange), {255, 215, 90, 170}, 96);
        fill_rect(r_, view_.sx(wxs) - 14, view_.sy(wys) - 14, 28, 28, {200, 160, 60, 255});
        text(r_, view_.sx(wxs) - 16, view_.sy(wys) - 30, tr("SHOP"), {255, 230, 140});
    }
    for (const DrawUnit& du : draw_)  // enemy turret ranges, for my safety
        if (du.u.alive && du.u.kind == std::uint8_t(UnitKind::Turret) && du.u.team != net.my_team)
            ground_ring(r_, view_, du.x, du.y, 680 + 75, {255, 80, 60, 70}, 64);
    terrain_begin(net);  // [U-389] the terrain rows are drawn between the units (draw_units); cracks with them [U-438]
    draw_clouds(net);
    draw_fx_marks(now_s);  // [U-459] scorch marks / impact rings of skills that ended
    // skills [U-131]: fluid body + exact edge
    for (const auto& f : fx_draw_) {
        const Shape s = fx_shape(FxKind(f.kind), f.x, f.y, f.radius, f.width, f.length, {f.dx, f.dy}, Flow(f.flow), f.seed, local_tick_, f.geo, f.inner);
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
        if (f.vcol) col = hex(f.vcol);  // [U-459] the skill's own colour
        if (k == FxKind::Burst && !f.armed) {  // telegraph: exact edge only (+ where the sweet spot will be [U-461])
            outline_shape(r_, view_, s, with_alpha(col, 230), 2);
            if (f.spct > 0)
                if (const Shape sw = sweet_shape(k, Geo(f.geo), SweetZone(f.szone), f.ssize, f.x, f.y, f.radius, f.width, f.length, {f.dx, f.dy}); !sw.empty())
                    outline_shape(r_, view_, sw, {255, 120, 230, 220}, 1.5f);
            continue;
        }
        const std::uint8_t a = f.vfill ? f.vfill : k == FxKind::Field ? 70 : k == FxKind::Tornado ? 90 : k == FxKind::Heal ? 80 : 150;
        draw_fx_look(f, s, std::uint32_t(col.r) << 16 | std::uint32_t(col.g) << 8 | col.b, now_s);  // [U-459] glow / trail / sparks: under the body
        fill_shape(r_, view_, s, with_alpha(col, a));
        outline_shape(r_, view_, s, mixc(col, {255, 255, 255, 255}, 40), 1.5f);
        if (f.inner > 0) ground_ring(r_, view_, float(f.x), float(f.y), float(f.inner), {0, 0, 0, 120}, 40);  // [U-462] the hole
        if (f.spct > 0) {  // [U-461] the sweet spot of this skill, on its real hitbox
            const Shape sw = sweet_shape(k, Geo(f.geo), SweetZone(f.szone), f.ssize, f.x, f.y, f.radius, f.width, f.length, {f.dx, f.dy});
            if (!sw.empty()) {
                fill_shape(r_, view_, sw, {255, 80, 220, 70});
                outline_shape(r_, view_, sw, {255, 120, 230, 220}, 1.5f);
            }
        }
        if (f.feeds) text(r_, view_.sx(float(f.x)) - 12, view_.sy(float(f.y)) - 4, std::format("x{}", f.feeds + 1), {255, 255, 255}, 2);
        if (k == FxKind::Lantern) fill_circle(r_, view_.sx(float(f.x)), view_.sy_h(float(f.y), 140), 10 * z + 3, {255, 220, 120, 255});
        if (view_.hz > 0 && (k == FxKind::Bolt || k == FxKind::Auto || k == FxKind::TowerShot || k == FxKind::Boulder)) {
            float h = k == FxKind::TowerShot ? 110.f : 40.f;
            if (k == FxKind::Boulder) {  // [U-399] the boulder is pulled out of the ground (0.4 s), with broken earth around
                auto [it, fresh] = fx_born_.try_emplace(f.id, now_s);
                const float up = std::min(1.f, float(now_s - it->second) / 0.4f);
                h = -20.f + 60.f * up;
                if (up < 1.f) ground_fill(r_, view_, float(f.x), float(f.y), float(f.radius) * (1.3f - 0.3f * up), {110, 90, 70, std::uint8_t(160 * (1 - up))}, 18);
            }
            if (k == FxKind::Boulder) {  // [U-406] a lifted stone: shadow on the ground, a lumpy rock above it
                const float rr = std::max(4.f, float(f.radius) * z), bx = view_.sx(float(f.x)), by = view_.sy_h(float(f.y), h);
                ground_fill(r_, view_, float(f.x), float(f.y), float(f.radius) * 0.9f, {0, 0, 0, 80}, 16);
                const Rgba stone = f.sub == std::uint8_t(SubElement::Lava) ? Rgba{120, 60, 40, 255} : f.sub == std::uint8_t(SubElement::Metal) ? Rgba{150, 155, 165, 255}
                                                                                                                                              : Rgba{118, 100, 82, 255};
                fill_circle(r_, bx, by, rr, mixc(stone, {0, 0, 0, 255}, 25), 12);
                fill_circle(r_, bx - rr * 0.35f, by - rr * 0.2f, rr * 0.7f, stone, 10);
                fill_circle(r_, bx + rr * 0.3f, by - rr * 0.35f, rr * 0.5f, mixc(stone, {255, 255, 255, 255}, 18), 9);
                fill_circle(r_, bx - rr * 0.45f, by - rr * 0.5f, rr * 0.22f, mixc(stone, {255, 255, 255, 255}, 40), 6);
            } else {
                fill_circle(r_, view_.sx(float(f.x)), view_.sy_h(float(f.y), h), std::max(3.f, float(f.radius) * z * 0.7f),
                            with_alpha(mixc(col, {255, 255, 255, 255}, 25), 230), 14);
            }
        }
    }
    for (auto it = fx_track_.begin(); it != fx_track_.end();)  // [U-459] an effect that vanished: its impact flash + mark
        if (it->second.seen < now_s - 0.001) {
            const FxTrack& t = it->second;
            if (t.flash) fx_marks_.push_back({t.x, t.y, t.r, now_s, 0.3, t.col, true});
            if (t.mark_ms) fx_marks_.push_back({t.x, t.y, t.r, now_s, t.mark_ms / 1000.0, t.col, false});
            it = fx_track_.erase(it);
        } else {
            ++it;
        }
    draw_tethers(net, now_s);
    draw_ghosts(net);
    draw_companions(net, now_s);  // [U-407] under the riders
    draw_units(net, now_s);
    draw_wall_damage(net);  // [U-288][U-341] falling pieces fly over everything
    draw_paths(net);
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
    if (ping_wheel_ && ui_) {  // [U-246][U-414] the wheel while dragging: icons (icons/pings/<kind>.png), the chosen one lit
        const float cx = ping_x_, cy = ping_y_;
        fill_circle(r_, cx, cy, 110, {0, 0, 0, 110}, 40);
        circle(r_, cx, cy, 70, {255, 255, 255, 160}, 40);
        // same sectors as the release code: up danger, right on my way, down assist, left missing, up-left bait
        const float dx = ui_->mx - cx, dy = ui_->my - cy;
        int pick = 0;
        if (dx * dx + dy * dy > 30 * 30) {
            const float a = std::atan2(dy, dx);
            pick = a < -2.36f ? 2 : a < -1.96f ? 5 : a < -0.78f ? 1 : a < 0.78f ? 3 : a < 2.36f ? 4 : 2;
        }
        static constexpr Rgba kCol[] = {{120, 220, 255}, {255, 70, 70}, {255, 220, 60}, {90, 220, 120}, {255, 150, 60}, {200, 120, 255}};
        const struct { int k; float dx, dy; } opts[] = {{1, 0, -80}, {3, 80, 0}, {4, 0, 80}, {2, -80, 0}, {5, -60, -60}};
        for (const auto& o : opts) {
            const bool lit = pick == o.k;
            const float ox = cx + o.dx, oy = cy + o.dy;
            fill_circle(r_, ox, oy, lit ? 24.f : 19.f, with_alpha(kCol[o.k], lit ? 230 : 140), 20);
            if (SDL_Texture* ic = res_.texture(std::format("icons/pings/{}.png", o.k))) {
                const SDL_FRect d{ox - 16, oy - 16, 32, 32};
                SDL_RenderTexture(r_, ic, nullptr, &d);
            }
            const std::string n = tr(kPingNames[std::size_t(o.k)]);
            outlined(r_, ox - text_w(n) / 2, oy + 26, n, {255, 255, 255, 255}, 1);
        }
        line(r_, cx, cy, ui_->mx, ui_->my, {255, 255, 255, 200});
    }
    if (emote_wheel_ && ui_) {  // [U-414] the emote wheel: 6 emotes, pictures from the resource pack
        const float cx = ping_x_, cy = ping_y_;
        fill_circle(r_, cx, cy, 120, {0, 0, 0, 110}, 40);
        const float dx = ui_->mx - cx, dy = ui_->my - cy;
        const int n = wheel_count();
        const float step = 6.2831853f / float(n);
        const int pick = dx * dx + dy * dy > 30 * 30 ? (int(std::lround(std::atan2(dx, -dy) / step)) % n + n) % n : -1;
        for (int k = 0; k < n; ++k) {
            const float a = float(k) * step, ox = cx + std::sin(a) * 85, oy = cy - std::cos(a) * 85;
            const bool lit = pick == k;
            fill_circle(r_, ox, oy, lit ? 30.f : 24.f, lit ? Rgba{255, 230, 120, 230} : Rgba{60, 60, 80, 200}, 20);
            if (k >= 6) {  // [U-414] my own emotes after the official ones
                const OwnEmote& oe = my_emotes_[std::size_t(k - 6)];
                if (SDL_Texture* pic = res_.texture_from_bmp(0xE0000000u + std::uint32_t(k), oe.png)) {
                    const SDL_FRect d{ox - 20, oy - 20, 40, 40};
                    SDL_RenderTexture(r_, pic, nullptr, &d);
                } else {
                    outlined(r_, ox - text_w(oe.name) / 2, oy - 5, oe.name, {255, 255, 255, 255}, 1);
                }
                continue;
            }
            const std::string id(kEmoteNames[std::size_t(k)]);
            if (SDL_Texture* pic = res_.texture("emotes/" + id + "/icon.png")) {
                const SDL_FRect d{ox - 20, oy - 20, 40, 40};
                SDL_RenderTexture(r_, pic, nullptr, &d);
            } else {
                outlined(r_, ox - text_w(tr(id)) / 2, oy - 5, tr(id), {255, 255, 255, 255}, 1);
            }
        }
        line(r_, cx, cy, ui_->mx, ui_->my, {255, 255, 255, 200});
    }
}

}  // namespace em
