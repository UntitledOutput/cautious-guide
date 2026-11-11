#include "engine.hpp"
#include "core/file.hpp"
#include "graphics/render.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class TextureFileOperator : public filesys::FileOperator
{
public:
    void* onFileBytes(byte* data, int dataSize) override
    {
        int width, height, channels;
        byte* image = stbi_load_from_memory(data, (dataSize), &width, &height, &channels, 0);
        if (!image)
            return nullptr;

        Engine* engine = static_cast<Engine*>(enginePtr);
        graphics::RenderMgr& renderMgr = engine->renderer();

        graphics::TextureDescriptor descriptor{width, height, std::span<byte>(image, size_t(width) * height * channels), GL_RGBA};

        switch (channels)
        {
            case 1: descriptor.format = GL_RED; break;
            case 2: descriptor.format = GL_RG; break;
            case 3: descriptor.format = GL_RGB; break;
            case 4: descriptor.format = GL_RGBA; break;
        }

        var texture = renderMgr.create_texture(descriptor);
        stbi_image_free(image);

        // caller owns this
        return new graphics::TextureHandle(texture);
    }
};

void Engine::setup_file_operators()
{
    var op = std::make_unique<TextureFileOperator>();
    op->enginePtr = this;
    op->extensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
    op->operationType = filesys::Binary;

    _filesystem->register_file_operator(std::move(op));
}
