#pragma once
#include "types.h"
#include "board.h"
#include "animation.h"

#include <vector>
#include <optional>
#include <SDL.h>

struct BoardLayout;

// Visual representation of a single tile.
struct VisualTile
{
    CellType type {CellType::Red};
    IVec2 cell {0, 0};    // current target cell
    float x {0.0f};       // top-left pixel position
    float y {0.0f};
    float alpha {1.0f};   // 0..1
    float sx {1.0f};      // scale X
    float sy {1.0f};      // scale Y
};

class VisualBoard
{
public:
    void BuildFromBoard(const Board & board, const BoardLayout & layout);

    // Must be called when layout changes (e.g., window resize).
    void SnapToLayout(const BoardLayout & layout);

    // Animations:
    // Swap two neighbor cells visually (tiles at 'a' and 'b').
    uint64_t AnimateSwap(const IVec2 & a, const IVec2 & b, const BoardLayout & layout,
                         AnimationSystem & anims, float seconds, uint64_t group_id = 0);

    // Fade out matched cells by mask; does not remove tiles, only animates alpha.
    uint64_t AnimateFadeMask(const std::vector<bool> & mask, AnimationSystem & anims,
                             float seconds, uint64_t group_id = 0);

    uint64_t AnimatePulseMask(const std::vector<bool> & mask, AnimationSystem & anims,
                              float seconds, float peak_scale = 1.18f, uint64_t group_id = 0);

    // Remove tiles that are true in mask (after fade completed).
    void RemoveByMask(const std::vector<bool> & mask);

    // Animate falling moves (existing tiles moving to new cells).
    uint64_t AnimateMoves(const std::vector<Move> & moves, const BoardLayout & layout,
                          AnimationSystem & anims, float seconds, uint64_t group_id = 0);

    // Spawn new tiles above and animate them down.
    uint64_t AnimateSpawns(const std::vector<Spawn> & spawns, const BoardLayout & layout,
                           AnimationSystem & anims, float seconds, uint64_t group_id = 0);

    // Small bounce after landing (used on all cells that just received a tile).
    uint64_t AnimateBumpCells(const std::vector<IVec2> & cells, AnimationSystem & anims,
                              float seconds = 0.10f, float peak_scale = 1.10f, uint64_t group_id = 0);

    const std::vector<VisualTile> & Tiles() const { return tiles_; }

    // Render all tiles.
    void Draw(SDL_Renderer * r) const;
    
private:
    std::vector<VisualTile> tiles_;

    static SDL_Rect CellRect(const IVec2 & c, const BoardLayout & layout);
    static void SetColor(SDL_Renderer * r, CellType type, uint8_t alpha);
};
