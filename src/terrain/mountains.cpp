#include "mountains.h"
#include "util.h"

#include <cmath>

namespace terrain
{

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
