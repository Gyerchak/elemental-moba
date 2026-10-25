// Match flow [X-07][X-08][X-12]: setup, wave timer, passive gold, respawns of inhibitors / monsters.
#include <format>
#include <algorithm>
#include "game/match/layout.hpp"
#include "game/world.hpp"

namespace em {

void World::setup_match() {
    spawn_structures();
    spawn_jungle();
}

void World::step_match() {
    // [U-296] the planetary alignment awakens every non-bender (once): Q W E R become a bender's set
    if (sky.real_ms >= WorldClock::kAlignStartMs)
        for (Unit& u : units) {
            if (u.kind != UnitKind::Champion || u.awakened != 255) continue;
            const ChampionDef& c = champions()[std::size_t(u.champ)];
            if (c.element != Element::None) continue;
            const Element el = awaken_element_for(c, u.stat_dmg_from_champs);
            u.awakened = std::uint8_t(el);
            for (int s = 1; s <= 4; ++s)
                if (const SkillDef* d = awakened_skill(c, el, s)) u.skill[std::size_t(s)] = d;  // ranks carry over
            add_feed(std::format("{} awakens as a {}bender!", u.name, element_name(el)));
            announce(AnnounceKind::Awakened, u.id, std::uint32_t(el), u.team);
        }
    if (now_ms >= next_wave_ms) {
        spawn_wave();
        next_wave_ms += cfg.wave_period_ms;
    }
    // passive gold ~2 / s from 1:50 [X-11]
    if (now_ms >= 110'000 && tick % cfg.tick_hz == 0)
        for (Unit& u : units)
            if (u.kind == UnitKind::Champion) u.gold += 2;
    // inhibitors respawn; monsters respawn at home
    for (Unit& u : units) {
        if (u.alive || u.respawn_ms == 0 || now_ms < u.respawn_ms) continue;
        if (u.kind == UnitKind::Inhibitor || u.kind == UnitKind::Monster || u.kind == UnitKind::Boss) {
            if (u.kind == UnitKind::Inhibitor) clearance_dirty = true;
            u.alive = true;
            u.hp = u.stats.health.total();
            u.x = u.home_x;
            u.y = u.home_y;
            u.st.clear();
            u.order = {};
            u.aggro = 0;
        }
    }
    // Spirit Flask revive window ends
}

}  // namespace em
