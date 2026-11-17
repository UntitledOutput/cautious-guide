#pragma once

#include "core/util.hpp"
#include "math/vector.hpp"
#include <vector>

namespace graphics
{
    class RenderMgr;

    using RenderTargetHandle = u32;

    struct RenderTargetDescriptor
    {
        int width;
        int height;
        std::vector<u32> colorFormats; 
        u32 depthFormat = 0;
        math::vec4 clearColor{};
    };

    
    class GpuRenderTarget
    {
        class_def_no_destructor(GpuRenderTarget, u32 _fbo)

        GpuRenderTarget(GpuRenderTarget&& o) noexcept;
        GpuRenderTarget& operator=(GpuRenderTarget&& o) noexcept;

    private:
        friend RenderMgr;
        u32 fbo;
        int width = 0, height = 0;
        std::vector<u32> colors; 
        u32 depth = UINT32_MAX;
        math::vec4 clearColor{};
    };
}
