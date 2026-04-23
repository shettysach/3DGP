#include "valleys.h"
#include "util.h"

#include <algorithm>
#include <cmath>

namespace terrain {

ValleyNoiseComputation computeValleyNoiseComputation(const ValleyNoiseInput& in, float detail) {
    const float continental = 0.5f * (in.fbm(in.sampleX, in.sampleZ, in.octaves, in.lacunarity, in.gain) + 1.0f);
    const float basin = 0.5f * (in.fbm(in.sampleX * 0.28f - 191.7f,
                                       in.sampleZ * 0.28f + 83.4f,
                                       3,
                                       in.lacunarity,
                                       0.52f) +
                                1.0f);
    const float detailBand = 0.5f * (in.fbm(in.sampleX * 1.9f + 52.3f,
                                             in.sampleZ * 1.9f - 61.8f,
                                             std::max(3, in.octaves - 2),
                                             in.lacunarity,
                                             in.gain) +
                                     1.0f);
    const float rimMask = smoothstep(0.38f, 0.74f, 0.5f * (in.fbm(in.sampleX * 0.17f + 420.0f,
                                                                  in.sampleZ * 0.17f - 301.0f,
                                                                  3,
                                                                  in.lacunarity,
                                                                  0.45f) +
                                                           1.0f));
    const float slopeHint = std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);

    return {continental, basin, detailBand, rimMask, slopeHint};
}

ValleyResult computeValley(const ValleyInput& in) {
    const float valleySignal = std::clamp(in.basin * 0.60f + (1.0f - in.continental) * 0.22f + in.slopeHint * 0.28f, 0.0f, 1.0f);
    const float valleyFloor = smoothstep(0.18f, 0.72f, valleySignal);
    const float valleyRim = smoothstep(0.52f, 0.86f, in.rimMask);

    float depth = (0.70f * valleyFloor + 0.18f * std::pow(valleyFloor, 1.35f) + 0.12f * in.detail) * in.verticalScale * 0.42f;
    depth *= (1.0f - 0.35f * valleyRim);

    const float peakKnee = in.verticalScale * 0.28f;
    if (depth > peakKnee) {
        depth = peakKnee + (depth - peakKnee) * 0.45f;
    }

    const float weight = std::clamp(0.48f * valleyFloor + 0.52f * smoothstep(0.42f, 0.82f, valleySignal), 0.0f, 1.0f);
    return {depth, weight};
}

ValleyResult computeValleyResult(const ValleyNoiseInput& in, float detail) {
    const auto noise = computeValleyNoiseComputation(in, detail);
    const ValleyInput valleyIn{noise.continental, noise.basin, noise.detail, noise.slopeHint, noise.rimMask, in.verticalScale};
    return computeValley(valleyIn);
}

} // namespace terrain
