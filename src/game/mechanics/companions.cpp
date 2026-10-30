// [U-407] Travel companions (replace the recall; the old channel stays behind MatchConfig::classic_recall).
// The chosen animal leaves the own fountain at kCompanionSpeed, runs to the champion (following them), then carries
// them home at the same speed. Nothing interrupts or cancels it; the rider cannot act but can be hit and killed.
#include <algorithm>
#include <format>

#include "game/world.hpp"

namespace em {

void World::call_companion(Unit& u) {
    if (u.kind != UnitKind::Champion || !u.alive) return;
    if (std::ranges::any_of(companions, [&](const Companion& c) { return c.owner == u.id; })) return refuse(u, "your companion is already on the way");
    if (in_base(u)) return refuse(u, "you are already at home");
    const auto [fx_, fy_] = fountain(u.team);
    companions.push_back({u.id, u.companion, fx_, fy_, false});
    add_feed(std::format("{} calls the {}", u.name, kCompanionNames[std::size_t(u.companion)]));
}

void World::step_companions() {
    const std::int64_t step = std::max<std::int64_t>(1, kCompanionSpeed / std::int64_t(cfg.tick_hz));
    for (Companion& c : companions) {
        Unit* u = find(c.owner);
        if (!u || !u->alive) {  // the rider died: the companion goes home (removed)
            if (u) u->riding = false;
            c.owner = 0;
            continue;
        }
        std::int32_t tx = u->ux(), ty = u->uy();
        if (c.carrying) std::tie(tx, ty) = fountain(u->team);
        const std::int64_t d = isqrt(dist2(c.x, c.y, tx, ty));
        if (d <= step) {
            c.x = tx, c.y = ty;
            if (!c.carrying) {  // arrived: the champion climbs on
                c.carrying = true;
                u->riding = true;
                u->order = {};
                u->path.clear();
                u->casting = false;
                u->channel_end_ms = 0;
            } else {  // home
                u->riding = false;
                u->x = tx * kSub, u->y = ty * kSub;
                place_on_walkable(*u);
                c.owner = 0;
            }
            continue;
        }
        c.x += std::int32_t((tx - c.x) * step / d);
        c.y += std::int32_t((ty - c.y) * step / d);
        if (c.carrying) u->x = c.x * kSub, u->y = c.y * kSub, u->moving = true;
    }
    std::erase_if(companions, [](const Companion& c) { return c.owner == 0; });
}

}  // namespace em
