// Gold, XP, levels, skill points, shop, item actives [X-11][U-80][U-81][U-111][U-120][U-122][X-46][X-100].
#include <algorithm>
#include <format>
#include "game/match/layout.hpp"
#include "game/world.hpp"

namespace em {

std::int64_t World::give_xp(Unit& u, std::int64_t xp) {
    if (u.kind != UnitKind::Champion || xp <= 0) return 0;
    const Element el = champions()[std::size_t(u.champ)].element;
    xp = xp * sky_boost(el, sky).xp_pct / 100;  // [U-122] full moon / comet benders level faster
    u.xp += xp;
    const std::int32_t lvl = std::int32_t(level_at_xp(u.xp));  // [U-111] no cap
    if (lvl > u.level) {
        u.skill_points += lvl - u.level;
        u.level = lvl;
        recompute_stats(u);
        if (u.bot)  // bots spend points at once, spread over the kit
            for (int k = 0; k < kSkillSlots && u.skill_points > 0; ++k)
                for (Slot s : {Slot::Y, Slot::Q, Slot::W, Slot::E, Slot::R, Slot::T})  // the ultimate first [U-291]
                    if (s == Slot::Y || u.ab[std::size_t(s)].rank <= u.ab[std::size_t(Slot::Q)].rank) level_up(u, s);
    }
    return xp;
}

// [U-159][X-109] gold + XP for a kill; the client shows "+21g +60xp" where it died
void World::reward(Unit& c, const Unit& dead, std::int32_t gold, std::int64_t xp) {
    c.gold += gold;
    c.stat_gold += gold;
    const std::int64_t got = give_xp(c, xp);
    if (gold > 0 || got > 0) rewards.push_back({c.id, dead.ux(), dead.uy(), gold, std::int32_t(got), now_ms});
}

void World::level_up(Unit& u, Slot s) {
    if (s == Slot::P || s == Slot::F || s == Slot::G) return refuse(u, "P and the specials need no points");  // specials come ready [U-71]
    if (u.skill_points <= 0) return refuse(u, "no skill points (1 per level)");
    AbilityRt& a = u.ab[std::size_t(s)];
    // [U-291] every skill's own rule (ultimates: 6 / 12 / 18 …; others: 2 before 6, 3 before 12 …)
    const SkillDef* d = u.skill[std::size_t(s)];
    if (!d) return refuse(u, "no skill here");
    if (a.rank >= skill_max_rank(*d, u.level)) {
        const int step = d->rank_step ? d->rank_step : 6;
        const int next = d->ultimate ? (a.rank == 0 ? (d->unlock_level ? d->unlock_level : 6) : (d->unlock_level ? d->unlock_level : 6) + a.rank * step)
                                     : (u.level / step + 1) * step;
        return refuse(u, std::format("{}: next rank at level {}", d->name, next));
    }
    ++a.rank;
    --u.skill_points;
}

void World::buy(Unit& u, int item) {
    if (item < 0 || item >= int(kItems.size())) return;
    if (!in_base(u) && u.alive) return refuse(u, "stand inside the shop ring at your fountain");  // [X-100]
    const ItemDef& d = kItems[std::size_t(item)];
    // [U-261] parts I carry are used up; I pay only the rest
    const BuyPlan plan = plan_buy(u.items, item);
    if (u.gold < plan.price) return refuse(u, "not enough gold");
    // [U-245] buy limits (items used up as parts do not count)
    for (std::size_t s = 0; s < u.items.size(); ++s) {
        const std::int16_t it = u.items[s];
        if (it < 0 || plan.used[s]) continue;
        if ((d.unique || d.limit == BuyLimit::One) && it == item) return refuse(u, "you can carry only one of this item");
        if (d.limit == BuyLimit::OnePerGroup && kItems[std::size_t(it)].group == d.group)
            return refuse(u, "you can carry only one item of this kind (e.g. one pair of boots)");
    }
    // [U-322] where it goes: boots → the boots slot, special items → the special slot, the rest → slots 1…8
    auto open_slot = [&](std::size_t s) { return u.items[s] < 0 || plan.used[s]; };
    int free_slot = -1;
    if (d.group == ItemGroup::Boots) free_slot = open_slot(kBootsSlot) ? kBootsSlot : -1;
    else if (d.group == ItemGroup::Special) free_slot = open_slot(kSpecialSlot) ? kSpecialSlot : -1;
    else
        for (std::size_t s = 1; s <= 8 && free_slot < 0; ++s)
            if (open_slot(s)) free_slot = int(s);
    if (free_slot < 0)
        return refuse(u, d.group == ItemGroup::Boots ? "the boots slot is taken" : d.group == ItemGroup::Special ? "the special slot is taken"
                                                                                  : "inventory full (8 items)");
    for (std::size_t s = 0; s < u.items.size(); ++s)
        if (plan.used[s]) {
            u.items[s] = -1;
            u.item_impact[s] = 0;
        }
    u.items[std::size_t(free_slot)] = std::int16_t(item);
    u.gold -= plan.price;
    recompute_stats(u);  // (it adds the new max HP to the current HP, like LoL)
}

void World::use_item(Unit& u, int slot, std::int32_t target_x, std::int32_t target_y) {
    if (slot < 0 || slot >= kItemSlots || slot == kBootsSlot) return;  // [U-322] boots never have an active
    if (u.items[std::size_t(slot)] < 0 || u.item_cd_ms[std::size_t(slot)] > now_ms) return;
    const ItemDef& d = kItems[std::size_t(u.items[std::size_t(slot)])];
    switch (d.active) {
        case ItemActive::LotusTile:  // [U-114][X-46]
            ++u.item_impact[std::size_t(slot)];  // [U-209] times used
            u.lotus_until = now_ms + kLotusMs;
            recompute_stats(u);
            break;
        case ItemActive::SpiritFlask:  // [U-81] 60 s intense heal + immortal
            u.flask_until = now_ms + 60'000;
            break;
        case ItemActive::Falcon: {  // [U-235] the falcon flies to the cursor
            FalconRun f;
            f.owner = u.id;
            f.team = u.team;
            f.x = u.ux();
            f.y = u.uy();
            f.tx = std::clamp(target_x, 0, kMapUnits - 1);
            f.ty = std::clamp(target_y, 0, kMapUnits - 1);
            falcons.push_back(std::move(f));
            ++u.item_impact[std::size_t(slot)];
            break;
        }
        case ItemActive::None: return;
    }
    u.item_cd_ms[std::size_t(slot)] = now_ms + d.active_cd_ms;
}

}  // namespace em
