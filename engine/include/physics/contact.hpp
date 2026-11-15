#pragma once
#include "core/ecs.hpp"
#include "core/util.hpp"
#include "math/vector.hpp"

namespace physics
{

    struct ContactManifold
    {
        math::vec3 point, normal; // normal points a -> b
        float depth;
        u16 surface = 0;
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
        u16 surface = 0;

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

} // namespace
