#pragma once
#include <span>

#include "core/util.hpp"
#include <vector>

struct SarcFile
{
    std::string name;
    std::span<const byte> data;
    void* processedData;
};

struct BfresFile
{
    struct DictionaryNode
    {
        std::string name;
        size_t dataOffset;
    };

    struct Dictionary
    {
        std::vector<DictionaryNode> nodes;
    };

    struct FMDL
};