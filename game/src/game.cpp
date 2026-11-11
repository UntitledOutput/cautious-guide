#include "game.hpp"

#include <iostream>

void SplatGame::on_init(Engine& engine)
{
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

    engine.renderer().set_material_texture(mat, "uTexture", tex);

    std::cout << "SplatGame initialized" << std::endl;
}

void SplatGame::on_render(Engine& engine, double alpha)
{
    if (engine.frame_count() == 100)
        engine.renderer().screenshot("screenshot.png");

    var drawCall = graphics::DrawCall{};
    drawCall.mesh = mesh;
    drawCall.material = mat;

    engine.renderer().submit_draw_call(drawCall);
}
