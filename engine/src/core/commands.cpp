#include "core/commands.hpp"

#include "core/ecs.hpp"



void ecs::CommandBuffer::apply(ecs::World& world)
{
    for (auto e : toDestroy)
    {
        world.destroy(e);
    }
    toDestroy.clear();
}
