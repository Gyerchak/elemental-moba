// Vision [X-10], light [U-25][U-116..U-118][X-60][X-65][X-85] and the senses that are not vision:
// seismic [U-142], heat [U-143], Wind Sense for skills [U-141]. Senses NEVER reveal a unit; they only
// make waves (position, no identity). Things already in vision make no waves.
#include <algorithm>

#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {

void World::step_lights() {
    lights_tick = tick;
    lights.clear();
    const bool night = vision_range_pct(sky) < 100;
    for (const Effect& e : fx) {
        if (e.dead || e.light <= 0) continue;
        if (e.kind == FxKind::Lantern) {
            lights.push_back({e.ux(), e.uy(), e.light, 100});
            continue;
        }
        lights.push_back({e.ux(), e.uy(), e.light, 90});
    }
    for (const Unit& u : units) {
        if (!u.alive) continue;
        // [X-85] structures carry braziers so lanes are not pitch black at new moon
        if (u.is_structure() && u.kind != UnitKind::Inhibitor) lights.push_back({u.ux(), u.uy(), 420, 80});
        // [X-65] firebenders glow at night (2 cells): see more, but are easier to spot
        if (u.kind != UnitKind::Champion) continue;
        const bool firebender = champions()[std::size_t(u.champ)].element == Element::Fire;
        // [U-228][U-244] light you carry (only matters at night): Fire in Hand > lantern > firebender glow [X-65]
        if (night && has_passive(u, PassiveId::FireInHand)) lights.push_back({u.ux(), u.uy(), elements::kFireInHandLight, 100});
        else if (night && has_passive(u, PassiveId::Lantern)) lights.push_back({u.ux(), u.uy(), elements::kLanternLight, 85});
        else if (night && firebender) lights.push_back({u.ux(), u.uy(), elements::kFirebenderGlow, 100});
        // [U-244] every fire skill lights the caster while it is being cast
        if (u.casting && u.skill[u.cast.slot] && fire_family(u.skill[u.cast.slot]->sub))
            lights.push_back({u.ux(), u.uy(), elements::kFireCastLight, 100});
    }
    // [U-256] afterglows + burning trees
    std::erase_if(glows, [&](const TimedLight& g) { return g.until <= now_ms; });
    for (const TimedLight& g : glows) lights.push_back(g.l);
    // [U-256] magma glows (scanned every 2 s: magma changes slowly)
    if (now_ms - lava_lights_ms_ >= 2'000) {
        lava_lights_ms_ = now_ms;
        lava_lights_.clear();
        for (int y = 1; y < kGridSize; y += 3)
            for (int x = 1; x < kGridSize; x += 3)
                if (grid.at(x, y).surface() == Surface::Magma) lava_lights_.push_back({x * kCellUnits + 16, y * kCellUnits + 16, 110, 75});
    }
    lights.insert(lights.end(), lava_lights_.begin(), lava_lights_.end());
    light.clear();
    for (const LightOut& l : lights) light.add(LightSource{l.x, l.y, l.radius, l.strength}, grid);
}

void World::step_vision(int only_team) {
    const int ambient = vision_range_pct(sky);
    if (only_team <= 0) cloud_mask(clouds, cloud_cells);  // [U-241] steam / smoke / sand / snow block vision for both teams
    for (int t = 0; t < kTeams; ++t) {
        if (only_team >= 0 && t != only_team) continue;  // [U-287] one team per tick: no 4 ms tick
        std::vector<Viewer> vs;
        for (const Unit& u : units) {
            if (!u.alive || u.team != Team(t)) continue;
            if (u.st.has(StatusKind::Blind)) continue;  // [U-160] blinded: sees nothing, relies on hearing / senses
            std::int32_t s = u.sight;
            Viewer v{u.ux(), u.uy(), std::max(64, s)};
            if (!u.is_structure() && u.kind != UnitKind::Inhibitor) {  // [U-150] living things see with eyes
                const std::int32_t eo = eye_offset(u.body == &kBeastBody, size_pct(u));  // [U-152]
                v.x += u.face.x * eo / kOne;
                v.y += u.face.y * eo / kOne;
                v.fx = u.face.x;
                v.fy = u.face.y;
                v.fov_cos = kEyeFovCos;
                v.near = kBodySense;
            }
            vs.push_back(v);
        }
        vision[std::size_t(t)].compute(grid, vs, light, ambient, &cloud_cells);
    }
}

void World::step_senses() {
    // [X-80] seismic every 0.5 s, [X-81] heat every 0.75 s, [X-79] wind every 0.25 s
    const std::int64_t prev_ms = std::int64_t((tick - 1) * 1'000 / cfg.tick_hz);
    auto due = [&](std::int32_t period) { return now_ms / period != prev_ms / period; };
    const bool seis = due(kSeismicPeriodMs), heat = due(kHeatPeriodMs), wind = due(kWindPeriodMs);
    // [U-151] sound: outside vision but inside hearing → sound waves (no identity, a bit off the exact spot)
    if (due(kSoundPeriodMs))
        for (int t = 0; t < kTeams; ++t) {
            const Team team = Team(t);
            auto heard = [&](std::int32_t x, std::int32_t y, std::int64_t range) {
                for (const Unit& h : units)
                    if (h.alive && h.team == team && h.kind == UnitKind::Champion &&
                        dist2(h.ux(), h.uy(), x, y) <= range * range)
                        return true;
                return false;
            };
            for (Unit& o : units) {
                if (!o.alive || o.team == team || o.is_structure() || o.kind == UnitKind::Inhibitor) continue;
                const bool loud = o.casting || o.windup_end_ms > 0 || now_ms - o.attack_ready_ms < 300;
                const bool steps = now_ms - o.last_move_ms < 400;
                if (!loud && !steps) continue;
                if (visible_to(team, o)) continue;
                if (!heard(o.ux(), o.uy(), loud ? kHearLoud : kHearSteps)) continue;
                const std::int32_t jx = std::int32_t(mix_hash(o.id + 9, std::uint32_t(now_ms / 400)) % 81) - 40;
                const std::int32_t jy = std::int32_t(mix_hash(o.id + 99, std::uint32_t(now_ms / 400)) % 81) - 40;
                waves.push_back({{loud ? WaveKind::SoundLoud : WaveKind::Sound, o.ux() + jx, o.uy() + jy}, now_ms, team, 0});
                o.sensed_until[std::size_t(team) & 1] = now_ms + 600;  // [U-431]
            }
            for (const Effect& e : fx)  // explosions, fields, storms
                if (!e.dead && e.team != team && (e.kind == FxKind::Burst || e.kind == FxKind::Field || e.kind == FxKind::Tornado) &&
                    !visible_to(team, e) && heard(e.ux(), e.uy(), kHearLoud))
                    waves.push_back({{WaveKind::SoundLoud, e.ux(), e.uy()}, now_ms, team, 0});
        }
    // terrain changed by someone's earthbending → bending wave for the OTHER team's seismic senses [U-142]
    for (std::size_t i = 0; i < earth_events.size(); ++i) {
        const auto [cx, cy] = earth_events[i];
        const Team by = earth_event_team[i];
        for (const Unit& s : units) {
            if (!s.alive || s.kind != UnitKind::Champion || s.team == by || !has_passive(s, PassiveId::SeismicSense)) continue;
            if (s.flying() || s.st.has(StatusKind::Airborne) || s.st.has(StatusKind::WaterRide)) continue;  // [U-345] off the ground
            const std::int32_t x = cx * kCellUnits + 16, y = cy * kCellUnits + 16;
            if (dist2(s.ux(), s.uy(), x, y) > std::int64_t(kSeismicRange) * kSeismicRange) continue;
            if (vision[std::size_t(s.team)].at_units(x, y)) continue;
            // one wave per 4×4 cell block per tick is enough
            if ((cx % 4) || (cy % 4)) continue;
            waves.push_back({{WaveKind::SeismicBending, x, y}, now_ms, s.team, 0});
        }
    }
    if (!seis && !heat && !wind) return;  // (sound is above)
    for (const Unit& s : units) {
        if (!s.alive || s.kind != UnitKind::Champion) continue;
        // [U-345] seismic sense needs the feet on the ground: nothing while lifted (self, ally or enemy), airborne or riding water
        const bool grounded = !s.flying() && !s.st.has(StatusKind::Airborne) && !s.st.has(StatusKind::WaterRide);
        const bool use_seis = seis && grounded && has_passive(s, PassiveId::SeismicSense);
        const bool use_heat = heat && has_passive(s, PassiveId::HeatSense);
        const bool use_wind = wind && (has_passive(s, PassiveId::WindSense) || s.wind_sense_until > now_ms);  // [U-274]
        if (!use_seis && !use_heat && !use_wind) continue;
        const VisionMap& vis = vision[std::size_t(s.team)];
        if (use_seis || use_heat) {
            const std::int64_t range = use_seis ? kSeismicRange : kHeatRange;
            for (Unit& o : units) {
                if (!o.alive || o.team == s.team || o.is_structure() || o.kind == UnitKind::Inhibitor) continue;
                if (dist2(o.ux(), o.uy(), s.ux(), s.uy()) > range * range) continue;
                if (visible_to(s.team, o)) continue;  // vision already shows it
                // waves are centred near the unit, not exactly on it ("not seeing directly")
                const std::int32_t jx = std::int32_t(mix_hash(o.id, std::uint32_t(now_ms / 500)) % 49) - 24;
                const std::int32_t jy = std::int32_t(mix_hash(o.id + 77, std::uint32_t(now_ms / 500)) % 49) - 24;
                const bool moving = now_ms - o.last_move_ms < 400;
                if (use_seis && seismic_feels(o.flying(), o.st.has(StatusKind::Airborne), moving)) {
                    waves.push_back({{WaveKind::Seismic, o.ux() + jx, o.uy() + jy}, now_ms, s.team, 0});
                    o.sensed_until[std::size_t(s.team) & 1] = now_ms + 600;  // [U-431]
                }
                if (use_heat && heat_feels(o.st.has(StatusKind::Frozen))) {
                    waves.push_back({{WaveKind::Heat, o.ux() + jx, o.uy() + jy}, now_ms, s.team, 0});
                    o.sensed_until[std::size_t(s.team) & 1] = now_ms + 900;
                }
            }
            if (use_heat)  // enemy fire skills are heat sources too
                for (const Effect& e : fx)
                    if (!e.dead && e.team != s.team && base_element(e.sub) == Element::Fire && !vis.at_units(e.ux(), e.uy()) &&
                        dist2(e.ux(), e.uy(), s.ux(), s.uy()) <= range * range)
                        waves.push_back({{WaveKind::Heat, e.ux(), e.uy()}, now_ms, s.team, 0});
        }
        if (use_wind)  // [U-141] only HOW a skill moves, only for this airbender
            for (const Effect& e : fx) {
                if (e.dead || e.team == s.team || e.kind == FxKind::Steam) continue;
                if (dist2(e.ux(), e.uy(), s.ux(), s.uy()) > std::int64_t(kWindSenseRange) * kWindSenseRange) continue;
                if (visible_to(s.team, e)) continue;
                const Dir d = (e.vx || e.vy) ? dir_of(e.vx, e.vy) : Dir{0, 0};
                waves.push_back({{WaveKind::Wind, e.ux(), e.uy(), d.x, d.y}, now_ms, s.team, s.id});
            }
    }
}

// [U-235] falcons: out → watch (record every 0.25 s what they see) → back → the team replays it as ghosts
void World::step_falcons() {
    const std::int32_t step = kFalconSpeed * 1'000 / std::int32_t(cfg.tick_hz) / 1'000;
    auto fly = [&](FalconRun& f, std::int32_t x, std::int32_t y) {
        const std::int64_t dx = x - f.x, dy = y - f.y, d = isqrt(dx * dx + dy * dy);
        if (d <= step) {
            f.x = x;
            f.y = y;
            return true;
        }
        f.x += std::int32_t(dx * step / d);
        f.y += std::int32_t(dy * step / d);
        return false;
    };
    for (FalconRun& f : falcons) {
        const Unit* owner = find(f.owner);
        switch (f.state) {
            case 0:
                if (fly(f, f.tx, f.ty)) f.state = 1, f.watch_from = now_ms;
                break;
            case 1:
                if ((now_ms - f.watch_from) % kFalconFrameMs < std::int64_t(1'000 / cfg.tick_hz)) {
                    GhostFrame fr;
                    fr.t = now_ms - f.watch_from;
                    for (const Unit& o : units)
                        if (o.alive && !o.is_structure() && o.kind != UnitKind::Inhibitor &&
                            dist2(o.ux(), o.uy(), f.x, f.y) <= std::int64_t(rules::kSightFalcon) * rules::kSightFalcon)
                            fr.units.push_back({o.id, o.champ, std::uint8_t(o.team), std::uint8_t(o.kind), o.ux(), o.uy()});
                    f.frames.push_back(std::move(fr));
                }
                if (now_ms - f.watch_from >= kFalconWatchMs) f.state = 2;
                break;
            case 2:
                if (!owner) {
                    f.state = 4;  // nobody to return to
                    break;
                }
                if (fly(f, owner->ux(), owner->uy())) {
                    f.state = 3;
                    f.replay_from = now_ms;
                    add_feed("A falcon came back — its report is on the map");
                }
                break;
            default: break;
        }
    }
    std::erase_if(falcons, [&](const FalconRun& f) { return f.state == 4 || (f.state == 3 && now_ms - f.replay_from > kFalconWatchMs); });
}

}  // namespace em
