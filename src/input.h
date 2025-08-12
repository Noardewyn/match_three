#pragma once
#include "renderer.h"

#include <SDL.h>
#include <optional>

struct SwapRequest
{
    IVec2 a;
    IVec2 b;
};

class InputManager
{
public:
    std::optional<SwapRequest> HandleEvent(const SDL_Event & e, const BoardLayout & layout);

    void EnableMouse(bool enable)
    {
        mouse_enabled_ = enable;
    }

    // Currently selected (pressed) cell, if any.
    std::optional<IVec2> SelectedCell() const;

    // Neighbor cell we intend to swap with while holding drag (dominant axis).
    // Returns std::nullopt if no valid direction yet.
    std::optional<IVec2> PotentialTargetCell(const BoardLayout & layout) const;

private:
    // Touch state
    bool touch_active_ { false };
    SDL_FingerID finger_id_ { 0 };
    IVec2 touch_start_cell_ { -1, -1 };
    float touch_start_x_ { 0.0f }; // normalized 0..1
    float touch_start_y_ { 0.0f }; // normalized 0..1
    float touch_curr_x_ { 0.0f };  // normalized 0..1
    float touch_curr_y_ { 0.0f };  // normalized 0..1

    // Mouse state
    bool mouse_enabled_ { true };
    bool mouse_down_ { false };
    IVec2 mouse_start_cell_ { -1, -1 };
    int mouse_start_x_ { 0 };
    int mouse_start_y_ { 0 };
    int mouse_curr_x_ { 0 };
    int mouse_curr_y_ { 0 };

    static std::optional<IVec2> ScreenToCell(int sx, int sy, const BoardLayout & layout);
};
