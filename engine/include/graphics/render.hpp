#pragma once

#include "vertex.hpp"
#include "material.hpp"
#include <vector>
#include <unordered_map>
#include <span>
#include <cstring>

#include "camera.hpp"
#include "texture.hpp"
#include "render_target.hpp"

namespace graphics
{
    struct MeshData;


    using MeshHandle = u32;
    using MaterialHandle = u32;
    using ShaderHandle = u32;
    using TextureHandle = u32;
    using ComputeShaderHandle = u32;
    using BufferHandle = u32;

    constexpr u32 invalid_handle = UINT32_MAX;

    enum class BufferUsage : u32 {
        None = 0,
        Storage = 1 << 0,
        Vertex = 1 << 1,
        Index = 1 << 2,
        Indirect = 1 << 3,
        Readback = 1 << 4
    };
    constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return BufferUsage(u32(a) | u32(b)); };
    constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) { return BufferUsage(u32(a) & u32(b)); };

    enum class Access : u32 {
        Read = 0,
        Write = 1,
        ReadWrite = 2
    };

    struct BufferDescriptor {
        size_t size;
        BufferUsage usage;

        std::span<const byte> initialData;
    };

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

    struct ComputeShaderDescriptor
    {
        std::string computeSource;
    };

    struct TextureDescriptor
    {
        int width;
        int height;
        std::span<const byte> data;
        u32 format;
        bool sRGB;

        bool generate_mips = true;
        bool has_data = true;

        bool is_storage = false;
        u32 internalFormat = 0;
    };


    struct DispatchSize {
        u32 x = 1, y = 1, z = 1;
    };

    struct ComputeBinding {
        enum class Kind : u32 { Buffer, Image } kind;
        u32 slot;
        u32 resource;
        Access access;
    };

    struct ComputeDispatch {
        ComputeShaderHandle shader = invalid_handle;
        DispatchSize groups;
        std::vector<ComputeBinding> bindings;
        std::vector<byte> params;
    };


    struct DrawCall
    {
        u32 mesh;
        u32 material;
        math::mat4 model;
        u32 jointOffset = 0;
        u32 jointCount = 0;

        u32 instanceCount = 1;
        BufferHandle instanceBuffer = invalid_handle;

        RenderTargetHandle target = invalid_handle; 

        
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
                return; 
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

        void set_material_render_params(MaterialHandle h, RenderState state) { Material& m = materials[h];

            m.state = state;
        }
        
        void set_camera(const CameraData& cam) { camera_ = cam; }

        u32 push_joints(std::span<const math::mat4> joints);

        ShaderHandle create_shader(const ShaderDescriptor& descriptor);
        MaterialHandle create_material(const ShaderHandle& shader);
        MeshHandle create_mesh(const MeshDescriptor& descriptor);
        TextureHandle create_texture(const TextureDescriptor& descriptor);

        RenderTargetHandle create_render_target(const RenderTargetDescriptor& descriptor);
        TextureHandle render_target_texture(RenderTargetHandle h, u32 attachment = 0) ret(targets[h].colors[attachment])
        TextureHandle render_target_depth(RenderTargetHandle h) ret(targets[h].depth)

            BufferHandle create_buffer(const BufferDescriptor& descriptor);
        void update_buffer(BufferHandle h, size_t offset, std::span<const byte> data);
        void resize_buffer(BufferHandle h, size_t newSize);
        void read_buffer(BufferHandle h, size_t offset, std::span<byte> out); 
        size_t buffer_size(BufferHandle h) const { return buffers[h].size; }

        ComputeShaderHandle create_compute_shader(const ComputeShaderDescriptor& descriptor);
        ComputeDispatch make_dispatch(ComputeShaderHandle shader) const;
        DispatchSize groups_for(ComputeShaderHandle shader, u32 x, u32 y = 1, u32 z = 1) const;
        void set_dispatch_buffer(ComputeDispatch& d, std::string_view block, BufferHandle buffer, Access access);
        void set_dispatch_image(ComputeDispatch& d, std::string_view image, TextureHandle texture, Access access);
        void submit_dispatch(ComputeDispatch dispatch); 

        template<typename T>
        void set_dispatch_param(ComputeDispatch& d, std::string_view name, const T& value)
        {
            const auto& params = compute_shaders[d.shader].layout.params;
            auto it = params.find(std::string(name));
            if (it == params.end())
                return; 
            if (sizeof(T) > it->second.size)
                throw_error("param type doesn't match the shader");
            std::memcpy(d.params.data() + it->second.offset, &value, sizeof(T));
        }


        void submit_draw_call(const DrawCall& drawCall);

        void screenshot(std::string filename);

        void flush(); 
        
        TextureHandle white_texture() ret(whiteTexture);
        TextureHandle black_texture() ret(blackTexture);
        TextureHandle normal_texture() ret(normalTexture);

        TextureHandle default_texture(std::string_view sampler) const;

        u32 draw_call_count() ret(drawCalls.size())

    private:
        struct FormatPool
        {
            uint vao = 0, vbo = 0, ibo = 0;
            u32 vertexCt = 0, indexCt = 0;
            u32 vertexCap = 0, indexCap = 0;
        };

        struct GpuMesh
        {
            VertexFormat fmt;
            u32 base_vtx, first_idx, idx;
        };

        struct GpuBuffer
        {
            uint handle = 0;
            size_t size = 0;
            BufferUsage usage = BufferUsage::None;
        };

        void run_dispatches();

        std::vector<GpuBuffer> buffers;
        std::vector<GpuShader> compute_shaders;
        std::vector<ComputeDispatch> dispatches;

        uint computeParamsBuffer = 0;
        u32 computeParamsCap = 0;
        std::vector<byte> computeParamsStaging;
        DispatchSize maxGroups{};


        FormatPool& get_pool(VertexFormat fmt);

        void upload_materials();

        RenderConfiguration cfg;

        uint materialBuffer = 0;
        u32 materialStride = 256;
        u32 materialCap = 0;

        uint jointBuffer = 0;
        u32 jointCap = 0;
        std::vector<math::mat4> jointStaging;

        std::vector<DrawCall> drawCalls;
        std::vector<GpuMesh> meshes;
        std::vector<Material> materials;
        std::vector<GpuShader> shaders;
        std::vector<GpuTexture> textures;
        std::vector<GpuRenderTarget> targets;

        TextureHandle whiteTexture = 0, blackTexture = 0, normalTexture = 0;

        std::unordered_map<VertexFormat, FormatPool> pools;

        CameraData camera_{};
        uint cameraBuffer = 0;
        uint objectBuffer = 0;
        u32 objectCap = 0;
        std::vector<byte> objectStaging;
    };

} 
