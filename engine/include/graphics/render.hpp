#pragma once

#include "vertex.hpp"
#include "material.hpp"
#include <vector>
#include <unordered_map>
#include <span>

using MeshHandle = u32;
using MaterialHandle = u32;
using ShaderHandle = u32;

struct MeshDescriptor
{
    VertexFormat format;
    std::span<const Vertex> vertices;
    std::span<const u32> indices;
};

struct ShaderDescriptor
{
    std::string vertexSource;
    std::string fragmentSource;
};

struct DrawCall
{
    u32 mesh;
    u32 material;

    // todo: instancing and draw order
};

struct RenderConfiguration
{
    u32 maxDrawCalls = UINT32_MAX;
    u32 maxMaterials = UINT32_MAX;
};

class RenderMgr
{
public:
    explicit RenderMgr(const RenderConfiguration& cfg);
    ~RenderMgr();
    RenderMgr(const RenderMgr&) = delete;
    RenderMgr& operator=(const RenderMgr&) = delete;

    ShaderHandle create_shader(const ShaderDescriptor& descriptor);
    MaterialHandle create_material(const ShaderHandle& shader);
    MeshHandle create_mesh(const MeshDescriptor& descriptor);
    void submit_draw_call(const DrawCall& drawCall);

    void flush(); // render all draw calls and clear

private:
    struct FormatPool
    {
        uin vao = 0, vbo = 0, ibo = 0;
        u32 vertexCt = 0, indexCt = 0;
        u32 vertexCap = 0, indexCap = 0;
    };

    struct GpuMesh
    {
        VertexFormat fmt;
        u32 base_vtx, first_idx, idx;
    };

    FormatPool& get_pool(VertexFormat fmt);

    RenderConfiguration cfg;

    std::vector<DrawCall> drawCalls;
    std::vector<GpuMesh> meshes;
    std::vector<GpuMaterial> materials;
    std::vector<GpuShader> shaders;
    std::unordered_map<VertexFormat, FormatPool> pools;
};