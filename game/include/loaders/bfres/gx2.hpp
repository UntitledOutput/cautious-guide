#pragma once
#include "core/util.hpp"

#include <cmath>
#include <span>
#include <vector>

namespace gx2
{
    struct Surface
    {
        u32 width = 0, height = 0;
        u32 format = 0;   
        u32 tileMode = 0; 
        u32 swizzle = 0;
        u32 pitch = 0;    
    };

    u32 bits_per_element(u32 format);
    bool is_block_compressed(u32 format);

    
    std::vector<byte> deswizzle(const Surface& s, std::span<const byte> data);

    
    std::vector<byte> decode_rgba8(u32 format, u32 width, u32 height, std::span<const byte> data);
}