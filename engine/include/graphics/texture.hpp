#pragma once

#include "core/util.hpp"
#include <span>

namespace graphics {

    class RenderMgr;

    class GpuTexture {
        class_def(GpuTexture, u32 _handle)

        GpuTexture(GpuTexture&&) noexcept = default;
        GpuTexture& operator=(GpuTexture&&) noexcept = default;

    private: 
        friend RenderMgr;
        u32 handle;
    };

    // todo: add render textures
}
