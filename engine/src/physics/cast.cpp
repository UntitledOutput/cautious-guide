#include "physics/world.hpp"

#include <bit>
#include <cmath>

#include "core/components/transform.hpp"
#include "physics/geometry.hpp"

namespace physics
{
    using namespace geo;

    namespace
    {
        // moving sphere vs aabb = ray vs the box grown by radius, with rounded edges (ericson 5.5.7)
        std::optional<RayT> cast_box(math::vec3 o, math::vec3 dir, float maxT, math::vec3 lo, math::vec3 hi, float r)
        {
            const math::vec3 pad{r, r, r};
            const auto h = ray_aabb(o, dir, maxT, lo - pad, hi + pad);
            if (!h || r <= 0.0f)
                return h;

            const math::vec3 p = o + dir * h->t;
            int below = 0, above = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (p[i] < lo[i])
                    below |= 1 << i;
                if (p[i] > hi[i])
                    above |= 1 << i;
            }
            const int outside = below | above;
            if (std::popcount(unsigned(outside)) <= 1)
                return h; // flat face region

            auto corner = [&](int bits)
            {
                math::vec3 v;
                for (int i = 0; i < 3; ++i)
                    v[i] = (bits & (1 << i)) ? hi[i] : lo[i];
                return v;
            };

            std::optional<RayT> best;
            auto edge = [&](int fixedBits, int axis)
            {
                // edge along axis, other coords picked by fixedBits
                const math::vec3 a = corner(fixedBits & ~(1 << axis));
                const math::vec3 b = corner(fixedBits | (1 << axis));
                if (const auto e = ray_capsule(o, dir, best ? best->t : maxT, a, b, r))
                    best = e;
            };

            if (std::popcount(unsigned(outside)) == 2)
            {
                const int axis = std::countr_zero(unsigned(~outside & 7));
                edge(above, axis);
            }
            else
            {
                for (int axis = 0; axis < 3; ++axis)
                    edge(above, axis);
            }
            return best;
        }
    }

    std::optional<CastHit> PhysicsWorld::raycast(ecs::World& w, math::vec3 origin, math::vec3 dir, float maxDist,
                                                 const CastFilter& filter) const
    {
        return cast(w, origin, 0.0f, dir, maxDist, filter);
    }

    std::optional<CastHit> PhysicsWorld::sphere_cast(ecs::World& w, math::vec3 origin, float radius, math::vec3 dir,
                                                     float maxDist, const CastFilter& filter) const
    {
        return cast(w, origin, std::max(radius, 0.0f), dir, maxDist, filter);
    }

    std::optional<CastHit> PhysicsWorld::cast(ecs::World& w, math::vec3 o, float r, math::vec3 dir, float maxDist,
                                              const CastFilter& filter) const
    {
        const float len = math::length(dir);
        if (len < 1e-8f || maxDist <= 0.0f)
            return std::nullopt;
        dir = dir / len;

        std::optional<CastHit> best;
        float bestT = maxDist;

        w.view<scene::Transform, Collider>().each(
            [&](ecs::Entity e, scene::Transform& t, Collider& c)
            {
                if (e == filter.ignore || !(c.layer & filter.mask) || (c.isTrigger && !filter.hitTriggers))
                    return;

                const math::vec3 center = t.position + c.offset;
                std::optional<RayT> h;

                switch (c.shape)
                {
                    case ShapeType::Sphere:
                        h = ray_sphere(o, dir, bestT, center, c.sphere.radius + r);
                        break;
                    case ShapeType::Capsule:
                    {
                        const math::vec3 up{0.0f, c.capsule.halfHeight, 0.0f};
                        h = ray_capsule(o, dir, bestT, center - up, center + up, c.capsule.radius + r);
                        break;
                    }
                    case ShapeType::AABB:
                        h = cast_box(o, dir, bestT, center - c.box.halfExtents, center + c.box.halfExtents, r);
                        break;
                    case ShapeType::Mesh:
                    {
                        const TriangleMesh* m = mesh(c.mesh.id);
                        if (!m)
                            return;
                        if (const auto mh = m->cast(o - center, dir, r, bestT))
                        {
                            bestT = mh->t;
                            best = CastHit{e, mh->point + center, mh->normal, mh->t, m->attribute(mh->triangle),
                                           mh->triangle, mh->barycentric};
                        }
                        return;
                    }
                    default:
                        return;
                }

                if (!h)
                    return;
                bestT = h->t;
                // the touch point is one caster radius back from the swept center
                best = CastHit{e, o + dir * h->t - h->normal * r, h->normal, h->t};
            });

        return best;
    }
}