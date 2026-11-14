cmake_minimum_required(VERSION 3.20)
project(GambitPC VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W0 /MP)
else()
    add_compile_options(-w)
endif()

if (MSVC)
    add_link_options($<$<CONFIG:Debug>:/INCREMENTAL> $<$<CONFIG:Debug>:/DEBUG:FASTLINK>)
endif()

add_subdirectory(third_party/glad)
add_subdirectory(third_party/imgui_sdl2)

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(tests)

target_precompile_headers(engine PRIVATE
    <string> <vector> <memory> <unordered_map> <span>
    <filesystem> <functional> <variant> <array>
)
target_precompile_headers(game REUSE_FROM engine)