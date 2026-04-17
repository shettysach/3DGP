#include "mountains.h"
#include "util.h"

#include <cmath>

namespace terrain
{

float computeMountainWeight(const MountainNoiseInput& in)
{
    const float continental = 0.5f * (in.fbm(in.sampleX, in.sampleZ, in.octaves, in.lacunarity, in.gain) + 1.0f);
    const float ridges = in.ridgedFbm(in.sampleX + 101.3f, in.sampleZ - 77.9f, in.octaves, in.lacunarity, in.gain, in.ridgeSharpness);
    const float detail = 0.5f * (in.fbm(in.sampleX * 2.7f, in.sampleZ * 2.7f, 4, 2.0f, 0.5f) + 1.0f);
    const float rangeMask = smoothstep(0.42f, 0.72f, 0.5f * (in.fbm(in.sampleX * 0.3f + 400.0f, in.sampleZ * 0.3f - 250.0f, 3, in.lacunarity, 0.45f) + 1.0f));
    const float slopeHint = clamp01((ridges - 0.35f) * 1.55f + detail * 0.2f);

    const MountainInput mountainIn{continental, ridges, detail, slopeHint, rangeMask, in.verticalScale};
    return computeMountain(mountainIn).weight;
}

float computeMountainHeight(const MountainNoiseInput& in)
{
    const float continental = 0.5f * (in.fbm(in.sampleX, in.sampleZ, in.octaves, in.lacunarity, in.gain) + 1.0f);
    const float ridges = in.ridgedFbm(in.sampleX + 101.3f, in.sampleZ - 77.9f, in.octaves, in.lacunarity, in.gain, in.ridgeSharpness);
    const float detail = 0.5f * (in.fbm(in.sampleX * 2.7f, in.sampleZ * 2.7f, 4, 2.0f, 0.5f) + 1.0f);
    const float rangeMask = smoothstep(0.42f, 0.72f, 0.5f * (in.fbm(in.sampleX * 0.3f + 400.0f, in.sampleZ * 0.3f - 250.0f, 3, in.lacunarity, 0.45f) + 1.0f));
    const float slopeHint = clamp01((ridges - 0.35f) * 1.55f + detail * 0.2f);

    const MountainInput mountainIn{continental, ridges, detail, slopeHint, rangeMask, in.verticalScale};
    return computeMountain(mountainIn).height;
}

MountainResult computeMountain(const MountainInput& in)
{
    const float mountainSignal = clamp01(in.continental * 0.55f + in.slopeHint * 0.35f + in.rangeMask * 0.45f);
    const float mountainCore = smoothstep(0.50f, 0.85f, mountainSignal);
    const float mountainFoot = smoothstep(0.30f, 0.65f, mountainSignal);
    const float weight = clamp01(0.40f * mountainCore + 0.60f * mountainFoot) * in.rangeMask;

    const float mountainBase = 0.52f * in.continental + 0.38f * in.ridges + 0.10f * in.detail;
    const float mountainShape = smoothstep(0.12f, 0.90f, clamp01(mountainBase));
    const float mountainRidgeDetail = smoothstep(0.44f, 0.92f, in.ridges);
    float height =
        (0.78f * mountainShape + 0.22f * std::pow(mountainShape, 1.45f)) * in.verticalScale * 0.86f +
        mountainRidgeDetail * in.verticalScale * 0.06f;

    const float peakKnee = in.verticalScale * 0.74f;
    if (height > peakKnee)
    {
        height = peakKnee + (height - peakKnee) * 0.32f;
    }

    return {height, weight};
}

} // namespace terrain
