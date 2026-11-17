#pragma once

#include "math/vector.hpp"
#include <graphics/render.hpp>
#include <graphics/vertex.hpp>
#include <core/ecs.hpp>

namespace fx
{

    struct Particle
    {
        math::vec4 position;
        math::vec4 velocitySize;
        math::vec4 color;
    };

    struct EmitRequest {
        math::vec4 positionCount;
        math::vec4 velocitySpread;
        math::vec4 color;
    };

    struct ParticleShaderContainer
    {
        graphics::ComputeShaderHandle emit;
        graphics::ComputeShaderHandle update;
        graphics::ShaderHandle render;
    };

    struct ParticleEmitter {
        graphics::BufferHandle particleBuffer = graphics::invalid_handle;
        graphics::MaterialHandle material = graphics::invalid_handle;
        u32 capacity = 0;
        u32 head = 0;
        float size = 0.1f;
        float lifetime = 2.0f;
        std::vector<EmitRequest> pending;
    };

    class ParticleMgr {
    public:
        ParticleMgr(graphics::RenderMgr& r, const ParticleShaderContainer& s);
        ParticleEmitter create_emitter(u32 capacity);
        void emit(ParticleEmitter& e, math::vec3 pos, math::vec3 vel, float spread, u32 count, math::vec4 color);
        void flush(ecs::World& world, float dt); // before RenderMgr::flush

        math::vec4 gravity = {0, -9.8f, 0, 0};
        float drag = 0.5f;
    private:
        graphics::RenderMgr& renderer;
        ParticleShaderContainer shaders;
        graphics::BufferHandle emitBuffer = graphics::invalid_handle;
        std::vector<EmitRequest> emitStaging;
        graphics::MeshHandle quad;
        u32 frame = 0;
    };


}