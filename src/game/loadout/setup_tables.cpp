// [U-483] The setup tables IN USE: copies of the defaults in loadout.hpp, with free places. Game versions edit
// them (game/versions.cpp: setup.<kind>.<name>.<key> = …); restore_setup() puts the defaults back.
#include <deque>
#include <string>

#include "game/loadout/loadout.hpp"

namespace em {

namespace {
struct Tables {
    KeystoneTable key{};
    SpecTreeTable trees{};
    ShardTable shards{};
    ClassicTable classic{};
    QuintTable quints{};
    MasteryTable mastery{};
};
Tables defaults() {
    Tables t;
    for (std::size_t i = 0; i < kDefaultKeystones.size(); ++i) t.key[i] = kDefaultKeystones[i];
    for (std::size_t i = 0; i < kDefaultSpecTrees.size(); ++i) t.trees[i] = kDefaultSpecTrees[i];
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t i = 0; i < 3; ++i) t.shards[r][i] = kDefaultRunes[r][i];
    t.classic = kDefaultClassicRunes;
    for (std::size_t i = 0; i < kDefaultQuints.size(); ++i) t.quints[i] = kDefaultQuints[i];
    t.mastery = kDefaultMasteryTrees;
    return t;
}
Tables& store() {
    static Tables t = defaults();
    return t;
}
}  // namespace

const KeystoneTable& keystones() { return store().key; }
const SpecTreeTable& spec_trees() { return store().trees; }
const ShardTable& shards() { return store().shards; }
const ClassicTable& classic_runes() { return store().classic; }
const QuintTable& quints() { return store().quints; }
const MasteryTable& mastery_trees() { return store().mastery; }
KeystoneTable& keystones_mut() { return store().key; }
SpecTreeTable& spec_trees_mut() { return store().trees; }
ShardTable& shards_mut() { return store().shards; }
ClassicTable& classic_runes_mut() { return store().classic; }
QuintTable& quints_mut() { return store().quints; }
MasteryTable& mastery_trees_mut() { return store().mastery; }
void restore_setup() { store() = defaults(); }

std::string_view keep_setup_name(std::string_view s) {
    static std::deque<std::string> names;  // (deque: stable addresses)
    for (const std::string& n : names)
        if (n == s) return n;
    names.emplace_back(s);
    return names.back();
}

}  // namespace em
