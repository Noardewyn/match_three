#pragma once
#include <vector>
#include <functional>
#include <cstdint>

// Simple time-based animation system with groups.
// Each animation linearly interpolates between 0..1 over 'duration' seconds
// and calls 'apply(progress)'. When progress reaches 1, 'on_complete' (if any) is invoked.

struct Animation
{
    float t {0.0f};
    float duration {0.0f};
    std::function<void(float)> apply;
    std::function<void()> on_complete;
    uint64_t group_id {0};
    bool finished {false};
};

class AnimationSystem
{
public:
    // Start a new group; all animations added until EndGroup() gets the same id.
    uint64_t BeginGroup();
    uint64_t CurrentGroup() const { return current_group_id_; }
    void EndGroup();

    void Add(Animation anim);
    void Update(float dt);

    bool IsGroupActive(uint64_t id) const;
    bool HasActive() const;

private:
    std::vector<Animation> anims_;
    uint64_t next_group_id_ {1};
    uint64_t current_group_id_ {0};
};

inline float Lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}
