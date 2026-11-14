find_package(imgui CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)


add_library(imgui_sdl2 STATIC src/imgui_impl_sdl2.cpp)
target_include_directories(imgui_sdl2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(imgui_sdl2 PUBLIC imgui::imgui SDL2::SDL2)