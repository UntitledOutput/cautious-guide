#pragma once
#include "core/util.hpp"

#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

namespace ecs
{
    struct Entity
    {
        u32 index = UINT32_MAX;
        u32 generation = 0;
        bool operator==(const Entity&) const = default;
    };

    constexpr Entity null_entity{};

    namespace detail
    {
        inline u32 next_type_id()
        {
            static u32 next = 0;
            return next++;
        }
    } // namespace detail

    template <typename T>
    u32 type_id()
    {
        static const u32 id = detail::next_type_id();
        return id;
    }
    
        class IPool
    {
    public:
        virtual ~IPool() = default;
        virtual void remove(Entity e) = 0;
        virtual bool contains(Entity e) const = 0;
        virtual size_t size() const = 0;
        virtual std::span<const Entity> entities() const = 0;
    };

    template <typename T>
    class Pool final : public IPool
    {
    public:
        template <typename... Args>
        T& emplace(Entity e, Args&&... args);
        // todo: grow sparse to e.index + 1 (filled with none), record dense.size() as the slot,
        // push e and T{args...}, return data.back(). if it's already there, overwrite instead

        bool contains(Entity e) const override;
        // todo: index in range, slot != none, and dense[slot] == e (the generation check lives here)

        T& get(Entity e);
        // todo: data[sparse[e.index]], assert contains(e) in debug

        T* try_get(Entity e) { return contains(e) ? &get(e) : nullptr; }

        void remove(Entity e) override
        {
            if (!contains(e))
                return;

            const u32 slot = sparse[e.index];
            const u32 last = u32(dense.size() - 1);
            if (slot != last)
            {
                dense[slot] = dense[last];
                data[slot] = std::move(data[last]);
                sparse[dense[slot].index] = slot;
            }
            dense.pop_back();
            data.pop_back();
            sparse[e.index] = none;
        }

        size_t size() const override { return dense.size(); }
        std::span<const Entity> entities() const override { return dense; }
        std::span<T> components() { return data; }

    private:
        static constexpr u32 none = UINT32_MAX;

        std::vector<u32> sparse; // entity index -> slot in dense/data
        std::vector<Entity> dense;
        std::vector<T> data;
    };

} // namespace ecs
