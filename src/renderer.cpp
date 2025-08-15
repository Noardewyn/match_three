#include "renderer.h"

#include <algorithm>
#include <cmath>

Renderer::Renderer(SDL_Renderer * r)
    : r_(r)
{
    font_ = TTF_OpenFont("assets/fonts/Inter-Regular.ttf", 150);
}

Renderer::~Renderer()
{
    if (font_)
    {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
}

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
        case CellType::Red:
        {
            SDL_SetRenderDrawColor(r_, 230, 68, 68, alpha);
            break;
        }
        case CellType::Green:
        {
            SDL_SetRenderDrawColor(r_, 80, 200, 120, alpha);
            break;
        }
        case CellType::Blue:
        {
            SDL_SetRenderDrawColor(r_, 77, 148, 255, alpha);
            break;
        }
        case CellType::Yellow:
        {
            SDL_SetRenderDrawColor(r_, 245, 211, 66, alpha);
            break;
        }
        case CellType::Purple:
        {
            SDL_SetRenderDrawColor(r_, 170, 110, 255, alpha);
            break;
        }
        case CellType::Orange:
        {
            SDL_SetRenderDrawColor(r_, 255, 160, 80, alpha);
            break;
        }
        default:
        {
            SDL_SetRenderDrawColor(r_, 200, 200, 200, alpha);
            break;
        }
    }
}

void Renderer::DrawBackground(const BoardLayout & layout) const
{
    SDL_SetRenderDrawColor(r_, 22, 10, 40, 255);
    SDL_RenderClear(r_);

    SDL_Rect bg { layout.origin_x - 8, layout.origin_y - 8, layout.width_px + 16, layout.height_px + 16 };
    SDL_SetRenderDrawColor(r_, 40, 20, 70, 255);
    SDL_RenderFillRect(r_, &bg);

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

void Renderer::DrawHighlightCell(const BoardLayout & layout,
                                 const IVec2 & cell,
                                 bool is_primary,
                                 float pulse_t) const
{
    if (cell.x < 0 || cell.y < 0 || cell.x >= Board::kWidth || cell.y >= Board::kHeight)
    {
        return;
    }

    const int base_x = layout.origin_x + cell.x * (layout.cell_size + layout.gap);
    const int base_y = layout.origin_y + cell.y * (layout.cell_size + layout.gap);

    const float freq = 2.0f;
    const float phase = std::sin(2.0f * 3.14159265f * freq * pulse_t);
    const float scale = 1.1f + 0.10f * phase;

    const int w = static_cast<int>(layout.cell_size * scale);
    const int h = static_cast<int>(layout.cell_size * scale);
    const int cx = base_x + layout.cell_size / 2;
    const int cy = base_y + layout.cell_size / 2;

    SDL_Rect r { cx - w / 2, cy - h / 2, w, h };

    // Semi-transparent fill (different tint for primary / secondary).
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    if (is_primary)
    {
        SDL_SetRenderDrawColor(r_, 255, 255, 255, 60);   // warm
    }
    else
    {
        SDL_SetRenderDrawColor(r_, 255, 255, 0, 60);   // cool
    }
    SDL_RenderFillRect(r_, &r);

    // Thick border: 5 px primary, 4 px secondary.
    const int thick = is_primary ? 5 : 4;
    if (is_primary)
    {
        SDL_SetRenderDrawColor(r_, 255, 255, 255, 200);
    }
    else
    {
        SDL_SetRenderDrawColor(r_, 255, 255, 0, 200);
    }

    for (int i = 0; i < thick; ++i)
    {
        SDL_Rect outline { r.x - i, r.y - i, r.w + i * 2, r.h + i * 2 };
        SDL_RenderDrawRect(r_, &outline);
    }
}

void Renderer::DrawTiles(const std::vector<VisualTile> & tiles,
                         const BoardLayout & layout,
                         const std::optional<IVec2> & primary,
                         const std::optional<IVec2> & secondary,
                         float pulse_t) const
{
    // Draw tiles
    for (const auto & t : tiles)
    {
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

    // Draw highlights on top (avoid double-drawing the same cell).
    if (primary.has_value())
    {
        DrawHighlightCell(layout, *primary, true, pulse_t);
    }

    if (secondary.has_value())
    {
        if (!primary.has_value() || !(secondary->x == primary->x && secondary->y == primary->y))
        {
            DrawHighlightCell(layout, *secondary, false, pulse_t);
        }
    }
}

void Renderer::DrawScore(int score) const
{
    if (!font_) return;

    const std::string text = "Score: " + std::to_string(score);
    SDL_Color color{255, 255, 255, 255};
    SDL_Surface * surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surf) return;
    SDL_Texture * tex = SDL_CreateTextureFromSurface(r_, surf);
    int w = surf->w;
    int h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;

    const int padding = 20;
    SDL_Rect frame { padding, padding + 150, w + padding * 2, h + padding * 2 };
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 150);
    SDL_RenderFillRect(r_, &frame);
    SDL_SetRenderDrawColor(r_, 255, 255, 255, 255);
    SDL_RenderDrawRect(r_, &frame);

    SDL_Rect dst { frame.x + padding, frame.y + padding, w, h };
    SDL_RenderCopy(r_, tex, nullptr, &dst);

    SDL_DestroyTexture(tex);
}
