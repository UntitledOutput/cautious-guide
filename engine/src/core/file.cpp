#include "file.hpp"

using namespace filesys;

std::string filesys::FilesystemOperator::read_file_str(std::string path) { 
    var fs_path = std::filesystem::path(path);
    if (!std::filesystem::exists(fs_path))
        throw_error("file does not exist: " + path);

    std::ifstream file(fs_path, std::ios::in);
    if (!file.is_open())
        throw_error("failed to open file: " + path);

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return content;
}

byte* filesys::FilesystemOperator::read_file_bytes(std::string path) { 
    // byte* is just an alias for unsigned char*, so we can use it directly
    var fs_path = std::filesystem::path(path);
    if (!std::filesystem::exists(fs_path))
        throw_error("file does not exist: " + path);

    std::ifstream file(fs_path, std::ios::in);
    if (!file.is_open())
        throw_error("failed to open file: " + path);

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    byte* buffer = new byte[size];
    if (!file.read(reinterpret_cast<char*>(buffer), size))
    {
        delete[] buffer;
        throw_error("failed to read file: " + path);
    }
    file.close();
    return buffer;
}

void* filesys::FilesystemOperator::read_file_auto(std::string path) {
    auto& _operator = get_file_operator(std::filesystem::path(path).extension().string());

    if (_operator.operationType == FileType::String)
        return _operator.onFileString(read_file_str(path));
    else
        return _operator.onFileBytes(read_file_bytes(path));
}

FileOperator& filesys::FilesystemOperator::get_file_operator(std::string ext)
{
    for (auto& _operator : file_operators)
    {
        if (vcontains(_operator->extensions, ext))
            return *_operator;
    }
    throw_error("no file operator found for extension: " + ext);
}
