#include "game/components/player.hpp"

#include <math/math.hpp>

#include "core/components/transform.hpp"

void update_player(Engine& engine, float deltaTime)
{

    // player updating
    engine.world().view<scene::Transform, Player>().each(
        [&](ecs::Entity e, scene::Transform& wt, Player& _player)
        { 
            var cameraEntity = engine.world().view<PlayerCamera>().first();

            
            var moveAxis = engine.actions().axis2(engine.actions().find("move"));

            if (moveAxis.magnitude() > 0.1f )
            {
                // set the world forward to the camera forward
                var camForward = engine.world().get<scene::Transform>(cameraEntity).forward();
                camForward.y = 0;
                camForward = math::normalize(camForward);

                wt.rotation = math::look_rotation(camForward, math::vec3{0, 1, 0});
            }

            var forward = wt.forward();
            var right = wt.right();

            var move = math::vec3{};
            move += forward * moveAxis.y;
            move -= right * moveAxis.x;
            move = math::normalize(move);


            if (math::dot(move,move) > 0.0f)
            {
                wt.rotation = math::look_rotation(move, math::vec3{0, 1, 0});

                var add = move * 3.f * deltaTime;

                wt.position += add;
            }
        });

    // camera updating
    engine.world().view<scene::Transform, PlayerCamera>().each(
        [&](ecs::Entity e, scene::Transform& wt, PlayerCamera& _cam)
        { 
            if (!engine.input().is_mouse_captured())
                return;

            float angle = engine.input().mouse_delta().x;
            angle += engine.actions().axis2(engine.actions().find("look")).x;
            _cam.angle -= (angle * 3.14) * deltaTime * 0.05f;

            float yV = engine.input().mouse_delta().y;
            yV += engine.actions().axis2(engine.actions().find("look")).y;
            _cam.yValue += (yV * 0.2f) * deltaTime;
            _cam.yValue = std::clamp(_cam.yValue,0.f,1.f);

            var playerEntity = engine.world().view<Player>().first();
            var playerPosition = engine.world().get<scene::Transform>(playerEntity).position;

            var offset = math::vec3{};

            offset.x = std::sin(_cam.angle);
            offset.z = std::cos(_cam.angle);

            offset *= math::lerp(_cam.centerRadius, _cam.edgeRadius, math::abs(_cam.yValue - 0.5f) * 2);

            offset.y += (_cam.yValue);

            offset += math::vec3{0, _cam.yValue, 0};
            wt.position = playerPosition + offset;
            wt.look_at( playerPosition + math::vec3{0, _cam.yLookOffset, 0} );
        });
}
