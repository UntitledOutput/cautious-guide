#include "core/input.hpp"
#include <SDL.h>
#include <algorithm>

using namespace input;

namespace
{
    constexpr float stick_deadzone = 0.15f;

    float sample(const Input& in, const Binding& b)
    {
        switch (b.source)
        {
            case Binding::Source::Key:
                return in.key_down(b.code) ? 1.0f : 0.0f;
            case Binding::Source::MouseButton:
                return in.mouse_down(u8(b.code)) ? 1.0f : 0.0f;
            case Binding::Source::PadButton:
                return in.pad_down(u8(b.code)) ? 1.0f : 0.0f;
            case Binding::Source::PadAxis:
                return in.pad_axis(u8(b.code));
        }
        return 0.0f;
    }
} // namespace

Input::~Input() { close_pad(); }

void Input::begin_frame()
{
    mouseDelta = {};
    gyroSum = {};
    gyroSamples = 0;
}

void Input::handle_event(const SDL_Event& e)
{
    switch (e.type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (e.key.keysym.scancode < keys.size())
                keys[e.key.keysym.scancode] = e.type == SDL_KEYDOWN;
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (e.button.button < mouseButtons.size())
                mouseButtons[e.button.button] = e.type == SDL_MOUSEBUTTONDOWN;
            break;

        case SDL_MOUSEMOTION:
            mouseDelta.x += float(e.motion.xrel);
            mouseDelta.y += float(e.motion.yrel);
            break;

        case SDL_CONTROLLERDEVICEADDED:
            if (!pad)
                open_pad(e.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)))
                close_pad();
            break;

        case SDL_CONTROLLERSENSORUPDATE:
            if (e.csensor.sensor == SDL_SENSOR_GYRO)
            {
                gyroSum += math::vec3{e.csensor.data[0], e.csensor.data[1], e.csensor.data[2]};
                gyroSamples++;
                gyroRate = gyroSum / float(gyroSamples);
            }
            break;

        case SDL_WINDOWEVENT:
            // alt-tabbing while holding a key would otherwise leave it stuck down
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            {
                keys.fill(false);
                mouseButtons.fill(false);
            }
            break;
    }
}

bool Input::pad_down(u8 button) const
{
    return pad && SDL_GameControllerGetButton(pad, SDL_GameControllerButton(button));
}

float Input::pad_axis(u8 axis) const
{
    if (!pad)
        return 0.0f;

    float v = std::clamp(SDL_GameControllerGetAxis(pad, SDL_GameControllerAxis(axis)) / 32767.0f, -1.0f, 1.0f);
    float mag = std::abs(v);
    if (mag < stick_deadzone)
        return 0.0f;

    // rescale so output starts at 0 right past the deadzone instead of jumping to 0.15
    return std::copysign((mag - stick_deadzone) / (1.0f - stick_deadzone), v);
}

void Input::set_mouse_captured(bool captured) { SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE); }

void Input::open_pad(int deviceIndex)
{
    pad = SDL_GameControllerOpen(deviceIndex);
    if (pad && SDL_GameControllerHasSensor(pad, SDL_SENSOR_GYRO))
        SDL_GameControllerSetSensorEnabled(pad, SDL_SENSOR_GYRO, SDL_TRUE);
}

void Input::close_pad()
{
    if (pad)
        SDL_GameControllerClose(pad);
    pad = nullptr;
    gyroRate = {};
}

ActionId ActionMap::find(std::string_view name) const
{
    for (u16 i = 0; i < actions.size(); ++i)
        if (actions[i].name == name)
            return i;
    return invalid_action;
}

void ActionMap::bind(ActionId id, Binding b) { actions[id].bindings.push_back(b); }

void ActionMap::clear_bindings(ActionId id) { actions[id].bindings.clear(); }

ActionId ActionMap::add(std::string name, ActionKind kind)
{
    actions.push_back({std::move(name), kind});
    return ActionId(actions.size() - 1);
}

void ActionMap::update(const Input& in)
{
    for (Entry& a : actions)
    {
        math::vec2 v{};
        for (const Binding& b : a.bindings)
            (b.component == 0 ? v.x : v.y) += sample(in, b) * b.scale;

        if (a.kind == ActionKind::Axis2D && length(v) > 1.0f)
            v = normalize(v); // diagonal WASD shouldn't be faster

        const bool wasDown = a.down;
        a.value = v;
        a.down = std::abs(v.x) > 0.5f || std::abs(v.y) > 0.5f;
        if (a.down && !wasDown)
            a.latched = true;
    }
}

void ActionMap::consume_tick()
{
    for (Entry& a : actions)
        a.latched = false;
}
