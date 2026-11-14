#include "game/game.hpp"

#include "game/input.hpp"
#include "graphics/model.hpp"
#include "graphics/primitive.hpp"
#include "loaders/bfres/bfres.hpp"
#include "loaders/bfres/sarc.hpp"
#include "states/main_state.hpp"
#include "states/state_base.hpp"

void SplatGame::on_init(Engine& engine)
{
    SplatActions actions;
    actions.register_with(engine.actions());
    actions.default_bindings(engine.actions());
    engine.filesystem().register_file_operator( SARCFileOperator::initialize(engine) );

    var shader_descriptor = graphics::ShaderDescriptor{};
    shader_descriptor.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
    shader_descriptor.fragmentSource = engine.filesystem().read_file_str("assets/shaders/basic.frag");
    currentGameState = (std::make_unique<BootState>());

    var shader = engine.renderer().create_shader(shader_descriptor);

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
    
    setup_primitives(engine);

    currentGameState = (std::make_unique<BootState>());

    currentGameState->on_enter(*this, engine);
}



void SplatGame::on_update(Engine& engine, double dt)
{

    if (cachedGameState)
    {
        currentGameState->on_exit(*this, engine);
        currentGameState = std::move(cachedGameState);
        cachedGameState = nullptr;
        commandBuffer.clear();
        currentGameState->on_enter(*this, engine);
    }

    currentGameState->on_update(*this, engine, dt);

    commandBuffer.apply(engine.world());
}

void SplatGame::on_fixed_update(Engine& engine, double d)
{
    currentGameState->on_fixed_update(*this, engine, d);

    commandBuffer.apply(engine.world());
}

void SplatGame::load_new_state(std::unique_ptr<GameState> state)
{
    cachedGameState = std::move(state);
}

void SplatGame::on_render(Engine& engine, double alpha)
{
    var& win = engine.window();

    currentGameState->on_render(*this, engine, alpha);

}
