#pragma once
#include "core/ecs.hpp"
#include "core/components/transform.hpp"
#include "graphics/components.hpp"
#include "graphics/model.hpp"

namespace scene
{
    void render_models(ecs::World& world, const graphics::ModelCache& models, graphics::RenderMgr& r);
    ecs::Entity spawn_model(ecs::World& w, graphics::ModelHandle model, const scene::Transform& t = {});
    void run_animators(ecs::World& world, const graphics::ModelCache& models, float dt);
}
