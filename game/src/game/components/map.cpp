#include "game/components/map.hpp"

#include "core/ecs.hpp"
#include "core/components/transform.hpp"
#include "graphics/model.hpp"
#include "scene/render_system.hpp"
#include "core/components/hierarchy.hpp"
#include <core/hierarchy.hpp>

void setup_map(Engine& engine, ecs::Entity map, fx::PaintMgr& paint) {

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

    engine.world().add<Map>(map).model = model;

    const var& asset = engine.models().get(mapModelHandle);
    var& paintable = engine.world().add<fx::PaintableObject>(model, paint.create_paintable(asset));

    var& r = engine.renderer();
    for (var mat : asset.materials)
    {
        r.set_material_param(mat, "usePaintMap", 1);
        r.set_material_texture(mat, "paintMap", paintable.displayTexture);
        for (int i = 0; i < 4; ++i)
            r.set_material_param(mat, "paintColor" + std::to_string(i), paint.paintColors[i]);
    }
}
