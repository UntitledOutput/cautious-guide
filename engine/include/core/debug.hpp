#pragma once
#include "math/matrix.hpp"

enum PrimitiveType : int;
class Engine;

namespace debug
{
    void updateDebugMenu(Engine& engine, float delta);

    void draw(Engine& engine,PrimitiveType type, math::mat4 matrix);
}