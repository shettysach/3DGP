#ifndef TERRAIN_MOUNTAINS_H
#define TERRAIN_MOUNTAINS_H

#include <functional>

namespace terrain {

struct MountainInput {
    float continental = 0.0f;
    float ridges = 0.0f;
    float detail = 0.0f;
    float slopeHint = 0.0f;
    float rangeMask = 0.0f;
    float verticalScale = 1.0f;
};

struct MountainResult {
    float height = 0.0f;
    float weight = 0.0f;
};

struct MountainNoiseComputation {
    float continental = 0.0f;
    float ridges = 0.0f;
    float detail = 0.0f;
    float rangeMask = 0.0f;
    float slopeHint = 0.0f;
};

struct MountainNoiseInput {
    float sampleX = 0.0f;
    float sampleZ = 0.0f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeSharpness = 1.0f;
    float verticalScale = 1.0f;
    std::function<float(float, float, int, float, float)> fbm;
    std::function<float(float, float, int, float, float, float)> ridgedFbm;
};

MountainNoiseComputation computeMountainNoiseComputation(const MountainNoiseInput& in, float detail);
MountainResult computeMountain(const MountainInput& in);
MountainResult computeMountainResult(const MountainNoiseInput& in, float detail);

} // namespace terrain

#endif // TERRAIN_MOUNTAINS_H
