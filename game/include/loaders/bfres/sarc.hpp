#pragma once
#include <span>

#include "core/util.hpp"
#include <vector>
#include "math/vector.hpp"
#include "math/matrix.hpp"
#include "math/quaternion.hpp"

#include "reader.hpp"

#include <memory>
#include <string_view>
#include "graphics/model.hpp"
#include "reader.hpp"

class Engine;

struct BfresModel
{
    std::vector<graphics::ModelData> models;
    std::vector<graphics::TextureData> textures;
    std::vector<graphics::AnimationData> animations;
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
        };

        std::vector<FsklBone> bones;
        std::vector<u16> matrixToBone;
        std::vector<math::mat4> inverseMatrices;
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
            Three_32Bit_Float = 0x00000811,

            Four_8Bit_Normalized = 0x0000000A,
            Two_8Bit_UInt = 0x00000104,
            Four_8Bit_UInt = 0x0000010A,
        };

        struct FvtxVertexAttribute
        {
            std::string name;
            uint offset;
            FvtxVertexAttributeFormat format;
        };

        struct FvtxVertexData
        {
            std::span<const byte> data;
            u16 stride;
            std::vector<FvtxVertexAttribute> attributes;
        };

        u32 vertexCount = 0;
        u8 skinCount = 0;
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
            TexSrt = 0x1E
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
            u32 texSrtMode = 0;
            float texSrt[5] = {}; // scale x, scale y, rotation, translate x, translate y
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

        struct FSHPVisibilityGroup {
            uint offset;
            uint count;
        };

        struct FSHPIndexBuffer {
            std::vector<u32> data;
        };

        struct FSHPLodModel {
            std::vector<FSHPVisibilityGroup> visibilityGroups;
            uint totalPointCount;
            uint skipElements;
            FSHPIndexBuffer indexBuffer;
        };

        struct FSHPVisibilityNode {
            u16 leftNodeIndex;
            u16 rightNodeIndex;
            u16 visibilityGroupIndex;
            std::vector<FSHPVisibilityGroup> visibilityGroups;
        };
        

        std::string name;

        u16 index;
        u16 materialIndex;
        u16 boneIndex;
        u16 vertexBufferIndex;
        u8 vertexSkinCount = 0;

        std::vector<FSHPLodModel> lodModels;
        std::vector<FSHPVisibilityNode> visibilityNodes;
        std::vector<u16> visibilityIndices;
        std::vector<u16> skeletonOffsets;
    };

    struct FMDL
    {
        std::string name;
        FSKL skeleton;
        std::vector<FvtxVertexBuffer> vertexBuffers;
        std::vector<FMAT> materials;
        std::vector<FSHP> shapes;

    };

    enum SurfaceFormat : int
    {
        Invalid = 0x00000000,
        TC_R8_UNorm = 0x00000001,
        TC_R8_UInt = 0x00000101,
        TC_R8_SNorm = 0x00000201,
        TC_R8_SInt = 0x00000301,
        T_R4_G4_UNorm = 0x00000002,
        TCD_R16_UNorm = 0x00000005,
        TC_R16_UInt = 0x00000105,
        TC_R16_SNorm = 0x00000205,
        TC_R16_SInt = 0x00000305,
        TC_R16_Float = 0x00000806,
        TC_R8_G8_UNorm = 0x00000007,
        TC_R8_G8_UInt = 0x00000107,
        TC_R8_G8_SNorm = 0x00000207,
        TC_R8_G8_SInt = 0x00000307,
        TCS_R5_G6_B5_UNorm = 0x00000008,
        TC_R5_G5_B5_A1_UNorm = 0x0000000A,
        TC_R4_G4_B4_A4_UNorm = 0x0000000B,
        TC_A1_B5_G5_R5_UNorm = 0x0000000C,
        TC_R32_UInt = 0x0000010D,
        TC_R32_SInt = 0x0000030D,
        TCD_R32_Float = 0x0000080E,
        TC_R16_G16_UNorm = 0x0000000F,
        TC_R16_G16_UInt = 0x0000010F,
        TC_R16_G16_SNorm = 0x0000020F,
        TC_R16_G16_SInt = 0x0000030F,
        TC_R16_G16_Float = 0x00000810,
        D_D24_S8_UNorm = 0x00000011,
        T_R24_UNorm_X8 = 0x00000011,
        T_X24_G8_UInt = 0x00000111,
        D_D24_S8_Float = 0x00000811,
        TC_R11_G11_B10_Float = 0x00000816,
        TCS_R10_G10_B10_A2_UNorm = 0x00000019,
        TC_R10_G10_B10_A2_UInt = 0x00000119,
        TC_R10_G10_B10_A2_SNorm = 0x00000219,
        TC_R10_G10_B10_A2_SInt = 0x00000319,
        TCS_R8_G8_B8_A8_UNorm = 0x0000001A,
        TC_R8_G8_B8_A8_UInt = 0x0000011A,
        TC_R8_G8_B8_A8_SNorm = 0x0000021A,
        TC_R8_G8_B8_A8_SInt = 0x0000031A,
        TCS_R8_G8_B8_A8_SRGB = 0x0000041A,
        TCS_A2_B10_G10_R10_UNorm = 0x0000001B,
        TC_A2_B10_G10_R10_UInt = 0x0000011B,
        D_D32_Float_S8_UInt_X24 = 0x0000081C,
        T_R32_Float_X8_X24 = 0x0000081C,
        T_X32_G8_UInt_X24 = 0x0000011C,
        TC_R32_G32_UInt = 0x0000011D,
        TC_R32_G32_SInt = 0x0000031D,
        TC_R32_G32_Float = 0x0000081E,
        TC_R16_G16_B16_A16_UNorm = 0x0000001F,
        TC_R16_G16_B16_A16_UInt = 0x0000011F,
        TC_R16_G16_B16_A16_SNorm = 0x0000021F,
        TC_R16_G16_B16_A16_SInt = 0x0000031F,
        TC_R16_G16_B16_A16_Float = 0x00000820,
        TC_R32_G32_B32_A32_UInt = 0x00000122,
        TC_R32_G32_B32_A32_SInt = 0x00000322,
        TC_R32_G32_B32_A32_Float = 0x00000823,
        T_BC1_UNorm = 0x00000031,
        T_BC1_SRGB = 0x00000431,
        T_BC2_UNorm = 0x00000032,
        T_BC2_SRGB = 0x00000432,
        T_BC3_UNorm = 0x00000033,
        T_BC3_SRGB = 0x00000433,
        T_BC4_UNorm = 0x00000034,
        T_BC4_SNorm = 0x00000234,
        T_BC5_UNorm = 0x00000035,
        T_BC5_SNorm = 0x00000235,
        T_NV12_UNorm = 0x00000081
    };

    /// <summary>
    /// Indicates the desired tiling mode for a surface. You should use only <see cref="Default"/> in most cases. Don't
    /// use other modes unless you know what you're doing!
    /// </summary>
    /// <remarks>Native type: GX2TileMode</remarks>
    enum class TileMode : int
    {
        /// <summary>
        /// Driver will choose best mode.
        /// </summary>
        Default = 0x00000000,

        /// <summary>
        /// Typically not supported by HW.
        /// </summary>
        LinearSpecial = 0x00000010,

        /// <summary>
        /// Supported by HW, but not fast.
        /// </summary>
        LinearAligned = 0x00000001,
        OneDTiledThin1 = 0x00000002,
        OneDTiledThick = 0x00000003,

        /// <summary>
        /// (A typical default, but not always.)
        /// </summary>
        TwoDTiledThin1 = 0x00000004,
        TwoDTiledThin2 = 0x00000005,
        TwoDTiledThin4 = 0x00000006,
        TwoDTiledThick = 0x00000007,
        TwoBTiledThin1 = 0x00000008,
        TwoBTiledThin2 = 0x00000009,
        TwoBTiledThin4 = 0x0000000A,
        TwoBTiledThick = 0x0000000B,
        ThreeDTiledThin1 = 0x0000000C,
        ThreeDTiledThick = 0x0000000D,
        ThreeBTiledThin1 = 0x0000000E,
        ThreeBTiledThick = 0x0000000F,
    };

    /// <summary>
    /// Indicates how a given surface may be used.
    /// A &quot;final&quot; TV render target is one that will be copied to a TV scan buffer. It needs to be designed to
    /// handle certain display corner cases. (When a HD surface must be scaled down to display in NTSC/PAL.)
    /// </summary>
    /// <remarks>Native type: GX2SurfaceUse</remarks>
    enum class SurfaceUse : int
    {
        Texture = 1 << 0,
        ColorBuffer = 1 << 1,
        DepthBuffer = 1 << 2,

        /// <summary>
        /// Internal use only.
        /// </summary>
        ScanBuffer = 1 << 4,

        /// <summary>
        /// Modifier, designates a final TV render target.
        /// </summary>
        Ftv = 1 << 31,

        ColorBufferTexture = ColorBuffer | Texture,
        DepthBufferTexture = DepthBuffer | Texture,

        ColorBufferFtv = ColorBuffer | Ftv,
        ColorBufferTextureFtv = ColorBufferTexture | Ftv
    };

    /// <summary>
    /// Indicates the &quot;shape&quot; of a given surface or texture.
    /// </summary>
    /// <remarks>Native type: GX2SurfaceDim</remarks>
    enum class SurfaceDim : int
    {
        OneDimensional = 0,
        TwoDimensional = 1,
        ThreeDimensional = 2,
        Cube = 3,
        OneDimensionalArray = 4,
        TwoDimensionalArray = 5,
        TwoDimensionalMsaa = 6,
        TwoDimensionalMsaaArray = 7
    };

    /// <summary>
    /// Indicates the AA mode (number of samples) for a surface.
    /// </summary>
    /// <remarks>Native type: GX2AAMode</remarks>
    enum class AntiAliasMode : int
    {
        OneSample = 0,
        TwoSamples = 1,
        FourSamples = 2,
        EightSamples = 3
    };

    struct FTEX
    {
        std::string name;

        u32 width, height, depth;
        u32 mipmapCount;

        SurfaceDim dimension;
        SurfaceFormat format;
        AntiAliasMode antiAliasMode;
        SurfaceUse usage;

        TileMode tileMode;
        u32 swizzle;
        u32 alignment;
        u32 pitch;

        std::span<const byte> data;
    };


    enum class SkeletalAnimFlags : uint
    {
        /// <summary>
        /// The stored curve data has been baked.
        /// </summary>
        BakedCurve = 1 << 0,

        /// <summary>
        /// The animation repeats from the start after the last frame has been played.
        /// </summary>
        Looping = 1 << 2
    };


    enum class SkeletalAnimFlagsScale : uint
    {
        /// <summary>
        /// No scaling.
        /// </summary>
        None,

        /// <summary>
        /// Default scaling.
        /// </summary>
        Standard = 1 << 8,

        /// <summary>
        /// Autodesk Maya scaling.
        /// </summary>
        Maya = 2 << 8,

        /// <summary>
        /// Autodesk Softimage scaling.
        /// </summary>
        Softimage = 3 << 8
    };

    /// <summary>
    /// Represents the data format in which rotation values are stored.
    /// </summary>
    enum class SkeletalAnimFlagsRotate : uint
    {
        /// <summary>
        /// Quaternion, 4 components.
        /// </summary>
        Quaternion,

        /// <summary>
        /// Euler XYZ, 3 components.
        /// </summary>
        EulerXYZ = 1 << 12
    };

    struct FSKA
    {
        enum class FrameType : u8
        {
            Float = 0,
            Fixed10x5 = 1,
            Byte = 2
        };
        enum class KeyType : u8
        {
            Float = 0,
            Int16 = 1,
            SByte = 2
        };
        enum class CurveType : u8
        {
            Cubic = 0,
            Linear = 1,
            BakedFloat = 2,
            StepInt = 4,
            BakedInt = 5,
            StepBool = 6,
            BakedBool = 7
        };
        enum class WrapMode : u8
        {
            Clamp = 0,
            Repeat = 1,
            Mirror = 2
        };

        struct FSKAAnimCurve
        {



            FrameType frameType;
            KeyType keyType;
            CurveType curveType;
            WrapMode preWrap;
            WrapMode postWrap; 

            u32 target;
            float startFrame;
            float endFrame;
            float scale;
            float offset;
            float delta;
            std::vector<float> frames;
            std::vector<float> keys;

        };

        struct FSKABoneAnim
        {
            std::string name;
            u32 flags;
            math::vec3 baseScale;
            math::vec4 baseRotation;
            math::vec3 baseTranslation;
            std::vector<FSKAAnimCurve> curves;
        };

        std::string name;
        u32 flags;
        u32 frameCount;
        std::vector<FSKABoneAnim> bones;
        
        bool loops() const { return flags & 0x4; }
        bool eulerRotation() const { return (flags & 0x7000) == 0x1000; }
    };

    std::vector<FMDL> models;
    std::vector<FTEX> textures;
    std::vector<FSKA> skeletalAnims;
    ;

    BfresModel parse();
};

struct SarcFile
{
    std::string name;
    std::span<const byte> data;
    std::unique_ptr<BfresFile> bfres;
};

struct SarcArchive
{
    std::vector<byte> storage;
    std::vector<SarcFile> files;

    SarcArchive() = default;
    SarcArchive(const SarcArchive&) = delete;
    SarcArchive& operator=(const SarcArchive&) = delete;

    const SarcFile* find(std::string_view name) const
    {
        for (const auto& f : files)
            if (f.name == name)
                return &f;
        return nullptr;
    }
};

std::unique_ptr<BfresFile> load_bfres(std::span<const byte> data);
