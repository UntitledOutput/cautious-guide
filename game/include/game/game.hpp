#pragma once
#include "engine.hpp"
#include "core/commands.hpp"
#include "graphics/model.hpp"
#include "states/state_base.hpp"

struct SplatGame : GameCtrl
{
	void on_init(Engine&) override;
	void on_render(Engine&, double alpha) override;
    
    void on_update(Engine&, double delta) override;
    void on_fixed_update(Engine&, double delta) override;

    void load_new_state(std::unique_ptr<GameState> state);

private:
    ecs::CommandBuffer commandBuffer;

    std::unique_ptr<GameState> currentGameState;
    std::unique_ptr<GameState> cachedGameState;

};
