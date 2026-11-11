

#include "engine.hpp"
#include <SDL.h>

int main(int argc, char* argv[])
{
    try
    {
        Engine engine({.window = {.title = "Splat1"}});
        GameCtrl game;
        return engine.start(game);
    }
    catch (const std::exception& e)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);

        return 1;
    }
}

