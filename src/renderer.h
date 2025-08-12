#pragma once
#include "board.h"

#include <SDL.h>

struct BoardLayout
{
    int origin_x {0};
    int origin_y {0};
    int cell_size {0};
    int gap {2};
    int width_px {0};
    int height_px {0};
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

