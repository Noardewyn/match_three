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

    void EnableMouse(bool enable) { mouse_enabled_ = enable; }

private:
    bool touch_active_ {false};
    SDL_FingerID finger_id_ {0};
    IVec2 touch_start_cell_ {-1, -1};
    float touch_start_x_ {0.0f};
    float touch_start_y_ {0.0f};

    bool mouse_enabled_ {true};
    bool mouse_down_ {false};
    IVec2 mouse_start_cell_ {-1, -1};
    int mouse_start_x_ {0};
    int mouse_start_y_ {0};

    static std::optional<IVec2> ScreenToCell(int sx, int sy, const BoardLayout & layout);
};
