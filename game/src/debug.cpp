// debug.cpp
#include "debug/debug.hpp"
#include <SDL.h>
#include <algorithm>

math::quat FreeCam::rotation() const { return math::from_euler({pitch, yaw, 0.0f}); }

void FreeCam::update(Engine& engine, float dt)
{
    var& in = engine.input();

    // hold right mouse to look, so the cursor is free otherwise
    bool wantLook = in.mouse_down(SDL_BUTTON_RIGHT);
    if (wantLook != looking)
    {
        in.set_mouse_captured(wantLook);
        looking = wantLook;
    }

    if (looking)
    {
        yaw -= in.mouse_delta().x * sensitivity;
        pitch -= in.mouse_delta().y * sensitivity;
        pitch = std::clamp(pitch, -1.55f, 1.55f);
    }

    var rot = rotation();
    var forward = math::rotate(rot, math::vec3{0.0f, 0.0f, -1.0f});
    var right = math::rotate(rot, math::vec3{1.0f, 0.0f, 0.0f});

    var move = math::vec3{};
    if (in.key_down(SDL_SCANCODE_W))
        move += forward;
    if (in.key_down(SDL_SCANCODE_S))
        move -= forward;
    if (in.key_down(SDL_SCANCODE_D))
        move += right;
    if (in.key_down(SDL_SCANCODE_A))
        move -= right;
    if (in.key_down(SDL_SCANCODE_E))
        move.y += 1.0f;
    if (in.key_down(SDL_SCANCODE_Q))
        move.y -= 1.0f;

    if (dot(move, move) > 0.0f)
    {
        float s = speed * (in.key_down(SDL_SCANCODE_LSHIFT) ? 4.0f : 1.0f);
        position += normalize(move) * s * dt;
    }
}

graphics::CameraData FreeCam::camera_data(float aspect) const
{
    var cam = graphics::CameraData{};
    cam.view = math::inverse_affine(math::translate(position) * math::to_mat4(rotation()));
    cam.projection = math::perspective(1.047f, aspect, 0.1f, 500.0f);
    cam.viewProjection = cam.projection * cam.view;
    cam.position = {position.x, position.y, position.z, 1.0f};
    return cam;
}
