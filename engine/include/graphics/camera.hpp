#pragma once
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

class Engine;

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

    struct Camera
    {
        float fov = 60.0f;
        float z_near = 0.01f;
        float z_far = 1000.0f;
    };

    void update_camera(Engine& engine);

}
