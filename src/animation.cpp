#include "animation.h"

#include <algorithm>

uint64_t AnimationSystem::BeginGroup()
{
    current_group_id_ = next_group_id_++;
    return current_group_id_;
}

void AnimationSystem::EndGroup()
{
    current_group_id_ = 0;
}

void AnimationSystem::Add(Animation anim)
{
    if (anim.group_id == 0)
    {
        anim.group_id = current_group_id_;
    }
    anims_.push_back(std::move(anim));
}

void AnimationSystem::Update(float dt)
{
    for (auto & a : anims_)
    {
        if (a.finished) continue;
        a.t += dt;
        const float p = (a.duration <= 0.0f) ? 1.0f : std::min(1.0f, a.t / a.duration);
        if (a.apply) a.apply(p);
        if (p >= 1.0f)
        {
            a.finished = true;
            if (a.on_complete) a.on_complete();
        }
    }

    // Remove finished
    anims_.erase(std::remove_if(anims_.begin(), anims_.end(),
                                [](const Animation & a){ return a.finished; }),
                 anims_.end());
}

bool AnimationSystem::IsGroupActive(uint64_t id) const
{
    if (id == 0) return false;
    for (const auto & a : anims_)
    {
        if (!a.finished && a.group_id == id)
        {
            return true;
        }
    }
    return false;
}

bool AnimationSystem::HasActive() const
{
    for (const auto & a : anims_)
    {
        if (!a.finished) return true;
    }
    return false;
}
