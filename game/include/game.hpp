#pragma once
#include "engine.hpp"

struct SplatGame : GameCtrl
{
	void on_init(Engine&) override;
	void on_render(Engine&, double alpha) override;

private:
    graphics::MeshHandle mesh;
    graphics::MaterialHandle mat;
    graphics::ShaderHandle shader;
    graphics::TextureHandle tex;
};
