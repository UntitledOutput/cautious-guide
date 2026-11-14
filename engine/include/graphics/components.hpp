#pragma once
#include "model.hpp"
#include "render.hpp"

struct ModelRenderer
{
    graphics::ModelHandle model = graphics::invalid_model;
    std::vector<graphics::MaterialHandle> overrides; // per material slot, invalid_material keeps the default
    bool visible = true;
};

struct NodeTRS
{
    math::vec3 t;
    math::quat r;
    math::vec3 s;
};

struct Pose
{
    std::vector<NodeTRS> local;
    std::vector<math::mat4> nodeWorld;
};

struct Animator
{
    graphics::ModelHandle animationModel = graphics::invalid_model;
    u32 clip = 0;
    float time = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    bool playing = true;
};
