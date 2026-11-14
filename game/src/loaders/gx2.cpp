#include "loaders/bfres/gx2.hpp"

#include <algorithm>
#include <cstring>

namespace gx2
{
    namespace
    {
        
        constexpr u32 num_pipes = 2;
        constexpr u32 num_banks = 4;

        u32 thickness(u32 tileMode)
        {
            switch (tileMode)
            {
                case 3: case 7: case 11: case 13: case 15: return 4;
                case 16: case 17: return 8;
                default: return 1;
            }
        }

        u32 pixel_index(u32 x, u32 y, u32 z, u32 bpp, u32 tileMode)
        {
            u32 b0, b1, b2, b3, b4, b5;
            switch (bpp)
            {
                case 8:
                    b0 = x & 1; b1 = (x >> 1) & 1; b2 = (x >> 2) & 1;
                    b3 = (y >> 1) & 1; b4 = y & 1; b5 = (y >> 2) & 1;
                    break;
                case 16:
                    b0 = x & 1; b1 = (x >> 1) & 1; b2 = (x >> 2) & 1;
                    b3 = y & 1; b4 = (y >> 1) & 1; b5 = (y >> 2) & 1;
                    break;
                case 64:
                    b0 = x & 1; b1 = y & 1; b2 = (x >> 1) & 1;
                    b3 = (x >> 2) & 1; b4 = (y >> 1) & 1; b5 = (y >> 2) & 1;
                    break;
                case 128:
                    b0 = y & 1; b1 = x & 1; b2 = (x >> 1) & 1;
                    b3 = (x >> 2) & 1; b4 = (y >> 1) & 1; b5 = (y >> 2) & 1;
                    break;
                default: 
                    b0 = x & 1; b1 = (x >> 1) & 1; b2 = (x >> 2) & 1;
                    b3 = y & 1; b4 = (y >> 1) & 1; b5 = (y >> 2) & 1;
                    break;
            }

            u32 b6 = 0, b7 = 0, b8 = 0;
            const u32 t = thickness(tileMode);
            if (t > 1)
            {
                b6 = z & 1;
                b7 = (z >> 1) & 1;
            }
            if (t == 8)
                b8 = (z >> 2) & 1;

            return b8 << 8 | b7 << 7 | b6 << 6 | b5 << 5 | b4 << 4 | b3 << 3 | b2 << 2 | b1 << 1 | b0;
        }

        u32 addr_micro_tiled(u32 x, u32 y, u32 slice, u32 bpp, u32 pitch, u32 height, u32 tileMode)
        {
            const u32 t = thickness(tileMode);
            const u32 microTileBytes = (64 * t * bpp + 7) / 8;
            const u32 microTilesPerRow = pitch >> 3;
            const u32 microTileOffset = microTileBytes * ((x >> 3) + (y >> 3) * microTilesPerRow);
            const u32 sliceBytes = (pitch * height * t * bpp + 7) / 8;
            const u32 sliceOffset = (slice / t) * sliceBytes;
            const u32 pixelOffset = (bpp * pixel_index(x, y, slice, bpp, tileMode)) >> 3;
            return pixelOffset + microTileOffset + sliceOffset;
        }

        u32 addr_macro_tiled(u32 x, u32 y, u32 slice, u32 bpp, u32 pitch, u32 height, u32 tileMode,
                             u32 pipeSwizzle, u32 bankSwizzle)
        {
            const u32 t = thickness(tileMode);
            const u32 elemOffset = (bpp * pixel_index(x, y, slice, bpp, tileMode)) >> 3;

            const u32 pipe = ((y >> 3) ^ (x >> 3)) & 1;
            const u32 bank = (((y >> 5) ^ (x >> 3)) & 1) | ((((y >> 4) ^ (x >> 4)) & 1) << 1);
            u32 bankPipe = pipe + num_pipes * bank;

            const u32 rotation = (tileMode >= 4 && tileMode <= 11) ? num_pipes * ((num_banks >> 1) - 1) : 0;
            const u32 swizzle = pipeSwizzle + num_pipes * bankSwizzle;
            const u32 sliceIn = (t > 1) ? slice >> 2 : slice;

            bankPipe ^= swizzle + sliceIn * rotation;
            bankPipe %= num_pipes * num_banks;
            const u32 finalPipe = bankPipe % num_pipes;
            u32 finalBank = bankPipe / num_pipes;

            const u32 sliceBytes = (height * pitch * t * bpp + 7) / 8;
            const u32 sliceOffset = sliceBytes * (slice / t);

            u32 macroTilePitch = 8 * num_banks;
            u32 macroTileHeight = 8 * num_pipes;
            if (tileMode == 5 || tileMode == 9)
            {
                macroTilePitch /= 2;
                macroTileHeight *= 2;
            }
            else if (tileMode == 6 || tileMode == 10)
            {
                macroTilePitch /= 4;
                macroTileHeight *= 4;
            }

            const u32 macroTilesPerRow = pitch / macroTilePitch;
            const u32 macroTileBytes = (t * bpp * macroTileHeight * macroTilePitch + 7) / 8;
            const u32 macroTileX = x / macroTilePitch;
            const u32 macroTileY = y / macroTileHeight;
            const u32 macroTileOffset = (macroTileX + macroTilesPerRow * macroTileY) * macroTileBytes;

            if (tileMode == 8 || tileMode == 9 || tileMode == 10 || tileMode == 11 || tileMode == 14 || tileMode == 15)
                throw_error("gx2: bank-swapped tile modes aren't supported yet");

            const u32 total = elemOffset + ((macroTileOffset + sliceOffset) >> 3);
            return (finalBank << 9) | (finalPipe << 8) | (total & 0xFF) | ((total & ~0xFFu) << 3);
        }
    }

    u32 bits_per_element(u32 format)
    {
        switch (format & 0x3F)
        {
            case 0x01: return 8;    
            case 0x07: return 16;   
            case 0x1A: return 32;   
            case 0x31: return 64;   
            case 0x32: return 128;  
            case 0x33: return 128;  
            case 0x34: return 64;   
            case 0x35: return 128;  
            default: throw_error("gx2: unsupported surface format " + std::to_string(format));
        }
    }

    bool is_block_compressed(u32 format)
    {
        const u32 f = format & 0x3F;
        return f >= 0x31 && f <= 0x35;
    }

    std::vector<byte> deswizzle(const Surface& s, std::span<const byte> data)
    {
        const bool bc = is_block_compressed(s.format);
        const u32 width = bc ? (s.width + 3) / 4 : s.width;
        const u32 height = bc ? (s.height + 3) / 4 : s.height;
        const u32 bpp = bits_per_element(s.format);
        const u32 bytesPerElem = bpp / 8;

        
        const u32 paddedHeight = (height + 31) & ~31u;
        const u32 pipeSwizzle = (s.swizzle >> 8) & 1;
        const u32 bankSwizzle = (s.swizzle >> 9) & 3;

        std::vector<byte> out(size_t(width) * height * bytesPerElem);
        for (u32 y = 0; y < height; ++y)
            for (u32 x = 0; x < width; ++x)
            {
                u32 src;
                switch (s.tileMode)
                {
                    case 0: case 1: case 16:
                        src = (y * s.pitch + x) * bytesPerElem;
                        break;
                    case 2: case 3:
                        src = addr_micro_tiled(x, y, 0, bpp, s.pitch, paddedHeight, s.tileMode);
                        break;
                    default:
                        src = addr_macro_tiled(x, y, 0, bpp, s.pitch, paddedHeight, s.tileMode, pipeSwizzle, bankSwizzle);
                        break;
                }

                if (size_t(src) + bytesPerElem > data.size())
                    throw_error("gx2: tiled address past end of image data");
                std::memcpy(out.data() + (size_t(y) * width + x) * bytesPerElem, data.data() + src, bytesPerElem);
            }
        return out;
    }

    

    namespace
    {
        void rgb565(u16 c, u8 out[3])
        {
            out[0] = u8(((c >> 11) & 0x1F) * 255 / 31);
            out[1] = u8(((c >> 5) & 0x3F) * 255 / 63);
            out[2] = u8((c & 0x1F) * 255 / 31);
        }

        u16 le16(const byte* p) { return u16(p[0] | p[1] << 8); }

        
        void decode_bc1_block(const byte* b, u8 px[16][4], bool fourColour = false)
        {
            const u16 c0 = le16(b), c1 = le16(b + 2);
            const bool opaque = fourColour || c0 > c1;
            u8 pal[4][4];
            rgb565(c0, pal[0]);
            rgb565(c1, pal[1]);
            pal[0][3] = pal[1][3] = 255;

            for (int i = 0; i < 3; ++i)
            {
                if (opaque)
                {
                    pal[2][i] = u8((2 * pal[0][i] + pal[1][i]) / 3);
                    pal[3][i] = u8((pal[0][i] + 2 * pal[1][i]) / 3);
                }
                else
                {
                    pal[2][i] = u8((pal[0][i] + pal[1][i]) / 2);
                    pal[3][i] = 0;
                }
            }
            pal[2][3] = 255;
            pal[3][3] = opaque ? 255 : 0;

            const u32 bits = u32(b[4]) | u32(b[5]) << 8 | u32(b[6]) << 16 | u32(b[7]) << 24;
            for (int i = 0; i < 16; ++i)
                std::memcpy(px[i], pal[(bits >> (2 * i)) & 3], 4);
        }

        
        void decode_bc4_channel(const byte* b, bool isSigned, u8 out[16])
        {
            int e0, e1;
            if (isSigned)
            {
                e0 = std::max(int(int8_t(b[0])), -127);
                e1 = std::max(int(int8_t(b[1])), -127);
            }
            else
            {
                e0 = b[0];
                e1 = b[1];
            }

            int pal[8] = {e0, e1};
            if (e0 > e1)
                for (int i = 1; i < 7; ++i)
                    pal[i + 1] = ((7 - i) * e0 + i * e1) / 7;
            else
            {
                for (int i = 1; i < 5; ++i)
                    pal[i + 1] = ((5 - i) * e0 + i * e1) / 5;
                pal[6] = isSigned ? -127 : 0;
                pal[7] = isSigned ? 127 : 255;
            }

            uint64_t bits = 0;
            for (int i = 0; i < 6; ++i)
                bits |= uint64_t(b[2 + i]) << (8 * i);

            for (int i = 0; i < 16; ++i)
            {
                const int v = pal[(bits >> (3 * i)) & 7];
                out[i] = isSigned ? u8((v + 127) * 255 / 254) : u8(v);
            }
        }
    }

    std::vector<byte> decode_rgba8(u32 format, u32 width, u32 height, std::span<const byte> blocks)
    {
        std::vector<byte> out(size_t(width) * height * 4);
        const u32 f = format & 0x3F;
        const bool snorm = (format & 0x200) != 0;

        if (!is_block_compressed(format))
        {
            const u32 bytesPerPixel = bits_per_element(format) / 8;
            for (size_t i = 0; i < size_t(width) * height; ++i)
            {
                byte* px = out.data() + i * 4;
                const byte* src = blocks.data() + i * bytesPerPixel;
                px[0] = src[0];
                px[1] = bytesPerPixel > 1 ? src[1] : src[0];
                px[2] = bytesPerPixel > 2 ? src[2] : (bytesPerPixel == 1 ? src[0] : 0);
                px[3] = bytesPerPixel > 3 ? src[3] : 255;
            }
            return out;
        }

        const u32 bw = (width + 3) / 4, bh = (height + 3) / 4;
        const u32 blockBytes = bits_per_element(format) / 8;
        if (blocks.size() < size_t(bw) * bh * blockBytes)
            throw_error("gx2: not enough block data");

        for (u32 by = 0; by < bh; ++by)
            for (u32 bx = 0; bx < bw; ++bx)
            {
                const byte* b = blocks.data() + (size_t(by) * bw + bx) * blockBytes;
                u8 px[16][4];

                switch (f)
                {
                    case 0x31:
                        decode_bc1_block(b, px);
                        break;
                    case 0x34:
                    {
                        u8 r[16];
                        decode_bc4_channel(b, snorm, r);
                        for (int i = 0; i < 16; ++i)
                            px[i][0] = px[i][1] = px[i][2] = r[i], px[i][3] = 255;
                        break;
                    }
                    case 0x35:
                    {
                        u8 r[16], g[16];
                        decode_bc4_channel(b, snorm, r);
                        decode_bc4_channel(b + 8, snorm, g);
                        for (int i = 0; i < 16; ++i)
                        {
                            
                            const float nx = r[i] / 127.5f - 1.0f, ny = g[i] / 127.5f - 1.0f;
                            const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                            px[i][0] = r[i];
                            px[i][1] = g[i];
                            px[i][2] = u8((nz * 0.5f + 0.5f) * 255.0f);
                            px[i][3] = 255;
                        }
                        break;
                    }
                    case 0x32:
                    case 0x33:
                    {
                        u8 a[16];
                        decode_bc1_block(b + 8, px, true);
                        if (f == 0x33)
                            decode_bc4_channel(b, false, a);
                        else
                            for (int i = 0; i < 16; ++i)
                                a[i] = u8(((b[i / 2] >> (4 * (i & 1))) & 0xF) * 17);
                        for (int i = 0; i < 16; ++i)
                            px[i][3] = a[i];
                        break;
                    }
                }

                for (int i = 0; i < 16; ++i)
                {
                    const u32 x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if (x < width && y < height)
                        std::memcpy(out.data() + (size_t(y) * width + x) * 4, px[i], 4);
                }
            }
        return out;
    }
}