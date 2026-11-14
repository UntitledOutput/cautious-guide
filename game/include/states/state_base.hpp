#pragma once
#include "core/util.hpp"

class Engine;
class GameCtrl;

class GameState
{
    virt_class_def(GameState)

    virtual void on_enter(GameCtrl& game, Engine& engine);
    virtual void on_exit(GameCtrl& game, Engine& engine) = 0;
    virtual void on_update(GameCtrl& game, Engine& engine, float delta) = 0;
    virtual void on_fixed_update(GameCtrl& game, Engine& engine, float delta) = 0;
    virtual void on_render(GameCtrl& game, Engine& engine, float delta) = 0;


};
