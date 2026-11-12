#pragma once
#include <cassert>

#include "core/util.hpp"

#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

namespace scene
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
        T& emplace(Entity e, Args&&... args)
        {
            if (contains(e))
                return data[sparse[e.index]] = T(std::forward<Args>(args)...);

            sparse.resize(std::max(sparse.size(), size_t(e.index + 1)), none);
            sparse[e.index] = u32(dense.size());
            dense.push_back(e);

            return data.emplace_back(std::forward<Args>(args)...);
        }

        bool contains(Entity e) const override
        {
            return e.index < sparse.size() && sparse[e.index] != none && dense[sparse[e.index]] == e;
        }


        T& get(Entity e) { 
            assert(contains(e));
            return data[sparse[e.index]];
        }


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

        template <typename... Ts>
    class View;

    class World
    {
    public:
        Entity create();

        void destroy(Entity e);
        bool alive(Entity e) const;

        template <typename T, typename... Args>
        T& add(Entity e, Args&&... args)
        {
            return pool<T>().emplace(e, std::forward<Args>(args)...);
        }

        template <typename T>
        void remove(Entity e)
        {
            pool<T>().remove(e);
        }

        template <typename T>
        bool contains(Entity e)
        {
            return pool<T>().contains(e);
        }

        template <typename T>
        T& get(Entity e)
        {
            return pool<T>().get(e);
        }

        template <typename T>
        T* try_get(Entity e)
        {
            return pool<T>().try_get(e);
        }

        template <typename... Ts>
        View<Ts...> view()
        {
            return View<Ts...>(&pool<Ts>()...);
        }

    private:
        template <typename T>
        Pool<T>& pool()
        {
            const u32 id = type_id<std::remove_cvref_t<T>>();
            if (id >= pools.size())
                pools.resize(id + 1);
            if (!pools[id])
                pools[id] = std::make_unique<Pool<std::remove_cvref_t<T>>>();
            return static_cast<Pool<std::remove_cvref_t<T>>&>(*pools[id]);
        }

        std::vector<u32> generations;
        std::vector<u32> freeList;
        std::vector<std::unique_ptr<IPool>> pools;
    };
    template <typename... Ts>
    class View
    {
    public:
        explicit View(Pool<Ts>*... p) : pools{p...} {}

        template <typename Fn>
        void each(Fn&& fn)
        {
            const IPool* smallest = nullptr;
            ((smallest = (!smallest || std::get<Pool<Ts>*>(pools)->size() < smallest->size())
                  ? std::get<Pool<Ts>*>(pools)
                  : smallest),
             ...);

            for (Entity e : smallest->entities())
                if ((std::get<Pool<Ts>*>(pools)->contains(e) && ...))
                    fn(e, std::get<Pool<Ts>*>(pools)->get(e)...);
        }

    private:
        std::tuple<Pool<Ts>*...> pools;
    };

} // namespace ecs
