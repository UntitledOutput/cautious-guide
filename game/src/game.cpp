#include "game.hpp"


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

    mat = engine.renderer().create_material()
}

void SplatGame::on_render(Engine& engine, double alpha)
{

}
