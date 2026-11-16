#pragma once

#include "engine.hpp"
#include "core/ecs.hpp"

struct Map {
    graphics::TextureHandle uvMapImage;
}

void setup_map(Engine& engine, const Map& map);