#ifndef TERRAIN_UTIL_H
#define TERRAIN_UTIL_H

#include <algorithm>

namespace terrain
{

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline float smoothstep(float edge0, float edge1, float x)
{
    if (edge0 == edge1)
    {
        return x < edge0 ? 0.0f : 1.0f;
    }
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

} // namespace terrain

#endif // TERRAIN_UTIL_H
