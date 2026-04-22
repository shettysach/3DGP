#include "terrain_noise.h"

namespace terrain {

TerrainNoiseComputation computeTerrainNoiseComputation(const TerrainNoiseInput& in) {
    const float continental = 0.5f * (in.fbm(in.sampleX, in.sampleZ, in.octaves, in.lacunarity, in.gain) + 1.0f);
    const float detail = 0.5f * (in.fbm(in.sampleX * 2.7f, in.sampleZ * 2.7f, 4, 2.0f, 0.5f) + 1.0f);

    return {continental, detail};
}

} // namespace terrain
