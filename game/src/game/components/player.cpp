#include "game/components/player.hpp"

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
            move += right * moveAxis.x;

            wt.position += math::normalize(move) * 2.f * deltaTime;
        });

    // camera updating
    engine.world().view<scene::Transform, PlayerCamera>().each(
        [&](ecs::Entity e, scene::Transform& wt, PlayerCamera& _cam)
        { 
            var playerEntity = engine.world().view<Player>().first();
            var playerPosition = engine.world().get<scene::Transform>(playerEntity).position;

            var offset = math::vec3{};

            offset.x = std::sin(_cam.angle);
            offset.z = std::cos(_cam.angle);

            offset *= _cam.centerRadius;

            offset += math::vec3{0, _cam.yValue + _cam.yLookOffset, 0};
            wt.position = playerPosition + offset;
            wt.look_at( playerPosition + math::vec3{0, _cam.yValue, 0} );
        });
}
