#pragma once
#include "core/ecs.hpp"
#include "graphics/model.hpp"

namespace scene
{
    math::mat4 node_matrix(ecs::World& w, const graphics::ModelCache& models, ecs::Entity e, u32 node);
    void propagate(ecs::World& w, const graphics::ModelCache& models, ecs::Entity e,
                   const math::mat4& parent_matrix);
    void update_world_transforms(ecs::World& w, const graphics::ModelCache& models);
}
