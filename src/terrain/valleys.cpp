#include "valleys.h"
#include "util.h"

#include <cmath>

namespace terrain {

ValleyResult computeValley(const ValleyInput& in) {
    const auto& p = in.params;

    const float slopeHint = std::clamp((0.62f - in.basin) * 1.35f + in.detail * 0.22f, 0.0f, 1.0f);

    const float signal = std::clamp(in.basin * 0.60f + (1.0f - in.continental) * 0.22f + slopeHint * 0.28f, 0.0f, 1.0f);

    const float cov = p.coverage / 0.58f;
    const float floorLo = 0.18f / cov;
    const float floorHi = 0.72f / cov;
    const float floor = smoothstep(floorLo, floorHi, signal);
    const float rim = smoothstep(0.52f, 0.86f, in.rimMask);

    float depth = (0.70f * floor + 0.18f * std::pow(floor, 1.35f) + 0.12f * in.detail) * in.verticalScale * p.depthScale * 0.84f;
    depth *= (1.0f - 0.35f * rim);

    const float depthKnee = in.verticalScale * p.depthScale * 0.56f;
    if (depth > depthKnee) {
        depth = depthKnee + (depth - depthKnee) * 0.45f;
    }

    const float weightLo = 0.42f / cov;
    const float weightHi = 0.82f / cov;
    const float weight = std::clamp(0.48f * floor + 0.52f * smoothstep(weightLo, weightHi, signal), 0.0f, 1.0f);

    return {depth, weight};
}

} // namespace terrain
