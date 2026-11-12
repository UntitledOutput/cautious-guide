#include "scene/transform_system.hpp"

#include "core/components/hierarchy.hpp"
#include "core/components/transform.hpp"
#include "graphics/components.hpp"

namespace scene
{
    math::mat4 node_matrix(ecs::World& w, const graphics::ModelCache& models, ecs::Entity e, u32 node)
    {
        if (const Pose* p = w.try_get<Pose>(e); p && node < p->nodeWorld.size())
            return p->nodeWorld[node];
        if (const ModelRenderer* mr = w.try_get<ModelRenderer>(e); mr && mr->model != graphics::invalid_model)
        {
            const auto& asset = models.get(mr->model);
            if (node < asset.restWorld.size())
                return asset.restWorld[node];
        }
        return math::mat4::identity();
    }

    void propagate(ecs::World& w, const graphics::ModelCache& models, ecs::Entity e, const math::mat4& parentWorld)
    {
        const Transform* t = w.try_get<Transform>(e);
        const math::mat4 world = t ? parentWorld * t->matrix() : parentWorld;

        if (auto* wt = w.try_get<WorldTransform>(e))
            wt->matrix = world;

        const Hierarchy* h = w.try_get<Hierarchy>(e);
        if (!h)
            return;

        for (ecs::Entity c = h->firstChild; c != ecs::null_entity; c = w.get<Hierarchy>(c).nextSibling)
        {
            const u32 node = w.get<Hierarchy>(c).parentNode;
            const math::mat4 base = node == UINT32_MAX ? world : world * node_matrix(w, models, e, node);
            propagate(w, models, c, base);
        }
    }

    void update_world_transforms(ecs::World& w, const graphics::ModelCache& models)
    {
        w.view<Transform>().each(
            [&](ecs::Entity e, Transform&)
            {
                const Hierarchy* h = w.try_get<Hierarchy>(e);
                if (h && h->parent != ecs::null_entity)
                    return; // gets reached through its root

                propagate(w, models, e, math::mat4::identity());
            });
    }
}