

#include "engine.hpp"
#include <SDL.h>
#include <game/game.hpp>
#include <csignal>

#if defined(_MSC_VER) && !defined(NDEBUG)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
static void break_on_abort(int)
{
    if (IsDebuggerPresent())
        __debugbreak();
}
#endif


int main(int argc, char* argv[])
{
    #if defined(_MSC_VER) && !defined(NDEBUG)
        std::signal(SIGABRT, break_on_abort);
    #endif


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

