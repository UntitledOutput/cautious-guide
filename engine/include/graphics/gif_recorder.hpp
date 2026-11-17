#pragma once

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace graphics
{
    
    
    class GifRecorder
    {
    public:
        GifRecorder(int width = 1280/2, int height = 720/2, int fps = 25, double seconds = 5.0);
        ~GifRecorder();
        GifRecorder(const GifRecorder&) = delete;
        GifRecorder& operator=(const GifRecorder&) = delete;

        
        
        void capture(double dt, int srcW, int srcH);
        void capture_frame(int srcW, int srcH);

        
        void save(std::string path);

        bool enabled = true;

    private:
        void ensure_gl();

        int outW, outH, delayCs;
        double interval, acc = 0.0;

        unsigned int fbo = 0, tex = 0;

        std::vector<std::vector<std::uint8_t>> ring;
        size_t head = 0, count = 0;
        std::vector<std::uint8_t> scratch;

        std::vector<std::jthread> jobs;
    };
}
