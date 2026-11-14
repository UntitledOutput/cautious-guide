#include "loaders/bfres/sarc.hpp"
#include "loaders/bfres/reader.hpp"
#include "loaders/bfres/bfres.hpp"

#include "graphics/model.hpp"
#include "loaders/bfres/sarc.hpp"
#include "loaders/bfres/gx2.hpp"

#include <unordered_set>

using bfrs = BfresFile;

// ps. a lot of this code is copied from Syroot, KillzGaming, etc. also nintendo lol

float half_to_float(u16 h)
{
    const u32 sign = u32(h & 0x8000) << 16;
    u32 exp = (h >> 10) & 0x1F;
    u32 mant = h & 0x3FF;

    u32 bits;
    if (exp == 0)
    {
        if (mant == 0)
            bits = sign;
        else
        {
            exp = 127 - 15 + 1;
            while (!(mant & 0x400))
            {
                mant <<= 1;
                --exp;
            }
            mant &= 0x3FF;
            bits = sign | (exp << 23) | (mant << 13);
        }
    }
    else if (exp == 31)
        bits = sign | 0x7F800000 | (mant << 13);
    else
        bits = sign | ((exp + 127 - 15) << 23) | (mant << 13);

    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}


float packed_ufloat(u32 v, u32 mantBits)
{
    const u32 exp = v >> mantBits;
    const u32 mant = v & ((1u << mantBits) - 1);
    if (exp == 0)
        return std::ldexp(float(mant) / float(1u << mantBits), -14);
    if (exp == 31)
        return mant ? NAN : INFINITY;
    return std::ldexp(1.0f + float(mant) / float(1u << mantBits), int(exp) - 15);
}

math::vec4 decode_attribute(filesys::ByteReader& r, BfresFile::FvtxVertexBuffer::FvtxVertexAttributeFormat format)
{
    const u32 f = u32(format);
    const u32 layout = f & 0xFF;
    const u32 kind = f >> 8;

    if (kind != 0x0 && kind != 0x1 && kind != 0x2 && kind != 0x3 && kind != 0x8 && kind != 0xA)
        throw_error("unsupported vertex attribute format " + std::to_string(f));

    const bool isSigned = kind == 0x2 || kind == 0x3 || kind == 0xA;
    const bool normalized = kind == 0x0 || kind == 0x2;

    float out[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // turns a raw integer of `bits` width into the final value for this kind
    auto convert = [&](u32 raw, u32 bits) -> float
    {
        if (isSigned)
        {
            int64_t v = raw;
            if (raw & (1ull << (bits - 1)))
                v -= int64_t(1ull << bits);
            if (!normalized)
                return float(v);
            return std::max(float(v) / float((1ull << (bits - 1)) - 1), -1.0f);
        }
        if (!normalized)
            return float(raw);
        return float(raw) / float((1ull << bits) - 1);
    };

    // packed layouts
    switch (layout)
    {
        case 0x01: // 4_4
            {
                const u32 v = r.read<u8>();
                out[0] = convert(v & 0xF, 4);
                out[1] = convert(v >> 4, 4);
                return {out[0], out[1], out[2], out[3]};
            }
        case 0x09: // 10_11_11 float
            {
                const u32 v = r.read<u32>();
                out[0] = packed_ufloat(v & 0x7FF, 6);
                out[1] = packed_ufloat((v >> 11) & 0x7FF, 6);
                out[2] = packed_ufloat(v >> 22, 5);
                return {out[0], out[1], out[2], out[3]};
            }
        case 0x0B: // 10_10_10_2
            {
                const u32 v = r.read<u32>();
                out[0] = convert(v & 0x3FF, 10);
                out[1] = convert((v >> 10) & 0x3FF, 10);
                out[2] = convert((v >> 20) & 0x3FF, 10);
                out[3] = convert(v >> 30, 2);
                return {out[0], out[1], out[2], out[3]};
            }
    }

    // plain layouts: component count, bits per component, and whether the bits are a float
    u32 count, bits;
    bool isFloat = false;
    switch (layout)
    {
        case 0x00:
            count = 1;
            bits = 8;
            break;
        case 0x02:
            count = 1;
            bits = 16;
            break;
        case 0x03:
            count = 1;
            bits = 16;
            isFloat = true;
            break;
        case 0x04:
            count = 2;
            bits = 8;
            break;
        case 0x05:
            count = 1;
            bits = 32;
            break;
        case 0x06:
            count = 1;
            bits = 32;
            isFloat = true;
            break;
        case 0x07:
            count = 2;
            bits = 16;
            break;
        case 0x08:
            count = 2;
            bits = 16;
            isFloat = true;
            break;
        case 0x0A:
            count = 4;
            bits = 8;
            break;
        case 0x0C:
            count = 2;
            bits = 32;
            break;
        case 0x0D:
            count = 2;
            bits = 32;
            isFloat = true;
            break;
        case 0x0E:
            count = 4;
            bits = 16;
            break;
        case 0x0F:
            count = 4;
            bits = 16;
            isFloat = true;
            break;
        case 0x10:
            count = 3;
            bits = 32;
            break;
        case 0x11:
            count = 3;
            bits = 32;
            isFloat = true;
            break;
        case 0x12:
            count = 4;
            bits = 32;
            break;
        case 0x13:
            count = 4;
            bits = 32;
            isFloat = true;
            break;
        default:
            throw_error("unsupported vertex attribute format " + std::to_string(f));
    }

    for (u32 i = 0; i < count; ++i)
    {
        if (isFloat)
            out[i] = bits == 16 ? half_to_float(r.read<u16>()) : r.read<float>();
        else if (bits == 8)
            out[i] = convert(r.read<u8>(), 8);
        else if (bits == 16)
            out[i] = convert(r.read<u16>(), 16);
        else
            out[i] = convert(r.read<u32>(), 32);
    }

    return {out[0], out[1], out[2], out[3]};
}

template <typename T>
T convert_attribute(const math::vec4& v);

template <>
inline math::vec4 convert_attribute(const math::vec4& v)
{
    return v;
}
template <>
inline math::vec3 convert_attribute(const math::vec4& v)
{
    return {v.x, v.y, v.z};
}
template <>
inline math::vec2 convert_attribute(const math::vec4& v)
{
    return {v.x, v.y};
}
template <>
inline float convert_attribute(const math::vec4& v)
{
    return v.x;
}

// bone indices come through as exact whole-number floats
template <>
inline std::array<u16, 4> convert_attribute(const math::vec4& v)
{
    return {u16(std::lround(v.x)), u16(std::lround(v.y)), u16(std::lround(v.z)), u16(std::lround(v.w))};
}

template <typename T>
std::vector<T> read_attribute(const BfresFile::FvtxVertexBuffer& vb, std::string_view name)
{
    for (const auto& buf : vb.data)
        for (const auto& a : buf.attributes)
        {
            if (a.name != name)
                continue;

            filesys::ByteReader r(buf.data, true);
            std::vector<T> out;
            out.reserve(vb.vertexCount);
            for (u32 i = 0; i < vb.vertexCount; ++i)
            {
                r.seek(size_t(i) * buf.stride + a.offset);
                out.push_back(convert_attribute<T>(decode_attribute(r, a.format)));
            }
            return out;
        }
    return {};
}

// splatoon animates at 60fps
constexpr float anim_fps = 60.0f;

float sample_curve(const bfrs::FSKA::FSKAAnimCurve& c, float frame)
{
    using Type = bfrs::FSKA::CurveType;

    if (c.frames.empty())
        return 0.0f;

    if (c.curveType == Type::BakedFloat || c.curveType == Type::BakedInt || c.curveType == Type::BakedBool)
    {
        const float local = std::clamp(frame - c.startFrame, 0.0f, float(c.keys.size() - 1));
        return c.keys[size_t(local)];
    }

    const size_t per = c.curveType == Type::Cubic ? 4 : c.curveType == Type::Linear ? 2 : 1;
    const size_t last = c.frames.size() - 1;

    if (frame <= c.frames.front())
        return c.keys[0];

    size_t i = size_t(std::upper_bound(c.frames.begin(), c.frames.end(), frame) - c.frames.begin()) - 1;
    if (i >= last)
        return c.keys[last * per];

    const float span = c.frames[i + 1] - c.frames[i];
    const float t = span > 0.0f ? (frame - c.frames[i]) / span : 0.0f;
    const float* k = &c.keys[i * per];

    if (per == 4)
        return k[0] + t * (k[1] + t * (k[2] + t * k[3]));
    if (per == 2)
        return k[0] + t * k[1];
    return k[0];
}

math::quat euler_zyx(float x, float y, float z)
{
    return math::angle_axis(z, {0, 0, 1}) * math::angle_axis(y, {0, 1, 0}) * math::angle_axis(x, {1, 0, 0});
}

std::vector<SarcFile> sarc_read(std::span<const byte> src)
{
    filesys::ByteReader r(src);
    if (!r.magic("SARC"))
        throw_error("not sarc");

    r.bigEndian = r.peek<u16>(6) == 0xFFFE; // FE FF read as LE
    const u16 headerSize = r.peek<u16>(4);

    r.seek(8);
    r.read<u32>(); // file size
    const u32 dataStart = r.read<u32>();

    r.seek(headerSize);
    if (!r.magic("SFAT"))
        throw_error("sarc: missing SFAT");
    const u16 sfatSize = r.read<u16>();
    const u16 count = r.read<u16>();
    r.read<u32>(); // hash key

    const size_t nodesAt = r.position();
    const size_t sfntAt = nodesAt + size_t(count) * 16;
    r.seek(sfntAt);
    if (!r.magic("SFNT"))
        throw_error("sarc: missing SFNT");
    const size_t namesAt = sfntAt + r.read<u16>();

    std::vector<SarcFile> files;
    r.seek(nodesAt);
    for (u16 i = 0; i < count; ++i)
    {
        r.read<u32>(); // name hash
        const u32 attrs = r.read<u32>();
        const u32 begin = r.read<u32>();
        const u32 end = r.read<u32>();

        SarcFile f;
        if (attrs >> 24)
            f.name = r.string_at(namesAt + size_t(attrs & 0xFFFF) * 4);
        if (end < begin)
            throw_error("sarc: bad entry range");
        var data = r.slice(dataStart + begin, end - begin);
        f.data = std::move(data);
        files.push_back(std::move(f));
    }
    (void)sfatSize;
    return files;
}

std::vector<byte> yaz0_decompress(std::span<const byte> src)
{
    filesys::ByteReader r(src, true);
    if (!r.magic("Yaz0"))
        throw_error("not yaz0");

    const u32 size = r.read<u32>();
    r.skip(8);

    std::vector<byte> out;
    out.reserve(size);

    while (out.size() < size)
    {
        const u8 group = r.read<u8>();
        for (int bit = 7; bit >= 0 && out.size() < size; --bit)
        {
            if (group & (1 << bit))
            {
                out.push_back(r.read<u8>());
                continue;
            }

            const u8 b1 = r.read<u8>();
            const u8 b2 = r.read<u8>();
            const size_t dist = (size_t(b1 & 0x0F) << 8 | b2) + 1;
            size_t n = b1 >> 4;
            n = n == 0 ? size_t(r.read<u8>()) + 0x12 : n + 2;

            if (dist > out.size())
                throw_error("yaz0 back-reference before start");

            // byte by byte on purpose, runs can overlap themselves
            const size_t from = out.size() - dist;
            for (size_t i = 0; i < n && out.size() < size; ++i)
                out.push_back(out[from + i]);
        }
    }
    return out;
}

std::unique_ptr<SarcArchive> SARCFileOperator::loadFilePtr(byte* data, int dataSize)
{
    const std::span<const byte> src{data, size_t(dataSize)};
    auto archive = std::make_unique<SarcArchive>();

    if (src.size() >= 4 && std::memcmp(src.data(), "Yaz0", 4) == 0)
        archive->storage = yaz0_decompress(src);
    else
        archive->storage.assign(src.begin(), src.end());

    archive->files = sarc_read(archive->storage);
    for (auto& file : archive->files)
    {
        var extension = std::filesystem::path(file.name).extension();
        if (extension == ".bfres")
        {
            file.bfres = load_bfres(file.data);
        }
        else if (extension == ".szs")
        {
            // root back into sarc, recursively
            file.archive = loadFilePtr(const_cast<byte*>(file.data.data()), file.data.size());
        }
    }

    return archive;
}

void* SARCFileOperator::onFileBytes(byte* data, int dataSize)
{
    auto archive = loadFilePtr(data, dataSize);

    return archive.release();
}

BfresFile::Dictionary load_dictionary(filesys::ByteReader& r)
{
    var dictionary = BfresFile::Dictionary{};
    uint size = r.read<uint>();
    uint objectCount = r.read<uint>();
    for (int j = 0; j < objectCount + 1; ++j)
    {
        uint bitPosition = r.read<uint>();
        u16 leftIndex = r.read<u16>();
        u16 rightIndex = r.read<u16>();
        var name = r.read_string_offset();
        var dataOffset = r.read_rel_offset();

        if (j == 0)
            continue;

        var node = BfresFile::DictionaryNode{};
        node.dataOffset = dataOffset;
        node.name = name;
        dictionary.nodes.push_back(node);
    }
    return dictionary;
}

BfresFile::Dictionary load_dictionary_at(filesys::ByteReader& r, size_t offset)
{
    if (offset == 0)
        return {};
    r.seek(offset);
    return load_dictionary(r);
}

BfresFile::FSKL load_skeleton(filesys::ByteReader& r)
{
    if (!r.magic("FSKL"))
        throw_error("not FSKL");

    var fskl = BfresFile::FSKL{};
    var unk = r.read<uint>();

    var boneCount = r.read<u16>();
    var inverseIndexCount = r.read<u16>();
    var extraIndexCount = r.read<u16>();

    unk = r.read<u16>();

    var boneIndexGroupOffset = r.read_rel_offset();
    var boneArrayOffset = r.read_rel_offset();
    var inverseIndexArrayOffset = r.read_rel_offset();
    var inverseMatrixArrayOffset = r.read_rel_offset();

    var boneIndexGroup = load_dictionary_at(r, boneIndexGroupOffset);

    r.seek(boneArrayOffset);
    for (int i = 0; i < boneCount; ++i)
    {
        var bone = BfresFile::FSKL::FsklBone{};
        bone.name = r.read_string_offset();

        bone.index = r.read<u16>();
        bone.parentIndex = r.read<u16>();
        bone.smoothMatrixIndex = r.read<u16>();
        bone.rigidMatrixIndex = r.read<u16>();
        bone.billboardIndex = r.read<u16>();
        r.read<u16>(); // user data count
        const u32 flags = r.read<u32>();

        bone.scale = r.read_vec3();
        const math::vec4 rot = r.read_vec4();
        bone.rotation = (flags & 0x7000) == 0x1000 ? math::angle_axis(rot.z, {0, 0, 1}) *
                math::angle_axis(rot.y, {0, 1, 0}) * math::angle_axis(rot.x, {1, 0, 0})
                                                   : math::quat{rot.x, rot.y, rot.z, rot.w};
        bone.position = r.read_vec3();

        r.read<uint>(); // unk
        fskl.bones.push_back(bone);

    }

    r.seek(inverseIndexArrayOffset);
    var inverseIndexArray = r.read_list<u16>(inverseIndexCount);
    var extraIndexArray = r.read_list<u16>(extraIndexCount);

    r.seek(inverseMatrixArrayOffset);

    std::vector<math::mat4> inverseMatrices = std::vector<math::mat4>(inverseIndexCount);
    for (int i = 0; i < inverseIndexCount; ++i)
    {
        inverseMatrices[i] = r.read_mat3();
    }

    for (int i = 0; i < inverseIndexCount; ++i)
    {
        if (inverseIndexArray[i] >= fskl.bones.size())
            throw_error("fskl: inverse index out of range");
        fskl.bones[inverseIndexArray[i]].inverseMatrix = inverseMatrices[i];
    }

    fskl.matrixToBone = inverseIndexArray;
    fskl.matrixToBone.insert(fskl.matrixToBone.end(), extraIndexArray.begin(), extraIndexArray.end());
    fskl.inverseMatrices = std::move(inverseMatrices);


    return fskl;
}


BfresFile::FvtxVertexBuffer load_vertex_buffer(filesys::ByteReader& r)
{
    var buffer = BfresFile::FvtxVertexBuffer{};
    if (!r.magic("FVTX"))
        throw_error("not FVTX");

    var attributeCount = r.read<byte>();
    var bufferCount = r.read<byte>();
    var index = r.read<u16>();
    buffer.vertexCount = r.read<u32>();
    buffer.skinCount = u8(r.read<u32>() >> 24);

    var attributeArrayOffset = r.read_rel_offset();
    var attributeIndexGroupOffset = r.read_rel_offset();
    var bufferArrayOffset = r.read_rel_offset();

    if (r.read<u32>() != 0)
    {
        throw_error("padding not empty");
    }

    r.seek(bufferArrayOffset);
    for (int i = 0; i < bufferCount; ++i)
    {
        r.seek(bufferArrayOffset + size_t(i) * 0x18);

        BfresFile::FvtxVertexBuffer::FvtxVertexData dataBuffer{};
        r.read<u32>(); // runtime data pointer
        const u32 size = r.read<u32>();
        r.read<u32>(); // runtime handle
        dataBuffer.stride = r.read<u16>();
        r.read<u16>(); // buffering count
        r.read<u32>(); // runtime context pointer
        const size_t dataOffset = r.read_rel_offset();

        if (size > 0 && dataOffset != 0)
            dataBuffer.data = r.slice(dataOffset, size);

        buffer.data.push_back(std::move(dataBuffer));
    }

    r.seek(attributeArrayOffset);
    for (int i = 0; i < attributeCount; ++i)
    {
        BfresFile::FvtxVertexBuffer::FvtxVertexAttribute a{};

        a.name = r.read_string_offset();

        uint indexAndOffset = r.read<uint>();
        a.offset = indexAndOffset & 0x00FFFFFF;

        a.format = (BfresFile::FvtxVertexBuffer::FvtxVertexAttributeFormat)r.read<uint>();

        var bufferIndex = byte(indexAndOffset >> 24);
        buffer.data[bufferIndex].attributes.push_back(a);
    }


    var attributeIndexGroup = load_dictionary_at(r, attributeIndexGroupOffset);


    return buffer;
}

BfresFile::FMAT load_material(filesys::ByteReader& r)
{
    BfresFile::FMAT mat{};
    if (!r.magic("FMAT"))
        throw_error("not fmat");

    mat.name = r.read_string_offset();

    r.read<u32>();
    var index = r.read<u16>();

    var renderParamCount = r.read<u16>();
    var textureSelectorCount = r.read<byte>();
    var textureAttributeSelectorCount = r.read<byte>();
    var materialParameterCount = r.read<u16>();
    var materialParameterDataSize = r.read<u32>();

    r.read<u32>();

    var renderParamIndexGroupOffset = r.read_rel_offset();
    var materialStructureOffset = r.read_rel_offset();
    var shaderControlOffset = r.read_rel_offset();
    var textureSelectorOffset = r.read_rel_offset();
    var textureAttributeSelectorOffset = r.read_rel_offset();
    var textureAttributeSelectorIndexGroupOffset = r.read_rel_offset();
    var materialParameterOffset = r.read_rel_offset();
    var materialParameterIndexGroupOffset = r.read_rel_offset();
    var materialParameterDataOffset = r.read_rel_offset();
    var shadowParameterIndexGroupOffset = r.read_rel_offset();
    r.read_rel_offset();

    // render params
    var renderParameterIndexGroup = load_dictionary_at(r, renderParamIndexGroupOffset);

    mat.renderParams = std::vector<BfresFile::FMAT::FMATRenderParameter>(renderParameterIndexGroup.nodes.size());
    for (int i = 0; i < renderParameterIndexGroup.nodes.size(); ++i)
    {
        r.seek(renderParameterIndexGroup.nodes[i].dataOffset);
        var param = BfresFile::FMAT::FMATRenderParameter{};

        const u16 count = r.read<u16>();
        param.type = (BfresFile::FMAT::FmatRenderParameterType)r.read<byte>();
        r.read<byte>();
        param.name = r.read_string_offset();
        switch (u8(param.type))
        {
            case 0:
                param.ints = r.read_list<int32_t>(count);
                break;
            case 1:
                param.floats = r.read_list<float>(count);
                break;
            case 2:
                param.strings = r.read_string_list(count);
                break;
            default:
                throw_error("unknown render info type");
        }

        mat.renderParams[i] = param;
    }

    // texture selectors
    r.seek(textureSelectorOffset);
    mat.textureSelectors = std::vector<BfresFile::FMAT::FMATTextureSelector>(textureSelectorCount);
    for (int i = 0; i < textureSelectorCount; ++i)
    {
        var selector = BfresFile::FMAT::FMATTextureSelector{};
        selector.name = r.read_string_offset();
        selector.textureOffset = r.read_rel_offset();

        mat.textureSelectors[i] = selector;
    }

    // attribute selectors
    r.seek(textureAttributeSelectorOffset);
    mat.textureAttributeSelectors =
        std::vector<BfresFile::FMAT::FMATTextureAttributeSelector>(textureAttributeSelectorCount);
    for (int i = 0; i < textureAttributeSelectorCount; ++i)
    {
        var attributeSelector = BfresFile::FMAT::FMATTextureAttributeSelector{};

        r.read<byte>();
        r.read<byte>();
        r.read<byte>();
        r.read<byte>();
        r.read<sbyte>();
        r.read<byte>();
        r.read<u16>();
        r.read<u32>();
        r.read<u32>();


        attributeSelector.name = r.read_string_offset();
        attributeSelector.index = r.read<byte>();

        r.read<byte>();
        r.read<u16>(); 
        mat.textureAttributeSelectors[i] = attributeSelector;
    }

    // material parameters

    r.seek(materialParameterOffset);
    {
        mat.materialParameters = std::vector<BfresFile::FMAT::FMATMaterialParameter>(materialParameterCount);
        for (int i = 0; i < materialParameterCount; ++i)
        {
            var parameter = BfresFile::FMAT::FMATMaterialParameter{};

            parameter.type = (BfresFile::FMAT::FmatMaterialParameterType)r.read<byte>();

            var size = r.read<byte>();
            var offset = r.read<u16>();

            r.read<u32>();
            r.read<u32>();

            var index = r.read<u16>();
            var repeatedIndex = r.read<u16>();

            parameter.name = r.read_string_offset();

            var pos = r.position();
            r.seek(materialParameterDataOffset + offset);
            switch (parameter.type)
            {

                case BfresFile::FMAT::FmatMaterialParameterType::Int32:
                    parameter.intValue = r.read<int>();
                    break;
                case BfresFile::FMAT::FmatMaterialParameterType::Single:
                    parameter.singleValue = r.read<float>();
                    break;
                case BfresFile::FMAT::FmatMaterialParameterType::Vector2F:
                    parameter.vector2Value = r.read_vec2();
                    break;
                case BfresFile::FMAT::FmatMaterialParameterType::Vector3F:
                    parameter.vector3Value = r.read_vec3();
                    break;
                case BfresFile::FMAT::FmatMaterialParameterType::Vector4F:
                    parameter.vector4Value = r.read_vec4();
                    break;
                case BfresFile::FMAT::FmatMaterialParameterType::TexSrt:
                    parameter.texSrtMode = r.read<u32>();
                    for (float& f : parameter.texSrt)
                        f = r.read<float>();
                    break;
                default:
                    break;
            }
            r.seek(pos);
            mat.materialParameters[i] = parameter;
        }
    }

    // shader control
    r.seek(shaderControlOffset);
    {
        mat.shaderControl = {};

        mat.shaderControl.vertexShaderName = r.read_string_offset();
        mat.shaderControl.fragmentShaderName = r.read_string_offset();

        r.read<u32>(); // revision
        r.read<byte>(); // attrib assign count
        r.read<byte>(); // sampler assign count
        r.read<u16>(); // option count
        var vertexShaderInputIndexGroupOffset = r.read_rel_offset();
        var fragmentShaderInputIndexGroupOffset = r.read_rel_offset();
        var parameterIndexGroupOffset = r.read_rel_offset();



        var vtxShaderInputGroup = load_dictionary_at(r, vertexShaderInputIndexGroupOffset);

        for (auto& node : vtxShaderInputGroup.nodes)
            mat.shaderControl.vertexShaderInputs.push_back(node.name);

        var frgShaderInputGroup = load_dictionary_at(r, fragmentShaderInputIndexGroupOffset);

        for (auto& node : frgShaderInputGroup.nodes)
            mat.shaderControl.fragmentShaderInputs.push_back(node.name);

        var parameterInputGroup = load_dictionary_at(r, parameterIndexGroupOffset);

        for (auto& node : parameterInputGroup.nodes)
            mat.shaderControl.parameters.push_back(node.name);
    }

    // shadow parameters
    var shadowInputGroup = load_dictionary_at(r, shadowParameterIndexGroupOffset);

    for (auto node : shadowInputGroup.nodes)
    {
        r.seek(node.dataOffset);

        var param = BfresFile::FMAT::FMATShadowParameter{};
        param.name = r.read_string_offset();

        r.read<u16>();
        r.read<byte>();
        r.read<byte>();

        param.value = r.read<u32>();

        mat.shadowParameters.push_back(param);
    }

    return mat;
}

BfresFile::FSHP load_shape(filesys::ByteReader& r)
{
    BfresFile::FSHP shape{};

    if (!r.magic("FSHP"))
        throw_error("not fshp");

    // https://github.com/OatmealDome/NintenTools/blob/master/src/Syroot.NintenTools/Bfres/Fmdl/FshpModel.cs#L20
    // https://github.com/OatmealDome/NintenTools/blob/master/src/Syroot.NintenTools/Bfres/Fmdl/FmdlSection.cs#L92
    
    shape.name = r.read_string_offset();

    r.read<u32>();

    shape.index = r.read<u16>();
    shape.materialIndex = r.read<u16>();
    shape.boneIndex = r.read<u16>();
    shape.vertexBufferIndex = r.read<u16>();

    u16 skeletonIndexArrayCount = r.read<u16>();
    shape.vertexSkinCount = r.read<byte>();
    var lodModelCount = r.read<byte>();
    r.read<byte>(); // key shape count
    r.read<byte>(); // target attribute count
    var visibilityNodeCount = r.read<u16>();
    r.read<float>(); // bounding radius

    var vertexBufferOffset = r.read_rel_offset();
    var lodModelOffset = r.read_rel_offset();
    var skeletonIndexArrayOffset = r.read_rel_offset();

    r.read<u32>();

    var visibilityNodesOffset = r.read_rel_offset();
    var visibilityRangesOffset = r.read_rel_offset();
    var visibilityIndicesOffset = r.read_rel_offset();

    r.read<u32>();

    r.seek(lodModelOffset);
    for (int i = 0; i < lodModelCount; i++)
    {
        var lodModel = bfrs::FSHP::FSHPLodModel{};

        if (r.read<u32>() != 4)
            throw_error("fshp: only triangle lists are supported");
            const u32 indexFormat = r.read<u32>();

        lodModel.totalPointCount = r.read<u32>();
        lodModel.visibilityGroups = std::vector<bfrs::FSHP::FSHPVisibilityGroup>(r.read<u16>());

        r.read<u16>();
        var visibilityGroupOffset = r.read_rel_offset();
        var indexBufferOffset = r.read_rel_offset();
        lodModel.skipElements = r.read<u32>();

        var pos = r.position();

        r.seek(visibilityGroupOffset);
        for (int j = 0; j < lodModel.visibilityGroups.size(); j++)
        {
            var grp = bfrs::FSHP::FSHPVisibilityGroup{};

            grp.offset = r.read<u32>();
            grp.count = r.read<u32>();

            lodModel.visibilityGroups[j] = grp;
        }

        r.seek(indexBufferOffset);  
        {
            r.read<u32>();
            var indexBufferSize = r.read<u32>();

            r.read<u32>();
            r.read<u32>();
            r.read<u32>();
            var dataOffset = r.read_rel_offset();

            var indexBuffer = bfrs::FSHP::FSHPIndexBuffer{};

            r.seek(dataOffset);
            if (indexFormat == 4)
            {
                const auto idx = r.read_list<u16>(indexBufferSize / 2);
                indexBuffer.data.assign(idx.begin(), idx.end());
            }
            else if (indexFormat == 9)
                indexBuffer.data = r.read_list<u32>(indexBufferSize / 4);
            else
                throw_error("fshp: unsupported index format " + std::to_string(indexFormat));

            lodModel.indexBuffer = indexBuffer;
        }

        r.seek(pos);

        shape.lodModels.push_back(lodModel);
    }

    r.seek(visibilityNodesOffset);
    for (int i = 0; i < visibilityNodeCount; i++)
    {
        var visibilityNode = bfrs::FSHP::FSHPVisibilityNode{};

        visibilityNode.leftNodeIndex = r.read<u16>();
        visibilityNode.rightNodeIndex = r.read<u16>();

        r.read<u16>();
        var siblingIndex = r.read<u16>();
        visibilityNode.visibilityGroupIndex = r.read<u16>();
        visibilityNode.visibilityGroups = std::vector<bfrs::FSHP::FSHPVisibilityGroup>(r.read<u16>());

        shape.visibilityNodes.push_back(visibilityNode);
    }

    r.seek(visibilityRangesOffset);
    for (int i = 0; i < visibilityNodeCount; i++)
    {
        auto& node = shape.visibilityNodes[i];
        for (size_t j = 0; j < node.visibilityGroups.size(); j++)
        {
            const size_t g = j + node.visibilityGroupIndex;
            if (shape.lodModels.empty() || g >= shape.lodModels[0].visibilityGroups.size())
                throw_error("fshp: visibility group out of range");
            node.visibilityGroups[j] = shape.lodModels[0].visibilityGroups[g];
        }
        
        
        r.read_vec3();
        r.read_vec3();
        
    }    

    r.seek(visibilityIndicesOffset);
    shape.visibilityIndices = r.read_list<u16>(visibilityNodeCount);

    r.seek(skeletonIndexArrayOffset);
    shape.skeletonOffsets = r.read_list<u16>(skeletonIndexArrayCount);
    
    return shape;
}

BfresFile::FMDL load_model(filesys::ByteReader& r)
{

    if (!r.magic("FMDL"))
    {
        throw_error("not FMDL");
    }

    var fmdl = BfresFile::FMDL{};

    fmdl.name = r.read_string_offset();
    var path = r.read_string_offset();

    var skeletonOffset = r.read_rel_offset();
    var vertexBufferOffset = r.read_rel_offset();

    var shapeDictionaryOffset = r.read_rel_offset();
    var materialDictionaryOffset = r.read_rel_offset();
    var userDataDictionaryOffset = r.read_rel_offset();

    var vertexBufferCount = r.read<u16>();
    var shapeCount = r.read<u16>();
    var materialCount = r.read<u16>();
    var userDataCount = r.read<u16>();

    var vertexCount = r.read<uint>();

    r.read<uint>(); // reserved

    // skeleton loading
    r.seek(skeletonOffset);
    fmdl.skeleton = load_skeleton(r);

    // vertex buffer loading
    r.seek(vertexBufferOffset);
    fmdl.vertexBuffers = std::vector<BfresFile::FvtxVertexBuffer>(vertexBufferCount);
    for (int i = 0; i < vertexBufferCount; ++i)
    {
        r.seek(vertexBufferOffset + size_t(i) * 0x20);
        fmdl.vertexBuffers[i] = load_vertex_buffer(r);
    }

    var materialDictionary = load_dictionary_at(r, materialDictionaryOffset);
    fmdl.materials = std::vector<BfresFile::FMAT>(materialDictionary.nodes.size());
    for (int i = 0; i < materialDictionary.nodes.size(); ++i)
    {
        r.seek(materialDictionary.nodes[i].dataOffset);
        fmdl.materials[i] = load_material(r);
    }

    r.seek(shapeDictionaryOffset);
    var shapeDictionary = load_dictionary_at(r, shapeDictionaryOffset);
    fmdl.shapes = std::vector<BfresFile::FSHP>(shapeDictionary.nodes.size());
    for (int i = 0; i < shapeDictionary.nodes.size(); ++i)
    {
        r.seek(shapeDictionary.nodes[i].dataOffset);
        fmdl.shapes[i] = load_shape(r);
    }

    return fmdl;
}

graphics::ModelData parse_model(bfrs::FMDL& mdl,BfresModel& model)
{
    graphics::ModelData data;

    for (size_t i = 0; i < mdl.skeleton.bones.size(); ++i)
    {
        const auto& bone = mdl.skeleton.bones[i];
        graphics::NodeData node;
        node.name = bone.name;
        node.parent = bone.parentIndex == 0xFFFF ? -1 : int(bone.parentIndex);
        if (node.parent >= int(i))
            throw_error("fskl: bones aren't parent-first");
        node.translation = bone.position;
        node.rotation = bone.rotation;
        node.scale = bone.scale;
        data.nodes.push_back(std::move(node));
    }

    graphics::SkinData skin;
    for (size_t i = 0; i < mdl.skeleton.matrixToBone.size(); ++i)
    {
        const u16 b = mdl.skeleton.matrixToBone[i];
        if (b >= mdl.skeleton.bones.size())
            throw_error("fskl: matrix references a missing bone");
        skin.joints.push_back(b);
        skin.inverseBind.push_back(i < mdl.skeleton.inverseMatrices.size()
                                       ? mdl.skeleton.inverseMatrices[i]
                                       : math::mat4::identity());
    }
    data.skins.push_back(std::move(skin));

    const u32 skinnedNode = u32(data.nodes.size());
    graphics::NodeData skinned;
    skinned.name = "__skinned";
    skinned.skin = 0;
    data.nodes.push_back(std::move(skinned));

    for (const bfrs::FSHP& shape : mdl.shapes)
    {
        if (shape.lodModels.empty() || shape.lodModels[0].indexBuffer.data.empty())
            throw_error("fshp: no index data");
        if (shape.vertexBufferIndex >= mdl.vertexBuffers.size())
            throw_error("fshp: vertex buffer index out of range");

        const auto& vb = mdl.vertexBuffers[shape.vertexBufferIndex];
        graphics::MeshData mesh;
        mesh.name = shape.name;
        mesh.indices = shape.lodModels[0].indexBuffer.data;
        mesh.positions = read_attribute<math::vec3>(vb, "_p0");
        mesh.normals = read_attribute<math::vec3>(vb, "_n0");
        mesh.tangents = read_attribute<math::vec4>(vb, "_t0");
        mesh.uvs[0] = read_attribute<math::vec2>(vb, "_u0");
        mesh.colors = read_attribute<math::vec4>(vb, "_c0");
        mesh.material = shape.materialIndex;

        const u32 meshIndex = u32(data.meshes.size());

        if (shape.vertexSkinCount == 0)
        {
            if (shape.boneIndex >= mdl.skeleton.bones.size())
                throw_error("fshp: bone index out of range");
            data.nodes[shape.boneIndex].meshes.push_back(meshIndex);
        }
        else
        {
            mesh.joints = read_attribute<std::array<u16, 4>>(vb, "_i0");
            mesh.weights = read_attribute<math::vec4>(vb, "_w0");
            if (mesh.joints.size() != mesh.positions.size())
                throw_error("fshp: skinned shape is missing _i0");
            if (mesh.weights.empty())
                mesh.weights.assign(mesh.positions.size(), {1.0f, 0.0f, 0.0f, 0.0f});

            const u32 k = shape.vertexSkinCount;
            for (size_t v = 0; v < mesh.joints.size(); ++v)
            {
                auto& j = mesh.joints[v];
                auto& w = mesh.weights[v];
                if (k < 4)
                {
                    j[3] = 0;
                    w.w = 0.0f;
                }
                if (k < 3)
                {
                    j[2] = 0;
                    w.z = 0.0f;
                }
                if (k < 2)
                {
                    j[1] = 0;
                    w.y = 0.0f;
                    w.x = 1.0f;
                }
            }
            data.nodes[skinnedNode].meshes.push_back(meshIndex);
        }

        data.meshes.push_back(std::move(mesh));
    }

    
    std::unordered_map<std::string, u32> localTextures;
    for (const bfrs::FMAT& material : mdl.materials)
    {
        graphics::MaterialData m;
        m.name = material.name;
        m.shader = material.shaderControl.vertexShaderName + "_" +
                   material.shaderControl.fragmentShaderName;

        const size_t pairs = std::min(material.textureSelectors.size(), material.textureAttributeSelectors.size());
        for (size_t i = 0; i < pairs; ++i)
        {
            const std::string& texName = material.textureSelectors[i].name;
            auto it = localTextures.find(texName);
            if (it == localTextures.end())
            {
                auto found = std::find_if(model.textures.begin(), model.textures.end(),
                                          [&](const graphics::TextureData& t) { return t.name == texName; });
                if (found == model.textures.end() || found->mips.empty())
                    continue; // not decoded yet, white fallback
                it = localTextures.emplace(texName, u32(data.textures.size())).first;
                data.textures.push_back(*found);
            }
            m.textures[material.textureAttributeSelectors[i].name] = it->second;
        }

        data.materials.push_back(std::move(m));
    }

    return data;
}

bfrs::FTEX load_texture(filesys::ByteReader& r)
{
    bfrs::FTEX tex {};

    if (!r.magic("FTEX"))
        throw_error("not ftex");

    tex.dimension = (BfresFile::SurfaceDim)r.read<u32>();
    tex.width = r.read<u32>();
    tex.height = r.read<u32>();
    tex.depth = r.read<u32>();

    tex.mipmapCount = r.read<u32>();
    tex.format = (bfrs::SurfaceFormat)r.read<u32>();
    tex.antiAliasMode = (bfrs::AntiAliasMode)r.read<u32>();
    tex.usage = (bfrs::SurfaceUse)r.read<u32>();

    var dataLength = r.read<u32>();
    r.read<u32>();
    var mipMapSize = r.read<u32>();
    r.read<u32>();

    tex.tileMode = (bfrs::TileMode)r.read<u32>();
    tex.swizzle = r.read<u32>();
    tex.alignment = r.read<u32>();
    tex.pitch = r.read<u32>();

    r.read_array<byte>(0x6C);
    var dataOffset = r.read_rel_offset();
    var mipMapOffset = r.read_rel_offset();

    tex.data = r.slice(dataOffset, dataLength);
// note: maybe mipmaps soon

    return tex;
}

graphics::TextureData parse_texture(bfrs::FTEX& tex)
{
    if (tex.depth > 1)
        throw_error("ftex: 3d/array/cube textures aren't supported yet: " + tex.name);

    const gx2::Surface s{tex.width, tex.height, u32(tex.format), u32(tex.tileMode), tex.swizzle, tex.pitch};

    graphics::TextureData data;
    data.name = tex.name;
    data.width = tex.width;
    data.height = tex.height;
    data.format = graphics::TextureFormat::RGBA8;
    data.srgb = (u32(tex.format) & 0x400) != 0;
    data.mips.push_back(gx2::decode_rgba8(s.format, tex.width, tex.height, gx2::deswizzle(s, tex.data)));
    data.cacheKey = tex.name;
    return data;
}

bfrs::FSKA::FSKAAnimCurve load_anim_curve(filesys::ByteReader& r)
{
    bfrs::FSKA::FSKAAnimCurve curve {};

    var flags = r.read<u16>();
    curve.frameType = (bfrs::FSKA::FrameType)(flags & 3);
    curve.keyType = (bfrs::FSKA::KeyType)((flags >> 2) & 3);
    curve.curveType = (bfrs::FSKA::CurveType)((flags >> 4) & 7);
    curve.preWrap = (bfrs::FSKA::WrapMode)((flags >> 8) & 3);
    curve.postWrap = (bfrs::FSKA::WrapMode)((flags >> 12) & 3);

    var keyCount = r.read<u16>();
    curve.target = r.read<u32>();

    curve.startFrame = r.read<float>();
    curve.endFrame = r.read<float>();
    curve.scale = r.read<float>();
    curve.offset = r.read<float>();
    curve.delta = r.read<float>();

    var framesOffset = r.read_rel_offset();
    var keysOffset = r.read_rel_offset();

    r.seek(framesOffset);
    curve.frames.reserve(keyCount);
    for (u16 k = 0; k < keyCount; ++k)
    {
        switch (curve.frameType)
        {
            case bfrs::FSKA::FrameType::Float:
                curve.frames.push_back(r.read<float>());
                break;
            case bfrs::FSKA::FrameType::Fixed10x5:
                curve.frames.push_back(r.read<int16_t>() / 32.0f);
                break;
            case bfrs::FSKA::FrameType::Byte:
                curve.frames.push_back(float(r.read<u8>()));
                break;
            default:
                throw_error("fska: unknown frame type");
        }
    }

    u32 elementsPerKey = 1;
    if (curve.curveType == bfrs::FSKA::CurveType::Cubic)
        elementsPerKey = 4;
    else if (curve.curveType == bfrs::FSKA::CurveType::Linear)
        elementsPerKey = 2;

    const bool integerKeys =
        curve.curveType == bfrs::FSKA::CurveType::StepInt || curve.curveType == bfrs::FSKA::CurveType::StepBool;

    r.seek(keysOffset);
    curve.keys.reserve(size_t(keyCount) * elementsPerKey);
    for (size_t k = 0; k < size_t(keyCount) * elementsPerKey; ++k)
    {
        switch (curve.keyType)
        {
            case bfrs::FSKA::KeyType::Float:
                curve.keys.push_back(integerKeys ? float(r.read<u32>()) : r.read<float>());
                break;
            case bfrs::FSKA::KeyType::Int16:
                curve.keys.push_back(float(r.read<int16_t>()));
                break;
            case bfrs::FSKA::KeyType::SByte:
                curve.keys.push_back(float(r.read<int8_t>()));
                break;
            default:
                throw_error("fska: unknown key type");
        }
    }

    for (size_t k = 0; k < curve.keys.size(); ++k)
    {
        curve.keys[k] *= curve.scale;
        if (k % elementsPerKey == 0)
            curve.keys[k] += curve.offset;
    }

    return curve;
}

BfresFile::FSKA load_skeletal_animation(filesys::ByteReader& r)
{
    bfrs::FSKA fska {};

    if (!r.magic("FSKA"))
        throw_error("not fska");

    fska.name = r.read_string_offset();
    var path = r.read_string_offset();

    fska.flags = r.read<u32>();
    fska.frameCount = r.read<u32>();

    var boneAnimCount = r.read<u16>();
    r.read<u16>(); // user data
    var curveCount = r.read<u32>();
    var bakedSize = r.read<u32>();

    var boneAnimArrayOffset = r.read_rel_offset();
    var bindSkeletonOffset = r.read_rel_offset();
    var bindIndexArrayOffset = r.read_rel_offset();

    // bone anims
    r.seek(boneAnimArrayOffset);
    for (int i = 0; i < boneAnimCount; ++i)
    {
        r.seek(boneAnimArrayOffset + size_t(i) * 24);
        bfrs::FSKA::FSKABoneAnim anim{};

        anim.flags = r.read<u32>();
        anim.name = r.read_string_offset();
        var beginRotate = r.read<byte>();
        var beginTranslate = r.read<byte>();
        var _curveCount = r.read<byte>();
        var beginBaseTranslate = r.read<byte>();
        var beginCurve = r.read<byte>();
        r.skip(3);
        var curveArrayOffset = r.read_rel_offset();
        var baseDataOffset = r.read_rel_offset();
            
        r.seek(baseDataOffset);
        {
            if (anim.flags & 0x08)
            {
                anim.baseScale = r.read_vec3();
            }
            if (anim.flags & 0x10)
            {
                anim.baseRotation = r.read_vec4();
            }
            if (anim.flags & 0x20)
            {
                anim.baseTranslation = r.read_vec3();
            }
        }

        for (int j = 0; j < _curveCount; ++j)
        {
            r.seek(curveArrayOffset + j * 36);
            anim.curves.push_back(load_anim_curve(r));
        }

        fska.bones.push_back(anim);
    }

    return fska;
}

graphics::AnimationData parse_skeletal_animation(const bfrs::FSKA& fska, const bfrs::FSKL& skeleton)
{
    using graphics::AnimPath;

    graphics::AnimationData anim{};
    anim.name = fska.name;
    anim.duration = float(fska.frameCount) / anim_fps;

    const bool euler = fska.eulerRotation();
    const u32 frameCount = fska.frameCount;

    for (const auto& boneAnim : fska.bones)
    {
        u32 node = UINT32_MAX;
        for (size_t b = 0; b < skeleton.bones.size(); ++b)
            if (skeleton.bones[b].name == boneAnim.name)
            {
                node = u32(b);
                break;
            }
        if (node == UINT32_MAX)
            continue;

        const auto& bone = skeleton.bones[node];

        // 0..2 scale, 3..6 rotation, 7..9 translation
        float base[10];
        const bool hasScale = boneAnim.flags & 0x08;
        const bool hasRotate = boneAnim.flags & 0x10;
        const bool hasTranslate = boneAnim.flags & 0x20;

        const math::vec3 s = hasScale ? boneAnim.baseScale : bone.scale;
        const math::vec3 t = hasTranslate ? boneAnim.baseTranslation : bone.position;
        math::vec4 r = hasRotate ? boneAnim.baseRotation : math::vec4{0.0f, 0.0f, 0.0f, 1.0f};
        if (!hasRotate && !euler)
            r = {bone.rotation.x, bone.rotation.y, bone.rotation.z, bone.rotation.w};

        base[0] = s.x;
        base[1] = s.y;
        base[2] = s.z;
        base[3] = r.x;
        base[4] = r.y;
        base[5] = r.z;
        base[6] = r.w;
        base[7] = t.x;
        base[8] = t.y;
        base[9] = t.z;

        // base values set the pose even without curves (that's how static poses are stored)
        bool animScale = hasScale, animRotate = hasRotate, animTranslate = hasTranslate;
        for (const auto& c : boneAnim.curves)
        {
            if (c.target >= 0x04 && c.target <= 0x0C)
                animScale = true;
            else if (c.target >= 0x10 && c.target <= 0x18)
                animTranslate = true;
            else if (c.target >= 0x20 && c.target <= 0x2C)
                animRotate = true;
        }

        auto slot = [](u32 target) -> int
        {
            switch (target)
            {
                case 0x04:
                    return 0;
                case 0x08:
                    return 1;
                case 0x0C:
                    return 2;
                case 0x20:
                    return 3;
                case 0x24:
                    return 4;
                case 0x28:
                    return 5;
                case 0x2C:
                    return 6;
                case 0x10:
                    return 7;
                case 0x14:
                    return 8;
                case 0x18:
                    return 9;
                default:
                    return -1;
            }
        };

        graphics::AnimationData::Channel sc, rc, tc;
        sc.node = rc.node = tc.node = node;
        sc.path = AnimPath::Scale;
        rc.path = AnimPath::Rotation;
        tc.path = AnimPath::Translation;
        sc.interp = rc.interp = tc.interp = graphics::AnimInterp::Linear;

        const u32 lastFrame = boneAnim.curves.empty() ? 0 : frameCount;
        for (u32 f = 0; f <= lastFrame; ++f)
        {
            float v[10];
            std::copy(std::begin(base), std::end(base), v);

            for (const auto& c : boneAnim.curves)
            {
                const int i = slot(c.target);
                if (i >= 0)
                    v[i] = sample_curve(c, float(f));
            }

            const float time = float(f) / anim_fps;

            if (animScale)
            {
                sc.times.push_back(time);
                sc.values.push_back({v[0], v[1], v[2], 0.0f});
            }
            if (animRotate)
            {
                const math::quat q =
                    math::normalize(euler ? euler_zyx(v[3], v[4], v[5]) : math::quat{v[3], v[4], v[5], v[6]});
                rc.times.push_back(time);
                rc.values.push_back({q.x, q.y, q.z, q.w});
            }
            if (animTranslate)
            {
                tc.times.push_back(time);
                tc.values.push_back({v[7], v[8], v[9], 0.0f});
            }
        }

        if (animScale)
            anim.channels.push_back(std::move(sc));
        if (animRotate)
            anim.channels.push_back(std::move(rc));
        if (animTranslate)
            anim.channels.push_back(std::move(tc));
    }

    return anim;
} 

BfresModel BfresFile::parse(BfresFile* baseFile)
{
    BfresModel model{};

    var models = baseFile ? baseFile->models : this->models;
    var textures = baseFile ? baseFile->textures : this->textures;

    std::unordered_set<std::string> used;
    for (const auto& m : models)
        for (const auto& mat : m.materials)
            for (const auto& sel : mat.textureSelectors)
                used.insert(sel.name);

    for (auto& texture : textures)
        if (used.count(texture.name))
            model.textures.push_back(parse_texture(texture));

    std::vector<std::vector<graphics::AnimationData>> animsPerModel(models.size());
    for (const auto& fska : skeletalAnims)
    {
        size_t best = SIZE_MAX, bestMatches = 0;
        for (size_t m = 0; m < models.size(); ++m)
        {
            size_t matches = 0;
            for (const auto& boneAnim : fska.bones)
                for (const auto& bone : models[m].skeleton.bones)
                    if (bone.name == boneAnim.name)
                    {
                        ++matches;
                        break;
                    }
            if (matches > bestMatches)
            {
                best = m;
                bestMatches = matches;
            }
        }
        if (best == SIZE_MAX)
            continue;

        graphics::AnimationData anim = parse_skeletal_animation(fska, models[best].skeleton);
        if (!anim.channels.empty())
            animsPerModel[best].push_back(std::move(anim));
    }

    for (size_t m = 0; m < models.size(); ++m)
    {
        graphics::ModelData data = parse_model(models[m], model);
        data.animations = std::move(animsPerModel[m]);
        model.models.push_back(std::move(data));
    }

    return model;
}

std::unique_ptr<BfresFile> load_bfres(std::span<const byte> data)
{
    filesys::ByteReader r(data);
    if (!r.magic("FRES"))
        throw_error("not bfres");

    byte version0 = r.read<byte>();
    byte version1 = r.read<byte>();
    byte version2 = r.read<byte>();
    byte version3 = r.read<byte>();

    r.bigEndian = r.read<u16>() == 0xFFFE;

    short headerSize = r.read<short>();
    uint fileSize = r.read<uint>();
    uint fileAlign = r.read<uint>();
    uint fileNameOffset = r.read<uint>(); // add 0x14 for abs

    uint stringTblLen = r.read<uint>();
    uint stringTblOffset = r.read<uint>(); // add 0x1C

    var indexGroupOffsets = r.read_offset_array(12);
    var indexGroupCounts = r.read_list<u16>(12);

    uint unk0 = r.read<uint>();

    var dictionaries = std::vector<BfresFile::Dictionary>(12);
    for (int i = 0; i < 12; ++i)
    {
        dictionaries[i] = load_dictionary_at(r, indexGroupOffsets[i]);
    }

    auto bfres = std::make_unique<BfresFile>();
    bfres->models.resize(dictionaries[0].nodes.size());
    for (size_t i = 0; i < dictionaries[0].nodes.size(); ++i)
    {
        r.seek(dictionaries[0].nodes[i].dataOffset);
        bfres->models[i] = load_model(r);
    }

    bfres->textures.resize(dictionaries[1].nodes.size());
    for (int i = 0; i < dictionaries[1].nodes.size(); ++i)
    {
        r.seek(dictionaries[1].nodes[i].dataOffset);
        bfres->textures[i] = load_texture(r);
        bfres->textures[i].name = dictionaries[1].nodes[i].name;
    }

    bfres->skeletalAnims.resize(dictionaries[2].nodes.size());
    for (int i = 0; i < dictionaries[2].nodes.size(); ++i)
    {
        r.seek(dictionaries[2].nodes[i].dataOffset);
        bfres->skeletalAnims[i] = load_skeletal_animation(r);
    }

    return bfres;
}
