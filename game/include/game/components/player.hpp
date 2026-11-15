#pragma once
#include "engine.hpp"
#include "core/ecs.hpp"

struct Player
{
    float moveSpeed = 3.0f;
};

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

void update_player(Engine& engine, float delta);
