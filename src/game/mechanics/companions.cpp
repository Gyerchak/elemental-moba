// [U-407] Travel companions (replace the recall; the old channel stays behind MatchConfig::classic_recall).
// The chosen animal leaves the own fountain at kCompanionSpeed, runs to the champion (following them), then carries
// them home at the same speed; the rider cannot act but can be hit and killed.
// [U-491] While it comes the champion moves and acts freely; calling again CANCELS it. Every call starts a 3 min
// cooldown (cancelled or not). [U-495] After death it fetches the BODY (+50 % speed, no cooldown); home = respawn.
#include <algorithm>
#include <format>

#include "game/world.hpp"

namespace em {

void World::call_companion(Unit& u) {
    if (u.kind != UnitKind::Champion || !u.alive) return;
    for (Companion& c : companions)
        if (c.owner == u.id && !c.revive) {
            if (c.carrying) return refuse(u, "you are already riding home");
            c.owner = 0;  // [U-491] cancelled (the cooldown keeps running)
            add_feed(std::format("{} sends the {} back", u.name, kCompanionNames[std::size_t(u.companion)]));
            std::erase_if(companions, [](const Companion& k) { return k.owner == 0; });
            return;
        }
    if (in_base(u)) return refuse(u, "you are already at home");
    if (now_ms < u.companion_ready_ms) return refuse(u, std::format("companion ready in {} s", (u.companion_ready_ms - now_ms + 999) / 1000));
    const auto [fx_, fy_] = fountain(u.team);
    companions.push_back({u.id, u.companion, fx_, fy_, false});
    u.companion_ready_ms = now_ms + 180'000;  // [U-491] 3 min from the call
    add_feed(std::format("{} calls the {}", u.name, kCompanionNames[std::size_t(u.companion)]));
}

void World::step_companions() {
    for (Companion& c : companions) {
        const std::int64_t step = std::max<std::int64_t>(1, kCompanionSpeed * (c.revive ? 3 : 2) / 2 / std::int64_t(cfg.tick_hz));  // [U-495] +50 %
        Unit* u = find(c.owner);
        if (!u || (!u->alive && !c.revive)) {  // the rider died: the companion goes home (removed)
            if (u) u->riding = false;
            c.owner = 0;
            continue;
        }
        if (c.carrying) u->riding = true;  // [U-491] only on its back; while it comes the champion is free
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
                u->windup_end_ms = 0;
                u->st.remove(StatusKind::Channel);
                for (Effect& f : fx)  // [U-383] a dash in progress ends: the companion carries now
                    if (f.owner == u->id && f.def && f.def->kind == SkillKind::Dash) f.dead = true;
            } else {  // home
                u->riding = false;
                u->x = tx * kSub, u->y = ty * kSub;
                if (c.revive) respawn(*u);  // [U-495] the body is home: back to life at the fountain
                else place_on_walkable(*u);
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
