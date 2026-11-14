#pragma once
#include "state_base.hpp"

class BootState : public GameState
{
public:
    void on_enter(GameCtrl& game, Engine& engine) override;
    void on_exit(GameCtrl& game, Engine& engine) override;
    void on_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_fixed_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_render(GameCtrl& game, Engine& engine, float delta) override;

};

class MatchScene : public GameState
{
public:
    void on_enter(GameCtrl& game, Engine& engine) override;
    void on_exit(GameCtrl& game, Engine& engine) override;
    void on_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_fixed_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_render(GameCtrl& game, Engine& engine, float delta) override;
};