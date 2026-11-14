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

        std::optional<ContactManifold> sphere_sphere(const ShapeView& a, const ShapeView& b)
        {
            const math::vec3 d = b.center - a.center;
            const float dist2 = math::dot(d, d);
            const float r = a.c->sphere.radius + b.c->sphere.radius;
            if (dist2 >= r * r)
                return std::nullopt;

            const float dist = std::sqrt(dist2);
            const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3{0, 1, 0};
            return ContactManifold{a.center + n * a.c->sphere.radius, n, r - dist};
        }

        std::optional<ContactManifold> sphere_box(const ShapeView& s, const ShapeView& bx)
        {
            const float r = s.c->sphere.radius;
            const math::vec3 h = bx.c->box.halfExtents;
            const math::vec3 local = s.center - bx.center;

            const math::vec3 q{std::clamp(local.x, -h.x, h.x), std::clamp(local.y, -h.y, h.y),
                               std::clamp(local.z, -h.z, h.z)};
            const math::vec3 d = q - local;
            const float dist2 = math::dot(d, d);
            if (dist2 > r * r)
                return std::nullopt;

            if (dist2 > 1e-8f)
            {
                const float dist = std::sqrt(dist2);
                return ContactManifold{bx.center + q, d / dist, r - dist};
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
            return ContactManifold{s.center, n, r + best};
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
            //            Sphere         Capsule  AABB        Mesh
            /* Sphere  */ {sphere_sphere, nullptr, sphere_box, nullptr},
            /* Capsule */ {nullptr, nullptr, nullptr, nullptr},
            /* AABB    */ {box_sphere, nullptr, box_box, nullptr},
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
                Body& a = bodies_[i];
                Body& b = bodies_[j];

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

    void PhysicsWorld::resolve(Body& a, Body& b, const ContactManifold& m)
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
