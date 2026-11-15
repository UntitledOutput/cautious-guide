#pragma once
#include "engine.hpp"
#include "core/ecs.hpp"

struct Player
{
    
};

struct PlayerCamera
{
    float angle;
    float yValue = 0.75f;

    float centerRadius = 5.f;
    float edgeRadius = 4.f;

    float yLookOffset = 1.f;

    ecs::Entity player = ecs::null_entity;
};

void update_player(Engine& engine, float delta);
