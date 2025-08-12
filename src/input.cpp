#include "input.h"

#include <cmath>

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

    if (local_x - cell_px_x >= layout.cell_size || local_y - cell_px_y >= layout.cell_size)
    {
        return std::nullopt;
    }

    return IVec2 { cx, cy };
}

std::optional<SwapRequest> InputManager::HandleEvent(const SDL_Event & e, const BoardLayout & layout)
{
    const float threshold_px = std::max(12.0f, layout.cell_size * 0.35f);

    switch (e.type)
    {
        case SDL_FINGERDOWN:
        {
            if (touch_active_)
            {
                break;
            }

            int out_w = 0;
            int out_h = 0;
            if (SDL_Renderer * r = SDL_GetRenderer(SDL_GetWindowFromID(e.tfinger.windowID)))
            {
                SDL_GetRendererOutputSize(r, &out_w, &out_h);
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
            touch_curr_x_ = e.tfinger.x;
            touch_curr_y_ = e.tfinger.y;
            break;
        }

        case SDL_FINGERMOTION:
        {
            if (touch_active_ && e.tfinger.fingerId == finger_id_)
            {
                touch_curr_x_ = e.tfinger.x;
                touch_curr_y_ = e.tfinger.y;
            }
            break;
        }

        case SDL_FINGERUP:
        {
            if (!touch_active_ || e.tfinger.fingerId != finger_id_)
            {
                break;
            }

            int out_w = 0;
            int out_h = 0;
            if (SDL_Renderer * r = SDL_GetRenderer(SDL_GetWindowFromID(e.tfinger.windowID)))
            {
                SDL_GetRendererOutputSize(r, &out_w, &out_h);
            }

            const float dx = (e.tfinger.x - touch_start_x_) * static_cast<float>(out_w);
            const float dy = (e.tfinger.y - touch_start_y_) * static_cast<float>(out_h);

            touch_active_ = false;

            if (std::hypot(dx, dy) >= threshold_px)
            {
                const IVec2 dir = DirectionFromDelta(dx, dy);
                const IVec2 b { touch_start_cell_.x + dir.x, touch_start_cell_.y + dir.y };
                const IVec2 a = touch_start_cell_;
                return SwapRequest { a, b };
            }
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
                    mouse_curr_x_ = e.button.x;
                    mouse_curr_y_ = e.button.y;
                }
            }
            break;
        }

        case SDL_MOUSEMOTION:
        {
            if (mouse_down_)
            {
                mouse_curr_x_ = e.motion.x;
                mouse_curr_y_ = e.motion.y;
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

                if (std::hypot(dx, dy) >= threshold_px)
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

std::optional<IVec2> InputManager::SelectedCell() const
{
    if (touch_active_)
    {
        return touch_start_cell_;
    }

    if (mouse_enabled_ && mouse_down_)
    {
        return mouse_start_cell_;
    }

    return std::nullopt;
}

std::optional<IVec2> InputManager::PotentialTargetCell(const BoardLayout & layout) const
{
    // Small threshold, so secondary highlight appears quickly.
    const float threshold_px = std::max(6.0f, layout.cell_size * 0.20f);

    if (touch_active_)
    {
        // Convert normalized to pixels using renderer output size is not available here.
        // We compare deltas in normalized units scaled by an assumed screen height of 1,
        // so we must reconstruct approx pixels via cell size; simpler: use layout width/height.
        // To avoid renderer dependency, approximate using layout cell pixels in both axes:
        const float dx_px = (touch_curr_x_ - touch_start_x_) * static_cast<float>(layout.width_px);
        const float dy_px = (touch_curr_y_ - touch_start_y_) * static_cast<float>(layout.height_px);

        if (std::hypot(dx_px, dy_px) >= threshold_px)
        {
            const IVec2 dir = DirectionFromDelta(dx_px, dy_px);
            const IVec2 b { touch_start_cell_.x + dir.x, touch_start_cell_.y + dir.y };

            if (b.x >= 0 && b.y >= 0 && b.x < Board::kWidth && b.y < Board::kHeight)
            {
                return b;
            }
        }
    }

    if (mouse_enabled_ && mouse_down_)
    {
        const float dx = static_cast<float>(mouse_curr_x_ - mouse_start_x_);
        const float dy = static_cast<float>(mouse_curr_y_ - mouse_start_y_);

        if (std::hypot(dx, dy) >= threshold_px)
        {
            const IVec2 dir = DirectionFromDelta(dx, dy);
            const IVec2 b { mouse_start_cell_.x + dir.x, mouse_start_cell_.y + dir.y };

            if (b.x >= 0 && b.y >= 0 && b.x < Board::kWidth && b.y < Board::kHeight)
            {
                return b;
            }
        }
    }

    return std::nullopt;
}
