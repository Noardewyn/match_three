#pragma once
#include "board.h"
#include "renderer.h"
#include "input.h"

#include <SDL.h>

class Game
{
public:
    bool Init();
    void Run();
    void Shutdown();

private:
    SDL_Window * window_ {nullptr};
    SDL_Renderer * renderer_ {nullptr};

    Board board_;
    Renderer * drawer_ {nullptr};
    InputManager input_;

    BoardLayout layout_{};

    void UpdateLayout();
};
