#pragma once
#include "engine.hpp"
#include "math/quaternion.hpp"
#include "math/vector.hpp"

struct FreeCam
{
    math::vec3 position{0.0f, 1.0f, 4.0f};
    float yaw = 0.0f, pitch = 0.0f;
    float speed = 5.0f;
    float sensitivity = 0.002f;
    bool enabled = true;

    void update(Engine& engine, float dt);
    math::quat rotation() const;
    graphics::CameraData camera_data(float aspect) const;

private:
    bool looking = false;
};
