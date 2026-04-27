#ifndef TERRAIN_PLAINS_H
#define TERRAIN_PLAINS_H

#include "../graph/types.h"

namespace terrain {

using PlainsParams = graph::PlainsParams;

struct PlainsInput {
    float continental = 0.0f;
    float plainsBase = 0.0f;
    float macroRelief = 0.5f;
    float hilliness = 0.5f;
    float basinNoise = 0.5f;
    float detail = 0.0f;
    float verticalScale = 1.0f;
    PlainsParams params;
};

float computePlainsHeight(const PlainsInput& in);

} // namespace terrain

#endif // TERRAIN_PLAINS_H
