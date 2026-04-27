#ifndef TERRAIN_BLENDING_H
#define TERRAIN_BLENDING_H

#include <cstddef>
#include <vector>

namespace terrain {

struct BlendInput {
    float mountainHeight = 0.0f;
    float mountainWeight = 0.0f;
    float plainsHeight = 0.0f;
    float plateauHeight = 0.0f;
    float plateauWeight = 0.0f;
    float valleyDepth = 0.0f;
    float detail = 0.0f;
    float verticalScale = 1.0f;
};

struct BlendResult {
    float height = 0.0f;
    float mountainWeight = 0.0f;
};

BlendResult blendTerrain(const BlendInput& in);

void smoothHeights(
    std::vector<float>& heights,
    const std::vector<float>& mountainWeights,
    const std::vector<float>& valleyWeights,
    int width,
    int depth);

} // namespace terrain

#endif // TERRAIN_BLENDING_H
