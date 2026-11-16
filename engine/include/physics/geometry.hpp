#pragma once
#include <algorithm>

#include "math/vector.hpp"

// most of this code i *did not* write because i simply don't understand the physics equations going on here

// closest point helpers shared by the narrowphase and mesh colliders
namespace physics::geo
{
    struct Segment
    {
        math::vec3 p0, p1;
    };

    inline math::vec3 vmin(math::vec3 a, math::vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
    inline math::vec3 vmax(math::vec3 a, math::vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }

    inline math::vec3 closest_on_segment(const Segment& s, math::vec3 p)
    {
        const math::vec3 d = s.p1 - s.p0;
        const float len2 = math::dot(d, d);
        if (len2 < 1e-12f)
            return s.p0;
        const float t = std::clamp(math::dot(p - s.p0, d) / len2, 0.0f, 1.0f);
        return s.p0 + d * t;
    }

    inline void closest_between_segments(const Segment& s1, const Segment& s2, math::vec3& c1, math::vec3& c2)
    {
        const math::vec3 d1 = s1.p1 - s1.p0, d2 = s2.p1 - s2.p0, r = s1.p0 - s2.p0;
        const float a = math::dot(d1, d1), e = math::dot(d2, d2), f = math::dot(d2, r);
        constexpr float eps = 1e-12f;
        float s = 0.0f, t = 0.0f;

        if (a <= eps && e <= eps)
        {
            c1 = s1.p0;
            c2 = s2.p0;
            return;
        }
        if (a <= eps)
            t = std::clamp(f / e, 0.0f, 1.0f);
        else
        {
            const float c = math::dot(d1, r);
            if (e <= eps)
                s = std::clamp(-c / a, 0.0f, 1.0f);
            else
            {
                const float b = math::dot(d1, d2);
                const float denom = a * e - b * b;
                s = denom > eps ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
                t = (b * s + f) / e;
                if (t < 0.0f)
                {
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                }
                else if (t > 1.0f)
                {
                    t = 1.0f;
                    s = std::clamp((b - c) / a, 0.0f, 1.0f);
                }
            }
        }
        c1 = s1.p0 + d1 * s;
        c2 = s2.p0 + d2 * t;
    }

    inline math::vec3 closest_on_triangle(math::vec3 p, math::vec3 a, math::vec3 b, math::vec3 c)
    {
        const math::vec3 ab = b - a, ac = c - a, ap = p - a;
        const float d1 = math::dot(ab, ap), d2 = math::dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f)
            return a;

        const math::vec3 bp = p - b;
        const float d3 = math::dot(ab, bp), d4 = math::dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3)
            return b;

        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            return a + ab * (d1 / (d1 - d3));

        const math::vec3 cp = p - c;
        const float d5 = math::dot(ab, cp), d6 = math::dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6)
            return c;

        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            return a + ac * (d2 / (d2 - d6));

        const float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

        const float denom = 1.0f / (va + vb + vc);
        return a + ab * (vb * denom) + ac * (vc * denom);
    }

    struct RayT
    {
        float t;
        math::vec3 normal;
    };

    inline std::optional<RayT> ray_sphere(math::vec3 o, math::vec3 dir, float maxT, math::vec3 c, float r)
    {
        const math::vec3 m = o - c;
        const float b = math::dot(m, dir);
        const float cc = math::dot(m, m) - r * r;
        if (cc <= 0.0f || b > 0.0f)
            return std::nullopt;
        const float disc = b * b - cc;
        if (disc < 0.0f)
            return std::nullopt;
        const float t = -b - std::sqrt(disc);
        if (t < 0.0f || t > maxT)
            return std::nullopt;
        return RayT{t, (o + dir * t - c) / r};
    }

    inline std::optional<RayT> ray_capsule(math::vec3 o, math::vec3 dir, float maxT, math::vec3 p0, math::vec3 p1,
                                           float r)
    {
        std::optional<RayT> best;
        auto take = [&](const std::optional<RayT>& h)
        {
            if (h && (!best || h->t < best->t))
                best = h;
        };

        const math::vec3 d = p1 - p0, m = o - p0;
        const float dd = math::dot(d, d), md = math::dot(m, d), nd = math::dot(dir, d);
        const float a = dd - nd * nd;
        const float k = math::dot(m, m) - r * r;
        const float c = dd * k - md * md;
        if (dd > 1e-12f && std::abs(a) > 1e-9f && c > 0.0f)
        {
            const float b = dd * math::dot(m, dir) - nd * md;
            const float disc = b * b - a * c;
            if (disc >= 0.0f)
            {
                const float t = (-b - std::sqrt(disc)) / a;
                const float along = md + t * nd;
                if (t >= 0.0f && t <= maxT && along >= 0.0f && along <= dd)
                {
                    const math::vec3 hit = o + dir * t;
                    take(RayT{t, (hit - (p0 + d * (along / dd))) / r});
                }
            }
        }

        take(ray_sphere(o, dir, maxT, p0, r));
        take(ray_sphere(o, dir, maxT, p1, r));
        return best;
    }

    inline std::optional<RayT> ray_aabb(math::vec3 o, math::vec3 dir, float maxT, math::vec3 lo, math::vec3 hi)
    {
        float tmin = 0.0f, tmax = maxT;
        int axis = -1;
        float sign = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            if (std::abs(dir[i]) < 1e-12f)
            {
                if (o[i] < lo[i] || o[i] > hi[i])
                    return std::nullopt;
                continue;
            }
            const float inv = 1.0f / dir[i];
            float t0 = (lo[i] - o[i]) * inv, t1 = (hi[i] - o[i]) * inv;
            float s = -1.0f;
            if (t0 > t1)
            {
                std::swap(t0, t1);
                s = 1.0f;
            }
            if (t0 > tmin)
            {
                tmin = t0;
                axis = i;
                sign = s;
            }
            tmax = std::min(tmax, t1);
            if (tmin > tmax)
                return std::nullopt;
        }
        if (axis < 0)
            return std::nullopt; // started inside
        math::vec3 n{};
        n[axis] = sign;
        return RayT{tmin, n};
    }

    inline math::vec3 barycentric(math::vec3 p, math::vec3 a, math::vec3 b, math::vec3 c)
    {
        const math::vec3 v0 = b - a, v1 = c - a, v2 = p - a;
        const float d00 = math::dot(v0, v0), d01 = math::dot(v0, v1), d11 = math::dot(v1, v1);
        const float d20 = math::dot(v2, v0), d21 = math::dot(v2, v1);
        const float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-20f)
            return {1.0f, 0.0f, 0.0f};
        const float v = (d11 * d20 - d01 * d21) / denom;
        const float w = (d00 * d21 - d01 * d20) / denom;
        return {1.0f - v - w, v, w};
    }
}