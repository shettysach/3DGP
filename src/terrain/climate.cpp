#include "../terrain.h"
#include "fields.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace terrain {

namespace {

std::vector<int> buildRiverDistanceField(const TerrainFields& fields, int maxRadius) {
    const int cappedRadius = std::max(1, maxRadius);
    const int unreachableDistance = cappedRadius + 1;
    std::vector<int> riverDistance(fields.size(), unreachableDistance);
    std::queue<size_t> q;

    for (size_t idx = 0; idx < fields.size(); ++idx) {
        if (fields.riverWeights[idx] > 0.02f) {
            riverDistance[idx] = 0;
            q.push(idx);
        }
    }

    const int neighborDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int neighborDz[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    while (!q.empty()) {
        const size_t cur = q.front();
        q.pop();

        const int nextDistance = riverDistance[cur] + 1;
        if (nextDistance > cappedRadius) {
            continue;
        }

        const int cx = static_cast<int>(cur % fields.width);
        const int cz = static_cast<int>(cur / fields.width);
        for (int i = 0; i < 8; ++i) {
            const int nx = cx + neighborDx[i];
            const int nz = cz + neighborDz[i];
            if (nx < 0 || nx >= fields.width || nz < 0 || nz >= fields.depth) {
                continue;
            }

            const size_t nidx = fieldIndex(nx, nz, fields.width);
            if (nextDistance >= riverDistance[nidx]) {
                continue;
            }

            riverDistance[nidx] = nextDistance;
            q.push(nidx);
        }
    }

    return riverDistance;
}

} // namespace

void TerrainGenerator::computeClimateFields(TerrainFields& fields) const {
    if (fields.heights.empty()) {
        return;
    }

    const float invHeightRange = 1.0f / std::max(0.0001f, fields.maxHeight - fields.minHeight);
    const std::vector<int> riverDistance = buildRiverDistanceField(fields, settings_.climate.riverMoistureRadius);
    const float baseFrequency = std::max(0.00001f, settings_.noise.frequency);
    const float temperatureScale = settings_.climate.temperatureFrequency / baseFrequency;
    const float precipitationScale = settings_.climate.precipitationFrequency / baseFrequency;
    const float moistureScale = settings_.climate.moistureFrequency / baseFrequency;

    for (int z = 0; z < fields.depth; ++z) {
        const float latitude = fields.depth > 1
                                   ? static_cast<float>(z) / static_cast<float>(fields.depth - 1)
                                   : 0.5f;
        for (int x = 0; x < fields.width; ++x) {
            const size_t idx = fieldIndex(x, z, fields.width);

            const float sampleX = fields.sampleXs[idx];
            const float sampleZ = fields.sampleZs[idx];
            const float elevationNorm = (fields.heights[idx] - fields.minHeight) * invHeightRange;
            const float slope = fields.slopes[idx];
            const float riverDist = static_cast<float>(riverDistance[idx]);
            const float riverWeight = std::clamp(fields.riverWeights[idx], 0.0f, 1.0f);
            const float mountainWeight = std::clamp(fields.mountainWeights[idx], 0.0f, 1.0f);
            const float latitudeWarmth = 1.0f - std::fabs(latitude * 2.0f - 1.0f);

            const float temperatureNoise = 0.5f * (noiseContext_.fbm(
                                                       sampleX * temperatureScale + 181.4f,
                                                       sampleZ * temperatureScale - 93.7f,
                                                       settings_.climate.temperatureOctaves,
                                                       settings_.noise.lacunarity,
                                                       settings_.noise.gain,
                                                       baseFrequency) +
                                                   1.0f);
            float temperature =
                temperatureNoise * (1.0f - settings_.climate.latitudeStrength) + latitudeWarmth * settings_.climate.latitudeStrength;
            temperature -= elevationNorm * settings_.climate.temperatureLapseRate;
            temperature = std::clamp(temperature, 0.0f, 1.0f);

            const float precipitationNoise = 0.5f * (noiseContext_.fbm(
                                                         sampleX * precipitationScale - 211.8f,
                                                         sampleZ * precipitationScale + 75.3f,
                                                         settings_.climate.precipitationOctaves,
                                                         settings_.noise.lacunarity,
                                                         settings_.noise.gain,
                                                         baseFrequency) +
                                                     1.0f);
            const float precipitationBands = 0.5f * (noiseContext_.fbm(
                                                         sampleX * precipitationScale * 0.55f - 517.0f,
                                                         sampleZ * precipitationScale * 0.55f + 142.0f,
                                                         std::max(2, settings_.climate.precipitationOctaves - 1),
                                                         settings_.noise.lacunarity,
                                                         settings_.noise.gain,
                                                         baseFrequency) +
                                                     1.0f);
            float precipitation = precipitationNoise * 0.72f + precipitationBands * 0.20f;
            precipitation += mountainWeight * settings_.climate.orographicPrecipitationStrength * smoothstep(0.05f, 0.7f, slope);
            precipitation += latitudeWarmth * 0.08f;
            precipitation -= elevationNorm * 0.06f;
            precipitation = std::clamp(precipitation, 0.0f, 1.0f);

            const float localMoistureNoise = 0.5f * (noiseContext_.fbm(
                                                         sampleX * moistureScale + 63.5f,
                                                         sampleZ * moistureScale + 194.8f,
                                                         settings_.climate.moistureOctaves,
                                                         settings_.noise.lacunarity,
                                                         settings_.noise.gain,
                                                         baseFrequency) +
                                                     1.0f);
            const float riverRadius = static_cast<float>(std::max(1, settings_.climate.riverMoistureRadius));
            const float riverInfluence = 1.0f - smoothstep(0.0f, riverRadius, riverDist);
            const float flatness = 1.0f - std::clamp(slope / 1.25f, 0.0f, 1.0f);
            const float basinBonus = flatness * (1.0f - elevationNorm) * 0.12f;

            const float mountainAridity =
                (1.0f - smoothstep(0.28f, 0.72f, slope)) *
                (1.0f - smoothstep(0.60f, 0.88f, elevationNorm)) *
                mountainWeight * 0.18f;

            const float valleyHumidity = smoothstep(0.18f, 0.72f, 1.0f - elevationNorm) * riverInfluence * 0.08f;

            float moisture = precipitation * 0.68f + localMoistureNoise * 0.14f;
            moisture += riverInfluence * settings_.climate.riverMoistureStrength + riverWeight * 0.08f + basinBonus + valleyHumidity;
            moisture -= mountainAridity;
            moisture -= std::clamp(slope, 0.0f, 1.0f) * settings_.climate.slopeDryingStrength;
            moisture -= temperature * settings_.climate.temperatureDryingStrength;
            moisture = std::clamp(moisture, 0.0f, 1.0f);

            fields.temperature[idx] = temperature;
            fields.precipitation[idx] = precipitation;
            fields.moisture[idx] = moisture;
        }
    }
}

} // namespace terrain
