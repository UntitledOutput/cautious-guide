#pragma once
#include "engine.hpp"
#include "core/ecs.hpp"

struct Player
{
    
};

struct PlayerCamera
{
    float angle;
    float yValue;

    float centerRadius = 3.f;
    float edgeRadius = 2.f;

    float yLookOffset;

    ecs::Entity player = ecs::null_entity;
};

void update_player(Engine& engine, float delta);
