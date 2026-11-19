#pragma once
#include "engine.hpp"
#include "core/ecs.hpp"

struct SplatGame;

struct Player
{
    float moveSpeed = 3.0f;
    float jumpSpeed = 30.0f;
    float gravity = 80.0f;
    float verticalVelocity = 0.0f;

    ecs::Entity weapon;
    ecs::Entity model;
};

struct Weapon 
{
    float particleSpeed = 5.0f;
    float particleSize = 2.0f;
    float shootCooldown = 0.1f;

    ecs::Entity paintEmitter;

    void shoot(Engine& engine, SplatGame& game, ecs::Entity& p);

private:
    float lastShootTime = 0.0f;
};

void update_player(Engine& engine, SplatGame& game, float delta); // camera, per frame
void update_player_movement(Engine& engine, float dt); // fixed tick


struct PlayerCamera
{
    float angle;
    float yValue = 0.75f;

    float centerRadius = 5.f;
    float edgeRadius = 4.f;

    float yLookOffset = 1.f;
    float yScale = 1.f;

    float angleSensitivity = 0.05f;
    float ySensitivity = 0.2f;

    ecs::Entity player = ecs::null_entity;
};