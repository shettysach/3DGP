#ifndef TERRAIN_FIELDS_H
#define TERRAIN_FIELDS_H

#include "../terrain.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace terrain {

struct TerrainFields {
    int width = 0;
    int depth = 0;
    std::vector<float> heights;
    std::vector<float> mountainWeights;
    std::vector<float> riverWeights;
    std::vector<float> slopes;
    std::vector<float> temperature;
    std::vector<float> precipitation;
    std::vector<float> moisture;
    std::vector<float> sampleXs;
    std::vector<float> sampleZs;
    std::vector<uint8_t> landformIds;
    std::vector<float> landformSignal;
    std::vector<uint8_t> ecologyIds;
    std::vector<uint8_t> primaryBiomeIds;
    std::vector<uint8_t> secondaryBiomeIds;
    std::vector<float> primaryBiomeWeights;
    std::vector<float> secondaryBiomeWeights;

    TerrainFields() = default;

    TerrainFields(int widthIn, int depthIn) {
        resize(widthIn, depthIn);
    }

    void resize(int widthIn, int depthIn) {
        width = widthIn;
        depth = depthIn;
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(depth);
        heights.assign(count, 0.0f);
        mountainWeights.assign(count, 0.0f);
        riverWeights.assign(count, 0.0f);
        slopes.assign(count, 0.0f);
        temperature.assign(count, 0.5f);
        precipitation.assign(count, 0.5f);
        moisture.assign(count, 0.5f);
        sampleXs.assign(count, 0.0f);
        sampleZs.assign(count, 0.0f);
        landformIds.assign(count, static_cast<uint8_t>(LandformId::Plain));
        landformSignal.assign(count, 0.0f);
        ecologyIds.assign(count, static_cast<uint8_t>(EcologyId::Grassland));
        primaryBiomeIds.assign(count, static_cast<uint8_t>(BiomeId::GrasslandPlain));
        secondaryBiomeIds.assign(count, static_cast<uint8_t>(BiomeId::GrasslandPlain));
        primaryBiomeWeights.assign(count, 1.0f);
        secondaryBiomeWeights.assign(count, 0.0f);
    }

    size_t size() const {
        return heights.size();
    }
};

inline size_t fieldIndex(int x, int z, int width) {
    return static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x);
}

} // namespace terrain

#endif // TERRAIN_FIELDS_H
