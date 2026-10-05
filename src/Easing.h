#ifndef EASING_H
#define EASING_H

#include <algorithm>
#include <cmath>

// Shaping functions for the animation phases. Linear motion is the single
// biggest giveaway that something was animated by a programmer, so nothing in
// this project moves without passing through one of these.
namespace Easing
{
    inline float clamp01(float x)
    {
        return std::min(1.0f, std::max(0.0f, x));
    }

    inline float mix(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    // Signed difference between two angles in degrees, folded into
    // [-180, 180]. Interpolating raw degrees sends a rotation the long way
    // round whenever it crosses the +-180 boundary - the classic bug in both
    // camera framing and character turning.
    inline float shortestAngleDelta(float from, float to)
    {
        return std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    }

    // Classic S-curve: starts slow, ends slow.
    inline float smoothstep01(float x)
    {
        x = clamp01(x);
        return x * x * (3.0f - 2.0f * x);
    }

    // Accelerates away from rest.
    inline float easeInQuad(float x)
    {
        x = clamp01(x);
        return x * x;
    }

    // Decelerates into rest.
    inline float easeOutQuad(float x)
    {
        x = clamp01(x);
        return 1.0f - (1.0f - x) * (1.0f - x);
    }

    inline float easeOutCubic(float x)
    {
        x = clamp01(x);
        const float inv = 1.0f - x;
        return 1.0f - inv * inv * inv;
    }

    // Overshoots past 1 before settling back. Gives the lamp lid a physical
    // "pop" as it swings open rather than a soft glide.
    inline float easeOutBack(float x)
    {
        x = clamp01(x);

        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;

        const float inv = x - 1.0f;
        return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
    }

    // A decaying oscillation about 1: overshoots, swings back, settles. This
    // is what makes the scale beam look like it has mass instead of snapping
    // to its final angle.
    // `frequency` is in whole oscillations across the 0..1 span. Below about
    // 1.2 the curve overshoots once and decays straight back, which reads as
    // a soft landing rather than a wobble - it needs to cross 1 more than
    // once to look like a physical settle.
    inline float dampedSettle(float x, float frequency = 1.6f, float decay = 4.2f)
    {
        x = clamp01(x);

        if (x >= 1.0f)
        {
            return 1.0f;
        }

        constexpr float kTwoPi = 6.28318530718f;
        return 1.0f - std::exp(-decay * x) * std::cos(frequency * kTwoPi * x);
    }
}

#endif // EASING_H
