#include "plains.h"

#include <algorithm>

namespace terrain
{

float computePlainsHeightFromNoise(const PlainsNoiseInput& params, float detail)
{
    const float continental = 0.5f * (params.fbm(params.sampleX, params.sampleZ, params.octaves, params.lacunarity, params.gain) + 1.0f);
    const int plainsOctaves = std::max(3, params.octaves - 2);
    const float plainsBase = 0.5f * (params.fbm(params.sampleX - 63.2f, params.sampleZ + 41.8f, plainsOctaves, params.lacunarity, params.gain) + 1.0f);

    const PlainsInput in{continental, plainsBase, detail, params.verticalScale};
    return computePlainsHeight(in);
}

float computePlainsHeight(const PlainsInput& in)
{
    return (0.62f * in.continental + 0.28f * in.plainsBase + 0.10f * in.detail) *
           in.verticalScale * 0.56f;
}

} // namespace terrain
