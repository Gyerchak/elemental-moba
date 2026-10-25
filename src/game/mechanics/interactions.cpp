// Interactions between skills, elements and terrain — the base rules other content builds on.
#include <algorithm>
#include <format>
#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {

// [X-58] lightning on water: every cell of the connected shallow water
void flood_water(const Grid& g, int sx, int sy, std::vector<std::uint8_t>& mark, std::vector<std::pair<int, int>>& cells) {
    if (!Grid::in_bounds(sx, sy) || g.at(sx, sy).surface() != Surface::Water) return;
    std::vector<std::pair<int, int>> stack{{sx, sy}};
    mark[std::size_t(sy) * kGridSize + std::size_t(sx)] = 1;
    while (!stack.empty() && cells.size() < 6'000) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        cells.push_back({x, y});
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int nx = x + dx, ny = y + dy;
            if (!Grid::in_bounds(nx, ny)) continue;
            auto& m = mark[std::size_t(ny) * kGridSize + std::size_t(nx)];
            if (m || g.at(nx, ny).surface() != Surface::Water) continue;
            m = 1;
            stack.push_back({nx, ny});
        }
    }
}

// Skill vs skill: blocking [U-17][U-20][U-38], combos [U-134][X-72], group bending [U-135][X-73],
// a 2nd earthbender pushing a raised boulder [U-27]. Called for every moving effect, every tick.
void World::effect_interactions(Effect& e, std::size_t i) {
    const Shape me = shape_of(e);
    const Element my = base_element(e.sub);
    for (std::size_t j = 0; j < fx.size() && !e.dead; ++j) {
        if (j == i) continue;
        Effect& o = fx[j];
        if (o.dead || o.kind == FxKind::Steam || o.kind == FxKind::Auto || o.kind == FxKind::TowerShot) continue;
        const Element other = base_element(o.sub);
        const bool o_moving = o.kind == FxKind::Bolt || o.kind == FxKind::Wave;
        const bool o_zone = o.kind == FxKind::Field || o.kind == FxKind::Tornado || o.kind == FxKind::Burst;
        if (!touches(me, shape_of(o))) continue;
        // [U-241] fire meets fire (ally or enemy, different casters): they clash at the middle; the stronger
        // caster (elemental power) keeps part of its fire, the weaker one is gone; equal = both gone
        if (my == Element::Fire && other == Element::Fire && e.owner != o.owner && o_moving &&
            (e.kind == FxKind::Bolt || e.kind == FxKind::Wave)) {
            const std::int64_t ep_me = e.src.elemental_power.total() + e.damage * 100, ep_o = o.src.elemental_power.total() + o.damage * 100;
            Effect& strong = ep_me >= ep_o ? e : o;
            Effect& weak = ep_me >= ep_o ? o : e;
            const std::int32_t left = elements::clash_left_pct(std::max(ep_me, ep_o), std::min(ep_me, ep_o));
            weak.dead = true;
            if (left <= 0) strong.dead = true;
            else {
                strong.radius = std::max(8, strong.radius * left / 100);
                strong.width = std::max(16, strong.width * left / 100);
                strong.damage = strong.damage * left / 100;
            }
            add_cloud(CloudKind::Smoke, (e.ux() + o.ux()) / 2, (e.uy() + o.uy()) / 2, 60, 1'500);
            continue;
        }
        if (o.team != e.team) {
            if (!o_moving || !(e.kind == FxKind::Bolt || e.kind == FxKind::Wave)) continue;
            const BlockResult br = skill_blocks(my, {.sub = o.sub});
            if (br == BlockResult::Countered) {  // [U-17] water vs fire: both gone
                e.dead = true;
                o.dead = true;
            } else if (br == BlockResult::Blocked) {
                o.dead = true;  // earth stops air / fire [U-20]
            }
            continue;
        }
        // allies
        if (o.kind == FxKind::Boulder && o.vx == 0 && o.vy == 0 && my == Element::Earth && e.kind == FxKind::Bolt) {
            // [U-27] a second earthbender pushes the raised stone
            o.dir = e.dir;
            const std::int32_t sp = 1'100 * kSub / std::int32_t(cfg.tick_hz);
            o.vx = e.dir.x * sp / kOne;
            o.vy = e.dir.y * sp / kOne;
            o.end_ms = now_ms + 5'000;
            e.dead = true;
            continue;
        }
        if (!o_zone || !(e.kind == FxKind::Bolt || e.kind == FxKind::Wave || e.kind == FxKind::Tornado)) continue;
        if (e.def && e.def->kind == SkillKind::Dash) continue;
        if (my == other && o.group && e.owner != o.owner && o.feeds < kMaxGroupFeeds) {
            // [U-135] group bending: the zone grows, the feeding skill is absorbed
            ++o.feeds;
            o.radius = o.radius * group_size_pct(o.feeds) / group_size_pct(o.feeds - 1);
            o.end_ms += 1'000;
            if (o.travel_left > 0) o.travel_left += 200 * kSub;
            e.dead = true;
            add_feed(std::format("Group bending: {} grows (x{})", o.def ? o.def->name : "zone", o.feeds + 1));
            continue;
        }
        const Combo cb = combo_of(my, other);
        if (cb == Combo::None || o.combo == cb) continue;
        o.combo = cb;  // [U-134][X-72]
        switch (cb) {
            case Combo::Firestorm:
                e.combo = Combo::Firestorm;  // [U-241] air empowers fire: +25 % size (and +25 % damage on hit)
                e.radius = e.radius * (100 + elements::kAirEmpowersFirePct) / 100;
                o.sub = o.kind == FxKind::Tornado ? SubElement::Fire : o.sub;
                o.flow = Flow::Fire;
                o.light = std::max(o.light, std::int32_t(kFireLightRadius));
                break;
            case Combo::Steam: add_cloud(CloudKind::Steam, o.ux(), o.uy(), o.radius, elements::kSteamMs); break;  // [X-57]
            case Combo::Lava: lava_pool(o.ux(), o.uy(), o.radius * 2 / 3, 8'000); break;  // [X-56]
            case Combo::IceShards:
                e.combo = Combo::IceShards;
                e.sub = SubElement::Ice;
                e.damage = state_change_nerf(e.damage);  // [U-28]
                break;
            case Combo::Mud:
                o.cc = StatusKind::Slow;
                o.cc_ms = 700;
                o.cc_val = 50;
                break;
            case Combo::Sandstorm:
                o.cc = StatusKind::Blind;
                o.cc_ms = 700;
                o.cc_val = 70;
                add_cloud(CloudKind::Sand, o.ux(), o.uy(), o.radius, 2'000);
                break;
            case Combo::None: break;
        }
        add_feed(std::format("Combo: {}", cb == Combo::Firestorm ? "Firestorm"
                                          : cb == Combo::Steam   ? "Steam cloud"
                                          : cb == Combo::Lava    ? "Lava"
                                          : cb == Combo::IceShards ? "Ice shards"
                                          : cb == Combo::Mud     ? "Mud"
                                                                 : "Sandstorm"));
    }
}

}  // namespace em
