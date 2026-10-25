#pragma once
// Shared shortcuts for champion data files [X-13][X-59]. Field order of SkillDef:
// name kind sub flow | dmg dmg_rank ep ad bonus_ad | range width speed radius | cast_ms cd_ms cd_rank cost |
// dur_ms delay_ms tick_ms | cc cc_ms cc_val | knock pierce group ally | casts recast_ms move light light_ms
// targeted full_moon_only size_bonus passive desc
#include "game/content.hpp"

namespace em::data {
using K = SkillKind;
using S = SubElement;
using C = StatusKind;
}  // namespace em::data
