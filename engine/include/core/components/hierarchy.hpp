#pragma once

#include "core/ecs.hpp"

struct Hierarchy
{
    ecs::Entity parent = ecs::null_entity;
    ecs::Entity firstChild = ecs::null_entity;
    ecs::Entity nextSibling = ecs::null_entity;
    ecs::Entity prevSibling = ecs::null_entity;
    u32 parentNode = UINT32_MAX; // attach to a node/bone of the parent's model instead of its origin
};
