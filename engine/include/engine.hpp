#ifndef ENGINE_HPP
#define ENGINE_HPP


#include <memory>
#include <string>


#include "core/util.hpp"

#include <core/window.hpp>
#include <core/ecs.hpp>
#include "graphics/render.hpp"
#include <core/file.hpp>

struct Engine;

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
    RenderConfiguration renderer;
    FilesystemConfiguration filesystem;
    double tick_rate = 60.0;

};


class Engine
{
public:
    explicit Engine(const EngineConfiguration& cfg);
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    RenderMgr& renderer() ret(*_renderMgr);

    int start(GameCtrl& game);
    void quit() { _running = false; }

private:

    struct SDLContext
    {
        SDLContext();
        ~SDLContext();
        SDLContext(const SDLContext&) = delete;
        SDLContext& operator=(const SDLContext&) = delete;
    };


    EngineConfiguration _cfg;
    SDLContext _sdl;

    std::unique_ptr<FilesystemOperator> _filesystem;
    std::unique_ptr<Window> _window;
    std::unique_ptr<RenderMgr> _renderMgr;
    //std::unique_ptr<WorldMgr> _worldMgr;

    bool _running = true;
};

#endif
