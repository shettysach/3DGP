#ifndef EXECUTE_H
#define EXECUTE_H

#include "../terrain.h"
#include "../terrain/terrain_noise.h"
#include "graph/types.h"

namespace graph {

terrain::TerrainFields execute(
    const CompiledGraph& compiled,
    const terrain::TerrainSettings& settings,
    const terrain::NoiseContext& noiseContext
);

} // namespace graph

#endif // EXECUTE_H
