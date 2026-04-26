#ifndef TERRAIN_PLAINS_H
#define TERRAIN_PLAINS_H

namespace terrain {

struct PlainsInput {
    float continental = 0.0f;
    float plainsBase = 0.0f;
    float macroRelief = 0.5f;
    float hilliness = 0.5f;
    float basinNoise = 0.5f;
    float detail = 0.0f;
    float verticalScale = 1.0f;
};

float computePlainsHeight(const PlainsInput& in);

} // namespace terrain

#endif // TERRAIN_PLAINS_H
