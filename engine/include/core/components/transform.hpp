#pragma once;
#include <math/vector.hpp>
#include <math/quaternion.hpp>
#include <math/matrix.hpp>

namespace scene
{
    struct Transform
    {
        math::vec3 position{};
        math::quat rotation = math::quat::identity();
        math::vec3 scale{1.0f, 1.0f, 1.0f};

        math::mat4 matrix() const
        {
            return math::translate(position) * math::to_mat4(rotation) * math::scale(scale);
        }
    };

    struct WorldTransform
    {
        math::mat4 matrix = math::mat4::identity();
    };

}
