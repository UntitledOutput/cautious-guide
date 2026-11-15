#include "physics/world.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <tuple>

#include "core/components/transform.hpp"

namespace physics
{
    namespace
    {
        struct ShapeView
        {
            math::vec3 center;
            const Collider* c;
        };

        using NarrowFn = std::optional<ContactManifold> (*)(const ShapeView&, const ShapeView&);

        std::optional<ContactManifold> point_point(math::vec3 pa, float ra, math::vec3 pb, float rb)
        {
            const math::vec3 d = pb - pa;
            const float dist2 = math::dot(d, d);
            const float r = ra + rb;
            if (dist2 >= r * r)
                return std::nullopt;

            const float dist = std::sqrt(dist2);
            const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3{0, 1, 0};
            return ContactManifold{pa + n * ra, n, r - dist};
        }

        std::optional<ContactManifold> point_box(math::vec3 p, float r, math::vec3 boxCenter, math::vec3 h)
        {
            const math::vec3 local = p - boxCenter;
            const math::vec3 q{std::clamp(local.x, -h.x, h.x), std::clamp(local.y, -h.y, h.y),
                               std::clamp(local.z, -h.z, h.z)};
            const math::vec3 d = q - local;
            const float dist2 = math::dot(d, d);
            if (dist2 > r * r)
                return std::nullopt;

            if (dist2 > 1e-8f)
            {
                const float dist = std::sqrt(dist2);
                return ContactManifold{boxCenter + q, d / dist, r - dist};
            }

            // center is inside the box, push out through the nearest face
            int axis = 0;
            float best = h.x - std::abs(local.x);
            for (int i = 1; i < 3; ++i)
            {
                const float f = h[i] - std::abs(local[i]);
                if (f < best)
                {
                    best = f;
                    axis = i;
                }
            }
            math::vec3 n{};
            n[axis] = local[axis] > 0.0f ? -1.0f : 1.0f;
            return ContactManifold{p, n, r + best};
        }

        // capsules are y-up segments, halfHeight is the straight part only
        struct Segment
        {
            math::vec3 p0, p1;
        };

        Segment capsule_segment(const ShapeView& s)
        {
            const math::vec3 up{0.0f, s.c->capsule.halfHeight, 0.0f};
            return {s.center - up, s.center + up};
        }

        math::vec3 closest_on_segment(const Segment& s, math::vec3 p)
        {
            const math::vec3 d = s.p1 - s.p0;
            const float len2 = math::dot(d, d);
            if (len2 < 1e-12f)
                return s.p0;
            const float t = std::clamp(math::dot(p - s.p0, d) / len2, 0.0f, 1.0f);
            return s.p0 + d * t;
        }

        // ericson, real-time collision detection 5.1.9
        void closest_between_segments(const Segment& s1, const Segment& s2, math::vec3& c1, math::vec3& c2)
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

        std::optional<ContactManifold> sphere_sphere(const ShapeView& a, const ShapeView& b)
        {
            return point_point(a.center, a.c->sphere.radius, b.center, b.c->sphere.radius);
        }

        std::optional<ContactManifold> sphere_box(const ShapeView& s, const ShapeView& bx)
        {
            return point_box(s.center, s.c->sphere.radius, bx.center, bx.c->box.halfExtents);
        }

        std::optional<ContactManifold> capsule_sphere(const ShapeView& cap, const ShapeView& s)
        {
            const math::vec3 p = closest_on_segment(capsule_segment(cap), s.center);
            return point_point(p, cap.c->capsule.radius, s.center, s.c->sphere.radius);
        }

        std::optional<ContactManifold> sphere_capsule(const ShapeView& s, const ShapeView& cap)
        {
            auto m = capsule_sphere(cap, s);
            if (m)
                m->normal = -m->normal;
            return m;
        }

        std::optional<ContactManifold> capsule_capsule(const ShapeView& a, const ShapeView& b)
        {
            math::vec3 ca, cb;
            closest_between_segments(capsule_segment(a), capsule_segment(b), ca, cb);
            return point_point(ca, a.c->capsule.radius, cb, b.c->capsule.radius);
        }

        std::optional<ContactManifold> capsule_box(const ShapeView& cap, const ShapeView& bx)
        {
            // segment is vertical and the box is axis aligned, so the closest point only depends on y
            const Segment seg = capsule_segment(cap);
            const math::vec3 h = bx.c->box.halfExtents;
            const float lo = std::max(seg.p0.y, bx.center.y - h.y);
            const float hi = std::min(seg.p1.y, bx.center.y + h.y);

            math::vec3 p = cap.center;
            if (lo <= hi)
                p.y = (lo + hi) * 0.5f;
            else
                p.y = seg.p0.y > bx.center.y ? seg.p0.y : seg.p1.y;

            return point_box(p, cap.c->capsule.radius, bx.center, h);
        }

        std::optional<ContactManifold> box_capsule(const ShapeView& bx, const ShapeView& cap)
        {
            auto m = capsule_box(cap, bx);
            if (m)
                m->normal = -m->normal;
            return m;
        }

        std::optional<ContactManifold> box_sphere(const ShapeView& bx, const ShapeView& s)
        {
            auto m = sphere_box(s, bx);
            if (m)
                m->normal = -m->normal;
            return m;
        }

        std::optional<ContactManifold> box_box(const ShapeView& a, const ShapeView& b)
        {
            const math::vec3 d = b.center - a.center;
            const math::vec3 ha = a.c->box.halfExtents, hb = b.c->box.halfExtents;

            int axis = 0;
            float best = FLT_MAX;
            for (int i = 0; i < 3; ++i)
            {
                const float o = ha[i] + hb[i] - std::abs(d[i]);
                if (o <= 0.0f)
                    return std::nullopt;
                if (o < best)
                {
                    best = o;
                    axis = i;
                }
            }
            math::vec3 n{};
            n[axis] = d[axis] >= 0.0f ? 1.0f : -1.0f;
            return ContactManifold{a.center + d * 0.5f, n, best}; // point is approximate
        }

        constexpr size_t kShapes = size_t(ShapeType::Count);
        constexpr NarrowFn kNarrow[kShapes][kShapes] = {
            //            Sphere          Capsule          AABB         Mesh
            /* Sphere  */ {sphere_sphere, sphere_capsule, sphere_box, nullptr},
            /* Capsule */ {capsule_sphere, capsule_capsule, capsule_box, nullptr},
            /* AABB    */ {box_sphere, box_capsule, box_box, nullptr},
            /* Mesh    */ {nullptr, nullptr, nullptr, nullptr},
        };

        float inv_mass(const RigidBody* rb) { return rb && rb->type == RigidBody::Type::Dynamic ? rb->invMass : 0.0f; }

        auto order(const CollisionEvent& e) { return std::tie(e.a.index, e.b.index, e.phase); }
    } // namespace

    // ---- events ----

    CollisionEvent CollisionEvent::flipped() const
    {
        CollisionEvent e = *this;
        std::swap(e.a, e.b);
        std::swap(e.layerA, e.layerB);
        e.normal = -e.normal;
        return e;
    }

    EntityPairKey EntityPairKey::make(ecs::Entity x, ecs::Entity y)
    {
        return x.index < y.index ? EntityPairKey{x, y} : EntityPairKey{y, x};
    }

    size_t EntityPairKeyHash::operator()(const EntityPairKey& k) const noexcept
    {
        const std::uint64_t idx = (std::uint64_t(k.a.index) << 32 | k.b.index) * 0x9E3779B97F4A7C15ull;
        const std::uint64_t gen = (std::uint64_t(k.a.generation) << 32 | k.b.generation) * 0xC2B2AE3D27D4EB4Full;
        return size_t(idx ^ (gen >> 1));
    }

    void ContactTracker::beginStep()
    {
        std::swap(prev_, curr_);
        curr_.clear();
    }

    void ContactTracker::report(CollisionEvent ev)
    {
        if (ev.a.index > ev.b.index)
            ev = ev.flipped();
        curr_[{ev.a, ev.b}] = ev;
    }

    void ContactTracker::endStep(std::vector<CollisionEvent>& out)
    {
        const size_t start = out.size();
        for (auto& [key, ev] : curr_)
        {
            auto e = ev;
            e.phase = prev_.contains(key) ? ContactPhase::Stay : ContactPhase::Begin;
            out.push_back(e);
        }
        for (auto& [key, ev] : prev_)
        {
            if (!curr_.contains(key))
            {
                auto e = ev;
                e.phase = ContactPhase::End;
                out.push_back(e);
            }
        }
        // hash map order isn't stable across platforms, which matters for netcode
        std::sort(out.begin() + start, out.end(), [](auto& x, auto& y) { return order(x) < order(y); });
    }

    void ContactTracker::forget(ecs::Entity e, std::vector<CollisionEvent>& out)
    {
        auto touches = [&](const auto& kv) { return kv.first.a == e || kv.first.b == e; };
        std::erase_if(curr_,
                      [&](const auto& kv)
                      {
                          if (!touches(kv))
                              return false;
                          auto ev = kv.second;
                          ev.phase = ContactPhase::End;
                          out.push_back(ev);
                          return true;
                      });
        std::erase_if(prev_, touches);
    }

    void CollisionEvents::subscribe(u32 layerA, u32 layerB, Handler h)
    {
        subs_.push_back({layerA, layerB, std::move(h)});
    }

    void CollisionEvents::dispatch(const std::vector<CollisionEvent>& events) const
    {
        for (const auto& ev : events)
            for (const auto& s : subs_)
            {
                if ((s.a & ev.layerA) && (s.b & ev.layerB))
                    s.fn(ev);
                else if ((s.a & ev.layerB) && (s.b & ev.layerA))
                    s.fn(ev.flipped());
            }
    }

    // ---- world ----

    void PhysicsWorld::step(ecs::World& w, float dt)
    {
        tracker_.beginStep();
        gather(w);
        integrate(dt);
        collide();
        tracker_.endStep(pending_);
    }

    void PhysicsWorld::flush_events()
    {
        events_.dispatch(pending_);
        pending_.clear();
    }

    void PhysicsWorld::on_destroyed(ecs::Entity e) { tracker_.forget(e, pending_); }

    void PhysicsWorld::reset()
    {
        tracker_.clear();
        pending_.clear();
        bodies_.clear();
    }

    void PhysicsWorld::gather(ecs::World& w)
    {
        bodies_.clear();
        w.view<scene::Transform, Collider>().each([&](ecs::Entity e, scene::Transform& t, Collider& c)
                                                  { bodies_.push_back({e, &t, &c, w.try_get<RigidBody>(e)}); });
    }

    void PhysicsWorld::integrate(float dt)
    {
        for (auto& b : bodies_)
        {
            if (!b.rb || b.rb->type == RigidBody::Type::Static)
                continue;
            if (b.rb->type == RigidBody::Type::Dynamic)
                b.rb->velocity += gravity * (b.rb->gravityScale * dt);
            b.t->position += b.rb->velocity * dt;
        }
    }

    void PhysicsWorld::collide()
    {
        // brute force for now, swap for a grid/BVH once counts grow
        for (size_t i = 0; i < bodies_.size(); ++i)
            for (size_t j = i + 1; j < bodies_.size(); ++j)
            {
                InternalPhysicsBody& a = bodies_[i];
                InternalPhysicsBody& b = bodies_[j];

                if (!(a.c->layer & b.c->mask) || !(b.c->layer & a.c->mask))
                    continue;
                if (!a.rb && !b.rb)
                    continue;

                const NarrowFn fn = kNarrow[size_t(a.c->shape)][size_t(b.c->shape)];
                if (!fn)
                    continue;

                const auto m = fn({a.t->position + a.c->offset, a.c}, {b.t->position + b.c->offset, b.c});
                if (!m)
                    continue;

                const bool trigger = a.c->isTrigger || b.c->isTrigger;
                if (!trigger)
                    resolve(a, b, *m);

                tracker_.report(
                    {a.e, b.e, a.c->layer, b.c->layer, ContactPhase::Stay, trigger, m->point, m->normal, m->depth});
            }
    }

    void PhysicsWorld::resolve(InternalPhysicsBody& a, InternalPhysicsBody& b, const ContactManifold& m)
    {
        const float ia = inv_mass(a.rb), ib = inv_mass(b.rb);
        const float sum = ia + ib;
        if (sum <= 0.0f)
            return;

        constexpr float slop = 0.01f, percent = 0.8f;
        const math::vec3 corr = m.normal * (std::max(m.depth - slop, 0.0f) * percent / sum);
        a.t->position -= corr * ia;
        b.t->position += corr * ib;

        const math::vec3 va = a.rb ? a.rb->velocity : math::vec3{};
        const math::vec3 vb = b.rb ? b.rb->velocity : math::vec3{};
        const float vn = math::dot(vb - va, m.normal);
        if (vn >= 0.0f)
            return; // already separating

        const float e = std::min(a.rb ? a.rb->restitution : 0.0f, b.rb ? b.rb->restitution : 0.0f);
        const float j = -(1.0f + e) * vn / sum;
        if (ia > 0.0f)
            a.rb->velocity -= m.normal * (j * ia);
        if (ib > 0.0f)
            b.rb->velocity += m.normal * (j * ib);
    }
} // namespace physics
