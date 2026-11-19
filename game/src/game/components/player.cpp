#include "game/components/player.hpp"

#include <math/math.hpp>

#include "core/components/transform.hpp"
#include "debug/debug.hpp"
#include "game/game.hpp"
#include "game/components/map.hpp"
#include <graphics/components.hpp>
#include <scene/transform_system.hpp>

void update_player(Engine& engine, SplatGame& game, float deltaTime)
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

                if (engine.input().mouse_down(1))
                {

                    var& w = engine.world();
                    var ct = w.get<scene::Transform>(e);
                    var dir = math::rotate(ct.rotation, math::vec3{0, 0, -1});

                    var hit = engine.physics().raycast(w, ct.position, dir, 500.0f, physics::CastFilter{1 << 0});
                    // std::printf("[paint] hit=%d\n", hit.has_value());
                    if (hit)
                    {
                        var* m = w.try_get<Map>(hit->entity);
                        if (m)
                            game.paint->apply_paint(w.get<fx::PaintableObject>(m->model), hit->point, hit->normal, 0,
                                                    2.0f);
                    }
                }
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

    // basic player controller
    engine.world().view<Weapon>().each(
        [&](ecs::Entity e, Weapon& w)
        {
            var particleSys = engine.world().get<fx::ParticleEmitter>(w.paintEmitter);
            particleSys.size = w.particleSize;
        });

    // basic player controller
    engine.world().view<scene::Transform, Player, physics::CharacterController>().each(
        [&](ecs::Entity e, scene::Transform& wt, Player& p, physics::CharacterController& cc)
        {
            var& weapon = engine.world().get<Weapon>(p.weapon);

            weapon.shoot(engine, game, e);
        });
}

void update_player_movement(Engine& engine, float dt)
{
    var& w = engine.world();
    var cameraEntity = w.view<PlayerCamera>().first();
    const bool hasControl = cameraEntity != ecs::null_entity && !w.contains<FreeCam>(cameraEntity);


    w.view<scene::Transform, Player, physics::CharacterController>().each(
        [&](ecs::Entity e, scene::Transform& wt, Player& p, physics::CharacterController& cc)
        {
            var move = math::vec3{};
            if (hasControl) {
                var camForward = w.get<scene::Transform>(cameraEntity).forward();
                camForward.y = 0;
                camForward = math::normalize(camForward);
                wt.rotation = math::look_rotation(-camForward, math::vec3{0, 1, 0});

                const var moveAxis = engine.actions().axis2(engine.actions().find("move"));
                var move = -wt.forward() * moveAxis.y + wt.right() * moveAxis.x;
                if (math::dot(move, move) > 1.0f)
                    move = math::normalize(move); // only clamp, so half-tilted sticks stay half speed

            }

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

void Weapon::shoot(Engine& engine, SplatGame& game, ecs::Entity& p) {

    if (engine.time() - lastShootTime < shootCooldown)
        return;
    lastShootTime = engine.time();

    var player = engine.world().get<Player>(p);
    var model = engine.world().get<ModelRenderer>(player.model);

    var shootBone = scene::find_node(engine.models(), model.model, "Nose");
    var boneTransform = scene::node_world_matrix(engine.world(), engine.models(), player.model, shootBone);

    var boneRot = math::rotation(boneTransform);
    var bonePos = math::position(boneTransform);

    var fwd = math::rotate(boneRot, math::vec3{0, 1, 0});
    var playerForward = math::normalize(fwd);

    var shootPos = bonePos + playerForward * 2.f;
    var shootVel = playerForward * particleSpeed;
    float spread = 5.0f;
    math::vec4 color = {1, 0, 1, 1};

    var& particleSys = engine.world().get<fx::ParticleEmitter>(paintEmitter);
    //particleSys.gravity = {0, 0.0f, 0, 0};
    particleSys.drag = 0.0f;
    particleSys.lifetimeLoss = 1.0f;
    game.particles->emit(particleSys, shootPos, shootVel, spread, 1, color);

}
