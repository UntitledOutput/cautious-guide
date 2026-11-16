#include "physics/world.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <tuple>

#include "core/components/transform.hpp"
#include "graphics/model.hpp"
#include "physics/geometry.hpp"
#include "physics/contact.hpp"

namespace physics
{
    namespace
    {
        using geo::Segment;
        using geo::closest_between_segments;
        using geo::closest_on_segment;

        struct ShapeView
        {
            math::vec3 center;
            const Collider* c;
            const TriangleMesh* mesh = nullptr; // only set for mesh colliders
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
        Segment capsule_segment(const ShapeView& s)
        {
            const math::vec3 up{0.0f, s.c->capsule.halfHeight, 0.0f};
            return {s.center - up, s.center + up};
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

        std::optional<ContactManifold> to_manifold(const std::optional<MeshHit>& h, const ShapeView& m, bool flip)
        {
            if (!h)
                return std::nullopt;
            ContactManifold out{h->point + m.center, flip ? -h->normal : h->normal, h->depth};
            out.surface = m.mesh->attribute(h->triangle);
            return out;
        }

        std::optional<ContactManifold> sphere_mesh(const ShapeView& s, const ShapeView& m)
        {
            if (!m.mesh)
                return std::nullopt;
            return to_manifold(m.mesh->collide_sphere(s.center - m.center, s.c->sphere.radius), m, false);
        }

        std::optional<ContactManifold> mesh_sphere(const ShapeView& m, const ShapeView& s)
        {
            if (!m.mesh)
                return std::nullopt;
            return to_manifold(m.mesh->collide_sphere(s.center - m.center, s.c->sphere.radius), m, true);
        }

        std::optional<ContactManifold> capsule_mesh(const ShapeView& cap, const ShapeView& m)
        {
            if (!m.mesh)
                return std::nullopt;
            const Segment seg = capsule_segment(cap);
            return to_manifold(m.mesh->collide_capsule(seg.p0 - m.center, seg.p1 - m.center, cap.c->capsule.radius), m,
                               false);
        }

        std::optional<ContactManifold> mesh_capsule(const ShapeView& m, const ShapeView& cap)
        {
            if (!m.mesh)
                return std::nullopt;
            const Segment seg = capsule_segment(cap);
            return to_manifold(m.mesh->collide_capsule(seg.p0 - m.center, seg.p1 - m.center, cap.c->capsule.radius), m,
                               true);
        }

        constexpr size_t kShapes = size_t(ShapeType::Count);
        constexpr NarrowFn kNarrow[kShapes][kShapes] = {
            //            Sphere          Capsule          AABB         Mesh
            /* Sphere  */ {sphere_sphere,  sphere_capsule,  sphere_box,  sphere_mesh},
            /* Capsule */ {capsule_sphere, capsule_capsule, capsule_box, capsule_mesh},
            /* AABB    */ {box_sphere,     box_capsule,     box_box,     nullptr},
            /* Mesh    */ {mesh_sphere,    mesh_capsule,    nullptr,     nullptr},
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

    void PhysicsWorld::overlap_capsule(ecs::World& w, math::vec3 center, float radius, float halfHeight,
                                       std::vector<Overlap>& out, const CastFilter& filter) const
    {
        Collider probe;
        probe.shape = ShapeType::Capsule;
        probe.capsule = {radius, halfHeight};
        const ShapeView pv{center, &probe};

        w.view<scene::Transform, Collider>().each(
            [&](ecs::Entity e, scene::Transform& t, Collider& c)
            {
                if (e == filter.ignore || !(c.layer & filter.mask) || (c.isTrigger && !filter.hitTriggers))
                    return;

                const NarrowFn fn = kNarrow[size_t(ShapeType::Capsule)][size_t(c.shape)];
                if (!fn)
                    return;

                const ShapeView ov{t.position + c.offset, &c, c.shape == ShapeType::Mesh ? mesh(c.mesh.id) : nullptr};
                if (const auto m = fn(pv, ov))
                    out.push_back({e, -m->normal, m->depth, m->surface});
            });
    }

    void PhysicsWorld::collide()
    {
        auto view_of = [this](const InternalPhysicsBody& b)
        {
            const bool isMesh = b.c->shape == ShapeType::Mesh;
            return ShapeView{b.t->position + b.c->offset, b.c, isMesh ? mesh(b.c->mesh.id) : nullptr};
        };

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

                const auto m = fn(view_of(a), view_of(b));
                if (!m)
                    continue;

                const bool trigger = a.c->isTrigger || b.c->isTrigger;
                if (!trigger)
                    resolve(a, b, *m);

                tracker_.report({a.e, b.e, a.c->layer, b.c->layer, ContactPhase::Stay, trigger, m->point, m->normal,
                                 m->depth, m->surface});
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

        // coulomb friction, using velocities after the normal impulse
        const math::vec3 rv = (b.rb ? b.rb->velocity : math::vec3{}) - (a.rb ? a.rb->velocity : math::vec3{});
        const math::vec3 vt = rv - m.normal * math::dot(rv, m.normal);
        const float vtLen = math::length(vt);
        if (vtLen < 1e-6f)
            return;

        constexpr float kDefaultFriction = 0.5f; // for colliders without a body
        const float mu = std::sqrt((a.rb ? a.rb->friction : kDefaultFriction) * (b.rb ? b.rb->friction : kDefaultFriction));
        const float jt = std::min(vtLen / sum, mu * j);
        const math::vec3 t = vt / vtLen;
        if (ia > 0.0f)
            a.rb->velocity += t * (jt * ia);
        if (ib > 0.0f)
            b.rb->velocity -= t * (jt * ib);
    }
} // namespace physics