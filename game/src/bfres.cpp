#include "loaders/bfres/sarc.hpp"
#include "loaders/bfres/reader.hpp"
#include "loaders/bfres/bfres.hpp"

#include "graphics/model.hpp"

using bfrs = BfresFile;

math::vec4 decode_attribute(filesys::ByteReader& r, BfresFile::FvtxVertexBuffer::FvtxVertexAttributeFormat format)
{
    switch (u32(format))
    {
        case 0x811:
            return {r.read<float>(), r.read<float>(), r.read<float>(), 1.0f};
        case 0x80D:
            return {r.read<float>(), r.read<float>(), 0.0f, 1.0f};
        case 0x007:
            return {r.read<u16>() / 65535.0f, r.read<u16>() / 65535.0f, 0.0f, 1.0f};
        case 0x004:
            return {r.read<u8>() / 255.0f, r.read<u8>() / 255.0f, 0.0f, 0.0f};
        case 0x00A:
            return {r.read<u8>() / 255.0f, r.read<u8>() / 255.0f, r.read<u8>() / 255.0f, r.read<u8>() / 255.0f};
        case 0x104:
            return {float(r.read<u8>()), float(r.read<u8>()), 0.0f, 0.0f};
        case 0x10A:
            return {float(r.read<u8>()), float(r.read<u8>()), float(r.read<u8>()), float(r.read<u8>())};
        case 0x20A:
            {
                auto s = [&] { return std::max(float(r.read<int8_t>()) / 127.0f, -1.0f); };
                return {s(), s(), s(), s()};
            }
        case 0x20B:
            {
                const u32 v = r.read<u32>();
                auto s = [&](int shift)
                {
                    int x = int((v >> shift) & 0x3FF);
                    if (x & 0x200)
                        x -= 0x400;
                    return float(x) / 511.0f;
                };
                return {s(0), s(10), s(20), 0.0f};
            }
        default:
            throw_error("unsupported vertex attribute format " + std::to_string(u32(format)));
    }
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

void* SARCFileOperator::onFileBytes(byte* data, int dataSize)
{
    const std::span<const byte> src{data, size_t(dataSize)};
    auto archive = std::make_unique<SarcArchive>();

    if (src.size() >= 4 && std::memcmp(src.data(), "Yaz0", 4) == 0)
        archive->storage = yaz0_decompress(src);
    else
        archive->storage.assign(src.begin(), src.end());

    archive->files = sarc_read(archive->storage);
    for (auto& file : archive->files)
        if (std::filesystem::path(file.name).extension() == ".bfres")
            file.bfres = load_bfres(file.data);

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
        bone.rotation = (flags & 0x7000) == 0x1000 ? math::from_euler({rot.x, rot.y, rot.z})
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

graphics::ModelData BfresFile::FMDL::parse()
{
    graphics::ModelData data;

    for (size_t i = 0; i < skeleton.bones.size(); ++i)
    {
        const auto& bone = skeleton.bones[i];
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
    for (size_t i = 0; i < skeleton.matrixToBone.size(); ++i)
    {
        const u16 b = skeleton.matrixToBone[i];
        if (b >= skeleton.bones.size())
            throw_error("fskl: matrix references a missing bone");
        skin.joints.push_back(b);
        // rigid entries come after the smooth ones, their vertices are already in bone space
        skin.inverseBind.push_back(i < skeleton.inverseMatrices.size() ? skeleton.inverseMatrices[i]
                                                                       : math::mat4::identity());
    }
    data.skins.push_back(std::move(skin));

    const u32 skinnedNode = u32(data.nodes.size());
    graphics::NodeData skinned;
    skinned.name = "__skinned";
    skinned.skin = 0;
    data.nodes.push_back(std::move(skinned));

    for (const FSHP& shape : shapes)
    {
        if (shape.lodModels.empty() || shape.lodModels[0].indexBuffer.data.empty())
            throw_error("fshp: no index data");
        if (shape.vertexBufferIndex >= vertexBuffers.size())
            throw_error("fshp: vertex buffer index out of range");

        const auto& vb = vertexBuffers[shape.vertexBufferIndex];
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
            if (shape.boneIndex >= skeleton.bones.size())
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

            // only the first skinCount slots are meaningful
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

    for (const FMAT& material : materials)
    {
        graphics::MaterialData m;
        m.name = material.name;
        m.shader = material.shaderControl.vertexShaderName + "_" + material.shaderControl.fragmentShaderName;
        data.materials.push_back(std::move(m));
    }
    return data;
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


    return bfres;
}