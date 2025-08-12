#include "game.h"

#include <chrono>

static float NowSeconds()
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    auto dt = clock::now() - t0;
    return std::chrono::duration<float>(dt).count();
}

bool Game::Init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("Match3 (Animations)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               720, 1280, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!window_)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    sdl_renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer_)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    drawer_ = new Renderer(sdl_renderer_);

    const uint32_t seed = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    board_.GenerateInitial(seed);

    UpdateLayout();
    vboard_.BuildFromBoard(board_, layout_);

    return true;
}

void Game::Run()
{
    bool quit = false;

    float prev = NowSeconds();
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
                vboard_.SnapToLayout(layout_);
            }
            else
            {
                // Accept input only when idle and no active animations
                if (phase_ == Phase::Idle && !anims_.HasActive())
                {
                    if (auto req = input_.HandleEvent(e, layout_))
                    {
                        if (board_.InBounds(req->a) && board_.InBounds(req->b) && board_.AreAdjacent(req->a, req->b))
                        {
                            // Start swap: swap logically first.
                            board_.Swap(req->a, req->b);
                            last_swap_a_ = req->a;
                            last_swap_b_ = req->b;

                            current_group_ = vboard_.AnimateSwap(req->a, req->b, layout_, anims_, t_swap_);
                            phase_ = Phase::SwapAnim;
                        }
                    }
                }
            }
        }

        // Update animations
        float now = NowSeconds();
        float dt = now - prev;
        prev = now;
        anims_.Update(dt);

        // When current animation group finishes, progress state machine
        StepStateMachine();

        // Render
        drawer_->DrawBackground(layout_);
        drawer_->DrawTiles(vboard_.Tiles(), layout_);

        SDL_Delay(1);
    }
}

void Game::Shutdown()
{
    delete drawer_;
    drawer_ = nullptr;

    if (sdl_renderer_)
    {
        SDL_DestroyRenderer(sdl_renderer_);
        sdl_renderer_ = nullptr;
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
    if (sdl_renderer_)
    {
        SDL_GetRendererOutputSize(sdl_renderer_, &w, &h);
    }
    else
    {
        SDL_GetWindowSize(window_, &w, &h);
    }

    layout_ = drawer_->ComputeLayout(w, h, 6);
}

void Game::StepStateMachine()
{
    // Progress only when no active animations for the last scheduled group.
    if (current_group_ != 0 && anims_.IsGroupActive(current_group_))
    {
        return;
    }

    switch (phase_)
    {
        case Phase::Idle:
            break;

        case Phase::SwapAnim:
        {
            // Swap animation finished → check matches.
            phase_ = Phase::CheckAfterSwap;
            // fallthrough
        }
        case Phase::CheckAfterSwap:
        {
            if (!board_.FindMatches(last_mask_))
            {
                // No matches: revert swap visually + logically
                board_.Swap(last_swap_a_, last_swap_b_);
                current_group_ = vboard_.AnimateSwap(last_swap_b_, last_swap_a_, layout_, anims_, t_swap_);
                phase_ = Phase::Idle; // After revert we will return to Idle; no cascades.
            }
            else
            {
                // There are matches → fade them
                current_group_ = vboard_.AnimateFadeMask(last_mask_, anims_, t_fade_);
                phase_ = Phase::FadeMatches;
            }
            break;
        }

        case Phase::FadeMatches:
        {
            // Fade done → remove visuals, collapse + spawn with plan, animate.
            vboard_.RemoveByMask(last_mask_);
            last_moves_.clear();
            last_spawns_.clear();
            board_.CollapseAndRefillPlanned(last_mask_, last_moves_, last_spawns_);

            // First animate moves (fall), then spawns in the same group so they end together.
            anims_.BeginGroup();
            const uint64_t g1 = vboard_.AnimateMoves(last_moves_, layout_, anims_, t_drop_);
            const uint64_t g2 = vboard_.AnimateSpawns(last_spawns_, layout_, anims_, t_drop_);
            anims_.EndGroup();
            current_group_ = (g1 != 0) ? g1 : g2;
            phase_ = Phase::DropAndSpawn;
            break;
        }

        case Phase::DropAndSpawn:
        {
            // Dropping + spawning finished → check for cascades.
            phase_ = Phase::CascadeCheck;
            // fallthrough
        }
        case Phase::CascadeCheck:
        {
            if (board_.FindMatches(last_mask_))
            {
                current_group_ = vboard_.AnimateFadeMask(last_mask_, anims_, t_fade_);
                phase_ = Phase::FadeMatches;
            }
            else
            {
                // Done
                phase_ = Phase::Idle;
                current_group_ = 0;
            }
            break;
        }
    }
}
