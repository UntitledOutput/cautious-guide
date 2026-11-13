#pragma once
#include <span>

#include "core/util.hpp"
#include <vector>
#include "math/vector.hpp"
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

#include "reader.hpp"

struct SarcFile
{
    std::string name;
    std::span<const byte> data;
    void* processedData;
};

struct BfresFile
{
    struct DictionaryNode
    {
        std::string name;
        size_t dataOffset;
    };

    struct Dictionary
    {
        std::vector<DictionaryNode> nodes;
    };

    struct FSKL
    {
        struct FsklBone
        {
            std::string name;
            math::vec3 scale;
            math::quat rotation;
            math::vec3 position;
            math::mat4 inverseMatrix;

            u16 index;
            const unsigned short* parentIndices;
            int parentCount;
            std::vector<FsklBone> parents;
        };

        std::vector<FsklBone> bones;
    };

    struct FvtxVertexBuffer
    {
        enum class FvtxVertexAttributeFormat : uint
        {
            /// <summary>
            /// 2 8-bit values, representing numbers between 0 and 1.
            /// </summary>
            Two_8Bit_Normalized = 0x00000004,

            /// <summary>
            /// 2 16-bit values, representing numbers between 0 and 1.
            /// </summary>
            Two_16Bit_Normalized = 0x00000007,

            /// <summary>
            /// 4 signed 8-bit values.
            /// </summary>
            Four_8Bit_Signed = 0x0000020A,

            /// <summary>
            /// 3 signed 10-bit values Divide by 511 to get decimal value. Top 2 bits are always 0x0B01.
            /// </summary>
            Three_10Bit_Signed = 0x0000020B,

            /// <summary>
            /// 2 32-bit floating point values.
            /// </summary>
            Two_32Bit_Float = 0x0000080D,

            /// <summary>
            /// 4 16-bit half precision floating point values.
            /// </summary>
            Four_16Bit_Float = 0x0000080F,

            /// <summary>
            /// 3 32-bit floating point values.
            /// </summary>
            Three_32Bit_Float = 0x00000811
        };

        struct FvtxVertexAttribute
        {
            std::string name;
            uint offset;
            FvtxVertexAttributeFormat format;
        };

        struct FvtxVertexData
        {
            const byte* data;
            std::vector<FvtxVertexAttribute> attributes;
        };


        std::vector<FvtxVertexData> data;
    };

    struct FMDL
    {
        std::string name;
        FSKL skeleton;
        std::vector<FvtxVertexBuffer> vertexBuffers;
    };

    std::vector<FMDL> models;
};