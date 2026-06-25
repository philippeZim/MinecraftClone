cmake_minimum_required(VERSION 3.16)
project(minecraft_clone C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -ffast-math -DNDEBUG")

# --- third_party: sokol headers (interface only) ---------------------------------
add_library(sokol INTERFACE)
target_include_directories(sokol INTERFACE third_party)

# --- core: math/config headers + the single sokol implementation unit ------------
add_library(core STATIC src/core/sokol_impl.c)
target_include_directories(core PUBLIC src/core)
target_link_libraries(core PUBLIC sokol)

# --- modules: each owns its headers, declares only the deps it truly needs --------
add_library(world  STATIC src/world/world.c src/world/terrain.c)
target_include_directories(world PUBLIC src/world)
target_link_libraries(world PRIVATE core)            # config.h only

add_library(player STATIC src/player/player.c)
target_include_directories(player PUBLIC src/player)
target_link_libraries(player PUBLIC core PRIVATE world)  # hmath.h + collision queries

add_library(render STATIC src/render/render.c)
target_include_directories(render PUBLIC src/render)
target_link_libraries(render PUBLIC core PRIVATE world)

# --- executable -------------------------------------------------------------------
add_executable(game src/main.c)
target_link_libraries(game PRIVATE core world player render)

# --- platform link libs for sokol_app (Linux/X11 + GL) ---------------------------
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(game PRIVATE X11 Xi Xcursor GL dl m Threads::Threads)
endif()
