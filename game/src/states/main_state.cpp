#include "states/main_state.hpp"

#include <iostream>
#include <ranges>

#include "engine.hpp"
#include "core/debug.hpp"
#include "core/hierarchy.hpp"
#include "debug/debug.hpp"
#include "game/game.hpp"
#include "game/components/player.hpp"
#include "graphics/primitive.hpp"
#include "scene/render_system.hpp"
#include "states/state_base.hpp"
#include "graphics/camera.hpp"
#include "loaders/bfres/sarc.hpp"
#include "scene/transform_system.hpp"


// base state

void GameState::on_enter(GameCtrl& game, Engine& engine)
{
    // clear world
    engine.regen_world();
}

// boot state

void BootState::on_enter(GameCtrl& game, Engine& engine)
{
    GameState::on_enter(game, engine);

    var splatGame = dynamic_cast<SplatGame*>(&game);
    splatGame->load_new_state(std::make_unique<MatchScene>());
}

void BootState::on_exit(GameCtrl& game, Engine& engine)
{
}

void BootState::on_update(GameCtrl& game, Engine& engine, float delta)
{
}

void BootState::on_fixed_update(GameCtrl& game, Engine& engine, float delta)
{
}

void BootState::on_render(GameCtrl& game, Engine& engine, float delta)
{
}

// match scene

#include <SDL.h>
namespace
{
    constexpr u32 L_Ground = 1 << 0, L_Player = 1 << 1;
}
#include "physics/world.hpp"
#include <imgui.h>
#include <random>

const SarcFile* get_sarc_file(Engine& engine,std::string path)
{ 
    var splitPath = util::split(path, ':');
    var main_path = splitPath[0];

    var archive = (SarcArchive*)engine.filesystem().read_file_auto(main_path);

    if (splitPath.size() == 3)
    {
        // theres a subfile
        var& entrySzs = archive->find(splitPath[1])->archive;
        return entrySzs->find(splitPath[2]);
    } else
    {
        return archive->find(splitPath[1]);
    }
}

void MatchScene::on_enter(GameCtrl& game, Engine& engine)
{
    GameState::on_enter(game, engine);
    engine.physics().events().clear();

    engine.input().set_mouse_captured(true);

    var cam = engine.world().create();
    engine.world().add<graphics::Camera>(cam);
    var& playerCamera = engine.world().add<PlayerCamera>(cam);
    playerCamera.centerRadius = 30;
    playerCamera.edgeRadius = 20;
    playerCamera.yLookOffset = 10;
    playerCamera.yScale = 20;
    playerCamera.angleSensitivity = 0.15f;
    engine.world().add<scene::Transform>(cam);


    engine.world().get<scene::Transform>(cam).position = {5, 5, 5};
    engine.world().get<scene::Transform>(cam).look_at({0, 0, 0});

    var playerEntity = engine.world().create();
    var& playerTransform = engine.world().add<scene::Transform>(playerEntity);
    playerTransform.position.y = 100.f;
    var& player = engine.world().add<Player>(playerEntity);
    player.moveSpeed = 15;

    var& c = engine.world().add<physics::Collider>(playerEntity);
    c.shape = physics::ShapeType::Capsule;
    c.capsule.radius = 5.f;
    c.capsule.halfHeight = 1.f;
    c.offset.y = c.capsule.halfHeight*2;
    c.layer = L_Player;
    engine.world().add<physics::RigidBody>(playerEntity);

    var& bfres = get_sarc_file(engine, "assets/Player.pack:Model/Player00.szs:Output.bfres")->bfres;
    var bfresModel = bfres->parse();

    var playerModelHandle = engine.models().load("assets/models/tofu.glb");

    playerModelHandle = engine.models().generate("plr", bfresModel.models[0]);

    var playerModel = scene::spawn_model(engine.world(), playerModelHandle);
    //engine.world().add<scene::Transform>(playerModel).scale = {1, 1, 1} * 4;
    scene::attach(engine.world(),playerModel,playerEntity);

    
    scene::Transform gt;
    gt.scale = {40, 1, 40}; // match this to your plane primitive's size
    var ground = scene::spawn_model(engine.world(), get_primitive(PrimitiveType::Plane, engine), gt);
    var& gc = engine.world().add<physics::Collider>(ground);
    gc.shape = physics::ShapeType::AABB;
    gc.box.halfExtents = {20, 0.5f, 20};
    gc.offset = {0, -0.5f, 0}; // visual plane at y=0, collision slab below it
    gc.layer = L_Ground;

}

void MatchScene::on_exit(GameCtrl& game, Engine& engine)
{

}

void free_cam(Engine& engine, float dt)
{
    engine.world().view<scene::Transform, FreeCam>().each(
        [&](ecs::Entity e, scene::Transform& wt, FreeCam& _cam)
        {
            var& in = engine.input();

            // hold right mouse to look, so the cursor is free otherwise
            bool wantLook = in.mouse_down(SDL_BUTTON_RIGHT);
            if (wantLook != _cam.looking)
            {
                in.set_mouse_captured(wantLook);
                _cam.looking = wantLook;
            }

            // load from quaternion
            var euler = math::to_euler(wt.rotation);

            if (_cam.looking)
            {
                euler.y -= in.mouse_delta().x * _cam.sensitivity;
                euler.x -= in.mouse_delta().y * _cam.sensitivity;
                euler.x = std::clamp(euler.x, -1.55f, 1.55f);
            }
            wt.rotation = math::from_euler(euler);

            var rot = wt.rotation;
            var forward = math::rotate(rot, math::vec3{0.0f, 0.0f, -1.0f});
            var right = math::rotate(rot, math::vec3{1.0f, 0.0f, 0.0f});

            var move = math::vec3{};

            var moveAxis = engine.actions().axis2(engine.actions().find("move"));

            move += forward * moveAxis.y;
            move += right * moveAxis.x;

            if (in.key_down(SDL_SCANCODE_E))
                move.y += 1.0f;
            if (in.key_down(SDL_SCANCODE_Q))
                move.y -= 1.0f;


            if (dot(move, move) > 0.0f)
            {
                float s = _cam.speed * (in.key_down(SDL_SCANCODE_LSHIFT) ? 4.0f : 1.0f);
                wt.position += normalize(move) * s * dt;
            }
        });
} 

void MatchScene::on_update(GameCtrl& game, Engine& engine, float delta)
{
    if (engine.input().key_press(SDL_SCANCODE_ESCAPE))
        engine.input().toggle_mouse_captured();

    if (engine.input().key_press(SDL_SCANCODE_F1))
    {
        var& w = engine.world();
        var cam = w.view<graphics::Camera>().first();
        if (cam != ecs::null_entity)
        {
            if (w.contains<FreeCam>(cam))
            {
                w.remove<FreeCam>(cam);
                engine.input().set_mouse_captured(true); // back to the player camera's capture
            }
            else
                w.add<FreeCam>(cam);
        }
    }

    free_cam(engine, delta);
    update_player(engine, delta);
    graphics::update_camera(engine);
}

void MatchScene::on_fixed_update(GameCtrl& game, Engine& engine, float delta)
{
}

void MatchScene::on_render(GameCtrl& game, Engine& engine, float delta)
{
    scene::update_world_transforms(engine.world(), engine.models());
    scene::render_models(engine.world(), engine.models(), engine.renderer());

        // window 1: top-right immediate debug menu
    debug::updateDebugMenu(engine, delta);

    ImGui::Begin("value tweaking");
    
    var entity = engine.world().view<physics::Collider>().first();
    var& component = engine.world().get<physics::Collider>(entity);

    ImGui::InputFloat("radius", &component.capsule.radius);
    ImGui::InputFloat("half", &component.capsule.halfHeight);

    ImGui::End();
}