#ifndef TERRAIN_PROVINCES_H
#define TERRAIN_PROVINCES_H

#include "../terrain.h"
#include "fields.h"

namespace terrain {

void computeProvinceFields(TerrainFields& fields, const TerrainSettings& settings);

} // namespace terrain

#endif // TERRAIN_PROVINCES_H
