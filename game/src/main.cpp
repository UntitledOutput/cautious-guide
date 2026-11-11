

#include "engine.hpp"
#include <SDL.h>
#include <game.hpp>

int main(int argc, char* argv[])
{
    try
    {
        Engine engine({.window = {.title = "Splat1"}});
        SplatGame game;
        return engine.start(game);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);

        return 1;
    }
}

