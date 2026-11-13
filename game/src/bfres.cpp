#include "loaders/bfres/sarc.hpp"
#include "loaders/bfres/reader.hpp"
#include "loaders/bfres/bfres.hpp"

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
    filesys::ByteReader r({data,size_t(dataSize)}, true);
    if (r.magic("Yaz0"))
    {
        auto decompressed = yaz0_decompress({data, size_t(dataSize)});
        return onFileBytes(decompressed.data(), decompressed.size());
    }

    std::vector<SarcFile> files = sarc_read({data, size_t(dataSize)});

    for (auto file : files)
    {

        file.processedData = (*(Engine*)enginePtr)
                                 .filesystem()
                                 .process_file(const_cast<byte*>(file.data.data()), file.data.size(),
                                               std::filesystem::path(file.name).extension().string());
    }

    SarcFile* filesBytes = new SarcFile[files.size()];
    std::copy(files.begin(), files.end(), filesBytes);

    return filesBytes;
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
        bone.parentIndices = r.read_list<u16>(4);
        r.read<u16>(); // unk
        r.read<u16>(); // flags
        r.read<u16>(); // unk

        bone.scale = r.read_vec3();
        bone.rotation = r.read_quat();
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
        fskl.bones[inverseIndexArray[i]].inverseMatrix = inverseMatrices[i];
    }

    for (auto& fskl_bone : fskl.bones)
    {
        fskl_bone.parents = std::vector<BfresFile::FSKL::FsklBone>(fskl_bone.parentIndices.size());
        for (int i = 0; i < fskl_bone.parentIndices.size(); ++i)
        {
            u16 parentIndex = fskl_bone.parentIndices[i];
            if (parentIndex != 0xFFFF)
                fskl_bone.parents[i] = fskl.bones[parentIndex];
        }
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
    var elementCount = r.read<u32>();

    var unk = r.read<u32>();

    if (unk != 0x00000000 && unk != 0x01000000 &&
        unk != 0x02000000 && unk != 0x03000000 && unk != 0x04000000)
    {
        throw_error("unk unexpected value ");
    }

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
        BfresFile::FvtxVertexBuffer::FvtxVertexData dataBuffer{};

        unk = r.read<u32>();
        var size = r.read<u32>();
        unk = r.read<u32>();
        dataBuffer.stride = r.read<u16>();
        r.read<u16>();
        r.read<u32>();
        var dataOffset = r.read_rel_offset();

        if (size > 0)
        {
            var pos = r.position();
            r.seek(dataOffset);
            dataBuffer.data = r.read_array<byte>(size);
            r.seek(pos);
        }

        buffer.data.push_back(dataBuffer);
    }

    r.seek(attributeArrayOffset);
    for (int i = 0; i < attributeCount; ++i)
    {
        BfresFile::FvtxVertexBuffer::FvtxVertexAttribute a{};

        a.name = r.read_string_offset();

        uint indexAndOffset = r.read<uint>();
        a.offset = indexAndOffset & 0x00FFFFFF;

        a.format = (BfresFile::FvtxVertexBuffer::FvtxVertexAttributeFormat)r.read<uint>();

        var bufferIndex = (byte)(indexAndOffset & 0xFF000000);
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

        r.read<u16>();

        param.type = (BfresFile::FMAT::FmatRenderParameterType)r.read<byte>();

        r.read<byte>();

        param.name = r.read_string_offset();

        switch (param.type)
        {
            case BfresFile::FMAT::FmatRenderParameterType::Null8Bytes:
                param.nullBytesValue = r.read_array<byte>(8);
                break;
            case BfresFile::FMAT::FmatRenderParameterType::Vector2F:
                param.vector2Value = r.read_vec2();
                break;
            case BfresFile::FMAT::FmatRenderParameterType::StringOffset:
                param.stringValue = r.read_string_offset();
                break;
            default:
                throw_error("unknown render parameter type");
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


        attributeSelector.name = r.read_string_offset();
        attributeSelector.index = r.read<byte>();
    }

    // material parameters
    r.seek(materialParameterOffset);
    mat.materialParameters =
        std::vector<BfresFile::FMAT::FMATTextureAttributeSelector>(materialParameterCount);

    return mat;
}

BfresFile::FSHP load_shape(filesys::ByteReader& r)
{
    BfresFile::FSHP shape{};

    if (!r.magic("FSHP"))
        throw_error("not fshp");

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

void* BFRESFileOperator::onFileBytes(byte* data, int dataSize)
{
    filesys::ByteReader r({data, size_t(dataSize)});
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

    var bfres = new BfresFile{};
    bfres->models = std::vector<BfresFile::FMDL>(dictionaries[0].nodes.size());
    for (int i = 0; i < dictionaries[0].nodes.size(); ++i)
    {
        var node = dictionaries[0].nodes[i];
        r.seek(node.dataOffset);

        var fmdl = load_model(r);

        bfres->models[i] = fmdl;
    }

    
    return bfres;
}
