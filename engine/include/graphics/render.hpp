#pragma once

#include "vertex.hpp"
#include "material.hpp"
#include <vector>
#include <unordered_map>
#include <span>
#include <glad/glad.h>

#include "camera.hpp"
#include "texture.hpp"

namespace graphics
{


    using MeshHandle = u32;
    using MaterialHandle = u32;
    using ShaderHandle = u32;
    using TextureHandle = u32;

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

    struct TextureDescriptor
    {
        int width;
        int height;
        std::span<byte> data;
        u32 format;
    };

    struct DrawCall
    {
        u32 mesh;
        u32 material;
        math::mat4 model;
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
        
        template<typename T>
        void set_material_param(MaterialHandle h, std::string_view name, const T& value)
        {
            Material& m = materials[h];
            const auto& params = shaders[m.shader].layout.params;
            auto it = params.find(std::string(name));
            if (it == params.end())
                return; // BFRES materials carry params your shader may not use
            if (sizeof(T) > it->second.size)
                throw_error("param type doesn't match the shader");

            std::memcpy(m.params.data() + it->second.offset, &value, sizeof(T));
            m.dirty = true;
        }

        void set_material_texture(MaterialHandle h, std::string_view sampler, TextureHandle texture)
        {
            Material& m = materials[h];
            const auto& samplers = shaders[m.shader].layout.samplers;
            if (auto it = samplers.find(std::string(sampler)); it != samplers.end())
                m.textures[it->second] = texture;
        }
        
        void set_camera(const CameraData& cam) { camera_ = cam; }

        ShaderHandle create_shader(const ShaderDescriptor& descriptor);
        MaterialHandle create_material(const ShaderHandle& shader);
        MeshHandle create_mesh(const MeshDescriptor& descriptor);
        TextureHandle create_texture(TextureDescriptor descriptor);
        void submit_draw_call(const DrawCall& drawCall);

        void screenshot(std::string filename);

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

        void upload_materials();

        RenderConfiguration cfg;

        uin materialBuffer = 0;
        u32 materialStride = 256;
        u32 materialCap = 0;

        std::vector<DrawCall> drawCalls;
        std::vector<GpuMesh> meshes;
        std::vector<Material> materials;
        std::vector<GpuShader> shaders;
        std::vector<GpuTexture> textures;
        std::unordered_map<VertexFormat, FormatPool> pools;

        CameraData camera_{};
        uin cameraBuffer = 0;
        uin objectBuffer = 0;
        u32 objectCap = 0;
        std::vector<byte> objectStaging;
    };

} // namespace graphics
