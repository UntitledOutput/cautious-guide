#include "graphics/camera.hpp"

#include "engine.hpp"
#include "core/components/transform.hpp"
#include "graphics/model.hpp"

void graphics::update_camera(Engine& engine)
{
    engine.world().view<scene::Transform, Camera>().each(
        [&](ecs::Entity e, scene::Transform& wt, Camera& _cam)
        {
            var position = wt.position;
            var aspect = engine.window().framebuffer_width() / double(engine.window().framebuffer_height());

            var cam = CameraData {};
            cam.view = math::inverse_affine(math::translate(position) * math::to_mat4(wt.rotation));
            cam.projection = math::perspective(math::radians(_cam.fov), aspect,  _cam.z_near, _cam.z_far);
            cam.viewProjection = cam.projection * cam.view;
            cam.position = {position.x, position.y, position.z, 1.0f};

            engine.renderer().set_camera(cam);
        });
}
