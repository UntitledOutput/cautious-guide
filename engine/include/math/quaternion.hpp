#pragma once
#include "vector.hpp"
#include "matrix.hpp"
#include <cmath>

namespace math {

struct quat
{
    float x, y, z, w;

    static constexpr quat identity() { return {0, 0, 0, 1}; }
};

constexpr quat operator*(quat a, quat b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

constexpr quat conjugate(quat q) { return {-q.x, -q.y, -q.z, q.w}; }
constexpr float dot(quat a, quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

inline quat normalize(quat q)
{
    float inv = 1.0f / std::sqrt(dot(q, q));
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

constexpr vec3 rotate(quat q, vec3 v)
{
    vec3 u{q.x, q.y, q.z};
    vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

inline quat angle_axis(float radians, vec3 axis)
{
    float s = std::sin(radians * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f)};
}

quat from_to(vec3 from, vec3 to);
quat look_rotation(vec3 forward, vec3 up);
quat slerp(quat a, quat b, float t);
quat nlerp(quat a, quat b, float t);
mat4 to_mat4(quat q);
quat from_mat4(const mat4& m);
quat from_euler(vec3 radians);

} // namespace math