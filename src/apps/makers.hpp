#pragma once
// [U-529] The makers: each one is its own program (em_champion_maker, em_map_maker, em_item_maker, em_loadout_maker) and
// also opens inside the window of the game, from the menu.
#include <SDL3/SDL.h>

#include "client/tool_shell.hpp"

namespace em::makers {

int champion_main(int argc, char** argv);
int map_main(int argc, char** argv);
int item_main(int argc, char** argv);
int loadout_main(int argc, char** argv);
[[nodiscard]] tool::ToolExit champion_embedded(SDL_Window* win, SDL_Renderer* ren);
[[nodiscard]] tool::ToolExit map_embedded(SDL_Window* win, SDL_Renderer* ren);
[[nodiscard]] tool::ToolExit item_embedded(SDL_Window* win, SDL_Renderer* ren);
[[nodiscard]] tool::ToolExit loadout_embedded(SDL_Window* win, SDL_Renderer* ren);

}  // namespace em::makers
