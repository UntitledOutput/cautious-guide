#include "loaders/bfres/sarc.hpp"
#include "loaders/bfres/reader.hpp"
#include "loaders/bfres/bfres.hpp"

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

std::vector<math::vec4> read_attribute(const BfresFile::FvtxVertexBuffer& vb, std::string_view name)
{
    for (const auto& buf : vb.data)
        for (const auto& a : buf.attributes)
        {
            if (a.name != name)
                continue;

            filesys::ByteReader r(buf.data, true);
            std::vector<math::vec4> out(vb.vertexCount);
            for (u32 i = 0; i < vb.vertexCount; ++i)
            {
                r.seek(size_t(i) * buf.stride + a.offset);
                out[i] = decode_attribute(r, a.format);
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

    r.seek(boneIndexGroupOffset);
    var boneIndexGroup = load_dictionary(r);

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

std::vector<math::vec2> BfresFile::FvtxVertexBuffer::FvtxVertexData::getVector2Data(FvtxVertexAttribute attribute)
{
    var _data = std::vector<math::vec2>();

    var reader = filesys::ByteReader(data, true);
    for (int i = 0; i < data.size(); i += stride)
    {
        reader.seek(i + attribute.offset);
        switch (attribute.format)
        {
            case FvtxVertexAttributeFormat::Two_32Bit_Float:
                _data.push_back(reader.read_vec2());
                break;
            case FvtxVertexAttributeFormat::Two_16Bit_Normalized:
                {
                    u16 x = reader.read<u16>();
                    u16 y = reader.read<u16>();
                    _data.push_back(math::vec2(cflt x / 65535.f, cflt y / 65535.f));
                }
                break;
            default:
                throw_error("mismatching format");
        }
    }

    return _data;
}

std::vector<math::vec3> BfresFile::FvtxVertexBuffer::FvtxVertexData::getVector3Data(FvtxVertexAttribute attribute)
{
    
    var _data = std::vector<math::vec3>();

    var reader = filesys::ByteReader(data, true);
    for (int i = 0; i < data.size(); i += stride)
    {
        reader.seek(i + attribute.offset);
        switch (attribute.format)
        {
            case FvtxVertexAttributeFormat::Three_32Bit_Float:
                _data.push_back(reader.read_vec3());
                break;
            case FvtxVertexAttributeFormat::Three_10Bit_Signed:
                {
                    uint packedBytes = reader.read<u32>();
                    short x = (short)((packedBytes >> 20) & 0x3FF);
                    short y = (short)((packedBytes >> 10) & 0x3FF);
                    short z = (short)(packedBytes & 0x3FF);
                    // Divide by 511 to get the float value.
                    _data.push_back(math::vec3(x / 511.f, y / 511.f, z / 511.f));
                }
                break;
            default:
                throw_error("mismatching format");
        }
    }

    return _data;

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

    r.seek(attributeIndexGroupOffset);
    var attributeIndexGroup = load_dictionary(r);


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
    r.seek(renderParamIndexGroupOffset);
    var renderParameterIndexGroup = load_dictionary(r);

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
                case BfresFile::FMAT::FmatMaterialParameterType::Matrix2x3:
                    {
                        float m11 = r.read<float>();
                        float m21 = r.read<float>();
                        float m12 = r.read<float>();
                        float m22 = r.read<float>();
                        float m13 = r.read<float>();
                        float m23 = r.read<float>();
                        parameter.matrixValue = math::mat4();
                        parameter.matrixValue[1] = {m11, m12, m13, 0};
                        parameter.matrixValue[2] = {m21, m22, m23, 0};
                    }
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

        r.seek(vertexShaderInputIndexGroupOffset);
        var vtxShaderInputGroup = load_dictionary(r);

        for (auto& node : vtxShaderInputGroup.nodes)
            mat.shaderControl.vertexShaderInputs.push_back(node.name);

        r.seek(fragmentShaderInputIndexGroupOffset);
        var frgShaderInputGroup = load_dictionary(r);

        for (auto& node : frgShaderInputGroup.nodes)
            mat.shaderControl.fragmentShaderInputs.push_back(node.name);

        r.seek(parameterIndexGroupOffset);
        var parameterInputGroup = load_dictionary(r);

        for (auto& node : parameterInputGroup.nodes)
            mat.shaderControl.parameters.push_back(node.name);
    }

    // shadow parameters
    r.seek(shadowParameterIndexGroupOffset);
    var shadowInputGroup = load_dictionary(r);

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
    var visibilityNodeCount = r.read<u32>();
    r.read<float>();

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

        r.read<u32>();
        r.read<u32>();

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
            indexBuffer.data = r.read_list<u16>(indexBufferSize / 2);

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
        var node = shape.visibilityNodes[i];

        for (int j = 0; j < node.visibilityGroups.size(); j++)
        {
            node.visibilityGroups[j] = shape.lodModels[0].visibilityGroups[j + node.visibilityGroupIndex];
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

    r.seek(materialDictionaryOffset);
    var materialDictionary = load_dictionary(r);
    fmdl.materials = std::vector<BfresFile::FMAT>(materialDictionary.nodes.size());
    for (int i = 0; i < materialDictionary.nodes.size(); ++i)
    {
        r.seek(materialDictionary.nodes[i].dataOffset);
        fmdl.materials[i] = load_material(r);
    }

    r.seek(shapeDictionaryOffset);
    var shapeDictionary = load_dictionary(r);
    fmdl.shapes = std::vector<BfresFile::FSHP>(shapeDictionary.nodes.size());
    for (int i = 0; i < shapeDictionary.nodes.size(); ++i)
    {
        r.seek(shapeDictionary.nodes[i].dataOffset);
        fmdl.shapes[i] = load_shape(r);
    }

    return fmdl;
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
        var offset = indexGroupOffsets[i];
        uint count = indexGroupCounts[i];

        var dictionary = BfresFile::Dictionary{};

        if (count > 0)
        {
            r.seek(offset);
            dictionary = load_dictionary(r);
        }
        dictionaries[i] = dictionary;
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