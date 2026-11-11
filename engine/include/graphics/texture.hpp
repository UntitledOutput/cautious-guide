#pragma once

#include "core/util.hpp"

namespace graphics {

    class Texture {
        class_def(Texture, u32 width, u32 height, std::span<const byte> data)

    };

    // todo: add render textures
}