#pragma once
#include "board.h"
#include "visuals.h"

#include <SDL.h>
#include <vector>
#include <optional>

struct BoardLayout
{
    int origin_x { 0 };
    int origin_y { 0 };
    int cell_size { 0 };
    int gap { 2 };
    int width_px { 0 };
    int height_px { 0 };
};

class Renderer
{
public:
    explicit Renderer(SDL_Renderer * r) : r_(r) {}

    BoardLayout ComputeLayout(int window_w, int window_h, int gap_px = 4) const;

    void DrawBackground(const BoardLayout & layout) const;

    // Draw tiles and optional highlights:
    //  - primary: currently pressed cell
    //  - secondary: intended swap neighbor
    void DrawTiles(const std::vector<VisualTile> & tiles,
                   const BoardLayout & layout,
                   const std::optional<IVec2> & primary = std::nullopt,
                   const std::optional<IVec2> & secondary = std::nullopt,
                   float pulse_t = 0.0f) const;

private:
    SDL_Renderer * r_ { nullptr };

    void SetColorForCell(CellType type, uint8_t alpha) const;

    void DrawHighlightCell(const BoardLayout & layout,
                           const IVec2 & cell,
                           bool is_primary,
                           float pulse_t) const;
};
