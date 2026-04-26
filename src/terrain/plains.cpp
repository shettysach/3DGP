#include "plains.h"
#include "util.h"

#include <algorithm>

namespace terrain {

float computePlainsHeight(const PlainsInput& in) {
    const float macroOffset = (in.macroRelief - 0.5f) * 0.34f;
    const float rollingOffset = (in.hilliness - 0.5f) * 0.18f;
    const float plateauBoost = smoothstep(0.56f, 0.82f, in.macroRelief) * 0.12f;
    const float basinCarve = smoothstep(0.54f, 0.82f, 1.0f - in.basinNoise) * 0.10f;

    const float baseSignal =
        0.46f * in.continental +
        0.18f * in.plainsBase +
        0.08f * in.detail +
        macroOffset +
        rollingOffset +
        plateauBoost -
        basinCarve;

    return std::max(0.0f, baseSignal) * in.verticalScale * 0.74f;
}

} // namespace terrain
