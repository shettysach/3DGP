#include "blending.h"
#include "util.h"

#include <algorithm>

namespace terrain
{

BlendResult blendTerrain(const BlendInput& in)
{
    float mw = in.mountainWeight;
    float pw = 1.0f - mw;

    const float weightSum = std::max(0.0001f, mw + pw);
    mw /= weightSum;
    pw /= weightSum;

    const float blendedHeight = mw * in.mountainHeight + pw * in.plainsHeight;
    const float fineDetail = (in.detail - 0.5f) * in.verticalScale * 0.035f;

    return {(blendedHeight + fineDetail) * in.falloff, mw};
}

void smoothHeights(
    std::vector<float>& heights,
    const std::vector<float>& mountainWeights,
    int width,
    int depth)
{
    const auto idxOf = [width](int x, int z) -> size_t
    {
        return static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    std::vector<float> smoothed = heights;

    for (int z = 0; z < depth; ++z)
    {
        const int z0 = std::max(0, z - 1);
        const int z1 = std::min(depth - 1, z + 1);

        for (int x = 0; x < width; ++x)
        {
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(width - 1, x + 1);

            const size_t idx = idxOf(x, z);

            const float filtered = (heights[idxOf(x0, z0)] + 2.0f * heights[idxOf(x, z0)] + heights[idxOf(x1, z0)] +
                                    2.0f * heights[idxOf(x0, z)] + 4.0f * heights[idx] +
                                    2.0f * heights[idxOf(x1, z)] + heights[idxOf(x0, z1)] +
                                    2.0f * heights[idxOf(x, z1)] + heights[idxOf(x1, z1)]) /
                                   16.0f;

            const float smoothAmount = smoothstep(0.28f, 0.88f, mountainWeights[idx]) * 0.62f;
            smoothed[idx] = lerp(heights[idx], filtered, smoothAmount);
        }
    }

    heights.swap(smoothed);
}

} // namespace terrain
