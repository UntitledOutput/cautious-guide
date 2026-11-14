#include "states/main_state.hpp"

#include "engine.hpp"
#include "core/debug.hpp"
#include "debug/debug.hpp"
#include "game/game.hpp"
#include "graphics/primitive.hpp"
#include "scene/render_system.hpp"
#include "states/state_base.hpp"
#include "graphics/camera.hpp"


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

void MatchScene::on_enter(GameCtrl& game, Engine& engine)
{
    GameState::on_enter(game, engine);

    var cam = engine.world().create();
    engine.world().add<graphics::Camera>(cam);
    engine.world().add<FreeCam>(cam);
    engine.world().add<scene::Transform>(cam);

    engine.world().get<scene::Transform>(cam).position = {5, 5, 5};
    engine.world().get<scene::Transform>(cam).look_at({0, 0, 0});

    var model = get_primitive(PrimitiveType::Plane, engine);
    var e = scene::spawn_model(engine.world(), model);
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
    scene::render_models(engine.world(), engine.models(), engine.renderer());

        // window 1: top-right immediate debug menu
    updateDebugMenu(engine, delta);
}

