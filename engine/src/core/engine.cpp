#include "engine.hpp"
#include <SDL.h>
#include <algorithm>
#include <glad/glad.h>

#include "graphics/model.hpp"
#include "graphics/render.hpp"
#include <chrono>
#include <format>


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
    _filesystem(std::make_unique<filesys::FilesystemOperator>(std::move(_cfg.filesystem))), _input(std::make_unique<input::Input>()), _actions(std::make_unique<input::ActionMap>())
{
#ifndef NDEBUG
    enable_gl_dbg();
#endif
    setup_file_operators();

    _models = std::make_unique<graphics::ModelCache>(*_renderMgr, *_filesystem);
    _models->add_loader(graphics::make_gltf_loader());

    regen_world();
}

Engine::~Engine() = default;

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
        _input->begin_frame();
        _window->poll_events(*_input);
        _actions->update(*_input);
        if (_window->should_close())
            _running = false;
        if (_window->was_resized())
            glViewport(0, 0, _window->framebuffer_width(), _window->framebuffer_height());
        while (acc >= tick_step)
        {
            game.on_fixed_update(*this, tick_step);
            _physics->step(*_world, float(tick_step));
            _actions->consume_tick();
            acc -= tick_step;
        }
        _physics->flush_events();

        game.on_update(*this, frame);
        game.on_render(*this, acc / tick_step);
        _renderMgr->flush();
        _window->render_ui();

        constexpr int gif_frames = 60;
        if (_frame_count < gif_frames)
            _gif->capture_frame(_window->framebuffer_width(), _window->framebuffer_height());
        if (_frame_count == gif_frames - 1) {
            _gif->save("screenshot.gif");
            _running = false; // for the remote test, we just want to take a capture and exit
        }

        _window->present();
        _frame_count++;
    }

    game.on_shutdown(*this);
    return 0;
}

void Engine::regen_world()
{
    _world = std::make_unique<ecs::World>();
    _physics = std::make_unique<physics::PhysicsWorld>(_physicsMeshes);
}

physics::MeshId Engine::collision_mesh(graphics::ModelHandle model, math::vec3 scale)
{
    const auto key = std::make_tuple(model, scale.x, scale.y, scale.z);
    if (auto it = _modelColliders.find(key); it != _modelColliders.end())
        return it->second;

    const auto& asset = _models->get(model);
    if (!asset.collision)
        throw_error("model was loaded with keepCollisionGeometry off");

    std::vector<math::vec3> verts = asset.collision->positions;
    for (auto& v : verts)
        v = v * scale;

    std::vector<u32> idx = asset.collision->indices;
    if (scale.x * scale.y * scale.z < 0.0f)
        for (size_t i = 0; i + 2 < idx.size(); i += 3)
            std::swap(idx[i + 1], idx[i + 2]);

    const auto id = _physicsMeshes->add(std::move(verts), std::move(idx));
    _modelColliders.emplace(key, id);
    return id;
}

Engine::SDLContext::SDLContext()
{

#if _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
        throw_error(SDL_GetError());
}

Engine::SDLContext::~SDLContext()
{ SDL_Quit(); }
