#include "file.hpp"

using namespace filesys;

std::string filesys::FilesystemOperator::read_file_str(std::string path) { 
    
 }

byte* filesys::FilesystemOperator::read_file_bytes(std::string path) { 
    return nullptr; 
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
