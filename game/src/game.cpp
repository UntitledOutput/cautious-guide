#include "game.hpp"

#include <iostream>

void SplatGame::on_init(Engine& engine)
{
    var descriptor = MeshDescriptor{};
    descriptor.format = VertexFormat::Position;

    var vertices = std::vector<Vertex>{
        {{-0.5f, -0.5f, 0.0f}}, {{0.5f, -0.5f, 0.0f}}, {{0.5f, 0.5f, 0.0f}}, {{-0.5f, 0.5f, 0.0f}}};
    descriptor.vertices = vertices;

    var indices = std::vector<uint32_t>{0, 1, 2, 2, 3, 0};
    descriptor.indices = indices;

	mesh = engine.renderer().create_mesh(descriptor);

    var shader_descriptor = ShaderDescriptor{};
    shader_descriptor.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
    shader_descriptor.fragmentSource = engine.filesystem().read_file_str("assets/shaders/basic.frag");

    shader = engine.renderer().create_shader(shader_descriptor);

    mat = engine.renderer().create_material(shader);

    std::cout << "SplatGame initialized" << std::endl;
}

void SplatGame::on_render(Engine& engine, double alpha)
{
    if (engine.frame_count() == 100)
        engine.renderer().screenshot("screenshot.png");

    var drawCall = DrawCall{};
    drawCall.mesh = mesh;
    drawCall.material = mat;

    engine.renderer().submit_draw_call(drawCall);
}
