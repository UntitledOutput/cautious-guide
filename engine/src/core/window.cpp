#include "core/window.hpp"
#include <glad/glad.h>
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "core/input.hpp"

Window::Window(const WindowDescription& desc)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

#ifndef NDEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    u32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (desc.resizable)
        flags |= SDL_WINDOW_RESIZABLE;



    _window = SDL_CreateWindow(desc.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.width,
                               desc.height, flags);

    if (_window)
    {
        _context = SDL_GL_CreateContext(_window);
    }

    if (!_window || !_context)
    {
        var error_message = SDL_GetError();
        destroy();
        throw std::runtime_error(error_message);
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        destroy();
        throw_error("Failed to initialize OpenGL context");
    }

    SDL_GL_SetSwapInterval(desc.vsync ? 1 : 0);
    SDL_GL_GetDrawableSize(_window, &fb_width, &fb_height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplSDL2_InitForOpenGL(_window, _context);
    ImGui_ImplOpenGL3_Init("#version 460");

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

}

Window::~Window()
{ destroy(); }

void Window::poll_events(input::Input& in)
{
    _resized = false;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        in.handle_event(e);

        switch (e.type)
        {
            case SDL_QUIT:
                _should_close = true;
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    _resized = true;
                    SDL_GL_GetDrawableSize(_window, &fb_width, &fb_height);
                }
                break;
            default: 
                // todo: check for input events
                break;
        }
    }
}

void Window::swap()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(_window);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Window::destroy()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // context first
    if (_context)
        SDL_GL_DeleteContext(_context);
    if (_window)
        SDL_DestroyWindow(_window);

    _context = nullptr;
    _window = nullptr;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
