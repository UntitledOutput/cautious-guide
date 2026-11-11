cmake_minimum_required(VERSION 3.20)
project(GambitPC VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

add_subdirectory(third_party/glad)
add_subdirectory(engine)
add_subdirectory(game)