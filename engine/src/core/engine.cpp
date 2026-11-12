#include "engine.hpp"
#include "engine.hpp"
#include <SDL.h>
#include <algorithm>
#include <glad/glad.h>

#include "graphics/render.hpp"


static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;
    std::fprintf(stderr, "[opengl] %s\n", msg);
}

static void enable_gl_dbg()
{
    if (!glDebugMessageCallback)
        return;
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, nullptr);
}


Engine::Engine(EngineConfiguration cfg) : _cfg(std::move(cfg)), _window(std::make_unique<Window>(_cfg.window)), _renderMgr(std::make_unique<graphics::RenderMgr>(_cfg.renderer)),
    _filesystem(std::make_unique<filesys::FilesystemOperator>(std::move(_cfg.filesystem))), _input(std::make_unique<input::Input>())
{
#ifndef NDEBUG
    enable_gl_dbg();
#endif
    setup_file_operators();
}

int Engine::start(GameCtrl& game)
{
    game.on_init(*this);

    dblc tick_step = 1.0 / _cfg.tick_rate;
    dblc frequency = double(SDL_GetPerformanceFrequency());
    var previousCtr = SDL_GetPerformanceCounter();
    dbl acc = 0.0;

    while (_running)
    {
        var nowCtr = SDL_GetPerformanceCounter();
        dbl frame = (nowCtr - previousCtr) / frequency;

        previousCtr = nowCtr;
        acc += std::min(frame, 0.25);

        _window->poll_events();
        if (_window->should_close())
            _running = false;
        if (_window->was_resized())
            glViewport(0, 0, _window->framebuffer_width(), _window->framebuffer_height());
        while (acc >= tick_step)
        {
            game.on_fixed_update(*this,tick_step);
            acc -= tick_step;
        }

        game.on_update(*this, frame);

        game.on_render(*this, frame);
        _renderMgr->flush();
        _window->swap();
        _frame_count++;
    }

    game.on_shutdown(*this);
    return 0;
}

Engine::SDLContext::SDLContext()
{

#if _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
        throw_error(SDL_GetError());
}

Engine::SDLContext::~SDLContext()
{ SDL_Quit(); }
