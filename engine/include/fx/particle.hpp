#pragma once

#include "math/vector.hpp"
#include <graphics/render.hpp>
#include <graphics/vertex.hpp>
#include <core/ecs.hpp>
#include <physics/world.hpp>
#include <random>

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

    struct ParticleHit {
        ecs::Entity emitter;
        ecs::Entity hitEntity;
        math::vec3 position, normal;
        math::vec4 color;
        u8 bounce;
    };

    struct ParticleEmitter {
        graphics::BufferHandle particleBuffer = graphics::invalid_handle;
        graphics::MaterialHandle material = graphics::invalid_handle;
        u32 capacity = 0;
        u32 head = 0;
        float size = 0.1f;
        float lifetime = 2.0f;
        std::vector<EmitRequest> pending;

        math::vec4 gravity = {0, -9.8f, 0, 0};
        float drag = 0.5f;

        bool collide = false;
        u32 collisionMask = ~0u;
        float collisionRadius = 0.5f;
        float bounce = 0.4f;
        u8 maxBounces = 3;

        std::vector<Particle> cpu;
        std::vector<u8> cpuBounces;
        float lifetimeLoss = 0.5f;
    };

    class ParticleMgr {
    public:
        ParticleMgr(graphics::RenderMgr& r, const ParticleShaderContainer& s);
        ParticleEmitter create_emitter(u32 capacity);
        void emit(ParticleEmitter& e, math::vec3 pos, math::vec3 vel, float spread, u32 count, math::vec4 color);
        // physics is passed per call: Engine::regen_world() replaces the PhysicsWorld on every state change
        void flush(ecs::World& world, physics::PhysicsWorld& physicsWorld, float dt); // before RenderMgr::flush

        std::function<void(const ParticleHit&)> onHit;
    
    private:
        graphics::RenderMgr& renderer;
        ParticleShaderContainer shaders;
        graphics::BufferHandle emitBuffer = graphics::invalid_handle;
        std::vector<EmitRequest> emitStaging;
        graphics::MeshHandle quad;
        u32 frame = 0;

        void emit_cpu(ParticleEmitter& e, const EmitRequest& r);
        void simulate_cpu(ecs::World& w, ecs::Entity id, ParticleEmitter& e, float dt);

        physics::PhysicsWorld* physics = nullptr; // valid during flush only
        std::minstd_rand rng{1234};
        std::vector<ParticleHit> hits;
    };


}