#include "game/components/player.hpp"

#include <math/math.hpp>

#include "core/components/transform.hpp"
#include "debug/debug.hpp"

void update_player(Engine& engine, float deltaTime)
{
    // camera updating
    engine.world().view<scene::Transform, PlayerCamera>().each(
        [&](ecs::Entity e, scene::Transform& wt, PlayerCamera& _cam)
        { 
            if (engine.world().contains<FreeCam>(e))
                return;


            if (engine.input().is_mouse_captured())
            {
                float angle = engine.input().mouse_delta().x;
                angle += engine.actions().axis2(engine.actions().find("look")).x;
                _cam.angle -= (angle * 3.14) * deltaTime * _cam.angleSensitivity;

                float yV = engine.input().mouse_delta().y;
                yV += engine.actions().axis2(engine.actions().find("look")).y;
                _cam.yValue += (yV * _cam.ySensitivity) * deltaTime;
                _cam.yValue = std::clamp(_cam.yValue, 0.f, 1.f);
            }

            var playerEntity = engine.world().view<Player>().first();
            var playerPosition = engine.world().get<scene::Transform>(playerEntity).position;

            var offset = math::vec3{};

            offset.x = std::sin(_cam.angle);
            offset.z = std::cos(_cam.angle);

            offset *= math::lerp(_cam.centerRadius, _cam.edgeRadius, math::abs(_cam.yValue - 0.5f) * 2);

            offset.y += (_cam.yValue * _cam.yScale);

            offset += math::vec3{0, _cam.yValue, 0};
            wt.position = playerPosition + offset;
            wt.look_at( playerPosition + math::vec3{0, _cam.yLookOffset, 0} );
        });
}

void update_player_movement(Engine& engine, float dt)
{
    var& w = engine.world();
    var cameraEntity = w.view<PlayerCamera>().first();
    if (cameraEntity == ecs::null_entity)
        return;

    w.view<scene::Transform, Player, physics::CharacterController>().each(
        [&](ecs::Entity e, scene::Transform& wt, Player& p, physics::CharacterController& cc)
        {
            var camForward = w.get<scene::Transform>(cameraEntity).forward();
            camForward.y = 0;
            camForward = math::normalize(camForward);
            wt.rotation = math::look_rotation(-camForward, math::vec3{0, 1, 0});

            const var moveAxis = engine.actions().axis2(engine.actions().find("move"));
            var move = -wt.forward() * moveAxis.y + wt.right() * moveAxis.x;
            if (math::dot(move, move) > 1.0f)
                move = math::normalize(move); // only clamp, so half-tilted sticks stay half speed

            if (cc.grounded)
            {
                p.verticalVelocity = -1.0f; // small push into the ground keeps slopes grounded
                if (engine.actions().pressed(engine.actions().find("jump")))
                    p.verticalVelocity = p.jumpSpeed;
            }
            else
            {
                if (cc.hitCeiling && p.verticalVelocity > 0.0f)
                    p.verticalVelocity = 0.0f;
                p.verticalVelocity -= p.gravity * dt;
            }

            const math::vec3 vel = move * p.moveSpeed + math::vec3{0, p.verticalVelocity, 0};
            engine.physics().move_character(w, e, vel * dt);
        });
}
