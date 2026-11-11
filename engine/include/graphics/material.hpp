#pragma once
#include "core/util.hpp"

class RenderMgr;

class GpuShader {
    class_def_no_destructor(GpuShader,u32 handle)

    GpuShader(GpuShader&& o) noexcept;
    GpuShader& operator=(GpuShader&& o) noexcept;

private:
    friend RenderMgr;
    u32 handle;
};

class GpuMaterial
{
    class_def(GpuMaterial, u32 shader)

    GpuMaterial(GpuMaterial&&) noexcept = default;
    GpuMaterial& operator=(GpuMaterial&&) noexcept = default;

    void apply();

private:
    friend RenderMgr;
    u32 shaderHandle;
};
