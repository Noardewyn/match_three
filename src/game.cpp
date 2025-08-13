#include "game.h"

#include <chrono>
#include <optional>
#include <SDL_ttf.h>

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

    window_ = SDL_CreateWindow("Match3 (Dual Highlight)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

    if (TTF_Init() != 0)
    {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
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
            else if (e.type == SDL_WINDOWEVENT &&
                     (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
            {
                UpdateLayout();
                vboard_.SnapToLayout(layout_);
            }
            else
            {
                if (phase_ == Phase::Idle && !anims_.HasActive())
                {
                    if (auto req = input_.HandleEvent(e, layout_))
                    {
                        if (board_.InBounds(req->a) && board_.InBounds(req->b) && board_.AreAdjacent(req->a, req->b))
                        {
                            board_.Swap(req->a, req->b);
                            last_swap_a_ = req->a;
                            last_swap_b_ = req->b;
                            current_group_ = vboard_.AnimateSwap(req->a, req->b, layout_, anims_, t_swap_);
                            phase_ = Phase::SwapAnim;
                        }
                    }
                }
                else
                {
                    // Even when not idle, still feed motion events to InputManager.
                    input_.HandleEvent(e, layout_);
                }
            }
        }

        float now = NowSeconds();
        float dt = now - prev;
        prev = now;
        anims_.Update(dt);

        StepStateMachine();

        std::optional<IVec2> primary;
        std::optional<IVec2> secondary;

        if (phase_ == Phase::Idle && !anims_.HasActive())
        {
            primary = input_.SelectedCell();
            secondary = input_.PotentialTargetCell(layout_);
        }

        drawer_->DrawBackground(layout_);
        drawer_->DrawTiles(vboard_.Tiles(), layout_, primary, secondary, now);
        drawer_->DrawScore(score_);
        SDL_RenderPresent(sdl_renderer_);
        SDL_Delay(1);
    }
}

void Game::Shutdown()
{
    delete drawer_;
    drawer_ = nullptr;

    if (sdl_renderer_) { SDL_DestroyRenderer(sdl_renderer_); sdl_renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }

    TTF_Quit();
    SDL_Quit();
}

void Game::UpdateLayout()
{
    int w = 0, h = 0;
    if (sdl_renderer_) SDL_GetRendererOutputSize(sdl_renderer_, &w, &h);
    else SDL_GetWindowSize(window_, &w, &h);

    layout_ = drawer_->ComputeLayout(w, h, 6);
}

void Game::StepStateMachine()
{
    if (current_group_ != 0 && anims_.IsGroupActive(current_group_)) return;

    switch (phase_)
    {
        case Phase::Idle:
            break;

        case Phase::SwapAnim:
        {
            phase_ = Phase::CheckAfterSwap;
            // fallthrough
        }
        case Phase::CheckAfterSwap:
        {
            int groups = 0;
            int cells = 0;
            if (!board_.FindMatches(last_mask_, groups, cells))
            {
                // Revert swap
                board_.Swap(last_swap_a_, last_swap_b_);
                current_group_ = vboard_.AnimateSwap(last_swap_b_, last_swap_a_, layout_, anims_, t_swap_);
                phase_ = Phase::Idle;
            }
            else
            {
                score_ += cells * groups;
                // Pulse + Fade together in a single group
                const uint64_t g = anims_.BeginGroup();
                vboard_.AnimatePulseMask(last_mask_, anims_, t_fade_ * 1.0f, 0.7f, g);
                vboard_.AnimateFadeMask(last_mask_, anims_, t_fade_, g);
                anims_.EndGroup();
                current_group_ = g;
                phase_ = Phase::FadeMatches;
            }
            break;
        }

        case Phase::FadeMatches:
        {
            // Remove visuals, collapse logically and animate fall + spawn
            vboard_.RemoveByMask(last_mask_);
            last_moves_.clear();
            last_spawns_.clear();
            board_.CollapseAndRefillPlanned(last_mask_, last_moves_, last_spawns_);

            anims_.BeginGroup();
            vboard_.AnimateMoves(last_moves_, layout_, anims_, t_drop_, anims_.CurrentGroup());
            vboard_.AnimateSpawns(last_spawns_, layout_, anims_, t_drop_, anims_.CurrentGroup());
            anims_.EndGroup();
            current_group_ = anims_.CurrentGroup(); // last BeginGroup id
            pending_bump_ = true;
            phase_ = Phase::DropAndSpawn;
            break;
        }

        case Phase::DropAndSpawn:
        {
            if (pending_bump_)
            {
                // First time we enter here after drop: schedule bump on all landing cells
                std::vector<IVec2> landed;
                landed.reserve(last_moves_.size() + last_spawns_.size());
                for (const auto & m : last_moves_) landed.push_back(m.to);
                for (const auto & s : last_spawns_) landed.push_back(s.to);

                current_group_ = vboard_.AnimateBumpCells(landed, anims_, t_bump_, 1.10f);
                pending_bump_ = false;
                break;
            }

            // Bump finished → go to cascade check
            phase_ = Phase::CascadeCheck;
            // fallthrough
        }

        case Phase::CascadeCheck:
        {
            int groups = 0;
            int cells = 0;
            if (board_.FindMatches(last_mask_, groups, cells))
            {
                score_ += cells * groups;
                const uint64_t g = anims_.BeginGroup();
                vboard_.AnimatePulseMask(last_mask_, anims_, t_fade_ * 1.0f, 0.7f, g);
                vboard_.AnimateFadeMask(last_mask_, anims_, t_fade_, g);
                anims_.EndGroup();
                current_group_ = g;
                phase_ = Phase::FadeMatches;
            }
            else
            {
                phase_ = Phase::Idle;
                current_group_ = 0;
            }
            break;
        }
    }
}
