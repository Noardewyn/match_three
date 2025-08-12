#include "game.h"

#include <chrono>

bool Game::Init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("Match3 (Iteration 2)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               720, 1280, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!window_)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    drawer_ = new Renderer(renderer_);

    const uint32_t seed = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    board_.GenerateInitial(seed);

    UpdateLayout();
    return true;
}

void Game::Run()
{
    bool quit = false;
    while (!quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
            {
                UpdateLayout();
            }
            else
            {
                if (auto req = input_.HandleEvent(e, layout_))
                {
                    // Try to swap and resolve. If no matches – swap is reverted inside.
                    board_.TrySwapAndResolve(req->a, req->b);
                }
            }
        }

        drawer_->Draw(board_, layout_);
    }
}

void Game::Shutdown()
{
    delete drawer_;
    drawer_ = nullptr;

    if (renderer_)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void Game::UpdateLayout()
{
    int w = 0, h = 0;
    if (renderer_)
    {
        SDL_GetRendererOutputSize(renderer_, &w, &h);
    }
    else
    {
        SDL_GetWindowSize(window_, &w, &h);
    }

    layout_ = drawer_->ComputeLayout(w, h, 6);
}
