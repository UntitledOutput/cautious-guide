#ifndef ENGINE_HPP
#define ENGINE_HPP


#include <memory>
#include <string>


#include "core/util.hpp"

#include <core/window.hpp>
#include <core/ecs.hpp>
#include "graphics/render.hpp"
#include <core/file.hpp>

#include "core/input.hpp"

namespace graphics
{
    class ModelCache;
}

class Engine;

class GameCtrl
{
public:
    virtual ~GameCtrl() = default;
    virtual void on_init(Engine&) { }
    virtual void on_fixed_update(Engine&, double delta) {}
    virtual void on_update(Engine&, double delta) {}
    virtual void on_render(Engine&, double alpha) {}
    virtual void on_shutdown(Engine&) {}
};

struct EngineConfiguration
{
    WindowDescription window;
    graphics::RenderConfiguration renderer;
    filesys::FilesystemConfiguration filesystem;
    double tick_rate = 60.0;

};


class Engine
{
public:
    explicit Engine(EngineConfiguration cfg);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    graphics::RenderMgr& renderer() ret(*_renderMgr);
    filesys::FilesystemOperator& filesystem() ret(*_filesystem);
    input::Input& input() ret(*_input);
    input::ActionMap& actions() ret(*_actions);
    Window& window() ret(*_window)
    graphics::ModelCache& models() ret(*_models)

    int start(GameCtrl& game);
    void quit() { _running = false; }

    int frame_count() ret(_frame_count);

private:

    void setup_file_operators();

    struct SDLContext
    {
        SDLContext();
        ~SDLContext();
        SDLContext(const SDLContext&) = delete;
        SDLContext& operator=(const SDLContext&) = delete;
    };


    EngineConfiguration _cfg;
    SDLContext _sdl;

    std::unique_ptr<filesys::FilesystemOperator> _filesystem;
    std::unique_ptr<Window> _window;
    std::unique_ptr<input::Input> _input;
    std::unique_ptr<::input::ActionMap> _actions;
    std::unique_ptr<graphics::RenderMgr> _renderMgr;

    std::unique_ptr<graphics::ModelCache> _models;
    //std::unique_ptr<WorldMgr> _worldMgr;


    int _frame_count = 0;
    bool _running = true;
};

#endif
