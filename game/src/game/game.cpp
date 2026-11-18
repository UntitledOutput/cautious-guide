#include "game/game.hpp"

#include "game/input.hpp"
#include "graphics/model.hpp"
#include "graphics/primitive.hpp"
#include "loaders/bfres/bfres.hpp"
#include "loaders/bfres/sarc.hpp"
#include "states/main_state.hpp"
#include "states/state_base.hpp"
#include "fx/particle.hpp"

static graphics::ShaderHandle defaultShader;

void SplatGame::on_init(Engine& engine)
{
    SplatActions actions;
    actions.register_with(engine.actions());
    actions.default_bindings(engine.actions());
    engine.filesystem().register_file_operator( SARCFileOperator::initialize(engine) );

    enum Layer : u32
    {
        L_World = 1 << 0,
        L_Player = 1 << 1,
        L_Ink = 1 << 2,
        L_Zone = 1 << 3
    };

    engine.physics().events().subscribe(L_Player, L_Zone,
                                        [&](const physics::CollisionEvent& ev)
                                        {
                                            //if (ev.phase == physics::ContactPhase::Begin)
                                                //enter_zone(ev.a, ev.b); // ev.a is always the player here
                                        });

    var shader_descriptor = graphics::ShaderDescriptor{};
    shader_descriptor.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
    shader_descriptor.fragmentSource = engine.filesystem().read_file_str("assets/shaders/basic.frag");
    currentGameState = (std::make_unique<BootState>());

    defaultShader = engine.renderer().create_shader(shader_descriptor);

    engine.models().set_material_resolver(
        [&](const graphics::MaterialData& md, std::span<const graphics::TextureHandle> textures)
        {
            var& r = engine.renderer();
            var h = r.create_material(defaultShader);
            for (const auto& [name, value] : md.params)
                std::visit([&](const auto& v) { r.set_material_param(h, name, v); }, value);
            for (const auto& [sampler, index] : md.textures)
                r.set_material_texture(h, sampler, textures[index]);
            return h;
        });
    
    setup_primitives(engine);

    currentGameState = (std::make_unique<BootState>());

    currentGameState->on_enter(*this, engine);

    var& r = engine.renderer();
    var& fs = engine.filesystem();

    fx::PaintShaders paintShaders;
    paintShaders.bake = r.create_shader(
        {fs.read_file_str("assets/shaders/paint_bake.vert"), fs.read_file_str("assets/shaders/paint_bake.frag")});
    paintShaders.splat = r.create_compute_shader({fs.read_file_str("assets/shaders/paint_splat.comp")});
    paintShaders.extend = r.create_compute_shader({fs.read_file_str("assets/shaders/paint_extend.comp")});
    paint = std::make_unique<fx::PaintMgr>(r, paintShaders);

    fx::ParticleShaderContainer particleShaders;
    particleShaders.emit   = r.create_compute_shader({fs.read_file_str("assets/shaders/particle_emit.comp")});
    particleShaders.update = r.create_compute_shader({fs.read_file_str("assets/shaders/particle_update.comp")});
    particleShaders.render = r.create_shader(
        {fs.read_file_str("assets/shaders/particle.vert"), fs.read_file_str("assets/shaders/particle.frag")});
    particles = std::make_unique<fx::ParticleMgr>(r, particleShaders);

    particles->onHit = [this, &engine](const fx::ParticleHit& h)
    {
        if (h.bounce != 0)               // paint on first impact only
            return;

        var& w = engine.world();         // look it up each call: regen_world() replaces it
        const InkEmitter* ink = w.try_get<InkEmitter>(h.emitter);
        if (!ink)
            return;

        if (const Map* m = w.try_get<Map>(h.entity))
            paint->apply_paint(w.get<fx::PaintableObject>(m->model), h.point, h.normal, ink->color, ink->splatSize);
    };


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
