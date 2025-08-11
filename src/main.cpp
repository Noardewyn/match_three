#include "game.h"

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    Game g;
    if (!g.Init())
    {
        return 1;
    }

    g.Run();
    g.Shutdown();
    return 0;
}

