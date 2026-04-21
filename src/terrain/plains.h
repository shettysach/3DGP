#ifndef TERRAIN_PLAINS_H
#define TERRAIN_PLAINS_H

#include <functional>

namespace terrain
{

struct PlainsInput
{
    float continental = 0.0f;
    float plainsBase = 0.0f;
    float macroRelief = 0.5f;
    float hilliness = 0.5f;
    float basinNoise = 0.5f;
    float detail = 0.0f;
    float verticalScale = 1.0f;
};

struct PlainsNoiseInput
{
    float sampleX = 0.0f;
    float sampleZ = 0.0f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float verticalScale = 1.0f;
    std::function<float(float, float, int, float, float)> fbm;
};

float computePlainsHeight(const PlainsInput& in);
float computePlainsHeightFromNoise(const PlainsNoiseInput& in, float detail);

} // namespace terrain

#endif // TERRAIN_PLAINS_H
