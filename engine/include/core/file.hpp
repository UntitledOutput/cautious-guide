#pragma once

#include <util.hpp>
#include <string>
#include <filesystem>
#include <span>

namespace filesys 
{

    enum FileType {
        String,
        Binary
    };

    class BinaryReader {
        class_def(BinaryReader, std::span<byte> data)
        std::span<byte> data;

        template<typename T>
        T read() {
            if (data.size() < sizeof(T))
                throw_error("not enough data to read");

            T value;
            std::memcpy(&value, data.data(), sizeof(T));
            data = data.subspan(sizeof(T));
            
            return value;
        }

        void seek(size_t offset) {
            if (offset > data.size())
                throw_error("seek offset out of bounds");

            data = data.subspan(offset);
        }

        void skip(size_t count) {
            if (count > data.size())
                throw_error("skip count out of bounds");

            data = data.subspan(count);
        }

        void reset() {
            data = data.subspan(0);
        }
    };

    class FileOperator {
        class_def(FileOperator,std::span<std::string> exts)
        std::span<std::string> extensions;
        FileType operationType;

        virtual void* onFileString(std::string data) {}
        virtual void* onFileBytes(byte* data) {}

    };

    struct FilesystemConfiguration {
        std::string localPath;
        std::vector<std::unique_ptr<FileOperator>> fileOperators;
    };

    class FilesystemOperator {
        class_def(FilesystemOperator, FilesystemConfiguration cfg)

        void set_local_path(std::string directory);

        std::string read_file_str(std::string path);
        byte* read_file_bytes(std::string path);
        void* read_file_auto(std::string path);
        
    private:
        FilesystemConfiguration _cfg;

        FileOperator& get_file_operator(std::string ext);

        std::vector<std::unique_ptr<FileOperator>> file_operators;

    };

}