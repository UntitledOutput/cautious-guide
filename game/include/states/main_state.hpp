#pragma once
#include "state_base.hpp"

namespace ecs
{
    struct Entity;
}

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


        std::vector<ecs::Entity> _balls;
    int _ballCount = 200;
    float _restitution = 0.2f;
    int _groundHits = 0;
    int debugImage = 0;
};


class DebugState : public GameState
{
public:

    void on_enter(GameCtrl& game, Engine& engine) override;
    void on_exit(GameCtrl& game, Engine& engine) override;
    void on_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_fixed_update(GameCtrl& game, Engine& engine, float delta) override;
    void on_render(GameCtrl& game, Engine& engine, float delta) override;


};