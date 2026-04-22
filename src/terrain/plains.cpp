#include "plains.h"
#include "util.h"

#include <algorithm>

namespace terrain {

float computePlainsHeightFromNoise(const PlainsNoiseInput& params, float detail) {
    const float continental = 0.5f * (params.fbm(params.sampleX, params.sampleZ, params.octaves, params.lacunarity, params.gain) + 1.0f);
    const int plainsOctaves = std::max(3, params.octaves - 2);
    const float plainsBase = 0.5f * (params.fbm(params.sampleX - 63.2f, params.sampleZ + 41.8f, plainsOctaves, params.lacunarity, params.gain) + 1.0f);
    const float macroRelief = 0.5f * (params.fbm(params.sampleX * 0.30f + 219.4f,
                                                 params.sampleZ * 0.30f - 174.6f,
                                                 3,
                                                 params.lacunarity,
                                                 0.48f) +
                                      1.0f);
    const float hilliness = 0.5f * (params.fbm(params.sampleX * 0.82f - 141.5f,
                                               params.sampleZ * 0.82f + 96.8f,
                                               std::max(3, params.octaves - 1),
                                               params.lacunarity,
                                               params.gain) +
                                    1.0f);
    const float basinNoise = 0.5f * (params.fbm(params.sampleX * 0.18f - 331.7f,
                                                params.sampleZ * 0.18f + 271.4f,
                                                2,
                                                params.lacunarity,
                                                0.55f) +
                                     1.0f);

    const PlainsInput in{continental, plainsBase, macroRelief, hilliness, basinNoise, detail, params.verticalScale};
    return computePlainsHeight(in);
}

float computePlainsHeight(const PlainsInput& in) {
    const float macroOffset = (in.macroRelief - 0.5f) * 0.34f;
    const float rollingOffset = (in.hilliness - 0.5f) * 0.18f;
    const float plateauBoost = smoothstep(0.56f, 0.82f, in.macroRelief) * 0.12f;
    const float basinCarve = smoothstep(0.54f, 0.82f, 1.0f - in.basinNoise) * 0.10f;

    const float baseSignal =
        0.46f * in.continental +
        0.18f * in.plainsBase +
        0.08f * in.detail +
        macroOffset +
        rollingOffset +
        plateauBoost -
        basinCarve;

    return std::max(0.0f, baseSignal) * in.verticalScale * 0.74f;
}

} // namespace terrain
