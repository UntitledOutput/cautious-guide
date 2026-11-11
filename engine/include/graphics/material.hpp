#pragma once
#include "core/util.hpp"
#include "unordered_map"

namespace graphics
{
    class RenderMgr;

    struct ShaderParamInfo
    {
        u32 offset;
        u32 size;
        u32 type;
    };

    struct ShaderLayout
    {
        std::unordered_map<std::string, ShaderParamInfo> params; // name -> parameter
        std::unordered_map<std::string, u32> samplers; // name -> texture unit
        u32 blockSize = 0;
    };

    class GpuShader
    {
        class_def_no_destructor(GpuShader, u32 handle)

        GpuShader(GpuShader&& o) noexcept;
        GpuShader& operator=(GpuShader&& o) noexcept;

        ShaderLayout layout;

    private:
        friend RenderMgr;
        u32 handle;
    };

    struct RenderState
    {
        bool blend = false, depthWrite = true, cullBack = true;
    };

    struct Material
    {
        u32 shader;
        RenderState state;
        std::vector<byte> params;
        std::vector<u32> textures;
        u32 gpuOffset; // offset in the material buffer;
        bool dirty = true;
    };
}
