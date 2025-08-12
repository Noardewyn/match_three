#include "renderer.h"
#include "visuals.h"

BoardLayout Renderer::ComputeLayout(int window_w, int window_h, int gap_px) const
{
    const int cols = Board::kWidth;
    const int rows = Board::kHeight;

    const int max_board_w = window_w - 20;
    const int max_board_h = window_h - 20;

    const int cell_w = (max_board_w - gap_px * (cols - 1)) / cols;
    const int cell_h = (max_board_h - gap_px * (rows - 1)) / rows;
    const int cell_size = std::max(8, std::min(cell_w, cell_h));

    const int width_px = cell_size * cols + gap_px * (cols - 1);
    const int height_px = cell_size * rows + gap_px * (rows - 1);

    const int origin_x = (window_w - width_px) / 2;
    const int origin_y = (window_h - height_px) / 2;

    BoardLayout layout;
    layout.origin_x = origin_x;
    layout.origin_y = origin_y;
    layout.cell_size = cell_size;
    layout.gap = gap_px;
    layout.width_px = width_px;
    layout.height_px = height_px;
    return layout;
}

void Renderer::SetColorForCell(CellType type, uint8_t alpha) const
{
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    switch (type)
    {
        case CellType::Red:     SDL_SetRenderDrawColor(r_, 230, 68, 68, alpha); break;
        case CellType::Green:   SDL_SetRenderDrawColor(r_, 80, 200, 120, alpha); break;
        case CellType::Blue:    SDL_SetRenderDrawColor(r_, 77, 148, 255, alpha); break;
        case CellType::Yellow:  SDL_SetRenderDrawColor(r_, 245, 211, 66, alpha); break;
        case CellType::Purple:  SDL_SetRenderDrawColor(r_, 170, 110, 255, alpha); break;
        case CellType::Orange:  SDL_SetRenderDrawColor(r_, 255, 160, 80, alpha); break;
        default:                SDL_SetRenderDrawColor(r_, 200, 200, 200, alpha); break;
    }
}

void Renderer::DrawBackground(const BoardLayout & layout) const
{
    SDL_SetRenderDrawColor(r_, 22, 10, 40, 255);
    SDL_RenderClear(r_);

    SDL_Rect bg { layout.origin_x - 8, layout.origin_y - 8, layout.width_px + 16, layout.height_px + 16 };
    SDL_SetRenderDrawColor(r_, 40, 20, 70, 255);
    SDL_RenderFillRect(r_, &bg);

    // Optional grid shadows
    SDL_SetRenderDrawColor(r_, 15, 5, 35, 255);

    for (int y = 0; y < Board::kHeight; ++y)
    {
        for (int x = 0; x < Board::kWidth; ++x)
        {
            const int px = layout.origin_x + x * (layout.cell_size + layout.gap);
            const int py = layout.origin_y + y * (layout.cell_size + layout.gap);
            SDL_Rect rect { px, py, layout.cell_size, layout.cell_size };
            SDL_RenderDrawRect(r_, &rect);
        }
    }
}

void Renderer::DrawTiles(const std::vector<VisualTile> & tiles, const BoardLayout & layout) const
{
    for (const auto & t : tiles)
    {
        // Center-based scaling
        const float cx = t.x + layout.cell_size * 0.5f;
        const float cy = t.y + layout.cell_size * 0.5f;
        const int w = static_cast<int>(layout.cell_size * t.sx);
        const int h = static_cast<int>(layout.cell_size * t.sy);
        const int px = static_cast<int>(cx - w * 0.5f);
        const int py = static_cast<int>(cy - h * 0.5f);

        SDL_Rect rect { px, py, w, h };
        const uint8_t a = static_cast<uint8_t>(std::clamp(t.alpha, 0.0f, 1.0f) * 255.0f);
        SetColorForCell(t.type, a);
        SDL_RenderFillRect(r_, &rect);

        SDL_SetRenderDrawColor(r_, 15, 5, 35, a);
        SDL_RenderDrawRect(r_, &rect);
    }

    SDL_RenderPresent(r_);
}
