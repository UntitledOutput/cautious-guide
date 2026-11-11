#pragma once
#include "core/util.hpp"
#include "math/vector.hpp"
#include <array>

union SDL_Event;

class Input
{
public:
    void begin_frame();                 // clear per-frame edges and deltas
    void handle_event(const SDL_Event& e);

    bool key_down(u16 scancode) const ret(keys[scancode]);
    bool mouse_down(u8 button) const ret(mouseButtons[button]);
    math::vec2 mouse_delta() const ret(mouseDelta);

    bool pad_down(u8 button) const;
    float pad_axis(u8 axis) const;      // deadzoned, -1..1
    math::vec3 gyro() const ret(gyroRate); // rad/s

private:
    std::array<bool, 512> keys{};
    std::array<bool, 8> mouseButtons{};
    math::vec2 mouseDelta{};

    SDL_GameController* pad = nullptr;
    math::vec3 gyroRate{};
};