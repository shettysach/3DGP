#include "plateaus.h"
#include "util.h"

#include <algorithm>

namespace terrain {

PlateauResult computePlateau(const PlateauInput& in) {
    const auto& p = in.params;

    const float regionSignal = std::clamp(in.plateauMask * 0.80f + in.continental * 0.20f, 0.0f, 1.0f);

    const float bandHalf = 0.05f * (0.53f / p.coverage);
    const float center = 0.59f;
    const float tableTop = smoothstep(center - bandHalf, center + bandHalf, regionSignal);

    const float weight = std::clamp(tableTop, 0.0f, 1.0f);

    const float baseHeight = tableTop * in.verticalScale * p.heightScale;

    const float surfaceDetail = (in.detail - 0.5f) * in.verticalScale * 0.025f;

    const float rimLo = center - bandHalf * 1.4f;
    const float rimMid = center;
    const float rimHi = center + bandHalf * 1.8f;
    const float rim = smoothstep(rimLo, rimMid, regionSignal) *
                      (1.0f - smoothstep(rimMid, rimHi, regionSignal));
    const float rimBoost = rim * in.verticalScale * p.cliffness * 0.06f;

    float height = baseHeight + surfaceDetail + rimBoost;
    height = std::max(0.0f, height);

    return {height, weight};
}

} // namespace terrain
