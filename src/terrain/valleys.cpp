#include "valleys.h"
#include "util.h"

#include <algorithm>
#include <cmath>

namespace terrain {

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

} // namespace terrain
