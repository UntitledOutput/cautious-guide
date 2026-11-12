#pragma once

#include "core/util.hpp"
#include "math/vector.hpp"
#include <cstdint>
#include <type_traits>

enum class VertexFormat : uint8_t
{
    None = 0, 
    Position = 1 << 0,
    Normal = 1 << 1,
    UV0 = 1 << 2,
    UV1 = 1 << 3,
    UV2 = 1 << 4,
    Joints = 1 << 5,
    Weights = 1 << 6,
};

inline VertexFormat operator|(VertexFormat lhs, VertexFormat rhs)
{
    using T = std::underlying_type_t<VertexFormat>;
    return static_cast<VertexFormat>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline VertexFormat operator&(VertexFormat lhs, VertexFormat rhs)
{
    using T = std::underlying_type_t<VertexFormat>;
    return static_cast<VertexFormat>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

inline VertexFormat& operator|=(VertexFormat& lhs, VertexFormat rhs)
{
    lhs = lhs | rhs;
    return lhs;
}


struct Vertex
{
    math::vec3 position;
    math::vec3 normal;
    math::vec2 uv0;
    u16 joints[4];
    math::vec4 weights;
};