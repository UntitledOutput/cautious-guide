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

    class FileOperator {
        class_def(FileOperator,std::span<std::string> exts)
        std::span<std::string> extensions;
        FileType operationType;

        virtual void* onFileString(std::string data);
        virtual void* onFileBytes(byte* data);

    };

    class FilesystemOperator {
        class_def(FilesystemOperator)

        void set_local_path(std::string directory);

        std::string read_file_str(std::string path);
        byte* read_file_bytes(std::string path);
        void* read_file_auto(std::string path);
        
    private:

        FileOperator& get_file_operator(std::string ext);

        std::vector<std::unique_ptr<FileOperator>> file_operators;

    };

}