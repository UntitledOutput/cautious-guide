#pragma once;
#include <math/vector.hpp>
#include <math/quaternion.hpp>
#include <math/matrix.hpp>

#include "core/util.hpp"

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

        math::vec3 forward() const { return math::rotate(rotation, {0, 0, 1}); }
        math::vec3 right() const { return math::rotate(rotation, {1, 0, 0}); }
        math::vec3 up() const { return math::rotate(rotation, {0, 1, 0}); }

        void look_at(math::vec3 target, math::vec3 up = {0,1,0})
        {
            var direction = position - target;
            rotation = math::look_rotation(-direction, up);
        }
    };

    struct WorldTransform
    {
        math::mat4 matrix = math::mat4::identity();
    };

}
