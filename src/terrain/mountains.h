#ifndef TERRAIN_MOUNTAINS_H
#define TERRAIN_MOUNTAINS_H

namespace terrain {

struct MountainInput {
    float continental = 0.0f;
    float ridges = 0.0f;
    float detail = 0.0f;
    float slopeHint = 0.0f;
    float rangeMask = 0.0f;
    float verticalScale = 1.0f;
};

struct MountainResult {
    float height = 0.0f;
    float weight = 0.0f;
};

MountainResult computeMountain(const MountainInput& in);

} // namespace terrain

#endif // TERRAIN_MOUNTAINS_H
