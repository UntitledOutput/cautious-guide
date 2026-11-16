#include "physics/world.hpp"

#include <cmath>

#include "core/components/transform.hpp"

namespace physics
{
    namespace
    {
        constexpr int kSlideIterations = 4;
        constexpr int kDepenetrateIterations = 4;

        math::vec3 horizontal(math::vec3 v) { return {v.x, 0.0f, v.z}; }

        struct Mover
        {
            const PhysicsWorld& pw;
            ecs::World& w;
            CharacterController& cc;
            CastFilter filter;
            float radius, halfHeight;
            float minGroundY;
            bool blocked = false; // sideways pass ran into something too steep to walk on

            bool walkable(math::vec3 n) const { return n.y >= minGroundY; }

            // rounded capsule bottoms touch ledge edges at an angle, so a steep contact normal under our feet
            // doesn't mean steep ground. check what's actually under the contact with a short ray
            bool supports(const CastHit& h, math::vec3 center, math::vec3& groundN) const
            {
                if (walkable(h.normal))
                {
                    groundN = h.normal;
                    return true;
                }

                const float bottomY = center.y - halfHeight;
                if (h.point.y > bottomY - 0.01f)
                    return false; // beside or above us, that's a wall

                math::vec3 out = horizontal(h.point - center);
                const float ol = math::length(out);
                out = ol > 1e-6f ? out / ol : math::vec3{};
                const math::vec3 from{h.point.x + out.x * 0.02f, bottomY, h.point.z + out.z * 0.02f};
                const auto ray = pw.raycast(w, from, {0, -1, 0}, bottomY - h.point.y + 0.05f, filter);
                if (ray && walkable(ray->normal))
                {
                    groundN = ray->normal;
                    return true;
                }
                return false;
            }

            std::optional<CastHit> cast(math::vec3 pos, math::vec3 dir, float dist) const
            {
                return pw.capsule_cast(w, pos, radius, halfHeight, dir, dist, filter);
            }

            // returns true if it was ground
            bool note(const CastHit& h, math::vec3 center)
            {
                math::vec3 n;
                if (supports(h, center, n))
                {
                    cc.grounded = true;
                    cc.groundNormal = n;
                    cc.ground = h.entity;
                    cc.groundSurface = h.surface;
                    return true;
                }
                if (h.normal.y < -0.3f)
                    cc.hitCeiling = true;
                else
                    cc.hitWall = true;
                return false;
            }

            // move and slide. flattenWalls stops steep slopes from being climbed during the sideways pass
            math::vec3 slide(math::vec3 pos, math::vec3 delta, bool flattenWalls, bool stopOnGround)
            {
                math::vec3 firstNormal{};
                for (int i = 0; i < kSlideIterations; ++i)
                {
                    const float len = math::length(delta);
                    if (len < 1e-6f)
                        break;
                    const math::vec3 dir = delta / len;

                    const auto hit = cast(pos, dir, len + cc.skin);
                    if (!hit)
                    {
                        pos += delta;
                        break;
                    }

                    const float travel = std::max(hit->distance - cc.skin, 0.0f);
                    pos += dir * travel;
                    const bool isGround = note(*hit, pos);

                    if (stopOnGround && isGround)
                        break;

                    // sideways uses the raw normal, so the rounded bottom can't ride up ledges (that's what steps are for)
                    math::vec3 n = hit->normal;
                    if (flattenWalls && !walkable(n))
                    {
                        blocked = true;
                        n = horizontal(n);
                        const float nl = math::length(n);
                        n = nl > 1e-6f ? n / nl : hit->normal;
                    }

                    math::vec3 remaining = dir * (len - travel);
                    remaining -= n * math::dot(remaining, n);

                    // second wall: slide along the crease instead of bouncing between the two
                    if (i > 0 && math::dot(firstNormal, firstNormal) > 0.0f)
                    {
                        const math::vec3 crease = math::cross(firstNormal, n);
                        const float cl = math::length(crease);
                        if (cl > 1e-4f && math::dot(remaining, firstNormal) < 0.0f)
                        {
                            const math::vec3 cd = crease / cl;
                            remaining = cd * math::dot(remaining, cd);
                        }
                    }
                    firstNormal = n;
                    delta = remaining;
                }
                return pos;
            }

            math::vec3 depenetrate(math::vec3 pos)
            {
                std::vector<Overlap> hits;
                for (int i = 0; i < kDepenetrateIterations; ++i)
                {
                    hits.clear();
                    pw.overlap_capsule(w, pos, radius, halfHeight, hits, filter);
                    if (hits.empty())
                        break;

                    const Overlap* deepest = &hits[0];
                    for (const Overlap& o : hits)
                        if (o.depth > deepest->depth)
                            deepest = &o;
                    pos += deepest->normal * (deepest->depth + cc.skin);
                }
                return pos;
            }
        };
    }

    void PhysicsWorld::move_character(ecs::World& w, ecs::Entity e, math::vec3 displacement) const
    {
        auto* t = w.try_get<scene::Transform>(e);
        auto* col = w.try_get<Collider>(e);
        auto* cc = w.try_get<CharacterController>(e);
        if (!t || !col || !cc || col->shape != ShapeType::Capsule)
            return;

        const bool wasGrounded = cc->grounded;
        cc->grounded = false;
        cc->hitWall = false;
        cc->hitCeiling = false;

        Mover m{*this, w, *cc, {.mask = cc->mask, .ignore = e}, col->capsule.radius, col->capsule.halfHeight,
                std::cos(cc->maxSlopeDeg * 3.14159265f / 180.0f)};

        math::vec3 pos = m.depenetrate(t->position + col->offset);

        // sideways, with a step-up attempt when something blocks us while on the ground
        const math::vec3 side = horizontal(displacement);
        if (math::dot(side, side) > 1e-12f)
        {
            m.blocked = false;
            const math::vec3 flat = m.slide(pos, side, true, false);
            const bool blocked = m.blocked;

            math::vec3 chosen = flat;
            if (blocked && wasGrounded && cc->stepHeight > 0.0f)
            {
                const CharacterController before = *cc;

                // up, across, then back down onto the step
                const auto upHit = m.cast(pos, {0, 1, 0}, cc->stepHeight + cc->skin);
                const float rise = upHit ? std::max(upHit->distance - cc->skin, 0.0f) : cc->stepHeight;
                math::vec3 stepped = m.slide(pos + math::vec3{0, rise, 0}, side, true, false);

                const auto downHit = m.cast(stepped, {0, -1, 0}, rise + cc->skin * 2.0f);
                const auto movedSq = [&](math::vec3 p) { const math::vec3 d = horizontal(p - pos); return math::dot(d, d); };

                math::vec3 landN;
                const math::vec3 landed = stepped - math::vec3{0, downHit ? std::max(downHit->distance - cc->skin, 0.0f) : 0.0f, 0};
                const float feetY = pos.y - m.halfHeight - m.radius;
                const bool lowEnough = downHit && downHit->point.y - feetY <= cc->stepHeight + cc->skin;
                if (lowEnough && m.supports(*downHit, landed, landN) && movedSq(stepped) > movedSq(flat) + 1e-6f)
                {
                    chosen = landed;
                    *cc = before;
                    cc->hitWall = false;
                    m.note(*downHit, landed);
                }
                else
                    *cc = before;
            }
            pos = chosen;
        }

        // vertical: gravity and jumps. landing on walkable ground stops it, steep ground slides
        const float dy = displacement.y;
        if (std::abs(dy) > 1e-7f)
            pos = m.slide(pos, {0.0f, dy, 0.0f}, false, dy < 0.0f);

        // ground probe, and snapping down slopes / steps when we were on the ground and aren't moving up
        if (!cc->grounded)
        {
            const bool snap = wasGrounded && dy <= 0.0f;
            const float probe = cc->skin * 2.0f + (snap ? cc->snapDistance : 0.0f);
            if (const auto g = m.cast(pos, {0, -1, 0}, probe))
            {
                const math::vec3 at = pos - math::vec3{0, std::max(g->distance - cc->skin, 0.0f), 0};
                math::vec3 n;
                if (m.supports(*g, at, n))
                {
                    if (snap)
                        pos = at;
                    m.note(*g, at);
                }
            }
        }

        t->position = pos - col->offset;
    }
}