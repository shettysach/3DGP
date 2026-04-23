#ifndef TERRAIN_VALLEYS_H
#define TERRAIN_VALLEYS_H

#include <functional>

namespace terrain {

struct ValleyInput {
    float continental = 0.0f;
    float basin = 0.0f;
    float detail = 0.0f;
    float slopeHint = 0.0f;
    float rimMask = 0.0f;
    float verticalScale = 1.0f;
};

struct ValleyResult {
    float depth = 0.0f;
    float weight = 0.0f;
};

struct ValleyNoiseComputation {
    float continental = 0.0f;
    float basin = 0.0f;
    float detail = 0.0f;
    float rimMask = 0.0f;
    float slopeHint = 0.0f;
};

struct ValleyNoiseInput {
    float sampleX = 0.0f;
    float sampleZ = 0.0f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float verticalScale = 1.0f;
    std::function<float(float, float, int, float, float)> fbm;
};

ValleyNoiseComputation computeValleyNoiseComputation(const ValleyNoiseInput& in, float detail);
ValleyResult computeValley(const ValleyInput& in);
ValleyResult computeValleyResult(const ValleyNoiseInput& in, float detail);

} // namespace terrain

#endif // TERRAIN_VALLEYS_H
