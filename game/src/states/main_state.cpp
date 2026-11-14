#include "states/main_state.hpp"

#include "engine.hpp"
#include "core/debug.hpp"
#include "debug/debug.hpp"
#include "game/game.hpp"
#include "graphics/primitive.hpp"
#include "scene/render_system.hpp"
#include "states/state_base.hpp"
#include "graphics/camera.hpp"
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
    constexpr u32 L_Ground = 1 << 0, L_Ball = 1 << 1;
}
#include "physics/world.hpp"
#include <imgui.h>
#include <random>

void MatchScene::on_enter(GameCtrl& game, Engine& engine)
{
    GameState::on_enter(game, engine);
    engine.physics().events().clear();

    var cam = engine.world().create();
    engine.world().add<graphics::Camera>(cam);
    engine.world().add<FreeCam>(cam);
    engine.world().add<scene::Transform>(cam);

    scene::Transform gt;
    gt.scale = {40, 1, 40}; // match this to your plane primitive's size
    var ground = scene::spawn_model(engine.world(), get_primitive(PrimitiveType::Plane, engine), gt);
    var& gc = engine.world().add<physics::Collider>(ground);
    gc.shape = physics::ShapeType::AABB;
    gc.box.halfExtents = {20, 0.5f, 20};
    gc.offset = {0, -0.5f, 0}; // visual plane at y=0, collision slab below it
    gc.layer = L_Ground;

    engine.physics().events().subscribe(L_Ball, L_Ground,
                                        [this](const physics::CollisionEvent& ev)
                                        {
                                            if (ev.phase == physics::ContactPhase::Begin)
                                                ++_groundHits;
                                        });

    engine.world().get<scene::Transform>(cam).position = {5, 5, 5};
    engine.world().get<scene::Transform>(cam).look_at({0, 0, 0});

    spawn_balls(engine);

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
    free_cam(engine, delta);
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
    updateDebugMenu(engine, delta);

        ImGui::Begin("physics test");
    ImGui::Text("bodies: %d", int(_balls.size()));
    ImGui::Text("ground hits: %d", _groundHits);
    ImGui::SliderInt("count", &_ballCount, 1, 1000);
    ImGui::SliderFloat("restitution", &_restitution, 0.0f, 1.0f);
    if (ImGui::Button("respawn") || engine.input().key_down(SDL_SCANCODE_R))
        spawn_balls(engine);
    ImGui::End();
}

void MatchScene::spawn_balls(Engine& engine)
{

    var& w = engine.world();
    for (var e : _balls)
    {
        engine.physics().on_destroyed(e);
        w.destroy(e);
    }
    _balls.clear();
    _groundHits = 0;

    std::mt19937 rng(1234); // fixed seed so runs are repeatable
    std::uniform_real_distribution<float> jitter(-0.2f, 0.2f);
    var sphere = get_primitive(PrimitiveType::Sphere, engine);

    const int side = int(std::ceil(std::cbrt(float(_ballCount))));
    for (int i = 0; i < _ballCount; ++i)
    {
        const int x = i % side, z = (i / side) % side, y = i / (side * side);
        scene::Transform t;
        t.position = {(x - side * 0.5f) * 1.2f + jitter(rng), 3.0f + y * 1.2f, (z - side * 0.5f) * 1.2f + jitter(rng)};

        var e = scene::spawn_model(w, sphere, t);
        var& c = w.add<physics::Collider>(e);
        c.sphere.radius = 1.0f;
        c.layer = L_Ball;
        w.add<physics::RigidBody>(e).restitution = _restitution;
        _balls.push_back(e);
    }
}

