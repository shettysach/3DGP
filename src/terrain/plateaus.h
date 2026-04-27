#ifndef TERRAIN_PLATEAUS_H
#define TERRAIN_PLATEAUS_H

#include "../graph/types.h"

namespace terrain {

using PlateauParams = graph::PlateauParams;

struct PlateauInput {
    float continental = 0.0f;
    float plateauMask = 0.0f;
    float detail = 0.0f;
    float verticalScale = 1.0f;
    PlateauParams params;
};

struct PlateauResult {
    float height = 0.0f;
    float weight = 0.0f;
};

PlateauResult computePlateau(const PlateauInput& in);

} // namespace terrain

#endif // TERRAIN_PLATEAUS_H
