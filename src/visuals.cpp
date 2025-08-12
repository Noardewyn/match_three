#include "visuals.h"
#include "renderer.h"

static inline int stride_px(const BoardLayout & layout)
{
    return layout.cell_size + layout.gap;
}

SDL_Rect VisualBoard::CellRect(const IVec2 & c, const BoardLayout & layout)
{
    const int px = layout.origin_x + c.x * stride_px(layout);
    const int py = layout.origin_y + c.y * stride_px(layout);
    SDL_Rect r { px, py, layout.cell_size, layout.cell_size };
    return r;
}

void VisualBoard::BuildFromBoard(const Board & board, const BoardLayout & layout)
{
    tiles_.clear();
    tiles_.reserve(Board::kWidth * Board::kHeight);

    for (int y = 0; y < Board::kHeight; ++y)
    {
        for (int x = 0; x < Board::kWidth; ++x)
        {
            const IVec2 c {x, y};
            const SDL_Rect r = CellRect(c, layout);
            VisualTile t;
            t.type = board.Get(c);
            t.cell = c;
            t.x = static_cast<float>(r.x);
            t.y = static_cast<float>(r.y);
            t.alpha = 1.0f;
            t.sx = t.sy = 1.0f;
            tiles_.push_back(t);
        }
    }
}

void VisualBoard::SnapToLayout(const BoardLayout & layout)
{
    for (auto & t : tiles_)
    {
        const SDL_Rect r = CellRect(t.cell, layout);
        t.x = static_cast<float>(r.x);
        t.y = static_cast<float>(r.y);
        t.sx = t.sy = 1.0f;
    }
}

uint64_t VisualBoard::AnimateSwap(const IVec2 & a, const IVec2 & b, const BoardLayout & layout,
                                  AnimationSystem & anims, float seconds, uint64_t group_id)
{
    VisualTile * ta = nullptr;
    VisualTile * tb = nullptr;
    for (auto & t : tiles_)
    {
        if (t.cell.x == a.x && t.cell.y == a.y) ta = &t;
        if (t.cell.x == b.x && t.cell.y == b.y) tb = &t;
    }
    if (!ta || !tb) return 0;

    const SDL_Rect ra = CellRect(a, layout);
    const SDL_Rect rb = CellRect(b, layout);

    const float ax0 = ta->x, ay0 = ta->y;
    const float bx0 = tb->x, by0 = tb->y;

    const float ax1 = static_cast<float>(rb.x);
    const float ay1 = static_cast<float>(rb.y);
    const float bx1 = static_cast<float>(ra.x);
    const float by1 = static_cast<float>(ra.y);

    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    anims.Add(Animation{
        0.0f, seconds,
        [ta, ax0, ay0, ax1, ay1](float p){ ta->x = Lerp(ax0, ax1, p); ta->y = Lerp(ay0, ay1, p); },
        nullptr, g, EaseOutCubic
    });

    anims.Add(Animation{
        0.0f, seconds,
        [tb, bx0, by0, bx1, by1](float p){ tb->x = Lerp(bx0, bx1, p); tb->y = Lerp(by0, by1, p); },
        nullptr, g, EaseOutCubic
    });

    if (group_id == 0) anims.EndGroup();

    std::swap(ta->cell, tb->cell);
    return g;
}

uint64_t VisualBoard::AnimateFadeMask(const std::vector<bool> & mask, AnimationSystem & anims,
                                      float seconds, uint64_t group_id)
{
    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    for (auto & t : tiles_)
    {
        const int idx = t.cell.y * Board::kWidth + t.cell.x;
        if (idx >= 0 && idx < static_cast<int>(mask.size()) && mask[idx])
        {
            const float a0 = t.alpha;
            anims.Add(Animation{
                0.0f, seconds,
                [&t, a0](float p){ t.alpha = Lerp(a0, 0.0f, p); },
                nullptr, g, EaseLinear
            });
        }
    }

    if (group_id == 0) anims.EndGroup();
    return g;
}

uint64_t VisualBoard::AnimatePulseMask(const std::vector<bool> & mask, AnimationSystem & anims,
                                       float seconds, float peak_scale, uint64_t group_id)
{
    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    for (auto & t : tiles_)
    {
        const int idx = t.cell.y * Board::kWidth + t.cell.x;
        if (idx >= 0 && idx < static_cast<int>(mask.size()) && mask[idx])
        {
            anims.Add(Animation{
                0.0f, seconds,
                [&t, peak_scale](float p){
                    // Piecewise yoyo: grow then return
                    float s = (p < 0.5f) ? Lerp(1.0f, peak_scale, p * 2.0f)
                                         : Lerp(peak_scale, 1.0f, (p - 0.5f) * 2.0f);
                    t.sx = t.sy = s;
                },
                nullptr, g, EaseOutCubic
            });
        }
    }

    if (group_id == 0) anims.EndGroup();
    return g;
}

void VisualBoard::RemoveByMask(const std::vector<bool> & mask)
{
    tiles_.erase(std::remove_if(tiles_.begin(), tiles_.end(),
                 [&mask](const VisualTile & t){
                     const int idx = t.cell.y * Board::kWidth + t.cell.x;
                     return idx >= 0 && idx < static_cast<int>(mask.size()) && mask[idx];
                 }),
                 tiles_.end());
}

uint64_t VisualBoard::AnimateMoves(const std::vector<Move> & moves, const BoardLayout & layout,
                                   AnimationSystem & anims, float seconds, uint64_t group_id)
{
    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    for (const auto & m : moves)
    {
        VisualTile * tv = nullptr;
        for (auto & t : tiles_)
        {
            if (t.cell.x == m.from.x && t.cell.y == m.from.y) { tv = &t; break; }
        }
        if (!tv) continue;

        const SDL_Rect r1 = CellRect(m.to, layout);
        const float x0 = tv->x, y0 = tv->y;
        const float x1 = static_cast<float>(r1.x);
        const float y1 = static_cast<float>(r1.y);

        anims.Add(Animation{
            0.0f, seconds,
            [tv, x0, y0, x1, y1](float p){ tv->x = Lerp(x0, x1, p); tv->y = Lerp(y0, y1, p); },
            nullptr, g, EaseOutCubic
        });

        tv->cell = m.to;
        tv->sx = tv->sy = 1.0f;
    }

    if (group_id == 0) anims.EndGroup();
    return g;
}

uint64_t VisualBoard::AnimateSpawns(const std::vector<Spawn> & spawns, const BoardLayout & layout,
                                    AnimationSystem & anims, float seconds, uint64_t group_id)
{
    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    for (const auto & s : spawns)
    {
        const SDL_Rect rt = CellRect(s.to, layout);
        const int start_y = layout.origin_y - (s.order_above + 1) * stride_px(layout);

        VisualTile t;
        t.type = s.type;
        t.cell = s.to;
        t.x = static_cast<float>(rt.x);
        t.y = static_cast<float>(start_y);
        t.alpha = 1.0f;
        t.sx = t.sy = 1.0f;
        tiles_.push_back(t);

        VisualTile & ref = tiles_.back();
        const float y0 = static_cast<float>(start_y);
        const float y1 = static_cast<float>(rt.y);
        const float x0 = static_cast<float>(rt.x);

        anims.Add(Animation{
            0.0f, seconds,
            [&ref, x0, y0, y1](float p){ ref.x = x0; ref.y = Lerp(y0, y1, p); },
            nullptr, g, EaseOutCubic
        });
    }

    if (group_id == 0) anims.EndGroup();
    return g;
}

uint64_t VisualBoard::AnimateBumpCells(const std::vector<IVec2> & cells, AnimationSystem & anims,
                                       float seconds, float peak_scale, uint64_t group_id)
{
    const uint64_t g = (group_id == 0) ? anims.BeginGroup() : group_id;

    for (auto & t : tiles_)
    {
        const bool target = std::any_of(cells.begin(), cells.end(),
            [&t](const IVec2 & c){ return c.x == t.cell.x && c.y == t.cell.y; });

        if (!target) continue;

        anims.Add(Animation{
            0.0f, seconds,
            [&t, peak_scale](float p){
                // Quick bump: overshoot up then relax to 1.0
                float s = (p < 0.6f) ? Lerp(1.0f, peak_scale, p / 0.6f)
                                     : Lerp(peak_scale, 1.0f, (p - 0.6f) / 0.4f);
                t.sx = t.sy = s;
            },
            nullptr, g, EaseOutBack
        });
    }

    if (group_id == 0) anims.EndGroup();
    return g;
}

