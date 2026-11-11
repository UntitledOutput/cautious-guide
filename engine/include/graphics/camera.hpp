#pragma once
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

namespace graphics
{
    struct CameraData
    {
        math::mat4 view;
        math::mat4 projection;
        math::mat4 viewProjection;
        math::vec4 position;
    };

    static_assert(sizeof(CameraData) == 208);

}
