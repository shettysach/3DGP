#include "plateaus.h"
#include "util.h"

#include <algorithm>
#include <cmath>

namespace terrain {

PlateauResult computePlateau(const PlateauInput& in) {
    // Broad region where plateaus can exist
    const float regionSignal = std::clamp(
        in.plateauMask * 0.80f + in.continental * 0.20f,
        0.0f, 1.0f);

    // Tabletop mask — top portion of noise becomes plateau
    const float tableTop = smoothstep(0.54f, 0.64f, regionSignal);

    // Weight follows the tabletop directly for sharp cliffs
    const float weight = std::clamp(tableTop, 0.0f, 1.0f);

    // Mesa height: clearly above plains but well below mountain peaks
    const float baseHeight = tableTop * in.verticalScale * 0.42f;

    // Subtle surface detail so the top isn't perfectly flat
    const float surfaceDetail = (in.detail - 0.5f) * in.verticalScale * 0.025f;

    // Small rim boost right at the cliff edge for sharper visual cliffs
    const float rim = smoothstep(0.52f, 0.60f, regionSignal) *
                      (1.0f - smoothstep(0.60f, 0.68f, regionSignal));
    const float rimBoost = rim * in.verticalScale * 0.06f;

    float height = baseHeight + surfaceDetail + rimBoost;
    height = std::max(0.0f, height);

    return {height, weight};
}

} // namespace terrain
