#pragma once

#ifdef _DEBUG
#include "math/quaternion.hpp"
#include "math/vector.hpp"

struct FreeCamCamera
{
    math::vec3 position;
    math::quat rotation;

    void update();
};

#endif  