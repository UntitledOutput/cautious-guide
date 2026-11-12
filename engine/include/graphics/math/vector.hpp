#pragma once
#include <cmath>
#include <concepts>

namespace math
{

    struct vec2
    {
        float x, y;
        static constexpr int N = 2;
        constexpr float& operator[](int i) { return i == 0 ? x : y; }
        constexpr float operator[](int i) const { return i == 0 ? x : y; }
    };

    struct vec3
    {
        float x, y, z;
        static constexpr int N = 3;
        constexpr float& operator[](int i) { return i == 0 ? x : i == 1 ? y : z; }
        constexpr float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
    };

    struct vec4
    {
        float x, y, z, w;
        static constexpr int N = 4;
        constexpr float& operator[](int i) { return i == 0 ? x : i == 1 ? y : i == 2 ? z : w; }
        constexpr float operator[](int i) const { return i == 0 ? x : i == 1 ? y : i == 2 ? z : w; }
    };

    template <class V>
    concept Vec = std::same_as<V, vec2> || std::same_as<V, vec3> || std::same_as<V, vec4>;

    template <Vec V>
    constexpr V& operator+=(V& a, V b)
    {
        for (int i = 0; i < V::N; ++i)
            a[i] += b[i];
        return a;
    }
    template <Vec V>
    constexpr V& operator-=(V& a, V b)
    {
        for (int i = 0; i < V::N; ++i)
            a[i] -= b[i];
        return a;
    }
    template <Vec V>
    constexpr V& operator*=(V& a, V b)
    {
        for (int i = 0; i < V::N; ++i)
            a[i] *= b[i];
        return a;
    }
    template <Vec V>
    constexpr V& operator*=(V& a, float s)
    {
        for (int i = 0; i < V::N; ++i)
            a[i] *= s;
        return a;
    }

    template <Vec V>
    constexpr V operator+(V a, V b)
    {
        return a += b;
    }
    template <Vec V>
    constexpr V operator-(V a, V b)
    {
        return a -= b;
    }
    template <Vec V>
    constexpr V operator*(V a, V b)
    {
        return a *= b;
    }
    template <Vec V>
    constexpr V operator*(V a, float s)
    {
        return a *= s;
    }
    template <Vec V>
    constexpr V operator*(float s, V a)
    {
        return a *= s;
    }
    template <Vec V>
    constexpr V operator/(V a, float s)
    {
        return a *= 1.0f / s;
    }
    template <Vec V>
    constexpr V operator-(V a)
    {
        return a *= -1.0f;
    }

    template <Vec V>
    constexpr bool operator==(V a, V b)
    {
        for (int i = 0; i < V::N; ++i)
            if (a[i] != b[i])
                return false;
        return true;
    }

    template <Vec V>
    constexpr float dot(V a, V b)
    {
        float r = 0.0f;
        for (int i = 0; i < V::N; ++i)
            r += a[i] * b[i];
        return r;
    }

    template <Vec V>
    inline float length(V v)
    {
        return std::sqrt(dot(v, v));
    }
    template <Vec V>
    inline V normalize(V v)
    {
        return v / length(v);
    }
    template <Vec V>
    constexpr V lerp(V a, V b, float t)
    {
        return a + (b - a) * t;
    }

    constexpr vec3 cross(vec3 a, vec3 b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    static_assert(sizeof(vec3) == 12);

} // namespace math
