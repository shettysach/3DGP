#ifndef TERRAIN_UTIL_H
#define TERRAIN_UTIL_H

#include <algorithm>
#include <cmath>

namespace terrain
{

inline float clamp01(float x)
{
    return std::max(0.0f, std::min(1.0f, x));
}

inline float smoothstep(float edge0, float edge1, float x)
{
    if (edge0 == edge1)
    {
        return x < edge0 ? 0.0f : 1.0f;
    }
    x = clamp01((x - edge0) / (edge1 - edge0));
    return x * x * (3.0f - 2.0f * x);
}

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

} // namespace terrain

#endif // TERRAIN_UTIL_H
