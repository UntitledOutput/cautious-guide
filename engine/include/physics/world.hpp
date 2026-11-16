#pragma once
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "contact.hpp"
#include "mesh.hpp"
#include "core/ecs.hpp"
#include "core/util.hpp"
#include "math/matrix.hpp"
#include "math/vector.hpp"

namespace graphics
{
    struct MeshData;
}

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
            struct
            {
                u32 id; // from PhysicsWorld::create_mesh
            } mesh;
        };

        math::vec3 offset{};
        u32 layer = 1;
        u32 mask = ~0u;
        bool isTrigger = false;

        math::mat4 matrix() const
        {
            // offset matrix applying the collider's offset and scale to the transform's matrix
            var mat = math::translate(offset);

            math::vec3 scale = {1,1,1};

            switch (shape)
            {

                case ShapeType::Sphere:
                    scale *= sphere.radius;
                    break;
                case ShapeType::Capsule:
                    scale.y *= capsule.halfHeight*2;
                    scale.x *= capsule.radius;
                    scale.z *= capsule.radius;
                    break;
                case ShapeType::AABB:
                    scale = box.halfExtents/2.f;
                    break;
                case ShapeType::Mesh:
                    break;
                case ShapeType::Count:
                    break;
            }

            mat = mat * math::scale(scale);
            return mat;
        }
    };

    struct InternalPhysicsBody
    {
        ecs::Entity e;
        scene::Transform* t;
        Collider* c;
        RigidBody* rb;
    };

    struct CastHit
    {
        ecs::Entity entity;
        math::vec3 point; // where the surface was touched
        math::vec3 normal; // surface normal at the touch, facing the caster
        float distance; // how far the ray / sphere center travelled
        u16 surface = 0; // mesh triangle attribute
        u32 triangle = UINT32_MAX;
        math::vec3 barycentric{}; // mesh hits only
    };

    struct CastFilter
    {
        u32 mask = ~0u; // layers to hit
        ecs::Entity ignore = ecs::null_entity;
        bool hitTriggers = false;
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

        explicit PhysicsWorld(std::shared_ptr<MeshCache> meshes = std::make_shared<MeshCache>()) :
            meshes_(std::move(meshes))
        {
        }

        MeshId create_mesh(std::vector<math::vec3> vertices, std::vector<u32> indices, std::vector<u16> attributes = {})
        {
            return meshes_->add(std::move(vertices), std::move(indices), std::move(attributes));
        }
        const TriangleMesh* mesh(MeshId id) const { return meshes_->get(id); }
        MeshCache& meshes() { return *meshes_; }
        
        std::vector<InternalPhysicsBody> bodies() const { return bodies_; }

                // casts read the ecs directly so they're safe to call any time. shapes the cast starts inside are skipped
        std::optional<CastHit> raycast(ecs::World& w, math::vec3 origin, math::vec3 dir, float maxDist,
                                       const CastFilter& filter = {}) const;
        std::optional<CastHit> sphere_cast(ecs::World& w, math::vec3 origin, float radius, math::vec3 dir,
                                           float maxDist, const CastFilter& filter = {}) const;

    private:
        std::shared_ptr<MeshCache> meshes_;

        void gather(ecs::World& w);
        void integrate(float dt);
        void collide();
        void resolve(InternalPhysicsBody& a, InternalPhysicsBody& b, const ContactManifold& m);

                std::optional<CastHit> cast(ecs::World& w, math::vec3 origin, float radius, math::vec3 dir, float maxDist,
                                    const CastFilter& filter) const;

        std::vector<InternalPhysicsBody> bodies_;
        ContactTracker tracker_;
        std::vector<CollisionEvent> pending_;
        CollisionEvents events_;
    };
} // namespace physics
