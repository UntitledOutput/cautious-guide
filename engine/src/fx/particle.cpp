#include "fx/particle.hpp"
#include <algorithm>
#include <random>

namespace
{
    math::vec3 xyz(const math::vec4& v) { return {v.x, v.y, v.z}; }
    math::vec4 with_w(math::vec3 v, float w) { return {v.x, v.y, v.z, w}; }
}

namespace fx
{


    
    ParticleMgr::ParticleMgr(graphics::RenderMgr& r, const ParticleShaderContainer& s)
        : renderer(r), shaders(s)
    {
        const Vertex verts[] = {
            {.position = {-1, -1, 0}, .uv0 = {0, 0}},
            {.position = { 1, -1, 0}, .uv0 = {1, 0}},
            {.position = { 1,  1, 0}, .uv0 = {1, 1}},
            {.position = {-1,  1, 0}, .uv0 = {0, 1}},
        };
        const u32 idx[] = {0, 1, 2, 2, 3, 0};
        quad = renderer.create_mesh({VertexFormat::Position | VertexFormat::UV0, verts, idx});
    }

    ParticleEmitter ParticleMgr::create_emitter(u32 capacity)
    {
        ParticleEmitter e;
        e.capacity = capacity;

        
        const std::vector<Particle> dead(capacity, Particle{});
        e.particleBuffer = renderer.create_buffer({
            capacity * sizeof(Particle), graphics::BufferUsage::Storage,
            {reinterpret_cast<const byte*>(dead.data()), dead.size() * sizeof(Particle)}});

        e.material = renderer.create_material(shaders.render);
        renderer.set_material_render_params(e.material, {.blend = true, .depthWrite = false, .cullBack = false});
        return e;
    }

    void ParticleMgr::emit_cpu(ParticleEmitter& e, const EmitRequest& r)
    {
        std::uniform_real_distribution<float> u01(0.0f, 1.0f), u11(-1.0f, 1.0f);
        const u32 n = std::min(u32(r.positionCount.w), e.capacity);
        for (u32 i = 0; i < n; ++i)
        {
            const math::vec3 dir = math::normalize(math::vec3{u11(rng), u11(rng), u11(rng)} + math::vec3{1e-5f, 0, 0});
            const math::vec3 vel = xyz(r.velocitySpread) + dir * (r.velocitySpread.w * u01(rng));
            const float life = e.lifetime * (0.75f + 0.25f * u01(rng));

            e.cpu[e.head] = {with_w(xyz(r.positionCount), life), with_w(vel, e.size), r.color};
            e.cpuBounces[e.head] = 0;
            e.head = (e.head + 1) % e.capacity;
        }
    }

void ParticleMgr::simulate_cpu(ecs::World& w, ecs::Entity id, ParticleEmitter& e, float dt)
{
    const physics::CastFilter filter{e.collisionMask};
    const float damp = std::exp(-e.drag * dt);
    constexpr float skin = 0.001f;

    for (u32 i = 0; i < e.capacity; ++i)
    {
        Particle& p = e.cpu[i];
        float life = p.position.w;
        if (life <= 0.0f)
            continue;
        life -= dt;

        math::vec3 pos = xyz(p.position);
        math::vec3 vel = (xyz(p.velocitySize) + xyz(e.gravity) * dt) * damp;

        const math::vec3 step = vel * dt;
        const float dist = math::length(step);
        if (dist > 1e-6f)
        {
            const math::vec3 dir = step / dist;
            const auto hit = e.collisionRadius > 0.0f
                ? physics->sphere_cast(w, pos, e.collisionRadius, dir, dist, filter)
                : physics->raycast(w, pos, dir, dist, filter);

            if (hit)
            {
                pos += dir * hit->distance + hit->normal * skin;   
                hits.push_back({id, hit->entity, hit->point, hit->normal, p.color, e.cpuBounces[i]});

                if (++e.cpuBounces[i] > e.maxBounces)
                    life = 0.0f;
                else
                    vel -= hit->normal * ((1.0f + e.bounce) * math::dot(vel, hit->normal));
            }
            else
                pos += step;
        }

        p.position = with_w(pos, life);
        p.velocitySize = with_w(vel, p.velocitySize.w);
    }

    renderer.update_buffer(e.particleBuffer, 0,
                           {reinterpret_cast<const byte*>(e.cpu.data()), e.cpu.size() * sizeof(Particle)});
}

    void ParticleMgr::emit(ParticleEmitter& e, math::vec3 pos, math::vec3 vel, float spread, u32 count, math::vec4 color)
    {
        if (count == 0)
            return;
        e.pending.push_back({{pos.x, pos.y, pos.z, float(count)}, {vel.x, vel.y, vel.z, spread}, color});
    }

    void ParticleMgr::flush(ecs::World& world, physics::PhysicsWorld& physicsWorld, float dt)
    {
        physics = &physicsWorld;
        struct Batch { ParticleEmitter* e; u32 offset, count, total; };
        std::vector<Batch> batches;
        emitStaging.clear();
        ++frame;

        
        
        world.view<ParticleEmitter>().each([&](ecs::Entity, ParticleEmitter& e)
        {
            if (e.capacity == 0 || e.pending.empty())
                return;

            if (e.collide)
            {
                if (e.cpu.size() != e.capacity) { e.cpu.assign(e.capacity, Particle{}); e.cpuBounces.assign(e.capacity, 0); }
                for (const EmitRequest& r : e.pending)
                    emit_cpu(e, r);
                e.pending.clear();
                return;
            }
            
            const u32 offset = u32(emitStaging.size());
            u32 total = 0;
            for (EmitRequest r : e.pending)
            {
                const u32 n = u32(r.positionCount.w);
                r.positionCount.w = float(total);
                total += n;
                emitStaging.push_back(r);
            }
            batches.push_back({&e, offset, u32(e.pending.size()), total});
            e.pending.clear();
        });

        
        if (!batches.empty())
        {
            const size_t bytes = emitStaging.size() * sizeof(EmitRequest);
            if (emitBuffer == graphics::invalid_handle)
                emitBuffer = renderer.create_buffer({std::max(bytes, 64 * sizeof(EmitRequest)), graphics::BufferUsage::Storage, {}});
            else if (renderer.buffer_size(emitBuffer) < bytes)
                renderer.resize_buffer(emitBuffer, bytes * 2);
            renderer.update_buffer(emitBuffer, 0, {reinterpret_cast<const byte*>(emitStaging.data()), bytes});

            for (const Batch& b : batches)
            {
                ParticleEmitter& e = *b.e;
                const u32 spawn = std::min(b.total, e.capacity); 

                var d = renderer.make_dispatch(shaders.emit);
                d.groups = renderer.groups_for(shaders.emit, spawn);
                renderer.set_dispatch_buffer(d, "EmitRequests", emitBuffer, graphics::Access::Read);
                renderer.set_dispatch_buffer(d, "Instances", e.particleBuffer, graphics::Access::Write);
                renderer.set_dispatch_param(d, "requestOffset", b.offset);
                renderer.set_dispatch_param(d, "requestCount", b.count);
                renderer.set_dispatch_param(d, "spawnCount", spawn);
                renderer.set_dispatch_param(d, "skip", b.total - spawn);
                renderer.set_dispatch_param(d, "head", e.head);
                renderer.set_dispatch_param(d, "capacity", e.capacity);
                renderer.set_dispatch_param(d, "seed", frame);
                renderer.set_dispatch_param(d, "size", e.size);
                renderer.set_dispatch_param(d, "lifetime", e.lifetime);
                renderer.submit_dispatch(std::move(d));

                e.head = (e.head + spawn) % e.capacity;
            }
        }

        
        world.view<ParticleEmitter>().each([&](ecs::Entity id, ParticleEmitter& e)
        {
            if (e.capacity == 0)
                return;

            if (e.collide)
            {
                if (e.cpu.size() != e.capacity) { e.cpu.assign(e.capacity, Particle{}); e.cpuBounces.assign(e.capacity, 0); }
                simulate_cpu(world, id, e, dt);
            }
            else {

                var d = renderer.make_dispatch(shaders.update);
                d.groups = renderer.groups_for(shaders.update, e.capacity);
                renderer.set_dispatch_buffer(d, "Instances", e.particleBuffer, graphics::Access::ReadWrite);
                renderer.set_dispatch_param(d, "gravity", e.gravity);
                renderer.set_dispatch_param(d, "dt", dt);
                renderer.set_dispatch_param(d, "drag", e.drag);
                renderer.set_dispatch_param(d, "capacity", e.capacity);
                renderer.submit_dispatch(std::move(d));

            }

            
            renderer.submit_draw_call({.mesh = quad, .material = e.material, .model = math::mat4::identity(),
                                       .instanceCount = e.capacity, .instanceBuffer = e.particleBuffer});
        });

        if (onHit)
        for (const ParticleHit& h : hits)
            onHit(h);
        hits.clear();
    }
}
