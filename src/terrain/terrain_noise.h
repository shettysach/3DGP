#ifndef TERRAIN_NOISE_H
#define TERRAIN_NOISE_H

#include <functional>

namespace terrain
{

struct TerrainNoiseInput
{
    float sampleX = 0.0f;
    float sampleZ = 0.0f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeSharpness = 1.0f;
    std::function<float(float, float, int, float, float)> fbm;
    std::function<float(float, float, int, float, float, float)> ridgedFbm;
};

struct TerrainNoiseComputation
{
    float continental = 0.0f;
    float detail = 0.0f;
};

TerrainNoiseComputation computeTerrainNoiseComputation(const TerrainNoiseInput& in);

} // namespace terrain

#endif // TERRAIN_NOISE_H
