#ifndef TERRAIN_VALLEYS_H
#define TERRAIN_VALLEYS_H

namespace terrain {

struct ValleyInput {
    float continental = 0.0f;
    float basin = 0.0f;
    float detail = 0.0f;
    float slopeHint = 0.0f;
    float rimMask = 0.0f;
    float verticalScale = 1.0f;
};

struct ValleyResult {
    float depth = 0.0f;
    float weight = 0.0f;
};

ValleyResult computeValley(const ValleyInput& in);

} // namespace terrain

#endif // TERRAIN_VALLEYS_H
