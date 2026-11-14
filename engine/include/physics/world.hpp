#pragma once
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/ecs.hpp"
#include "core/util.hpp"
#include "math/vector.hpp"

namespace scene
{
    struct Transform;
}

namespace physics
{
    struct RigidBody
    {
        enum class Type : u8
        {
            Static,
            Kinematic,
            Dynamic
        };
        Type type = Type::Dynamic;
        math::vec3 velocity{};
        float invMass = 1.0f;
        float gravityScale = 1.0f;
        float restitution = 0.0f;
        float friction = 0.5f;
    };

    enum class ShapeType : u8
    {
        Sphere,
        Capsule,
        AABB,
        Mesh,
        Count
    };

    struct Collider
    {
        ShapeType shape = ShapeType::Sphere;
        union
        {
            struct
            {
                float radius;
            } sphere{0.5f};
            struct
            {
                float radius, halfHeight;
            } capsule;
            struct
            {
                math::vec3 halfExtents;
            } box;
        };

        math::vec3 offset{};
        u32 layer = 1;
        u32 mask = ~0u;
        bool isTrigger = false;
    };

    struct ContactManifold
    {
        math::vec3 point, normal; // normal points a -> b
        float depth;
    };

    enum class ContactPhase : u8
    {
        Begin,
        Stay,
        End
    };

    struct CollisionEvent
    {
        ecs::Entity a, b;
        u32 layerA, layerB;
        ContactPhase phase;
        bool trigger;
        math::vec3 point, normal;
        float depth;

        CollisionEvent flipped() const;
    };

    struct EntityPairKey
    {
        ecs::Entity a, b;
        static EntityPairKey make(ecs::Entity x, ecs::Entity y);
        bool operator==(const EntityPairKey&) const = default;
    };

    struct EntityPairKeyHash
    {
        size_t operator()(const EntityPairKey& k) const noexcept;
    };

    class ContactTracker
    {
    public:
        void beginStep();
        void report(CollisionEvent ev);
        void endStep(std::vector<CollisionEvent>& out);
        void forget(ecs::Entity e, std::vector<CollisionEvent>& out);
        void clear()
        {
            prev_.clear();
            curr_.clear();
        }

    private:
        std::unordered_map<EntityPairKey, CollisionEvent, EntityPairKeyHash> prev_, curr_;
    };

    class CollisionEvents
    {
    public:
        using Handler = std::function<void(const CollisionEvent&)>;

        // handler always gets ev.a from layerA and ev.b from layerB
        void subscribe(u32 layerA, u32 layerB, Handler h);
        void dispatch(const std::vector<CollisionEvent>& events) const;
        void clear() { subs_.clear(); }

    private:
        struct Sub
        {
            u32 a, b;
            Handler fn;
        };
        std::vector<Sub> subs_;
    };

    class PhysicsWorld
    {
    public:
        math::vec3 gravity{0.0f, -9.81f, 0.0f};

        void step(ecs::World& w, float dt);
        void flush_events();
        void on_destroyed(ecs::Entity e);
        void reset();

        CollisionEvents& events() { return events_; }

    private:
        struct Body
        {
            ecs::Entity e;
            scene::Transform* t;
            Collider* c;
            RigidBody* rb;
        };

        void gather(ecs::World& w);
        void integrate(float dt);
        void collide();
        void resolve(Body& a, Body& b, const ContactManifold& m);

        std::vector<Body> bodies_;
        ContactTracker tracker_;
        std::vector<CollisionEvent> pending_;
        CollisionEvents events_;
    };
} // namespace physics
