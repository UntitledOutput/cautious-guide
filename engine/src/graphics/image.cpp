// image.cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/image.hpp"

bool graphics::decode_image(std::span<const byte> data, DecodedImage& out)
{
    stbi_set_flip_vertically_on_load(false);

    int w = 0, h = 0, comp = 0;
    byte* pixels = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &comp, 4);
    if (!pixels)
        return false;

    out.width = u32(w);
    out.height = u32(h);
    out.rgba.assign(pixels, pixels + size_t(w) * size_t(h) * 4);
    stbi_image_free(pixels);
    return true;
}