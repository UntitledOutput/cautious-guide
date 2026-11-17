#include "fx/particle.hpp"
#include <algorithm>

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

    void ParticleMgr::emit(ParticleEmitter& e, math::vec3 pos, math::vec3 vel, float spread, u32 count, math::vec4 color)
    {
        if (count == 0)
            return;
        e.pending.push_back({{pos.x, pos.y, pos.z, float(count)}, {vel.x, vel.y, vel.z, spread}, color});
    }

    void ParticleMgr::flush(ecs::World& world, float dt)
    {
        struct Batch { ParticleEmitter* e; u32 offset, count, total; };
        std::vector<Batch> batches;
        emitStaging.clear();
        ++frame;

        
        
        world.view<ParticleEmitter>().each([&](ecs::Entity, ParticleEmitter& e)
        {
            if (e.capacity == 0 || e.pending.empty())
                return;
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

        
        world.view<ParticleEmitter>().each([&](ecs::Entity, ParticleEmitter& e)
        {
            if (e.capacity == 0)
                return;

            var d = renderer.make_dispatch(shaders.update);
            d.groups = renderer.groups_for(shaders.update, e.capacity);
            renderer.set_dispatch_buffer(d, "Instances", e.particleBuffer, graphics::Access::ReadWrite);
            renderer.set_dispatch_param(d, "gravity", gravity);
            renderer.set_dispatch_param(d, "dt", dt);
            renderer.set_dispatch_param(d, "drag", drag);
            renderer.set_dispatch_param(d, "capacity", e.capacity);
            renderer.submit_dispatch(std::move(d));

            
            renderer.submit_draw_call({.mesh = quad, .material = e.material, .model = math::mat4::identity(),
                                       .instanceCount = e.capacity, .instanceBuffer = e.particleBuffer});
        });
    }
}
