#include <cmath>
#include "math/matrix.hpp"

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

} // namespace math
