#include "mountains.h"
#include "util.h"

#include <cmath>

namespace terrain {

MountainResult computeMountain(const MountainInput& in) {
    const auto& p = in.params;

    const float slopeHint = std::clamp((in.ridges - 0.35f) * 1.55f + in.detail * 0.2f, 0.0f, 1.0f);

    const float signal = std::clamp(in.continental * 0.55f + slopeHint * 0.35f + in.rangeMask * 0.45f, 0.0f, 1.0f);

    const float cov = p.coverage / 0.48f;
    const float coreLo = 0.42f / cov;
    const float coreHi = 0.78f / cov;
    const float footLo = 0.22f / cov;
    const float footHi = 0.55f / cov;

    const float core = smoothstep(coreLo, coreHi, signal);
    const float foot = smoothstep(footLo, footHi, signal);
    const float weight = std::clamp(0.40f * core + 0.60f * foot, 0.0f, 1.0f) * in.rangeMask;

    const float base = 0.52f * in.continental + 0.38f * in.ridges + 0.10f * in.detail;
    const float shape = smoothstep(0.12f, 0.90f, std::clamp(base, 0.0f, 1.0f));

    const float shp = p.sharpness / 1.35f;
    const float ridgeLo = 0.44f / shp;
    const float ridgeHi = 0.92f / shp;
    const float ridgeDetail = smoothstep(ridgeLo, ridgeHi, in.ridges);

    const float powExp = 1.0f + 0.333f * p.sharpness;
    float height = (0.78f * shape + 0.22f * std::pow(shape, powExp)) * in.verticalScale * p.heightScale * 1.316f
                   + ridgeDetail * in.verticalScale * p.heightScale * 0.126f;

    const float peakKnee = in.verticalScale * p.heightScale;
    if (height > peakKnee) {
        height = peakKnee + (height - peakKnee) * 0.50f;
    }

    return {height, weight};
}

} // namespace terrain
