#include <glad/glad.h>
#include "engine.hpp"
#include "core/file.hpp"
#include "graphics/image.hpp"
#include "graphics/render.hpp"

class TextureFileOperator : public filesys::FileOperator
{
public:
    void* onFileBytes(byte* data, int dataSize) override
    {
        graphics::DecodedImage img;
        if (!graphics::decode_image({data, size_t(dataSize)}, img))
            return nullptr;

        graphics::TextureDescriptor desc{int(img.width), int(img.height), img.rgba, GL_RGBA, true};
        var texture = static_cast<Engine*>(enginePtr)->renderer().create_texture(desc);
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
