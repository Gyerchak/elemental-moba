cmake_minimum_required(VERSION 3.28)
project(elemental_moba LANGUAGES CXX)

# [U-01] C++23 only
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)  # [U-02]
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(em_core STATIC src/map/default_map.cpp src/map/twisted_grove.cpp)
target_include_directories(em_core PUBLIC src)

file(GLOB_RECURSE EM_GAME_SRC CONFIGURE_DEPENDS src/game/*.cpp)
add_library(em_game STATIC ${EM_GAME_SRC})
target_link_libraries(em_game PUBLIC em_core)

add_executable(sim_run tools/sim_run.cpp)
target_link_libraries(sim_run PRIVATE em_game)

# [X-01][X-17] network + SDL3 client (system packages: sdl3, enet)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(ENET REQUIRED IMPORTED_TARGET libenet)
pkg_check_modules(SDL3TTF REQUIRED IMPORTED_TARGET sdl3-ttf)  # [U-250] real font text
file(GLOB EM_NET_SRC CONFIGURE_DEPENDS src/net/*.cpp)
add_library(em_net STATIC ${EM_NET_SRC} src/client/net_client.cpp)
target_link_libraries(em_net PUBLIC em_game PkgConfig::ENET)
file(GLOB_RECURSE EM_CLIENT_SRC CONFIGURE_DEPENDS src/client/*.cpp)
list(FILTER EM_CLIENT_SRC EXCLUDE REGEX "net_client\\.cpp$")
# [U-510][U-511] what the game, the champion maker and the server app share
set(EM_UI_SRC src/client/draw.cpp src/client/locale.cpp src/client/tool_shell.cpp src/client/sprites.cpp src/client/resources.cpp
    src/client/audio.cpp src/client/settings.cpp src/client/custom_champions.cpp)
list(FILTER EM_CLIENT_SRC EXCLUDE REGEX "/(draw|locale|tool_shell|sprites|resources|audio|settings|custom_champions)\\.cpp$")
add_library(em_ui STATIC ${EM_UI_SRC})
target_link_libraries(em_ui PUBLIC em_game PkgConfig::SDL3 PkgConfig::SDL3TTF)
add_executable(elemental_moba src/main.cpp ${EM_CLIENT_SRC})
target_link_libraries(elemental_moba PRIVATE em_ui em_net)
# [U-510] the champion maker
add_executable(em_champion_maker src/apps/champion_maker.cpp)
target_link_libraries(em_champion_maker PRIVATE em_ui)
# [U-518] the map maker
add_executable(em_map_maker src/apps/map_maker.cpp)
target_link_libraries(em_map_maker PRIVATE em_ui)
# [U-511] the server only: without the game, without joining as a player
add_executable(em_server src/apps/server_app.cpp)
target_link_libraries(em_server PRIVATE em_ui em_net)

# [U-197] generates the placeholder default resource pack (run once: ./build/make_default_pack)
add_executable(make_default_pack tools/make_default_pack.cpp)
target_link_libraries(make_default_pack PRIVATE em_game PkgConfig::SDL3)

add_executable(map_preview tools/map_preview.cpp)
target_link_libraries(map_preview PRIVATE em_core)

add_executable(sky_timeline tools/sky_timeline.cpp)
target_link_libraries(sky_timeline PRIVATE em_core)

# lists champions / skills / items as TSV for the art generator (tools/art/gen_art.py)
add_executable(dump_content tools/dump_content.cpp)
target_link_libraries(dump_content PRIVATE em_game)
