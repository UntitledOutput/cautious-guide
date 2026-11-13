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
            u16 parentIndex;
            u16 smoothMatrixIndex;
            u16 rigidMatrixIndex;
            u16 billboardIndex;
            FsklBone* parent;
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
            const byte* data = nullptr;
            u16 stride;
            std::vector<FvtxVertexAttribute> attributes;
        };


        std::vector<FvtxVertexData> data;
    };

    struct FMAT
    {

        
        enum class FmatRenderParameterType : byte
        {
            /// <summary>
            /// Probably represents an array of 8 zero bytes.
            /// </summary>
            Null8Bytes = 0x00,

            /// <summary>
            /// Probably represents 2 Vector2F instances.
            /// </summary>
            Vector2F = 0x01,

            /// <summary>
            /// Represents the offset to a string.
            /// </summary>
            StringOffset = 0x02
        };


        enum class FmatMaterialParameterType : byte
        {
            /// <summary>
            /// Represents a signed integer, having a size of 4 bytes.
            /// </summary>
            Int32 = 0x04,

            /// <summary>
            /// Represents a float, having a size of 4 bytes.
            /// </summary>
            Single = 0x0C,

            /// <summary>
            /// Represents a float vector with 2 components, having a size of 8 bytes.
            /// </summary>
            Vector2F = 0x0D,

            /// <summary>
            /// Represents a float vector with 3 components, having a size of 12 bytes.
            /// </summary>
            Vector3F = 0x0E,

            /// <summary>
            /// Represents a float vector with 4 components, having a size of 16 bytes.
            /// </summary>
            Vector4F = 0x0F,

            /// <summary>
            /// Represents a 2x3 texture matrix, having a size of 24 bytes.
            /// </summary>
            Matrix2x3 = 0x1E
        };

        struct FMATRenderParameter
        {
            std::string name;
            FmatRenderParameterType type;

            std::vector<int32_t> ints;
            std::vector<float> floats;
            std::vector<std::string> strings;
        };

        struct FMATTextureSelector
        {
            std::string name;
            size_t textureOffset;
            // todo: add field for texture itself
        };

        struct FMATTextureAttributeSelector
        {
            std::string name;
            byte index;
        };

        struct FMATMaterialParameter
        {
            std::string name;
            FmatMaterialParameterType type;

            int intValue;
            float singleValue;
            math::vec2 vector2Value;
            math::vec3 vector3Value;
            math::vec4 vector4Value;
            math::mat4 matrixValue;
        };


        struct FMATShaderControl
        {
            std::string vertexShaderName;
            std::string fragmentShaderName;

            std::vector<std::string> vertexShaderInputs;
            std::vector<std::string> fragmentShaderInputs;

            std::vector<std::string> parameters;
        };

        struct FMATShadowParameter
        {
            std::string name;
            uint value;
        };

        std::string name;
        std::vector<FMATRenderParameter> renderParams;
        std::vector<FMATTextureSelector> textureSelectors;
        std::vector<FMATTextureAttributeSelector> textureAttributeSelectors;
        std::vector<FMATMaterialParameter> materialParameters;

        FMATShaderControl shaderControl;
        std::vector<FMATShadowParameter> shadowParameters;
    };

    struct FSHP
    {
        
    };

    struct FMDL
    {
        std::string name;
        FSKL skeleton;
        std::vector<FvtxVertexBuffer> vertexBuffers;
        std::vector<FMAT> materials;
        std::vector<FSHP> shapes;
    };

    std::vector<FMDL> models;
};