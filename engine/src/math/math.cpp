#include <algorithm>
#include <cmath>
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

namespace
{
    // quat has no arithmetic operators, so these stay local
    math::quat qscale(math::quat q, float s) { return {q.x * s, q.y * s, q.z * s, q.w * s}; }
    math::quat qadd(math::quat a, math::quat b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
} // namespace

namespace math
{

    mat4 translate(vec3 t)
    {
        mat4 r = mat4::identity();
        r[3] = {t.x, t.y, t.z, 1.0f};
        return r;
    }

    mat4 scale(vec3 s)
    {
        mat4 r = mat4::identity();
        r[0].x = s.x;
        r[1].y = s.y;
        r[2].z = s.z;
        return r;
    }

    mat4 perspective(float fov_y, float aspect, float z_near, float z_far)
    {
        float f = 1.0f / std::tan(fov_y * 0.5f);
        mat4 r{};
        r[0].x = f / aspect;
        r[1].y = f;
        r[2].z = (z_far + z_near) / (z_near - z_far);
        r[2].w = -1.0f;
        r[3].z = 2.0f * z_far * z_near / (z_near - z_far);
        return r;
    }

    mat4 look_at(vec3 eye, vec3 target, vec3 up)
    {
        vec3 f = normalize(target - eye);
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);

        mat4 r = mat4::identity();
        r[0] = {s.x, u.x, -f.x, 0.0f};
        r[1] = {s.y, u.y, -f.y, 0.0f};
        r[2] = {s.z, u.z, -f.z, 0.0f};
        r[3] = {-dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f};
        return r;
    }

    mat4 inverse_affine(const mat4& m)
    {
        mat4 r = mat4::identity();
        for (int col = 0; col < 3; ++col)
            for (int row = 0; row < 3; ++row)
                r[col][row] = m[row][col];

        vec3 t{m[3].x, m[3].y, m[3].z};
        vec3 rt = transform_dir(r, t);
        r[3] = {-rt.x, -rt.y, -rt.z, 1.0f};
        return r;
    }

    mat4 to_mat4(quat q)
    {
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

        mat4 r = mat4::identity();
        r[0] = {1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0};
        r[1] = {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0};
        r[2] = {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0};
        return r;
    }

    // expects a pure rotation, normalize the columns first if there's scale
    quat from_mat4(const mat4& m)
    {
        // m[col][row], so R(row, col) = m[col][row]
        float r00 = m[0].x, r11 = m[1].y, r22 = m[2].z;
        float trace = r00 + r11 + r22;

        if (trace > 0.0f)
        {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            return {(m[1].z - m[2].y) / s, (m[2].x - m[0].z) / s, (m[0].y - m[1].x) / s, 0.25f * s};
        }
        if (r00 > r11 && r00 > r22)
        {
            float s = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
            return {0.25f * s, (m[1].x + m[0].y) / s, (m[2].x + m[0].z) / s, (m[1].z - m[2].y) / s};
        }
        if (r11 > r22)
        {
            float s = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
            return {(m[1].x + m[0].y) / s, 0.25f * s, (m[2].y + m[1].z) / s, (m[2].x - m[0].z) / s};
        }
        float s = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
        return {(m[2].x + m[0].z) / s, (m[2].y + m[1].z) / s, 0.25f * s, (m[0].y - m[1].x) / s};
    }

    quat nlerp(quat a, quat b, float t)
    {
        if (dot(a, b) < 0.0f)
            b = qscale(b, -1.0f); // take the short way around
        return normalize(qadd(qscale(a, 1.0f - t), qscale(b, t)));
    }

    quat slerp(quat a, quat b, float t)
    {
        float d = dot(a, b);
        if (d < 0.0f)
        {
            b = qscale(b, -1.0f);
            d = -d;
        }
        if (d > 0.9995f)
            return nlerp(a, b, t); // nearly parallel, sin(theta) blows up

        float theta = std::acos(d);
        float s = std::sin(theta);
        return qadd(qscale(a, std::sin((1.0f - t) * theta) / s), qscale(b, std::sin(t * theta) / s));
    }

    quat from_to(vec3 from, vec3 to)
    {
        from = normalize(from);
        to = normalize(to);
        float d = dot(from, to);

        if (d < -0.999999f)
        {
            // opposite directions: any perpendicular axis works for a 180 turn
            vec3 axis = cross(vec3{1, 0, 0}, from);
            if (dot(axis, axis) < 1e-6f)
                axis = cross(vec3{0, 1, 0}, from);
            axis = normalize(axis);
            return {axis.x, axis.y, axis.z, 0.0f};
        }

        vec3 c = cross(from, to);
        return normalize(quat{c.x, c.y, c.z, 1.0f + d});
    }

    // -Z faces along forward, same as look_at
    quat look_rotation(vec3 forward, vec3 up)
    {
        vec3 z = -normalize(forward);
        vec3 x = cross(up, z);
        if (dot(x, x) < 1e-6f)
            x = cross(std::abs(z.y) < 0.99f ? vec3{0, 1, 0} : vec3{1, 0, 0}, z);
        x = normalize(x);
        vec3 y = cross(z, x);

        mat4 m = mat4::identity();
        m[0] = {x.x, x.y, x.z, 0};
        m[1] = {y.x, y.y, y.z, 0};
        m[2] = {z.x, z.y, z.z, 0};
        return from_mat4(m);
    }

    quat from_euler(vec3 r)
    {
        return angle_axis(r.y, {0, 1, 0}) * angle_axis(r.x, {1, 0, 0}) * angle_axis(r.z, {0, 0, 1});
    }

    vec3 to_euler(quat q)
    {
        float sp = std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f);
        float pitch = std::asin(sp);

        if (std::abs(sp) > 0.9999f)
        {
            // gimbal lock: yaw and roll share an axis, so fold it all into yaw
            float yaw = std::atan2(-2.0f * (q.x * q.z - q.w * q.y), 1.0f - 2.0f * (q.y * q.y + q.z * q.z));
            return {pitch, yaw, 0.0f};
        }

        float yaw = std::atan2(2.0f * (q.x * q.z + q.w * q.y), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        float roll = std::atan2(2.0f * (q.x * q.y + q.w * q.z), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
        return {pitch, yaw, roll};
    }

    // matrix.hpp declares these too
    mat4 rotate(vec3 axis, float radians) { return to_mat4(angle_axis(radians, normalize(axis))); }

    mat4 ortho(float left, float right, float bottom, float top, float z_near, float z_far)
    {
        mat4 r = mat4::identity();
        r[0].x = 2.0f / (right - left);
        r[1].y = 2.0f / (top - bottom);
        r[2].z = -2.0f / (z_far - z_near);
        r[3] = {-(right + left) / (right - left), -(top + bottom) / (top - bottom),
                -(z_far + z_near) / (z_far - z_near), 1.0f};
        return r;
    }

} // namespace math
