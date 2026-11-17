#pragma once

#include "engine.hpp"
#include "core/ecs.hpp"
#include "fx/paint.hpp"

struct Map
{
    graphics::TextureHandle uvMapImage;
    ecs::Entity model;
};

void setup_map(Engine& engine, ecs::Entity map, fx::PaintMgr& paint);
