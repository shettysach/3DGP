#include "mountains.h"
#include "util.h"

#include <cmath>

namespace terrain {

MountainNoiseComputation computeMountainNoiseComputation(const MountainNoiseInput& in, float detail) {
    const float continental = 0.5f * (in.fbm(in.sampleX, in.sampleZ, in.octaves, in.lacunarity, in.gain) + 1.0f);
    const float ridges = in.ridgedFbm(in.sampleX + 101.3f, in.sampleZ - 77.9f, in.octaves, in.lacunarity, in.gain, in.ridgeSharpness);
    const float rangeMask = smoothstep(0.42f, 0.72f, 0.5f * (in.fbm(in.sampleX * 0.3f + 400.0f, in.sampleZ * 0.3f - 250.0f, 3, in.lacunarity, 0.45f) + 1.0f));
    const float slopeHint = std::clamp((ridges - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);

    return {continental, ridges, detail, rangeMask, slopeHint};
}

MountainResult computeMountainResult(const MountainNoiseInput& in, float detail) {
    const auto noise = computeMountainNoiseComputation(in, detail);
    const MountainInput mountainIn{noise.continental, noise.ridges, noise.detail, noise.slopeHint, noise.rangeMask, in.verticalScale};
    return computeMountain(mountainIn);
}

MountainResult computeMountain(const MountainInput& in) {
    const float mountainSignal = std::clamp(in.continental * 0.55f + in.slopeHint * 0.35f + in.rangeMask * 0.45f, 0.0f, 1.0f);
    const float mountainCore = smoothstep(0.50f, 0.85f, mountainSignal);
    const float mountainFoot = smoothstep(0.30f, 0.65f, mountainSignal);
    const float weight = std::clamp(0.40f * mountainCore + 0.60f * mountainFoot, 0.0f, 1.0f) * in.rangeMask;

    const float mountainBase = 0.52f * in.continental + 0.38f * in.ridges + 0.10f * in.detail;
    const float mountainShape = smoothstep(0.12f, 0.90f, std::clamp(mountainBase, 0.0f, 1.0f));
    const float mountainRidgeDetail = smoothstep(0.44f, 0.92f, in.ridges);
    float height =
        (0.78f * mountainShape + 0.22f * std::pow(mountainShape, 1.45f)) * in.verticalScale * 0.86f +
        mountainRidgeDetail * in.verticalScale * 0.06f;

    const float peakKnee = in.verticalScale * 0.74f;
    if (height > peakKnee) {
        height = peakKnee + (height - peakKnee) * 0.32f;
    }

    return {height, weight};
}

} // namespace terrain
