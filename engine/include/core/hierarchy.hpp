#pragma once
#include <cstdint>

#include "ecs.hpp"
#include "util.hpp"


namespace ecs
{
    void detatch(ecs::World& entity, ecs::Entity child);
    void attach(ecs::World& entity, ecs::Entity child, ecs::Entity parent, u32 node = UINT32_MAX);
    void destroy_recursive(ecs::World& w, ecs::Entity root);
}; // namespace ecs
