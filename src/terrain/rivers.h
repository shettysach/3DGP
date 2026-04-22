#ifndef TERRAIN_RIVERS_H
#define TERRAIN_RIVERS_H

#include "../terrain.h"

#include <vector>

namespace terrain {

struct RiverPassResult {
    std::vector<float> carvedHeights;
    std::vector<float> riverWeights;
};

RiverPassResult runRiverPass(
    const std::vector<float>& heights,
    int width,
    int depth,
    float verticalScale,
    const RiverSettings& settings,
    uint32_t seed);

} // namespace terrain

#endif // TERRAIN_RIVERS_H
