#pragma once
#include <string>

#include "util.hpp"

struct SDL_Window;
def_void SDL_GLContext;

struct WindowDescription
{
    std::string title;
    int width = 1280;
    int height = 720;

    bool vsync = true;
    bool resizable = true;
};

class Window
{
public:

    explicit Window(const WindowDescription& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void poll_events();
    void swap();

    bool should_close() ret(_should_close)
    bool was_resized() ret(_resized)

    int framebuffer_width() ret(fb_width);
int framebuffer_height() ret(fb_height)

    SDL_Window* get_handle() ret(_window)

private:
    SDL_Window* _window = nullptr;
    SDL_GLContext _context = nullptr;

    bool _should_close = false;;
    bool _resized = false;

    int fb_width = 0;
    int fb_height = 0;

    void destroy();
};
