#pragma once

#include <core/util.hpp>
#include <string>
#include <filesystem>

namespace filesys 
{

    enum FileType {
        String,
        Binary
    };

    class BinaryReader
    {
        class_def(BinaryReader, std::vector<byte> data) std::vector<byte> data;

        template <typename T>
        T read()
        {
            static_assert(std::is_trivially_copyable_v<T>, "read<T> needs a trivially copyable type");

            if (remaining() < sizeof(T))
                throw_error("not enough data to read");

            T value;
            std::memcpy(&value, data.data() + pos, sizeof(T));
            pos += sizeof(T);

            return value;
        }

        // absolute position from the start
        void seek(size_t offset)
        {
            if (offset > data.size())
                throw_error("seek offset out of bounds");

            pos = offset;
        }

        void skip(size_t count)
        {
            if (count > remaining())
                throw_error("skip count out of bounds");

            pos += count;
        }

        void reset() { pos = 0; }

        size_t position() const { return pos; }
        size_t remaining() const { return data.size() - pos; }

    private:
        size_t pos = 0;
    };

    inline BinaryReader::BinaryReader(std::vector<byte> data) : data(std::move(data)) {}

    class FileOperator {
        virt_class_def(FileOperator)
        std::vector<std::string> extensions;
        FileType operationType;
        void* enginePtr;

        virtual void* onFileString(std::string data) { return nullptr; }
        virtual void* onFileBytes(byte* data, int dataSize) { return nullptr; }

    };

    struct FilesystemConfiguration {
        std::string localPath;
        std::vector<std::unique_ptr<FileOperator>> fileOperators;
    };

    class FilesystemOperator {
        class_def(FilesystemOperator, FilesystemConfiguration&& cfg)

        void set_local_path(std::string directory);
        void register_file_operator(std::unique_ptr<FileOperator> operator_);

        std::string read_file_str(std::string path);
        std::vector<byte> read_file(std::string path);
        void* read_file_auto(std::string path);
        
    private:
        FilesystemConfiguration _cfg;

        FileOperator& get_file_operator(std::string ext);

        std::vector<std::unique_ptr<FileOperator>> file_operators;

    };

}