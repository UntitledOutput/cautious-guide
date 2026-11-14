#pragma once
#include "core/util.hpp"
#include "math/vector.hpp"

// bodies & colliders

struct RigidBody
{
    enum class Type : u8 { Static, Kinematic, Dynamic };
    Type type = Type::Dynamic;
    math::vec3 velocity;
    float invMass = 1.0f;
    float gravityScale = 1.0f;
    float restitution = 0.0f;
    float friction = 0.5f;
};

enum class ShapeType : u8 { Sphere, Capsule, AABB, Mesh };

struct Collider
{
    ShapeType shape = ShapeType::Sphere;
    union 
    {
        struct
        {
            float radius;
        } sphere;

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

// event layer

using Entity = u32;

struct EntityPairKey
{
    Entity a, b; // a should always be less than b
    static EntityPairKey make(Entity x, Entity y);
};

struct EntityPairKeyHash
{
    size_t operator()(const EntityPairKey& k) const noexcept;
};

enum class ContactPhase : u8 {Begin, Stay, End};

struct CollisionEvent
{
    Entity a, b;
    ContactPhase phase;
    bool trigger;
    math::vec3 point;
    math::vec3 normal;
    float depth;
};

class ContactTracker
{
public:
    void beginStep();

    void report(Entity x, Entity y, bool trigger, const math::vec3& p, const math::vec3& n, float d);

    void endStep(std::vector<CollisionEvent>& out);

    void forget(Entity e, std::vector<CollisionEvent>& out);

private:
    std::unordered_map<EntityPairKey, CollisionEvent, EntityPairKeyHash> prev_, curr_;
};

class CollisionEvents
{
public:
    using Handler = std::function<void(const CollisionEvent&)>;

    void subscribe(uint32_t layerA, uint32_t layerB, Handler h);

    void dispatch(const std::vector<CollisionEvent>& events, auto&& layerOf);

private:
    struct Sub
    {
        uint32_t a, b;
        Handler fn;
    };
    std::vector<Sub> subs_;
};


