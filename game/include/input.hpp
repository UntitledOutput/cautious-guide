#include "core/input.hpp"

enum class Action : u8 { Shoot, Jump, Swim, Sub, Special, Map, Count };

struct Binding
{
    enum class Source : u8 { Key, Mouse, PadButton, PadTrigger } source;
    u16 code;
};

class ActionMap
{
public:
    void bind(Action a, Binding b);
    void update(const input::Input& in);       // once per frame, after events

    bool down(Action a) const;
    bool pressed(Action a) const;       // latched until the next tick consumes it
    void consume_tick();                // call after each fixed update

    math::vec2 move(const input::Input& in) const; // WASD or left stick, normalized

private:
    std::array<std::vector<Binding>, size_t(Action::Count)> bindings;
    std::array<bool, size_t(Action::Count)> state{}, latched{};
};

struct PlayerCommand
{
    u32 tick;
    math::vec2 move;    // -1..1
    float yaw, pitch;   // absolute aim, not deltas
    u8 buttons;         // bitmask of Action
};