#pragma once
#include "board.h"
#include "renderer.h"
#include "input.h"
#include "animation.h"
#include "visuals.h"

#include <SDL.h>

class Game
{
public:
    bool Init();
    void Run();
    void Shutdown();

private:
    SDL_Window * window_ {nullptr};
    SDL_Renderer * sdl_renderer_ {nullptr};

    Board board_;
    Renderer * drawer_ {nullptr};
    InputManager input_;
    AnimationSystem anims_;
    VisualBoard vboard_;

    BoardLayout layout_{};

    enum class Phase
    {
        Idle,
        SwapAnim,
        CheckAfterSwap,
        FadeMatches,
        DropAndSpawn,
        CascadeCheck
    } phase_ { Phase::Idle };

    // For swap/revert
    IVec2 last_swap_a_ { -1, -1 };
    IVec2 last_swap_b_ { -1, -1 };
    uint64_t current_group_ {0};
    std::vector<bool> last_mask_;
    std::vector<Move> last_moves_;
    std::vector<Spawn> last_spawns_;

    bool pending_bump_ {false};

    const float t_swap_ = 0.15f;
    const float t_fade_ = 0.14f;
    const float t_drop_ = 0.20f;
    const float t_bump_ = 0.10f;

    void UpdateLayout();
    void StepStateMachine();
};
