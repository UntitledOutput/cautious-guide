#include "core/file.hpp"

#include <istream>
#include <iostream>
#include <fstream>

using namespace filesys;

FilesystemOperator::FilesystemOperator(FilesystemConfiguration&& cfg)
    : _cfg(std::move(cfg)), file_operators(std::move(_cfg.fileOperators))
{
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;

}

void FilesystemOperator::register_file_operator(std::unique_ptr<FileOperator> operator_)
{
    file_operators.push_back(std::move(operator_));
}

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

std::vector<byte> filesys::FilesystemOperator::read_file(std::string path) { 
    std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!file)
        throw_error("failed to open file: " + path);

    std::vector<byte> data(size_t(file.tellg()));
    file.seekg(0);
    if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size())))
        throw_error("failed to read file: " + path);
    return data;
}

void* filesys::FilesystemOperator::read_file_auto(std::string path) {
    auto& _operator = get_file_operator(std::filesystem::path(path).extension().string());

    if (_operator.operationType == FileType::String)
        return _operator.onFileString(read_file_str(path));

    var bytes = read_file(path);
    return _operator.onFileBytes(bytes.data(), int(bytes.size()));
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
