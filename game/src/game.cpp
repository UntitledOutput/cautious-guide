#include "game.hpp"

#include <iostream>

#include "input.hpp"
#include "graphics/model.hpp"
#include "loaders/bfres/bfres.hpp"
#include "loaders/bfres/sarc.hpp"
#include "scene/render_system.hpp"
#include "scene/transform_system.hpp"

void SplatGame::on_init(Engine& engine)
{
    SplatActions actions;
    actions.register_with(engine.actions());
    engine.filesystem().register_file_operator( SARCFileOperator::initialize(engine) );

    var shader_descriptor = graphics::ShaderDescriptor{};
    shader_descriptor.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
    shader_descriptor.fragmentSource = engine.filesystem().read_file_str("assets/shaders/basic.frag");

    shader = engine.renderer().create_shader(shader_descriptor);

    engine.models().set_material_resolver(
        [&](const graphics::MaterialData& md, std::span<const graphics::TextureHandle> textures)
        {
            var& r = engine.renderer();
            var h = r.create_material(shader);
            for (const auto& [name, value] : md.params)
                std::visit([&](const auto& v) { r.set_material_param(h, name, v); }, value);
            for (const auto& [sampler, index] : md.textures)
                r.set_material_texture(h, sampler, textures[index]);
            return h;
        });

    var sarc = (SarcFile*)engine.filesystem().read_file_auto("assets/Npc_Judge.szs");

    model = engine.models().load("assets/models/RiggedSimple.glb");
    var e = scene::spawn_model(engine.world(), model);
    engine.world().add<Animator>(e);
    engine.world().add<Pose>(e);

    _freecam = FreeCam{};

    _freecam.position = {0, 0, 5};

    std::cout << "SplatGame initialized" << std::endl;
}



void SplatGame::on_update(Engine& engine, double dt)
{
    if (engine.input().key_down(SDL_SCANCODE_F1) && !f1Held)
        _freecam.enabled = !_freecam.enabled;
    f1Held = engine.input().key_down(SDL_SCANCODE_F1);

    if (_freecam.enabled)
        _freecam.update(engine, float(dt));
}

void SplatGame::on_fixed_update(Engine& engine, double)
{
    var& actions = engine.actions();

    commandBuffer.apply(engine.world());
}

void SplatGame::on_render(Engine& engine, double alpha)
{
    var& win = engine.window();
    float aspect = win.framebuffer_height() > 0 ? float(win.framebuffer_width()) / float(win.framebuffer_height()): 1.0f; // minimized

    engine.renderer().set_camera(_freecam.camera_data(aspect));

        scene::run_animators(engine.world(), engine.models(), float(alpha));
    scene::update_world_transforms(engine.world(), engine.models());

    scene::render_models(engine.world(), engine.models(), engine.renderer());

}
