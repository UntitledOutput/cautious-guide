#pragma once
#include "core/util.hpp"

class RenderMgr;

class GpuShader {
    class_def(GpuShader,u32 handle)

private:
    friend RenderMgr;
    u32 handle;
};

class GpuMaterial
{
    class_def(GpuMaterial, GpuShader& shader)

    void apply();

private:
    friend RenderMgr;
    u32 shaderHandle;
};
