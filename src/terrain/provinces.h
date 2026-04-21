#ifndef TERRAIN_PROVINCES_H
#define TERRAIN_PROVINCES_H

#include "fields.h"
#include "../terrain.h"

namespace terrain
{

void computeProvinceFields(TerrainFields& fields, const TerrainSettings& settings);

} // namespace terrain

#endif // TERRAIN_PROVINCES_H
