#ifndef TERRAIN_UTIL_H
#define TERRAIN_UTIL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace terrain {

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float smoothstep(float a, float b, float x) {
    x = (x - a) / (b - a);
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

inline float hashJitter(size_t idx, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(idx) ^ (seed * 747796405u + 2891336453u);
    h ^= (h >> 16);
    h *= 2246822519u;
    h ^= (h >> 13);
    h *= 3266489917u;
    h ^= (h >> 16);
    return static_cast<float>(h & 1023u) / 1023.0f;
}

inline void computeHeightExtents(
    const std::vector<float>& heights,
    float& minHeight,
    float& maxHeight
) {
    minHeight = std::numeric_limits<float>::max();
    maxHeight = std::numeric_limits<float>::lowest();
    for (float h : heights) {
        minHeight = std::min(minHeight, h);
        maxHeight = std::max(maxHeight, h);
    }
}

inline float computeTerrace(float value, float steps) {
    if (steps <= 0.0f) {
        return value;
    }
    return std::floor(value * steps) / steps;
}

} // namespace terrain

#endif // TERRAIN_UTIL_H
