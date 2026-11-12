#pragma once
#include "ecs.hpp"

namespace ecs
{
    
class CommandBuffer
{
public:
    void destroy(ecs::Entity e) { toDestroy.push_back(e); }
    void apply(ecs::World& world);

private:
    std::vector<ecs::Entity> toDestroy;
};

}