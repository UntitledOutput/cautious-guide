#pragma once
#include "model.hpp"

enum PrimitiveType
{
    Cube,
    Sphere,
    Capsule,
    Cylinder,
    Torus,
    Plane
};

void setup_primitives(Engine& engine);

graphics::ModelHandle get_primitive(PrimitiveType primitive, Engine& engine);
