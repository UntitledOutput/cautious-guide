cmake_minimum_required(VERSION 3.20)
project(GambitPC VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party/glad)
add_subdirectory(engine)
add_subdirectory(game)