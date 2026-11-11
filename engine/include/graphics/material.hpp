#pragma once
#include "core/util.hpp"

class RenderMgr;

class GpuShader {
    class_def(GpuShader,u32 handle)

    GpuShader(GpuShader&& o) noexcept;
    GpuShader& operator=(GpuShader&& o) noexcept;

private:
    friend RenderMgr;
    u32 handle;
};

class GpuMaterial
{
    class_def(GpuMaterial, u32 shader)

    void apply();

private:
    friend RenderMgr;
    u32 shaderHandle;
};
