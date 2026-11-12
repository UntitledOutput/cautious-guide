#include "scene/render_system.hpp"

#include <algorithm>

#include "core/components/transform.hpp"

namespace
{
    math::vec4 sample(const graphics::AnimationData::Channel& c, float t)
    {
        using graphics::AnimInterp;
        const auto& times = c.times;
        const bool cubic = c.interp == AnimInterp::CubicSpline;
        auto value = [&](size_t k) { return c.values[cubic ? k * 3 + 1 : k]; };

        if (t <= times.front())
            return value(0);
        if (t >= times.back())
            return value(times.size() - 1);

        const size_t i = size_t(std::upper_bound(times.begin(), times.end(), t) - times.begin()) - 1;
        if (c.interp == AnimInterp::Step)
            return value(i);

        const float dt = times[i + 1] - times[i];
        const float u = (t - times[i]) / dt;

        if (cubic)
        {
            const math::vec4 p0 = value(i), p1 = value(i + 1);
            const math::vec4 m0 = c.values[i * 3 + 2] * dt;
            const math::vec4 m1 = c.values[(i + 1) * 3] * dt;
            const float u2 = u * u, u3 = u2 * u;
            return p0 * (2 * u3 - 3 * u2 + 1) + m0 * (u3 - 2 * u2 + u) + p1 * (-2 * u3 + 3 * u2) + m1 * (u3 - u2);
        }

        const math::vec4 a = value(i), b = value(i + 1);
        if (c.path == graphics::AnimPath::Rotation)
        {
            const math::quat q = math::slerp({a.x, a.y, a.z, a.w}, {b.x, b.y, b.z, b.w}, u);
            return {q.x, q.y, q.z, q.w};
        }
        return a + (b - a) * u;
    }
} // namespace

namespace scene
{
    void render_models(ecs::World& world, const graphics::ModelCache& models, graphics::RenderMgr& r)
    {
        world.view<WorldTransform, ModelRenderer>().each(
            [&](ecs::Entity e, WorldTransform& wt, ModelRenderer& mr)
            {
                if (!mr.visible || mr.model == graphics::invalid_model)
                    return;

                const Pose* pose = world.try_get<Pose>(e);
                graphics::submit_model(r, models.get(mr.model), wt.matrix, mr.overrides,
                                       pose ? std::span<const math::mat4>(pose->nodeWorld)
                                            : std::span<const math::mat4>{});
            });
    }

    ecs::Entity spawn_model(ecs::World& w, graphics::ModelHandle model, const scene::Transform& t)
    {
        var e = w.create();
        w.add<scene::Transform>(e, t);
        w.add<scene::WorldTransform>(e);
        w.add<ModelRenderer>(e, ModelRenderer{model});
        return e;
    }

    void run_animators(ecs::World& world, const graphics::ModelCache& models, float dt)
    {
        world.view<Animator, ModelRenderer, Pose>().each(
            [&](ecs::Entity, Animator& a, ModelRenderer& mr, Pose& pose)
            {
                if (mr.model == graphics::invalid_model)
                    return;
                const auto& asset = models.get(mr.model);
                if (a.clip >= asset.animations.size())
                    return;
                const auto& clip = asset.animations[a.clip];

                if (a.playing && clip.duration > 0.0f)
                {
                    a.time += dt * a.speed;
                    if (a.loop)
                    {
                        a.time = std::fmod(a.time, clip.duration);
                        if (a.time < 0.0f)
                            a.time += clip.duration;
                    }
                    else
                        a.time = std::clamp(a.time, 0.0f, clip.duration);
                }

                // reset to rest pose so untargeted nodes stay put
                pose.local.resize(asset.nodes.size());
                for (size_t i = 0; i < asset.nodes.size(); ++i)
                    pose.local[i] = {asset.nodes[i].translation, asset.nodes[i].rotation, asset.nodes[i].scale};

                for (const auto& c : clip.channels)
                {
                    const math::vec4 v = sample(c, a.time);
                    auto& n = pose.local[c.node];
                    switch (c.path)
                    {
                        case graphics::AnimPath::Translation:
                            n.t = {v.x, v.y, v.z};
                            break;
                        case graphics::AnimPath::Scale:
                            n.s = {v.x, v.y, v.z};
                            break;
                        case graphics::AnimPath::Rotation:
                            n.r = math::normalize(math::quat{v.x, v.y, v.z, v.w});
                            break;
                    }
                }

                pose.nodeWorld.resize(pose.local.size());
                for (size_t i = 0; i < pose.local.size(); ++i)
                {
                    const auto& n = pose.local[i];
                    const math::mat4 local = math::translate(n.t) * math::to_mat4(n.r) * math::scale(n.s);
                    const int parent = asset.nodes[i].parent;
                    pose.nodeWorld[i] = parent < 0 ? local : pose.nodeWorld[size_t(parent)] * local;
                }
            });
    }}
