#ifndef TERRAIN_VALLEYS_H
#define TERRAIN_VALLEYS_H

#include "../graph/types.h"

namespace terrain {

using ValleyParams = graph::ValleyParams;

struct ValleyInput {
    float continental = 0.0f;
    float basin = 0.0f;
    float detail = 0.0f;
    float rimMask = 0.0f;
    float verticalScale = 1.0f;
    ValleyParams params;
};

struct ValleyResult {
    float depth = 0.0f;
    float weight = 0.0f;
};

ValleyResult computeValley(const ValleyInput& in);

} // namespace terrain

#endif // TERRAIN_VALLEYS_H
