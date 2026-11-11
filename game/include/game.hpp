#pragma once
#include "engine.hpp"

struct SplatGame : GameCtrl
{
	void on_init(Engine&) override;
	void on_render(Engine&, double alpha) override;

private:
    MeshHandle mesh;
    MaterialHandle mat;
    ShaderHandle shader;
};
