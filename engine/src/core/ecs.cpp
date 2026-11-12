#include "core/ecs.hpp"

ecs::Entity ecs::World::create()
{
    if (freeList.size() > 0)
    {
        var idx = freeList.back();
        freeList.pop_back();
        return {idx, generations[idx]};
    }
    const u32 idx = u32(generations.size());
    generations.push_back(0);
    return {idx, 0};
}

void ecs::World::destroy(Entity e)
{
    if (!alive(e))
        return;

    for (auto& pool : pools)
        if (pool)
            pool->remove(e);

    ++generations[e.index];
    freeList.push_back(e.index);
}

bool ecs::World::alive(Entity e) const { return e.index < generations.size() && generations[e.index] == e.generation; }

