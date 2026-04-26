#ifndef GRAPH_EXECUTE_H
#define GRAPH_EXECUTE_H

#include "graph/types.h"
#include "../terrain.h"
#include "../terrain/terrain_noise.h"

namespace graph {

terrain::TerrainFields execute(
    const CompiledGraph& compiled,
    const terrain::TerrainSettings& settings,
    const terrain::NoiseContext& noiseContext);

} // namespace graph

#endif // GRAPH_EXECUTE_H
