#include "input.h"
#include <cmath>
#include <utility>

// Decide cardinal direction by dominant axis.
static IVec2 DirectionFromDelta(float dx, float dy)
{
    if (std::abs(dx) > std::abs(dy))
    {
        return IVec2 { dx > 0 ? 1 : -1, 0 };
    }
    else
    {
        return IVec2 { 0, dy > 0 ? 1 : -1 };
    }
}

// Helper: robustly get window pixel size that matches renderer output (handles HiDPI).
static std::pair<int, int> GetWindowPixelSizeFromEventWindow(Uint32 window_id)
{
    int w = 0, h = 0;
    SDL_Window * win = SDL_GetWindowFromID(window_id);
    if (win)
    {
        if (SDL_Renderer * r = SDL_GetRenderer(win))
        {
            SDL_GetRendererOutputSize(r, &w, &h);
        }
        else
        {
            // Fallback to logical window size if renderer is not found.
            SDL_GetWindowSize(win, &w, &h);
        }
    }
    return { w, h };
}

std::optional<IVec2> InputManager::ScreenToCell(int sx, int sy, const BoardLayout & layout)
{
    const int local_x = sx - layout.origin_x;
    const int local_y = sy - layout.origin_y;

    if (local_x < 0 || local_y < 0 || local_x >= layout.width_px || local_y >= layout.height_px)
    {
        return std::nullopt;
    }

    const int stride = layout.cell_size + layout.gap;
    const int cx = local_x / stride;
    const int cy = local_y / stride;

    const int cell_px_x = cx * stride;
    const int cell_px_y = cy * stride;

    // Ensure we did not hit the gap area.
    if (local_x - cell_px_x >= layout.cell_size || local_y - cell_px_y >= layout.cell_size)
    {
        return std::nullopt;
    }

    return IVec2 { cx, cy };
}

std::optional<SwapRequest> InputManager::HandleEvent(const SDL_Event & e, const BoardLayout & layout)
{
    const float threshold = std::max(12.0f, layout.cell_size * 0.35f);

    switch (e.type)
    {
        case SDL_FINGERDOWN:
        {
            if (touch_active_)
            {
                break;
            }

            auto [out_w, out_h] = GetWindowPixelSizeFromEventWindow(e.tfinger.windowID);
            if (out_w <= 0 || out_h <= 0)
            {
                break; // cannot map coordinates safely
            }

            const int sx = static_cast<int>(e.tfinger.x * static_cast<float>(out_w));
            const int sy = static_cast<int>(e.tfinger.y * static_cast<float>(out_h));

            auto cell = ScreenToCell(sx, sy, layout);
            if (!cell.has_value())
            {
                break;
            }

            touch_active_ = true;
            finger_id_ = e.tfinger.fingerId;
            touch_start_cell_ = *cell;
            touch_start_x_ = e.tfinger.x;
            touch_start_y_ = e.tfinger.y;
            break;
        }
        case SDL_FINGERUP:
        {
            if (!touch_active_ || e.tfinger.fingerId != finger_id_)
            {
                break;
            }

            auto [out_w, out_h] = GetWindowPixelSizeFromEventWindow(e.tfinger.windowID);
            if (out_w <= 0 || out_h <= 0)
            {
                touch_active_ = false;
                break;
            }

            const float dx = (e.tfinger.x - touch_start_x_) * static_cast<float>(out_w);
            const float dy = (e.tfinger.y - touch_start_y_) * static_cast<float>(out_h);

            if (std::hypot(dx, dy) >= threshold)
            {
                const IVec2 dir = DirectionFromDelta(dx, dy);
                const IVec2 b { touch_start_cell_.x + dir.x, touch_start_cell_.y + dir.y };
                const IVec2 a = touch_start_cell_;
                touch_active_ = false;
                return SwapRequest { a, b };
            }

            touch_active_ = false;
            break;
        }
        default:
            break;
    }

    if (!mouse_enabled_)
    {
        return std::nullopt;
    }

    switch (e.type)
    {
        case SDL_MOUSEBUTTONDOWN:
        {
            if (e.button.button == SDL_BUTTON_LEFT)
            {
                auto cell = ScreenToCell(e.button.x, e.button.y, layout);
                if (cell.has_value())
                {
                    mouse_down_ = true;
                    mouse_start_cell_ = *cell;
                    mouse_start_x_ = e.button.x;
                    mouse_start_y_ = e.button.y;
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:
        {
            if (e.button.button == SDL_BUTTON_LEFT && mouse_down_)
            {
                const float dx = static_cast<float>(e.button.x - mouse_start_x_);
                const float dy = static_cast<float>(e.button.y - mouse_start_y_);
                mouse_down_ = false;

                if (std::hypot(dx, dy) >= threshold)
                {
                    const IVec2 dir = DirectionFromDelta(dx, dy);
                    const IVec2 b { mouse_start_cell_.x + dir.x, mouse_start_cell_.y + dir.y };
                    const IVec2 a = mouse_start_cell_;
                    return SwapRequest { a, b };
                }
            }
            break;
        }
        default:
            break;
    }

    return std::nullopt;
}
