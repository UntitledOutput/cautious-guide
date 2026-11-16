#include "physics/mesh.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numeric>

#include "physics/geometry.hpp"
#include "graphics/model.hpp"

namespace physics
{
    using namespace geo;

    namespace
    {
        constexpr u32 kLeafSize = 4;
        constexpr int kMaxDepth = 30;
        constexpr float kEps = 1e-6f;
    }

    TriangleMesh::TriangleMesh(std::vector<math::vec3> vertices, std::vector<u32> indices, std::vector<u16> attributes)
        : vertices_(std::move(vertices)), indices_(std::move(indices)), attributes_(std::move(attributes))
    {
        indices_.resize(indices_.size() - indices_.size() % 3);
        tris_.resize(triangle_count());
        std::iota(tris_.begin(), tris_.end(), 0u);

        if (tris_.empty())
            return;
        nodes_.reserve(tris_.size() * 2 / kLeafSize + 1);
        nodes_.emplace_back();
        build_node(0, 0, u32(tris_.size()), 0);
    }

    void TriangleMesh::corners(u32 tri, math::vec3& a, math::vec3& b, math::vec3& c) const
    {
        a = vertices_[indices_[tri * 3 + 0]];
        b = vertices_[indices_[tri * 3 + 1]];
        c = vertices_[indices_[tri * 3 + 2]];
    }

    void TriangleMesh::build_node(u32 node, u32 first, u32 count, int depth)
    {
        math::vec3 lo{FLT_MAX, FLT_MAX, FLT_MAX}, hi{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        math::vec3 clo = lo, chi = hi;
        for (u32 i = first; i < first + count; ++i)
        {
            math::vec3 a, b, c;
            corners(tris_[i], a, b, c);
            lo = vmin(lo, vmin(a, vmin(b, c)));
            hi = vmax(hi, vmax(a, vmax(b, c)));
            const math::vec3 centroid = (a + b + c) / 3.0f;
            clo = vmin(clo, centroid);
            chi = vmax(chi, centroid);
        }
        nodes_[node].min = lo;
        nodes_[node].max = hi;

        const math::vec3 ext = chi - clo;
        const int axis = ext.x > ext.y ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);
        if (count <= kLeafSize || depth >= kMaxDepth || ext[axis] <= 0.0f)
        {
            nodes_[node].first = first;
            nodes_[node].count = count;
            return;
        }

        auto centroid = [&](u32 t)
        {
            math::vec3 a, b, c;
            corners(t, a, b, c);
            return a[axis] + b[axis] + c[axis];
        };
        const u32 mid = first + count / 2;
        std::nth_element(tris_.begin() + first, tris_.begin() + mid, tris_.begin() + first + count,
                         [&](u32 x, u32 y) { return centroid(x) < centroid(y); });

        const u32 left = u32(nodes_.size());
        nodes_.emplace_back();
        nodes_.emplace_back();
        nodes_[node].left = left; // re-index, emplace_back may have reallocated
        build_node(left, first, mid - first, depth + 1);
        build_node(left + 1, mid, first + count - mid, depth + 1);
    }

    template <typename Fn>
    void TriangleMesh::query(math::vec3 lo, math::vec3 hi, Fn&& fn) const
    {
        if (nodes_.empty())
            return;

        u32 stack[kMaxDepth * 2 + 2];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0)
        {
            const Node& n = nodes_[stack[--sp]];
            if (n.max.x < lo.x || n.min.x > hi.x || n.max.y < lo.y || n.min.y > hi.y || n.max.z < lo.z ||
                n.min.z > hi.z)
                continue;

            if (n.count > 0)
            {
                for (u32 i = n.first; i < n.first + n.count; ++i)
                    fn(tris_[i]);
            }
            else
            {
                stack[sp++] = n.left;
                stack[sp++] = n.left + 1;
            }
        }
    }

    std::optional<MeshHit> TriangleMesh::collide_sphere(math::vec3 center, float radius) const
    {
        std::optional<MeshHit> best;
        const math::vec3 r{radius, radius, radius};

        query(center - r, center + r,
              [&](u32 t)
              {
                  math::vec3 a, b, c;
                  corners(t, a, b, c);
                  math::vec3 n = math::cross(b - a, c - a);
                  const float len = math::length(n);
                  if (len < kEps)
                      return;
                  n = n / len;

                  if (math::dot(center - a, n) < 0.0f)
                      return; // behind the face

                  const math::vec3 q = closest_on_triangle(center, a, b, c);
                  const math::vec3 d = q - center;
                  const float dist2 = math::dot(d, d);
                  if (dist2 >= radius * radius)
                      return;

                  const float dist = std::sqrt(dist2);
                  const float depth = radius - dist;
                  if (!best || depth > best->depth)
                      best = MeshHit{q, dist > kEps ? d / dist : -n, depth, t};
              });
        return best;
    }

    std::optional<MeshHit> TriangleMesh::collide_capsule(math::vec3 p0, math::vec3 p1, float radius) const
    {
        std::optional<MeshHit> best;
        const math::vec3 r{radius, radius, radius};
        const Segment seg{p0, p1};

        auto consider = [&](const MeshHit& h)
        {
            if (!best || h.depth > best->depth)
                best = h;
        };

        query(vmin(p0, p1) - r, vmax(p0, p1) + r,
              [&](u32 t)
              {
                  math::vec3 a, b, c;
                  corners(t, a, b, c);
                  math::vec3 n = math::cross(b - a, c - a);
                  const float len = math::length(n);
                  if (len < kEps)
                      return;
                  n = n / len;

                  const float s0 = math::dot(p0 - a, n), s1 = math::dot(p1 - a, n);
                  if (s0 < 0.0f && s1 < 0.0f)
                      return; // fully behind the face

                  // segment passes through the triangle: push the buried end back out along the normal
                  if ((s0 < 0.0f) != (s1 < 0.0f))
                  {
                      const math::vec3 x = p0 + (p1 - p0) * (s0 / (s0 - s1));
                      const math::vec3 onTri = closest_on_triangle(x, a, b, c);
                      if (math::dot(onTri - x, onTri - x) < kEps * kEps)
                      {
                          consider({x, -n, radius - std::min(s0, s1), t});
                          return;
                      }
                  }

                  // otherwise the closest pair is an endpoint vs the face or a triangle edge vs the segment
                  math::vec3 bestSeg = p0, bestTri = closest_on_triangle(p0, a, b, c);
                  float bestD2 = math::dot(bestTri - bestSeg, bestTri - bestSeg);

                  auto test = [&](math::vec3 sp, math::vec3 tp)
                  {
                      const float d2 = math::dot(tp - sp, tp - sp);
                      if (d2 < bestD2)
                      {
                          bestD2 = d2;
                          bestSeg = sp;
                          bestTri = tp;
                      }
                  };

                  test(p1, closest_on_triangle(p1, a, b, c));
                  const Segment edges[3] = {{a, b}, {b, c}, {c, a}};
                  for (const Segment& e : edges)
                  {
                      math::vec3 cs, ce;
                      closest_between_segments(seg, e, cs, ce);
                      test(cs, ce);
                  }

                  if (bestD2 >= radius * radius)
                      return;

                  const float dist = std::sqrt(bestD2);
                  consider({bestTri, dist > kEps ? (bestTri - bestSeg) / dist : -n, radius - dist, t});
              });
        return best;
    }

    std::optional<MeshCastHit> TriangleMesh::cast(math::vec3 o, math::vec3 dir, float radius, float maxDist) const
    {
        if (nodes_.empty())
            return std::nullopt;

        std::optional<MeshCastHit> best;
        float bestT = maxDist;
        const math::vec3 pad{radius, radius, radius};

        auto test_triangle = [&](u32 t)
        {
            math::vec3 a, b, c;
            corners(t, a, b, c);
            math::vec3 n = math::cross(b - a, c - a);
            const float len = math::length(n);
            if (len < kEps)
                return;
            n = n / len;

            const float dn = math::dot(dir, n);
            const float s0 = math::dot(o - a, n);
            if (dn >= 0.0f || s0 < 0.0f)
                return; // moving away, parallel, or behind the face

            // face: first touch is when the center is radius above the plane
            const float tf = (s0 - radius) / -dn;
            if (tf >= 0.0f && tf <= bestT)
            {
                const math::vec3 contact = o + dir * tf - n * radius;
                const math::vec3 bc = barycentric(contact, a, b, c);
                if (bc.x >= -1e-5f && bc.y >= -1e-5f && bc.z >= -1e-5f)
                {
                    bestT = tf;
                    best = MeshCastHit{tf, contact, n, t, bc};
                    return; // nothing on this triangle can be hit earlier
                }
            }

            if (radius <= 0.0f)
                return;

            // edges and corners
            const Segment edges[3] = {{a, b}, {b, c}, {c, a}};
            for (const Segment& e : edges)
                if (const auto h = ray_capsule(o, dir, bestT, e.p0, e.p1, radius))
                {
                    const math::vec3 center = o + dir * h->t;
                    const math::vec3 contact = closest_on_segment(e, center);
                    bestT = h->t;
                    best = MeshCastHit{h->t, contact, h->normal, t, barycentric(contact, a, b, c)};
                }
        };

        u32 stack[kMaxDepth * 2 + 2];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0)
        {
            const Node& node = nodes_[stack[--sp]];
            const math::vec3 lo = node.min - pad, hi = node.max + pad;
            const bool inside = o.x >= lo.x && o.y >= lo.y && o.z >= lo.z && o.x <= hi.x && o.y <= hi.y && o.z <= hi.z;
            if (!inside && !ray_aabb(o, dir, bestT, lo, hi))
                continue;

            if (node.count > 0)
            {
                for (u32 i = node.first; i < node.first + node.count; ++i)
                    test_triangle(tris_[i]);
            }
            else
            {
                stack[sp++] = node.left;
                stack[sp++] = node.left + 1;
            }
        }
        return best;
    }

    MeshId MeshCache::add(std::vector<math::vec3> vertices, std::vector<u32> indices, std::vector<u16> attributes)
    {
        meshes_.push_back(
            std::make_unique<TriangleMesh>(std::move(vertices), std::move(indices), std::move(attributes)));
        return MeshId(meshes_.size() - 1);
    }
}

namespace graphics
{
    namespace
    {
        float det3(const math::mat4& m)
        {
            const math::vec3 x{m[0].x, m[0].y, m[0].z}, y{m[1].x, m[1].y, m[1].z}, z{m[2].x, m[2].y, m[2].z};
            return math::dot(math::cross(x, y), z);
        }
    } // namespace

    std::shared_ptr<const CollisionGeometry> build_collision_geometry(const ModelData& data,
                                                                      std::span<const math::mat4> nodeWorld)
    {
        auto geo = std::make_shared<CollisionGeometry>();

        for (u32 ni = 0; ni < data.nodes.size(); ++ni)
        {
            const NodeData& node = data.nodes[ni];
            if (node.skin >= 0)
                continue; // skinned meshes deform, they don't belong in static collision

            const math::mat4& m = nodeWorld[ni];
            const bool mirrored = det3(m) < 0.0f; // negative scale flips the winding

            for (u32 mi : node.meshes)
            {
                const MeshData& mesh = data.meshes[mi];
                const u32 base = u32(geo->positions.size());

                for (const math::vec3& p : mesh.positions)
                    geo->positions.push_back(math::transform_point(m, p));

                for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
                {
                    const u32 a = base + mesh.indices[i], b = base + mesh.indices[i + 1],
                              c = base + mesh.indices[i + 2];
                    geo->indices.push_back(a);
                    geo->indices.push_back(mirrored ? c : b);
                    geo->indices.push_back(mirrored ? b : c);
                }
            }
        }
        return geo;
    }
} // namespace graphics
