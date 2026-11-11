#pragma once

#include "vector.hpp"

// column major, right handed
namespace math
{
	
	struct mat4
	{
        vec4 c[4];

		constexpr vec4& operator[](int i) { return c[i]; }
	    constexpr const vec4& operator[](int i) const { return c[i]; }

		const float* data() const { return &c[0].x; }

        static constexpr mat4 identity() { return {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}}; }
	};

	static_assert(sizeof(mat4) == 64);

	constexpr vec4 operator*(const mat4& m, vec4 v) { return m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w; }

    constexpr mat4 operator*(const mat4& a, const mat4& b)
    {
        mat4 r{};
        for (int i = 0; i < 4; ++i)
            r[i] = a * b[i];
        return r;
    }

    constexpr mat4 transpose(const mat4& m)
    {
        mat4 r{};
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                r[col][row] = m[row][col];
        return r;
    }

    constexpr vec3 transform_point(const mat4& m, vec3 p)
    {
        vec4 r = m * vec4{p.x, p.y, p.z, 1.0f};
        return {r.x, r.y, r.z};
    }

    constexpr vec3 transform_dir(const mat4& m, vec3 d)
    {
        vec4 r = m * vec4{d.x, d.y, d.z, 0.0f};
        return {r.x, r.y, r.z};
    }

    mat4 translate(vec3 t);
    mat4 scale(vec3 s);
    mat4 rotate(vec3 axis, float radians);
    mat4 perspective(float fov_y, float aspect, float z_near, float z_far);
    mat4 ortho(float left, float right, float bottom, float top, float z_near, float z_far);
    mat4 look_at(vec3 eye, vec3 target, vec3 up);
    mat4 inverse(const mat4& m);
    mat4 inverse_affine(const mat4& m);

}
