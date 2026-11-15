#pragma once
#include <optional>
#include <vector>

#include "core/util.hpp"
#include "math/vector.hpp"

namespace physics
{
    using MeshId = u32;
    constexpr MeshId invalid_mesh = UINT32_MAX;

    struct MeshHit
    {
        math::vec3 point, normal; // normal points from the shape into the mesh
        float depth;
        u32 triangle;
    };

    // static triangle soup with a bvh, triangles are one sided (ccw = front)
    class TriangleMesh
    {
    public:
        TriangleMesh(std::vector<math::vec3> vertices, std::vector<u32> indices, std::vector<u16> attributes = {});

        u32 triangle_count() const { return u32(indices_.size() / 3); }
        u16 attribute(u32 tri) const { return tri < attributes_.size() ? attributes_[tri] : u16(0); }
        math::vec3 bounds_min() const { return nodes_.empty() ? math::vec3{} : nodes_[0].min; }
        math::vec3 bounds_max() const { return nodes_.empty() ? math::vec3{} : nodes_[0].max; }

        // positions are in mesh space, deepest contact wins
        std::optional<MeshHit> collide_sphere(math::vec3 center, float radius) const;
        std::optional<MeshHit> collide_capsule(math::vec3 p0, math::vec3 p1, float radius) const;

    private:
        struct Node
        {
            math::vec3 min, max;
            u32 left = 0;  // children are left and left + 1
            u32 first = 0; // into tris_
            u32 count = 0; // > 0 means leaf
        };

        void build_node(u32 node, u32 first, u32 count, int depth);

        template <typename Fn>
        void query(math::vec3 lo, math::vec3 hi, Fn&& fn) const;

        void corners(u32 tri, math::vec3& a, math::vec3& b, math::vec3& c) const;

        std::vector<math::vec3> vertices_;
        std::vector<u32> indices_;
        std::vector<u16> attributes_;
        std::vector<Node> nodes_;
        std::vector<u32> tris_;
    };

    class MeshCache
    {
    public:
        MeshId add(std::vector<math::vec3> vertices, std::vector<u32> indices, std::vector<u16> attributes = {});
        const TriangleMesh* get(MeshId id) const { return id < meshes_.size() ? meshes_[id].get() : nullptr; }

        size_t size() const { return meshes_.size(); }

    private:
        std::vector<std::unique_ptr<TriangleMesh>> meshes_;
    };
}