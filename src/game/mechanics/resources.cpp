// Health / resource regeneration [U-26][U-60..U-64][U-81][U-138][U-139][X-89].
#include <algorithm>
#include "game/match/layout.hpp"
#include "game/world.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mechanics/element_rules.hpp"

namespace em {

namespace {
// Amount for this tick of a per-second rate, exact over time (no rounding drift).
std::int64_t per_tick(std::int64_t per_s, std::uint64_t tick, std::uint32_t hz) {
    return per_s * std::int64_t(tick) / hz - per_s * std::int64_t(tick - 1) / hz;
}
}  // namespace

void World::regen(Unit& u) {
    if (u.kind != UnitKind::Champion) return;
    const std::int32_t max_hp = u.stats.health.total();
    std::int64_t hp_rate = 600 + u.level * 60 + u.stats.hp_regen;  // hundredths / s [X-11]; [U-476] + HP regen (Strength, items)
    const auto [fx_, fy_] = fountain(u.team);
    if (dist2(u.ux(), u.uy(), fx_, fy_) < 700LL * 700) hp_rate += max_hp / 8;  // fountain [X-12]
    if (u.flask_until > now_ms) {  // [U-81] intense flat heal; [U-209] counted on the flask
        hp_rate += 3'000;
        for (std::size_t sl = 0; sl < u.items.size(); ++sl)
            if (u.items[sl] >= 0 && kItems[std::size_t(u.items[sl])].active == ItemActive::SpiritFlask && tick % cfg.tick_hz == 0)
                u.item_impact[sl] += 30;  // 30 HP per second
    }
    if (!u.st.has(StatusKind::Frozen) || u.untargetable_until <= now_ms) u.hp = std::min<std::int64_t>(max_hp, u.hp + lol_healing(u, per_tick(hp_rate, tick, cfg.tick_hz)));  // [U-481] regen is healing too
    // [U-81] revive: 5 × 5 % max HP, one per second
    if (u.flask_revive > 0 && now_ms >= u.flask_next_ms) {
        heal(u, max_hp * 5 / 100, &u);  // (the flask's revive: my item heals me)
        --u.flask_revive;
        u.flask_next_ms = now_ms + 1'000;
    }
    if (u.kind == UnitKind::Champion) passive_proc(u, nullptr, 3);
    if (!u.courier.empty()) dota_courier_step(u);  // [U-478]  // [U-416] passives on a timer (e.g. a heal every N s)
    // [U-339] the Avatar's small water pool: refills slowly, fast next to water, full on pole ice
    if (u.water_max > 0) {
        const int big = big_water_near(u);
        const Surface under = grid.at(u.ux() / kCellUnits, u.uy() / kCellUnits).surface();
        const std::int64_t wr = under == Surface::Glacier ? u.water_max : elements::kAvatarWaterRegen * (big ? 6 : 1);
        u.water = std::int32_t(std::min<std::int64_t>(u.water_max, u.water + per_tick(wr, tick, cfg.tick_hz)));
    }
    // resources
    std::int64_t r = u.res_regen;
    if (u.jbuff == std::uint8_t(JungleBuff::Wisdom) && u.jbuff_until > now_ms) r = r * (100 + lol_buff(&u, kWisdomRegenPct - 100)) / 100;  // [U-366][U-484]
    switch (u.stats.resource) {
        case ResourceKind::Fuel:
            if (unlimited_fuel(sky)) {  // [U-138]
                u.res = u.res_max;
                return;
            }
            r = r * fuel_regen_pct(sky) / 100;  // [U-139] sun height
            break;
        case ResourceKind::Rage:  // [U-268] drains after 5 s without dealing or taking damage; the comet keeps it full
            if (unlimited_fuel(sky)) {
                u.res = u.res_max;
                return;
            }
            r = (now_ms - u.last_hurt_ms > kRageCombatMs && now_ms - u.last_victim_ms > kRageCombatMs) ? -u.res_regen : 0;
            if (r < 0) {
                u.res = std::int32_t(std::max<std::int64_t>(0, u.res + per_tick(r, tick, cfg.tick_hz)));
                return;
            }
            break;
        case ResourceKind::Water: {
            // [U-26][U-60] environment: on ice = unlimited, near water = very fast
            const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
            const Surface under = grid.at(cx, cy).surface();
            if (under == Surface::Glacier) {
                u.res = u.res_max;
                return;
            }
            const int reach = (200 + u.stats.water_collect_range / 100) / kCellUnits;
            int water = 0;
            for (int y = cy - reach; y <= cy + reach; y += 2)
                for (int x = cx - reach; x <= cx + reach; x += 2)
                    if (Grid::in_bounds(x, y)) {
                        const Surface s = grid.at(x, y).surface();
                        water += s == Surface::Water || s == Surface::DeepWater;
                    }
            r = r * (100 + std::min(water, 20) * 25) / 100;  // up to 6× [X-89]
            break;
        }
        default: break;
    }
    if (u.res_max > 0) u.res = std::int32_t(std::min<std::int64_t>(u.res_max, u.res + per_tick(r, tick, cfg.tick_hz)));
}

}  // namespace em
