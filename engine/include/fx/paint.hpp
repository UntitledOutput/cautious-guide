#pragma once

#include "core/ecs.hpp"
#include "graphics/model.hpp"
#include "graphics/render.hpp"

#include <array>
#include <vector>

namespace fx
{
    struct Splat
    {
        math::vec4 positionSize;
        math::vec4 normalFalloff;
        math::vec4 colorStrength; // color index, strength, noise amount
    };

    struct PaintableObject
    {
        graphics::RenderTargetHandle bakeTarget = graphics::invalid_handle; // position + normal in uv1 space
        graphics::TextureHandle paintTexture = graphics::invalid_handle;
        graphics::TextureHandle displayTexture = graphics::invalid_handle; // extended, sample this one
        std::vector<Splat> pending;
    };

    struct PaintShaders
    {
        graphics::ShaderHandle bake;
        graphics::ComputeShaderHandle splat;
        graphics::ComputeShaderHandle extend;
    };

    class PaintMgr
    {
    public:
        PaintMgr(graphics::RenderMgr& _renderer, const PaintShaders& _shaders);

        PaintableObject create_paintable(const graphics::ModelAsset& model);
        void apply_paint(PaintableObject& paintable, math::vec3 position, math::vec3 normal, int color, float size);
        void flush(ecs::World& world); // once per frame, before RenderMgr::flush

        int globalPaintTextureSize = 1024;
        float paintFalloff = 5.0f;
        float noiseAmount = 1.0f;
        float noiseScale = 15.0f;
        int extendRadius = 2;
        graphics::TextureHandle noiseTexture;

        std::array<math::vec4, 4> paintColors = {{
            {1, 0, 0, 1},
            {0, 1, 0, 1},
            {0, 0, 1, 1},
            {1, 1, 0, 1},
        }};

    private:
        graphics::RenderMgr& renderer;
        PaintShaders shaders;
        graphics::MaterialHandle bakeMaterial;
        graphics::BufferHandle splatBuffer = graphics::invalid_handle;
        std::vector<Splat> splatStaging;
    };
}
