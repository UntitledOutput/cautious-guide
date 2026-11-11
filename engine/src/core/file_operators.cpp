#include "engine.hpp"
#include "core/file.hpp"
#include "graphics/render.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class TextureFileOperator : filesys::FileOperator
{
    void* onFileBytes(byte* data) override
    {
        int width, height, channels;
        unsigned char* image = stbi_load_from_memory(data, 0, &width, &height, &channels, 0);
        if (!image) {
            return nullptr;
        }

        Engine* engine = static_cast<Engine*>(enginePtr);
        graphics::RenderMgr& renderMgr = engine->renderer();

        graphics::TextureDescriptor descriptor{width, height, std::span<byte>(data)};

        return image;
    }
};

void Engine::setup_file_operators()
{
    _filesystem->register_file_operator(std::make_unique<TextureFileOperator>{std::vector<std::string>{".png", ".jpg", ".jpeg", ".bmp", ".tga"}, filesys::FileType::Binary,this});
}
