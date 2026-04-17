#include "plains.h"

namespace terrain
{

float computePlainsHeight(const PlainsInput& in)
{
    return (0.62f * in.continental + 0.28f * in.plainsBase + 0.10f * in.detail) *
           in.verticalScale * 0.56f;
}

} // namespace terrain
