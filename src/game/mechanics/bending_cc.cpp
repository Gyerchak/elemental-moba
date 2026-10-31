// [U-474] OWN MECHANICS of this game: crowd control that splits BENDING from WEAPONS.
//   Chi block (StatusKind::Mute) — bending of the elements in its value is gone: those bending skills AND bending
//                                  autoattacks (0 = every element). Non-bending skills still work.
//   Disarm                       — no weapon autoattacks, no non-magical (weapon) skills. Bending still works.
//   Subdue                       — both kinds at once: no bending at all and disarmed (only arcane magic is left).
// Silence (no abilities at all) is a League of Legends mechanic: lol.cpp.
// A skill is BENDING when its sub-element has an element (fire, water, earth, air families); WEAPON = no element and
// not arcane (SkillDef::arcane = non-elemental magic, never blocked by these three).
#include "game/content.hpp"
#include "game/world.hpp"

namespace em {

bool World::chi_blocked(const Unit& u, Element el) const {
    if (el == Element::None) return false;
    if (u.st.has(StatusKind::Subdue)) return true;
    if (!u.st.has(StatusKind::Mute)) return false;
    const std::int32_t mask = u.st.value(StatusKind::Mute);
    return mask == 0 || (mask & (1 << int(el))) != 0;
}

SubElement World::auto_sub(const Unit& u) const {
    if (u.kind != UnitKind::Champion) return SubElement::Physical;
    const ChampionDef& c = champions()[std::size_t(u.champ)];
    if (c.element != Element::None) return c.sub;
    if (u.magic_until > now_ms) return SubElement(u.magic_sub);  // [U-264] a non-bender in a magic stance
    return SubElement::Physical;
}

const char* World::skill_block_reason(const Unit& u, const SkillDef& d) const {
    if (lol_silenced(u)) return "silenced: no abilities";
    const SubElement sub = skill_sub(u, d);
    const Element el = base_element(sub);
    if (chi_blocked(u, el)) return "chi blocked: no bending";
    if (el == Element::None && !d.arcane && (u.st.has(StatusKind::Disarm) || u.st.has(StatusKind::Subdue)))
        return "disarmed: no weapon skills";
    return nullptr;
}

bool World::auto_allowed(const Unit& u) const {
    if (!u.st.can_attack()) return false;
    const Element el = base_element(auto_sub(u));
    if (el != Element::None) return !chi_blocked(u, el);  // a bending autoattack
    return !u.st.has(StatusKind::Disarm) && !u.st.has(StatusKind::Subdue);  // a weapon autoattack
}

}  // namespace em
