#pragma once
#include "board.h"

#include <SDL.h>

// Layout parameters describing where and how the board is drawn.
struct BoardLayout
{
    int origin_x {0};   // top-left of board area
    int origin_y {0};
    int cell_size {0};  // size of each square cell in pixels
    int gap {2};        // space between cells in pixels
    int width_px {0};   // total drawn width
    int height_px {0};  // total drawn height
};

class Renderer
{
public:
    explicit Renderer(SDL_Renderer * r) : r_(r) {}

    BoardLayout ComputeLayout(int window_w, int window_h, int gap_px = 4) const;
    void Draw(const Board & board, const BoardLayout & layout) const;

private:
    SDL_Renderer * r_ {nullptr};
    void SetColorForCell(CellType type) const;
};
