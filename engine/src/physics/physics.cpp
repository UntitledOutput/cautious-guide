#include "physics/world.hpp"
#include "physics/collider.hpp"

void CollisionEvents::dispatch(const std::vector<CollisionEvent>& events, auto&& layerOf)
{
    for (auto& ev : events)
    {
        uint32_t la = layerOf(ev.a), lb = layerOf(ev.b);
        for (auto& s : subs_)
        {
            if ((s.a & la && s.b & lb) || (s.a & lb && s.b & la))
                s.fn(ev);
        }
    }
}


EntityPairKey EntityPairKey::make(Entity x, Entity y)
{ return x < y ? EntityPairKey{x, y} : EntityPairKey{y, x};}

size_t EntityPairKeyHash::operator()(const EntityPairKey& k) const noexcept
{ return (uint64_t(k.a) << 32 | k.b) * 0x9E3779B97F4A7C15ull; }

void ContactTracker::beginStep()
{
    std::swap(prev_, curr_);
    curr_.clear();
}

void ContactTracker::report(Entity x, Entity y, bool trigger, const math::vec3& p, const math::vec3& n, float d)
{
    auto key = EntityPairKey::make(x, y);
    math::vec3 normal = (key.a == x) ? n : -n;
    curr_[key] = {key.a, key.b, ContactPhase::Stay, trigger, p, normal, d};
}

void ContactTracker::endStep(std::vector<CollisionEvent>& out)
{
    for (auto& [key, ev] : curr_)
    {
        auto e = ev;
        e.phase = prev_.contains(key) ? ContactPhase::Stay : ContactPhase::Begin;
        out.push_back(e);
    }
    for (auto& [key, ev] : prev_)
    {
        if (!curr_.contains(key))
        {
            auto e = ev;
            e.phase = ContactPhase::End;
            out.push_back(e);
        }
    }
}

void ContactTracker::forget(Entity e, std::vector<CollisionEvent>& out)
{
    std::erase_if(curr_,
                  [&](auto& kv)
                  {
                      if (kv.first.a != e && kv.first.b != e)
                          return false;
                      auto ev = kv.second;
                      ev.phase = ContactPhase::End;
                      out.push_back(ev);
                      return true;
                  });
}

void CollisionEvents::subscribe(uint32_t layerA, uint32_t layerB, Handler h)
{
    subs_.push_back({layerA, layerB, std::move(h)});
}
