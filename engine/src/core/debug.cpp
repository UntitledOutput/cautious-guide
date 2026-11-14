#include "core/debug.hpp"

#include <SDL_scancode.h>
#include <imgui.h>
#include <glad/glad.h>

#include "engine.hpp"

static bool isDebugMenuOpen = true;
static bool drawWireframeModels = true;

void updateDebugMenu(Engine& engine, float delta)
{
	if (engine.input().key_down(SDL_SCANCODE_APOSTROPHE))
        isDebugMenuOpen = !isDebugMenuOpen;

	if (isDebugMenuOpen)
	{
        ImGui::SetNextWindowPos(ImVec2(engine.window().framebuffer_width() - 10.0f, 10.0f), ImGuiCond_Always,
                                ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("debug hotmenu", 0,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text(std::format("FPS: {:.2f}", 1.f / delta).c_str());
        ImGui::Text(std::format("Draw Calls: {0}", engine.renderer().draw_call_count()).c_str());
        if (ImGui::Checkbox("Draw Wireframe Models", &drawWireframeModels))
        {
            glPolygonMode(GL_FRONT_AND_BACK, drawWireframeModels ? GL_LINE : GL_FILL);
        }

        ImGui::End();
        ImGui::PopStyleVar();
	}
}
