#pragma once
#include "engine.hpp"
#include "core/commands.hpp"
#include "debug/debug.hpp"
#include "graphics/model.hpp"
#include "loaders/bfres/sarc.hpp"

struct SplatGame : GameCtrl
{
	void on_init(Engine&) override;
	void on_render(Engine&, double alpha) override;
    
    void on_update(Engine&, double delta) override;
    void on_fixed_update(Engine&, double delta) override;

private:
    ecs::CommandBuffer commandBuffer;

    graphics::MeshHandle mesh;
    graphics::MaterialHandle mat;
    graphics::ShaderHandle shader;
    graphics::TextureHandle tex;
    graphics::ModelHandle model;
    std::unique_ptr<SarcArchive> judge;

    FreeCam _freecam;
    bool f1Held;
};
