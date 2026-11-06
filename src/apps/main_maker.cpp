// [U-529] the start of each maker as a program of its own: EM_MAKER = 1 champion, 2 map, 3 item, 4 loadout
#include <SDL3/SDL_main.h>

#include "apps/makers.hpp"

int main(int argc, char** argv) {
#if EM_MAKER == 1
    return em::makers::champion_main(argc, argv);
#elif EM_MAKER == 2
    return em::makers::map_main(argc, argv);
#elif EM_MAKER == 3
    return em::makers::item_main(argc, argv);
#else
    return em::makers::loadout_main(argc, argv);
#endif
}
