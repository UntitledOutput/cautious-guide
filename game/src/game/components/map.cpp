#include "game/components/map.hpp"

#include "core/ecs.hpp"
#include "core/components/transform.hpp"
#include "graphics/model.hpp"
#include "scene/render_system.hpp"
#include "core/components/hierarchy.hpp"
#include <core/hierarchy.hpp>

void setup_map(Engine& engine, ecs::Entity map) {

    var mapModelHandle = engine.models().load("assets/models/map.glb");

    scene::Transform gt;
    gt.scale *= 20;
    var model = scene::spawn_model(engine.world(), mapModelHandle, gt);


    var& gc = engine.world().add<physics::Collider>(map);
    gc.shape = physics::ShapeType::Mesh;
    gc.mesh.id = engine.collision_mesh(mapModelHandle,gt.scale);
    gc.layer = 1 << 0; // L_Ground
    engine.world().get<ecs::Name>(map).value = "Map";
    engine.world().add<scene::Transform>(map);

    scene::attach(engine.world(),model,map);
}
