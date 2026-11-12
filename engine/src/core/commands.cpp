#include "core/commands.hpp"

#include "core/ecs.hpp"
#include "core/hierarchy.hpp"


void ecs::CommandBuffer::apply(ecs::World& world)
{
    for (ecs::Entity e : toDestroy)
        scene::destroy_recursive(world, e);
    toDestroy.clear();
}
