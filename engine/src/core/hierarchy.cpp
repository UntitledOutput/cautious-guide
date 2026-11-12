#include "core/hierarchy.hpp"
#include "core/components/hierarchy.hpp"

namespace scene
{
    void detach(ecs::World& w, ecs::Entity child)
    {
        auto* ch = w.try_get<scene::Hierarchy>(child);
        if (!ch || ch->parent == ecs::null_entity)
            return;

        if (ch->prevSibling != ecs::null_entity)
            w.get<scene::Hierarchy>(ch->prevSibling).nextSibling = ch->nextSibling;
        else if (auto* ph = w.try_get<scene::Hierarchy>(ch->parent))
            ph->firstChild = ch->nextSibling;

        if (ch->nextSibling != ecs::null_entity)
            w.get<scene::Hierarchy>(ch->nextSibling).prevSibling = ch->prevSibling;

        ch->parent = ch->prevSibling = ch->nextSibling = ecs::null_entity;
        ch->parentNode = UINT32_MAX;
    }

    void attach(ecs::World& w, ecs::Entity child, ecs::Entity parent, u32 node)
    {
        for (ecs::Entity p = parent; p != ecs::null_entity;)
        {
            if (p == child)
                throw_error("attach would create a cycle");
            const scene::Hierarchy* h = w.try_get<scene::Hierarchy>(p);
            p = h ? h->parent : ecs::null_entity;
        }

        detach(w, child);

        // add both before taking references: adding to a pool can reallocate it
        if (!w.contains<scene::Hierarchy>(parent))
            w.add<scene::Hierarchy>(parent);
        if (!w.contains<scene::Hierarchy>(child))
            w.add<scene::Hierarchy>(child);

        auto& ph = w.get<scene::Hierarchy>(parent);
        auto& ch = w.get<scene::Hierarchy>(child);

        ch.parent = parent;
        ch.parentNode = node;
        ch.prevSibling = ecs::null_entity;
        ch.nextSibling = ph.firstChild;
        if (ph.firstChild != ecs::null_entity)
            w.get<scene::Hierarchy>(ph.firstChild).prevSibling = child;
        ph.firstChild = child;
    }

    void destroy_recursive(ecs::World& w, ecs::Entity root)
    {
        detach(w, root);

        std::vector<ecs::Entity> stack{root}, doomed;
        while (!stack.empty())
        {
            ecs::Entity e = stack.back();
            stack.pop_back();
            doomed.push_back(e);

            if (const scene::Hierarchy* h = w.try_get<scene::Hierarchy>(e))
                for (ecs::Entity c = h->firstChild; c != ecs::null_entity; c = w.get<scene::Hierarchy>(c).nextSibling)
                    stack.push_back(c);
        }

        for (ecs::Entity e : doomed)
            w.destroy(e);
    }

}
