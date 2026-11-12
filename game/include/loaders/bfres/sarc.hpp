#pragma once
#include <span>

#include "core/util.hpp"
#include <vector>

struct SarcFile
{
    std::string name;
    std::span<const byte> data;
};