#include "graphics/render_target.hpp"

#include <glad/glad.h>
#include <utility>

namespace graphics
{
    GpuRenderTarget::GpuRenderTarget(u32 _fbo) : fbo(_fbo) { }

    GpuRenderTarget::~GpuRenderTarget() { glDeleteFramebuffers(1, &fbo); }

    GpuRenderTarget::GpuRenderTarget(GpuRenderTarget&& o) noexcept
        : fbo(std::exchange(o.fbo, 0)), width(o.width), height(o.height), colors(std::move(o.colors)), depth(o.depth),
          clearColor(o.clearColor)
    {
    }

    GpuRenderTarget& GpuRenderTarget::operator=(GpuRenderTarget&& o) noexcept
    {
        if (this != &o)
        {
            glDeleteFramebuffers(1, &fbo);
            fbo = std::exchange(o.fbo, 0);
            width = o.width;
            height = o.height;
            colors = std::move(o.colors);
            depth = o.depth;
            clearColor = o.clearColor;
        }
        return *this;
    }
}
