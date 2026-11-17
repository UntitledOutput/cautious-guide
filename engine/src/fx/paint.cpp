#include "fx/paint.hpp"

#include <algorithm>
#include <glad/glad.h>

#include "core/components/transform.hpp"

namespace fx
{
    PaintMgr::PaintMgr(graphics::RenderMgr& _renderer, const PaintShaders& _shaders)
        : renderer(_renderer), shaders(_shaders)
    {
        bakeMaterial = renderer.create_material(shaders.bake);
        renderer.set_material_render_params(bakeMaterial, {.blend = false, .depthWrite = false, .cullBack = false});
        noiseTexture = renderer.white_texture();
    }

    PaintableObject PaintMgr::create_paintable(const graphics::ModelAsset& model)
    {
        const int size = globalPaintTextureSize;
        PaintableObject p;

        p.bakeTarget = renderer.create_render_target({size, size, {GL_RGBA32F, GL_RGBA16F}});

        const std::vector<byte> blank(size_t(size) * size * 2, 0);
        const graphics::TextureDescriptor paintDesc{size, size, blank, GL_RG, false, false, true, true};
        p.paintTexture = renderer.create_texture(paintDesc);
        p.displayTexture = renderer.create_texture(paintDesc);

        // draws next flush, before the splat dispatches
        for (const auto& part : model.parts)
        {
            if (part.skin >= 0)
                continue;
            renderer.submit_draw_call(
                {.mesh = part.mesh, .material = bakeMaterial, .model = model.restWorld[part.node], .target = p.bakeTarget});
        }

        return p;
    }

    void PaintMgr::apply_paint(PaintableObject& paintable, math::vec3 position, math::vec3 normal, int color, float size)
    {
        paintable.pending.push_back({
            {position.x, position.y, position.z, size},
            {normal.x, normal.y, normal.z, paintFalloff},
            {float(color), 1.0f, noiseAmount, 0.0f},
        });
    }

    void PaintMgr::flush(ecs::World& world)
    {
        struct Batch
        {
            const PaintableObject* paintable;
            u32 offset, count;
        };
        std::vector<Batch> batches;
        splatStaging.clear();

        world.view<scene::WorldTransform, PaintableObject>().each(
            [&](ecs::Entity, scene::WorldTransform& wt, PaintableObject& p)
            {
                if (p.pending.empty())
                    return;

                const math::mat4 toLocal = math::inverse(wt.matrix);
                const float scale = math::length(math::transform_dir(toLocal, {1, 0, 0}));

                const u32 offset = u32(splatStaging.size());
                for (const Splat& s : p.pending)
                {
                    const math::vec3 pos = math::transform_point(toLocal, {s.positionSize.x, s.positionSize.y, s.positionSize.z});
                    const math::vec3 n = math::normalize(
                        math::transform_dir(toLocal, {s.normalFalloff.x, s.normalFalloff.y, s.normalFalloff.z}));

                    splatStaging.push_back({
                        {pos.x, pos.y, pos.z, s.positionSize.w * scale},
                        {n.x, n.y, n.z, s.normalFalloff.w},
                        s.colorStrength,
                    });
                }

                batches.push_back({&p, offset, u32(p.pending.size())});
                p.pending.clear();
            });

        if (batches.empty())
            return;

        const size_t bytes = splatStaging.size() * sizeof(Splat);
        if (splatBuffer == graphics::invalid_handle)
            splatBuffer = renderer.create_buffer({std::max(bytes, 64 * sizeof(Splat)), graphics::BufferUsage::Storage, {}});
        else if (renderer.buffer_size(splatBuffer) < bytes)
            renderer.resize_buffer(splatBuffer, bytes * 2);
        renderer.update_buffer(splatBuffer, 0, {reinterpret_cast<const byte*>(splatStaging.data()), bytes});

        const u32 size = u32(globalPaintTextureSize);
        for (const Batch& b : batches)
        {
            const PaintableObject& p = *b.paintable;
            const graphics::TextureHandle position = renderer.render_target_texture(p.bakeTarget, 0);
            const graphics::TextureHandle normal = renderer.render_target_texture(p.bakeTarget, 1);

            var splat = renderer.make_dispatch(shaders.splat);
            splat.groups = renderer.groups_for(shaders.splat, size, size);
            renderer.set_dispatch_buffer(splat, "Splats", splatBuffer, graphics::Access::Read);
            renderer.set_dispatch_image(splat, "paintTex", p.paintTexture, graphics::Access::ReadWrite);
            renderer.set_dispatch_image(splat, "positionTex", position, graphics::Access::Read);
            renderer.set_dispatch_image(splat, "normalTex", normal, graphics::Access::Read);
            renderer.set_dispatch_image(splat, "noiseTex", noiseTexture, graphics::Access::Read);
            renderer.set_dispatch_param(splat, "splatOffset", b.offset);
            renderer.set_dispatch_param(splat, "splatCount", b.count);
            renderer.set_dispatch_param(splat, "noiseScale", noiseScale);
            renderer.submit_dispatch(std::move(splat));

            var extend = renderer.make_dispatch(shaders.extend);
            extend.groups = renderer.groups_for(shaders.extend, size, size);
            renderer.set_dispatch_image(extend, "paintTex", p.paintTexture, graphics::Access::Read);
            renderer.set_dispatch_image(extend, "positionTex", position, graphics::Access::Read);
            renderer.set_dispatch_image(extend, "displayTex", p.displayTexture, graphics::Access::Write);
            renderer.set_dispatch_param(extend, "extendRadius", extendRadius);
            renderer.submit_dispatch(std::move(extend));
        }
    }
}
