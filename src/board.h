#pragma once
#include "types.h"

#include <vector>
#include <random>
#include <optional>
#include <utility>

struct Move
{
    IVec2 from;
    IVec2 to;
};

struct Spawn
{
    IVec2 to;
    CellType type;
    int order_above {0}; // 0,1,2... for stacking spawn start offsets
};

class Board
{
public:
    static constexpr int kWidth = 6;
    static constexpr int kHeight = 6;

    Board();

    void GenerateInitial(uint32_t seed);

    bool InBounds(const IVec2 & p) const;
    CellType Get(const IVec2 & p) const;
    void Set(const IVec2 & p, CellType c);

    bool AreAdjacent(const IVec2 & a, const IVec2 & b) const;

    // Low-level swap (used by the state machine).
    void Swap(const IVec2 & a, const IVec2 & b);

    // Public match finding. Returns true if any matches were found and
    // fills out_mask with matched cells. Additionally outputs the number
    // of distinct match groups and total matched cells for scoring.
    bool FindMatches(std::vector<bool> & out_mask, int & out_groups, int & out_cells) const;

    // Collapse columns and refill with new candies.
    // Mutates the board to the post-collapse state and returns planned tile moves and spawns.
    int CollapseAndRefillPlanned(const std::vector<bool> & mask,
                                 std::vector<Move> & out_moves,
                                 std::vector<Spawn> & out_spawns);

    // Find any possible swap that would produce a match.
    // Returns the pair of coordinates to swap if available.
    std::optional<std::pair<IVec2, IVec2>> FindAnySwap() const;

private:
    std::vector<CellType> cells_;
    std::mt19937 rng_;

    int Index(const IVec2 & p) const;

    // Internal helpers
    bool FindMatchesMask(std::vector<bool> & out_mask, int & out_groups, int & out_cells) const;
    CellType RandomCandy();
    CellType RandomCandyAvoiding(int x, int y);
};
