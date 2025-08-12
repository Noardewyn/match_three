#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <cmath>

// Simple time-based animation system with groups.
// Each animation linearly interpolates between 0..1 over 'duration' seconds
// and calls 'apply(progress)'. When progress reaches 1, 'on_complete' (if any) is invoked.

using EaseFn = std::function<float(float)>;

inline float EaseLinear(float t) { return t; }
inline float EaseOutCubic(float t) { const float u = 1.0f - t; return 1.0f - u * u * u; }
// Classic "back" overshoot on the end.
inline float EaseOutBack(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

struct Animation
{
    float t {0.0f};
    float duration {0.0f};
    std::function<void(float)> apply;   // receives eased progress in [0..1]
    std::function<void()> on_complete;
    uint64_t group_id {0};
    EaseFn ease {EaseLinear};
    bool finished {false};
};

class AnimationSystem
{
public:
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
