#pragma once
#include "core/util.hpp"
#include "math/vector.hpp"

#include <array>
#include <vector>

union SDL_Event;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

namespace input
{
    class Input
    {
    public:

        Input() = default;
        ~Input();
        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;

        void set_mouse_captured(bool captured);

        void begin_frame();                 // clear per-frame edges and deltas
        void handle_event(const SDL_Event& e);

        bool key_down(u16 scancode)  ret(keys[scancode]);
        bool mouse_down(u8 button)  ret(mouseButtons[button]);
        math::vec2 mouse_delta()  ret(mouseDelta);

        bool pad_down(u8 button) const;
        float pad_axis(u8 axis) const;      // deadzoned, -1..1
        math::vec3 gyro()  ret(gyroRate); // rad/s

    private:

        void open_pad(int deviceIndex);
        void close_pad();

        std::array<bool, 512> keys{};
        std::array<bool, 8> mouseButtons{};
        math::vec2 mouseDelta{};

        SDL_GameController* pad = nullptr;
        math::vec3 gyroRate{};
        math::vec3 gyroSum{};
        u32 gyroSamples = 0;
    };

    using ActionId = u16;
        
    constexpr ActionId invalid_action = 0xFFFF;


    enum class ActionKind : u8
    {
        Button,
        Axis,
        Axis2D
    };

    struct Binding
    {
        enum class Source : u8
        {
            Key,
            MouseButton,
            PadButton,
            PadAxis
        } source;
        u16 code;
        float scale = 1.0f; // -1 flips direction
        u8 component = 0; // for Axis2D: 0 feeds x, 1 feeds y
    };

    class ActionMap
    {
    public:
        ActionId add(std::string name, ActionKind kind);
        ActionId find(std::string_view name) const;
        void bind(ActionId id, Binding b);
        void clear_bindings(ActionId id);

        void update(const Input& in);
        void consume_tick();

        bool down(ActionId id)  ret(actions[id].down);
        bool pressed(ActionId id)  ret(actions[id].latched);
        float axis(ActionId id)  ret(actions[id].value.x);
        math::vec2 axis2(ActionId id)  ret(actions[id].value);

        u16 count() ret(u16(actions.size()));

    private:
        struct Entry
        {
            std::string name;
            ActionKind kind;
            std::vector<Binding> bindings;
            math::vec2 value{};
            bool down = false, latched = false;
        };

        std::vector<Entry> actions;
    };

}