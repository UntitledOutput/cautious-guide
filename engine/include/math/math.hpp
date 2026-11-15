#pragma once
#include "math/vector.hpp"
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

namespace math {

    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline float abs(float v) { return v < 0.0f ? -v : v; }
    inline float clamp(float v, float min, float max) { return v < min ? min : (v > max ? max : v); }

}