#pragma once

#include "core/util.hpp"
#include <span>

namespace graphics {

    

    class GpuTexture {
        class_def(GpuTexture, u32 _handle)

    private: 
        u32 handle;
    };

    // todo: add render textures
}