#include "core/input.hpp"
#include <SDL.h>

using namespace input;

struct SplatActions
{
    ActionId shoot, jump, swim, sub, special, map, move;

    void register_with(ActionMap& m)
    {
        shoot = m.add("shoot", ActionKind::Button);
        jump = m.add("jump", ActionKind::Button);
        swim = m.add("swim", ActionKind::Button);
        sub = m.add("sub", ActionKind::Button);
        special = m.add("special", ActionKind::Button);
        map = m.add("map", ActionKind::Button);
        move = m.add("move", ActionKind::Axis2D);
    }

    void default_bindings(ActionMap& m)
    {
        using S = Binding::Source;
        m.bind(move, {S::Key, SDL_SCANCODE_D, 1.0f, 0});
        m.bind(move, {S::Key, SDL_SCANCODE_A, -1.0f, 0});
        m.bind(move, {S::Key, SDL_SCANCODE_W, 1.0f, 1});
        m.bind(move, {S::Key, SDL_SCANCODE_S, -1.0f, 1});
        m.bind(move, {S::PadAxis, SDL_CONTROLLER_AXIS_LEFTX, 1.0f, 0});
        m.bind(move, {S::PadAxis, SDL_CONTROLLER_AXIS_LEFTY, -1.0f, 1});

        m.bind(shoot, {S::MouseButton, SDL_BUTTON_LEFT});
        m.bind(shoot, {S::PadAxis, SDL_CONTROLLER_AXIS_TRIGGERRIGHT});
        m.bind(swim, {S::Key, SDL_SCANCODE_LSHIFT});
        m.bind(swim, {S::PadAxis, SDL_CONTROLLER_AXIS_TRIGGERLEFT});
        m.bind(jump, {S::Key, SDL_SCANCODE_SPACE});
        m.bind(jump, {S::PadButton, SDL_CONTROLLER_BUTTON_B});
    }
};
