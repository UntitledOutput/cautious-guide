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
            const math::vec3 plo = lo - pad, phi = hi + pad;
            auto h = ray_aabb(o, dir, maxT, plo, phi);
            if (r <= 0.0f)
                return h;

            // the grown box has square edges, the real shape is rounded there. starting inside the grown
            // box but out in an edge or corner region still counts as outside
            math::vec3 p;
            if (h)
                p = o + dir * h->t;
            else
            {
                const bool inGrown = o.x >= plo.x && o.y >= plo.y && o.z >= plo.z && o.x <= phi.x && o.y <= phi.y &&
                                     o.z <= phi.z;
                if (!inGrown)
                    return std::nullopt;
                p = o;
            }

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
                return h; // flat face region (or truly inside if we started in the grown box)

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
                edge(above, std::countr_zero(unsigned(~outside & 7)));
            else
                for (int axis = 0; axis < 3; ++axis)
                    edge(above, axis);
            return best;
        }
    }

    std::optional<CastHit> PhysicsWorld::raycast(ecs::World& w, math::vec3 origin, math::vec3 dir, float maxDist,
                                                 const CastFilter& filter) const
    {
        return cast(w, origin, 0.0f, 0.0f, dir, maxDist, filter);
    }

    std::optional<CastHit> PhysicsWorld::sphere_cast(ecs::World& w, math::vec3 origin, float radius, math::vec3 dir,
                                                     float maxDist, const CastFilter& filter) const
    {
        return cast(w, origin, std::max(radius, 0.0f), 0.0f, dir, maxDist, filter);
    }

    std::optional<CastHit> PhysicsWorld::capsule_cast(ecs::World& w, math::vec3 center, float radius, float halfHeight,
                                                      math::vec3 dir, float maxDist, const CastFilter& filter) const
    {
        return cast(w, center, std::max(radius, 0.0f), std::max(halfHeight, 0.0f), dir, maxDist, filter);
    }

    // casts a y-up capsule (halfHeight 0 = sphere, radius 0 too = ray). against primitives this is exact
    // because a vertical segment swept around a sphere / vertical capsule / aabb is still one of those shapes.
    // against meshes the capsule is sampled as a stack of spheres
    std::optional<CastHit> PhysicsWorld::cast(ecs::World& w, math::vec3 o, float r, float hh, math::vec3 dir,
                                              float maxDist, const CastFilter& filter) const
    {
        const float len = math::length(dir);
        if (len < 1e-8f || maxDist <= 0.0f)
            return std::nullopt;
        dir = dir / len;

        const math::vec3 up{0.0f, hh, 0.0f};
        std::optional<CastHit> best;
        float bestT = maxDist;

        w.view<scene::Transform, Collider>().each(
            [&](ecs::Entity e, scene::Transform& t, Collider& c)
            {
                if (e == filter.ignore || !(c.layer & filter.mask) || (c.isTrigger && !filter.hitTriggers))
                    return;

                const math::vec3 center = t.position + c.offset;
                std::optional<RayT> h;
                math::vec3 touch{};

                switch (c.shape)
                {
                    case ShapeType::Sphere:
                    {
                        const float R = c.sphere.radius;
                        h = ray_capsule(o, dir, bestT, center - up, center + up, R + r);
                        if (h)
                            touch = center + h->normal * R;
                        break;
                    }
                    case ShapeType::Capsule:
                    {
                        const float R = c.capsule.radius;
                        const math::vec3 up2{0.0f, c.capsule.halfHeight, 0.0f};
                        h = ray_capsule(o, dir, bestT, center - up2 - up, center + up2 + up, R + r);
                        if (h)
                        {
                            const math::vec3 p = o + dir * h->t;
                            math::vec3 onOther, onSelf;
                            closest_between_segments({center - up2, center + up2}, {p - up, p + up}, onOther, onSelf);
                            touch = onOther + h->normal * R;
                        }
                        break;
                    }
                    case ShapeType::AABB:
                    {
                        const math::vec3 lo = center - c.box.halfExtents, hi = center + c.box.halfExtents;
                        h = cast_box(o, dir, bestT, lo - up, hi + up, r);
                        if (h)
                        {
                            // closest point on the box to the capsule's segment at the hit
                            const math::vec3 p = o + dir * h->t;
                            math::vec3 q = p;
                            const float segLo = p.y - hh, segHi = p.y + hh;
                            const float ylo = std::max(segLo, lo.y), yhi = std::min(segHi, hi.y);
                            q.y = ylo <= yhi ? (ylo + yhi) * 0.5f : (segLo > hi.y ? segLo : segHi);
                            touch = {std::clamp(q.x, lo.x, hi.x), std::clamp(q.y, lo.y, hi.y), std::clamp(q.z, lo.z, hi.z)};
                        }
                        break;
                    }
                    case ShapeType::Mesh:
                    {
                        const TriangleMesh* m = mesh(c.mesh.id);
                        if (!m)
                            return;

                        // spheres no further apart than one radius, so the dips between them stay small
                        int samples = 1;
                        if (hh > 0.0f)
                            samples = std::clamp(int(std::ceil(2.0f * hh / std::max(r, 1e-3f))) + 1, 2, 16);

                        for (int i = 0; i < samples; ++i)
                        {
                            const float y = samples == 1 ? 0.0f : -hh + 2.0f * hh * float(i) / float(samples - 1);
                            const math::vec3 so = o + math::vec3{0.0f, y, 0.0f};
                            if (const auto mh = m->cast(so - center, dir, r, bestT))
                            {
                                bestT = mh->t;
                                best = CastHit{e, mh->point + center, mh->normal, mh->t, m->attribute(mh->triangle),
                                               mh->triangle, mh->barycentric};
                            }
                        }
                        return;
                    }
                    default:
                        return;
                }

                if (!h)
                    return;
                bestT = h->t;
                best = CastHit{e, touch, h->normal, h->t};
            });

        return best;
    }
}