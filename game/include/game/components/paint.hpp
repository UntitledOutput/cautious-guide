#pragma once

#include "engine.hpp"
#include "core/ecs.hpp"
#include "fx/paint.hpp"

struct InkEmitter
{
    int color = 0;          // index into PaintMgr::paintColors
    float splatSize = 1.5f;
};