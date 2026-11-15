#include "core/debug.hpp"

#include <SDL_scancode.h>
#include <imgui.h>

#include "core/components/transform.hpp"
#include "engine.hpp"
#include "graphics/model.hpp"
#include "graphics/primitive.hpp"
#include "physics/world.hpp"

namespace
{
    bool isDebugMenuOpen = true;
    bool showPhysicsDebug = true;
    graphics::MaterialHandle debugMaterial = UINT32_MAX;

    math::mat4 trs(math::vec3 p, math::vec3 s) { return math::translate(p) * math::scale(s); }

    void draw_colliders(Engine& engine)
    {
        engine.world().view<scene::Transform, physics::Collider>().each(
            [&](ecs::Entity, scene::Transform& t, physics::Collider& c)
            {
                const math::vec3 p = t.position + c.offset;
                switch (c.shape)
                {
                    case physics::ShapeType::Sphere:
                        {
                            const float r = c.sphere.radius;
                            debug::draw(engine, PrimitiveType::Sphere, trs(p, {r, r, r}));
                            break;
                        }
                    case physics::ShapeType::Capsule:
                        {
                            const float r = c.capsule.radius, hh = c.capsule.halfHeight;
                            debug::draw(engine, PrimitiveType::Sphere, trs(p + math::vec3{0, hh, 0}, {r, r, r}));
                            debug::draw(engine, PrimitiveType::Sphere, trs(p - math::vec3{0, hh, 0}, {r, r, r}));
                            debug::draw(engine, PrimitiveType::Cylinder, trs(p, {r * 2, hh, r * 2}));
                            break;
                        }
                    case physics::ShapeType::AABB:
                        debug::draw(engine, PrimitiveType::Cube, trs(p, c.box.halfExtents));
                        break;
                    default:
                        break; 
                }
            });
    }
} // namespace

void debug::updateDebugMenu(Engine& engine, float)
{
    if (showPhysicsDebug)
        draw_colliders(engine);

    if (engine.input().key_press(SDL_SCANCODE_APOSTROPHE))
        isDebugMenuOpen = !isDebugMenuOpen;

    if (!isDebugMenuOpen)
        return;

    ImGui::SetNextWindowPos(ImVec2(engine.window().framebuffer_width() - 10.0f, 10.0f), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("debug hotmenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Draw Calls: %d", engine.renderer().draw_call_count());
    ImGui::Checkbox("Show Physics Debug", &showPhysicsDebug);

    ImGui::End();
    ImGui::PopStyleVar();
}

void debug::draw(Engine& engine, PrimitiveType type, math::mat4 matrix)
{
    if (debugMaterial == UINT32_MAX)
    {
        graphics::ShaderDescriptor sd;
        sd.vertexSource = engine.filesystem().read_file_str("assets/shaders/basic.vert");
        sd.fragmentSource = engine.filesystem().read_file_str("assets/shaders/debug.frag");

        var& r = engine.renderer();
        debugMaterial = r.create_material(r.create_shader(sd));
        r.set_material_render_params(debugMaterial,
                                     {.blend = false, .depthWrite = false, .cullBack = false, .wireframe = true});
    }

    const var model = get_primitive(type, engine);
    if (model == graphics::invalid_model)
        return;

    graphics::submit_model(engine.renderer(), engine.models().get(model), matrix,
                           std::span<const graphics::MaterialHandle>{&debugMaterial, 1});
}
