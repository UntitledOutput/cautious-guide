// image.hpp
#pragma once
#include "core/util.hpp"
#include <span>
#include <vector>

namespace graphics
{
    struct DecodedImage
    {
        u32 width = 0, height = 0;
        std::vector<byte> rgba; // always 4 channels, top row first
    };

    bool decode_image(std::span<const byte> data, DecodedImage& out);
}