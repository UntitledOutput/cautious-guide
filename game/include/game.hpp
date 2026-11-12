#pragma once
#include "engine.hpp"
#include "debug/debug.hpp"

struct SplatGame : GameCtrl
{
	void on_init(Engine&) override;
	void on_render(Engine&, double alpha) override;
    
    void on_update(Engine&, double delta) override;
    void on_fixed_update(Engine&, double delta) override;

private:
    graphics::MeshHandle mesh;
    graphics::MaterialHandle mat;
    graphics::ShaderHandle shader;
    graphics::TextureHandle tex;

    FreeCam _freecam;
    bool f1Held;
};
