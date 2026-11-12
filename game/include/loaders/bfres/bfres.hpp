#pragma once
#include "core/file.hpp"
#include "engine.hpp"

class SARCFileOperator : public filesys::FileOperator
{
public:
    void* onFileBytes(byte* data, int dataSize);

    inline static std::unique_ptr<SARCFileOperator> initialize(Engine& e)
    { 
        var ptr = std::make_unique<SARCFileOperator>();
        ptr->enginePtr = &e;
        ptr->extensions = {".szs"};
        ptr->operationType = filesys::Binary;
        return ptr;
    }
};

class BFRESFileOperator : public filesys::FileOperator
{
public:
    void* onFileBytes(byte* data, int dataSize);
    inline static std::unique_ptr<BFRESFileOperator> initialize(Engine& e)
    {
        var ptr = std::make_unique<BFRESFileOperator>();
        ptr->enginePtr = &e;
        ptr->extensions = {".bfres"};
        ptr->operationType = filesys::Binary;
        return ptr;
    }
};
