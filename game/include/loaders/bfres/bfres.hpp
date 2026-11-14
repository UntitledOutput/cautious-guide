#pragma once
#include "core/file.hpp"
#include "engine.hpp"

class SARCFileOperator : public filesys::FileOperator
{
public:
    std::unique_ptr<SarcArchive> loadFilePtr(byte* data, int dataSize);

    void* onFileBytes(byte* data, int dataSize);

    inline static std::unique_ptr<SARCFileOperator> initialize(Engine& e)
    { 
        var ptr = std::make_unique<SARCFileOperator>();
        ptr->enginePtr = &e;
        ptr->extensions = {".szs", ".pack"};
        ptr->operationType = filesys::Binary;
        return ptr;
    }
};
