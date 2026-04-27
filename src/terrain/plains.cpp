#include "plains.h"
#include "util.h"

#include <algorithm>

namespace terrain {

float computePlainsHeight(const PlainsInput& in) {
    const auto& p = in.params;

    const float macroAmt = p.relief * 0.94f;
    const float rollingAmt = p.relief * 0.50f;
    const float boostAmt = p.relief * 0.33f;
    const float carveAmt = p.relief * 0.28f;

    const float macroOffset = (in.macroRelief - 0.5f) * macroAmt;
    const float rollingOffset = (in.hilliness - 0.5f) * rollingAmt;
    const float plateauBoost = smoothstep(0.56f, 0.82f, in.macroRelief) * boostAmt;
    const float basinCarve = smoothstep(0.54f, 0.82f, 1.0f - in.basinNoise) * carveAmt;

    const float baseSignal =
        0.46f * in.continental +
        0.18f * in.plainsBase +
        0.08f * in.detail +
        macroOffset +
        rollingOffset +
        plateauBoost -
        basinCarve;

    return std::max(0.0f, baseSignal) * in.verticalScale * p.heightScale * 0.95f;
}

} // namespace terrain
