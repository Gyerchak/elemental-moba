// Prints every champion, skill and item as TSV: kind \t id \t element \t name \t description.
// Used by tools/art/gen_art.py to build the ComfyUI prompt list (one icon / splash / texture per line).
#include <cctype>
#include <print>
#include <string>

#include "core/items.hpp"
#include "game/content.hpp"
#include "game/skills/specials.hpp"

using namespace em;

namespace {
std::string slug(std::string_view s) {
    std::string id;
    for (char c : s) {
        if (c == '\'') continue;
        id += c == ' ' || c == '-' ? '_' : char(std::tolower(static_cast<unsigned char>(c)));
    }
    return id;
}
void skill(const SkillDef& d, Element e, std::string_view owner) {
    if (d.name.empty()) return;
    std::println("skill\t{}\t{}\t{}\t{}{}", slug(d.name), element_name(e), d.name, d.desc, owner);
}
}  // namespace

int main() {
    for (const ChampionDef& c : champions()) {
        std::println("champion\t{}\t{}\t{}\t{} {}\t{:06x}\t{:06x}\t{}", slug(c.name), element_name(c.element), c.name, role_name(c.role),
                     c.body == &kBroadBody ? "broad" : c.body == &kSlimBody ? "slim" : "humanoid", c.colour, element_colour(c.element),
                     c.avatar ? "avatar" : "");
        for (const SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y}) skill(*s, c.element, "");
    }
    for (int q = 0; q < int(q_options().size()); ++q) skill(q_options()[std::size_t(q)], q_option_element(q), "");
    for (const data::SpecialRow& r : data::kSpecialRows) skill(r.def, r.element, "");
    for (const ItemDef& i : kItems) std::println("item\t{}\tNone\t{}\t", slug(i.name), i.name);
}
