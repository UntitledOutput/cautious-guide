#include "graphics/gif_recorder.hpp"

#include <glad/glad.h>
#include <cstdio>
#include <cstring>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <gif.h> 
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace graphics
{
    GifRecorder::GifRecorder(int width, int height, int fps, double seconds)
        : outW(width), outH(height)
    {
        delayCs = fps > 0 ? 100 / fps : 4;
        if (delayCs < 2) delayCs = 2; 
        interval = delayCs / 100.0;

        const size_t frames = size_t(seconds / interval + 0.5);
        const size_t bytes = size_t(outW) * outH * 4;
        ring.resize(frames > 0 ? frames : 1, std::vector<std::uint8_t>(bytes));
        scratch.resize(bytes);
    }

    GifRecorder::~GifRecorder()
    {
        jobs.clear(); 
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (tex) glDeleteTextures(1, &tex);
    }

    void GifRecorder::ensure_gl()
    {
        if (fbo) return;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GifRecorder::save(std::string path)
    {
        if (count == 0)
            return;

        
        std::vector<std::vector<std::uint8_t>> frames;
        frames.reserve(count);
        const size_t start = (head + ring.size() - count) % ring.size();
        for (size_t i = 0; i < count; ++i)
            frames.push_back(ring[(start + i) % ring.size()]);

        
        std::erase_if(jobs, [](std::jthread& t) { return !t.joinable(); });

        jobs.emplace_back([frames = std::move(frames), path = std::move(path),
                           w = outW, h = outH, d = delayCs]
        {
            GifWriter writer{};
            if (!GifBegin(&writer, path.c_str(), uint32_t(w), uint32_t(h), uint32_t(d)))
            {
                std::fprintf(stderr, "[gif] failed to open %s\n", path.c_str());
                return;
            }
            for (const auto& f : frames)
                GifWriteFrame(&writer, f.data(), uint32_t(w), uint32_t(h), uint32_t(d));
            GifEnd(&writer);
            std::fprintf(stderr, "[gif] saved %s (%zu frames)\n", path.c_str(), frames.size());
        });
    }

    void GifRecorder::capture(double dt, int srcW, int srcH)
    {
        if (!enabled)
            return;

        acc += dt;
        if (acc < interval)
            return;
        acc -= interval;
        if (acc > interval) acc = 0.0; 

        capture_frame(srcW, srcH);
    }

    void GifRecorder::capture_frame(int srcW, int srcH)
    {
        if (!enabled || srcW <= 0 || srcH <= 0)
            return;

        ensure_gl();

        
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, outW, outH, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, outW, outH, GL_RGBA, GL_UNSIGNED_BYTE, scratch.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        
        auto& dst = ring[head];
        const size_t row = size_t(outW) * 4;
        for (int y = 0; y < outH; ++y)
            std::memcpy(&dst[size_t(y) * row], &scratch[size_t(outH - 1 - y) * row], row);
        for (size_t i = 3; i < dst.size(); i += 4)
            dst[i] = 255;

        head = (head + 1) % ring.size();
        if (count < ring.size()) ++count;
    }

}
