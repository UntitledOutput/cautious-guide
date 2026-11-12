#include "game.hpp"

#include <iostream>

#include "input.hpp"
#include "graphics/model.hpp"
#include "scene/render_system.hpp"
#include "scene/transform_system.hpp"

void SplatGame::on_init(Engine& engine)
{
    SplatActions actions;
    actions.register_with(engine.actions());

    var descriptor = graphics::MeshDescriptor{};
    descriptor.format = VertexFormat::Position | VertexFormat::Normal | VertexFormat::UV0;

    var vertices = std::vector<Vertex>{};
    var indices = std::vector<u32>{};

    auto face = [&](math::vec3 n, math::vec3 a, math::vec3 b, math::vec3 c, math::vec3 d)
    {
        u32 base = u32(vertices.size());
        vertices.push_back({a, n, {0.0f, 0.0f}});
        vertices.push_back({b, n, {1.0f, 0.0f}});
        vertices.push_back({c, n, {1.0f, 1.0f}});
        vertices.push_back({d, n, {0.0f, 1.0f}});
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    };

    constexpr float h = 0.5f;
    face({0, 0, 1}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}); // front
    face({0, 0, -1}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}); // back
    face({1, 0, 0}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}); // right
    face({-1, 0, 0}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}); // left
    face({0, 1, 0}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}); // top
    face({0, -1, 0}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}); // bottom

    descriptor.vertices = vertices;
    descriptor.indices = indices;

	mesh = engine.renderer().create_mesh(descriptor);

    var shader_descriptor = graphics::ShaderDescriptor{};
    shader_descriptor.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
    shader_descriptor.fragmentSource = engine.filesystem().read_file_str("assets/shaders/basic.frag");

    shader = engine.renderer().create_shader(shader_descriptor);

    mat = engine.renderer().create_material(shader);

    std::unique_ptr<graphics::TextureHandle> loaded(
        static_cast<graphics::TextureHandle*>(engine.filesystem().read_file_auto("assets/textures/test.png")));
    if (!loaded)
        throw_error("failed to load assets/textures/test.png");
    tex = *loaded;

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

    model = engine.models().load("assets/models/RiggedSimple.glb");
    var e = scene::spawn_model(engine.world(), model);
    engine.world().add<Animator>(e);
    engine.world().add<Pose>(e);

    engine.renderer().set_material_texture(mat, "uTexture", tex);

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
