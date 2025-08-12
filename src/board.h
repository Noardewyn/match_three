#pragma once
#include "types.h"

#include <vector>
#include <random>
#include <optional>

class Board
{
public:
    static constexpr int kWidth = 6;
    static constexpr int kHeight = 6;

    Board();

    // Create initial board without any matches.
    void GenerateInitial(uint32_t seed);

    // Bounds and accessors.
    bool InBounds(const IVec2 & p) const;
    CellType Get(const IVec2 & p) const;
    void Set(const IVec2 & p, CellType c);

    // Utilities.
    bool AreAdjacent(const IVec2 & a, const IVec2 & b) const;

    // Simple swap without validation (used internally).
    void Swap(const IVec2 & a, const IVec2 & b);

    // High-level operation: try swap, resolve cascades if it creates matches;
    // otherwise swap is reverted and returns false.
    bool TrySwapAndResolve(const IVec2 & a, const IVec2 & b);

private:
    std::vector<CellType> cells_;
    std::mt19937 rng_;

    int Index(const IVec2 & p) const;

    // Matching and resolve.
    bool FindMatchesMask(std::vector<bool> & out_mask) const;
    int CollapseColumnsAndRefill(const std::vector<bool> & mask);
    CellType RandomCandy();

    // Helpers for initial generation (avoid starting matches).
    CellType RandomCandyAvoiding(int x, int y);
};
