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

    SarcFile* filesBytes = new SarcFile[files.size()];
    std::copy(files.begin(), files.end(), filesBytes);

    return filesBytes;
}

void* BFRESFileOperator::onFileBytes(byte* data, int dataSize)
{
    filesys::ByteReader r({data, size_t(dataSize)}, true);
    if (!r.magic("FRES"))
        throw_error("not bfres");

    uin fileVersion = r.read<uin>();

    short bom = r.read<u16>(); // FE FF read as LE
    r.bigEndian = bom == 0xFFFE;

    short headerSize = r.read<short>();
    uin fileSize = r.read<uin>();
    uin fileAlign = r.read<uin>();
    uin fileNameOffset = r.read<uin>(); // add 0x14 for abs

    uin stringTblLen = r.read<uin>();
    uin stringTblOffset = r.read<uin>(); // add 0x1C

    uin* indexGroupOffsets = r.read_array<uin>(12);
    u16* indexGroupCounts = r.read_array<u16>(12);

    uin unk0 = r.read<uin>();

    

}
