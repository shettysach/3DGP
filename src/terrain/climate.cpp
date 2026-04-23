#include "climate.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

        const int cx = static_cast<int>(cur % static_cast<size_t>(fields.width));
        const int cz = static_cast<int>(cur / static_cast<size_t>(fields.width));
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

ClimateSample computeClimateSample(const ClimateNoiseInput& in) {
    const float safeBaseFrequency = std::max(0.00001f, in.baseFrequency);
    const float temperatureScale = in.settings.temperatureFrequency / safeBaseFrequency;
    const float precipitationScale = in.settings.precipitationFrequency / safeBaseFrequency;
    const float moistureScale = in.settings.moistureFrequency / safeBaseFrequency;

    const float latitudeWarmth = 1.0f - std::fabs(in.latitude * 2.0f - 1.0f);

    const float temperatureNoise = 0.5f * (in.fbm(
                                               in.sampleX * temperatureScale + 181.4f,
                                               in.sampleZ * temperatureScale - 93.7f,
                                               in.settings.temperatureOctaves,
                                               in.lacunarity,
                                               in.gain) +
                                           1.0f);
    float temperature =
        temperatureNoise * (1.0f - in.settings.latitudeStrength) + latitudeWarmth * in.settings.latitudeStrength;
    temperature -= in.elevationNorm * in.settings.temperatureLapseRate;
    temperature = std::clamp(temperature, 0.0f, 1.0f);

    const float precipitationNoise = 0.5f * (in.fbm(
                                                 in.sampleX * precipitationScale - 211.8f,
                                                 in.sampleZ * precipitationScale + 75.3f,
                                                 in.settings.precipitationOctaves,
                                                 in.lacunarity,
                                                 in.gain) +
                                             1.0f);
    const float precipitationBands = 0.5f * (in.fbm(
                                                 in.sampleX * precipitationScale * 0.55f - 517.0f,
                                                 in.sampleZ * precipitationScale * 0.55f + 142.0f,
                                                 std::max(2, in.settings.precipitationOctaves - 1),
                                                 in.lacunarity,
                                                 in.gain) +
                                             1.0f);
    float precipitation = precipitationNoise * 0.72f + precipitationBands * 0.20f;
    precipitation += in.mountainWeight * in.settings.orographicPrecipitationStrength * smoothstep(0.05f, 0.7f, in.slope);
    precipitation += latitudeWarmth * 0.08f;
    precipitation -= in.elevationNorm * 0.06f;
    precipitation = std::clamp(precipitation, 0.0f, 1.0f);

    const float localMoistureNoise = 0.5f * (in.fbm(
                                              in.sampleX * moistureScale + 63.5f,
                                              in.sampleZ * moistureScale + 194.8f,
                                              in.settings.moistureOctaves,
                                              in.lacunarity,
                                              in.gain) +
                                          1.0f);
    const float riverRadius = static_cast<float>(std::max(1, in.settings.riverMoistureRadius));
    const float riverInfluence = 1.0f - smoothstep(0.0f, riverRadius, in.riverDistance);
    const float flatness = 1.0f - std::clamp(in.slope / 1.25f, 0.0f, 1.0f);
    const float basinBonus = flatness * (1.0f - in.elevationNorm) * 0.12f;

    const float mountainAridity =
        (1.0f - smoothstep(0.28f, 0.72f, in.slope)) *
        (1.0f - smoothstep(0.60f, 0.88f, in.elevationNorm)) *
        in.mountainWeight * 0.18f;

    const float valleyHumidity = smoothstep(0.18f, 0.72f, 1.0f - in.elevationNorm) * riverInfluence * 0.08f;

    float moisture = precipitation * 0.68f + localMoistureNoise * 0.14f;
    moisture += riverInfluence * in.settings.riverMoistureStrength + in.riverWeight * 0.08f + basinBonus + valleyHumidity;
    moisture -= mountainAridity;
    moisture -= std::clamp(in.slope, 0.0f, 1.0f) * in.settings.slopeDryingStrength;
    moisture -= temperature * in.settings.temperatureDryingStrength;
    moisture = std::clamp(moisture, 0.0f, 1.0f);

    return {temperature, precipitation, moisture};
}

void computeClimateFields(
    TerrainFields& fields,
    const TerrainSettings& terrainSettings,
    std::function<float(float, float, int, float, float)> fbm) {
    if (fields.heights.empty()) {
        return;
    }

    float minHeight, maxHeight;
    computeHeightExtents(fields.heights, minHeight, maxHeight);

    const float invHeightRange = 1.0f / std::max(0.0001f, maxHeight - minHeight);
    const std::vector<int> riverDistance = buildRiverDistanceField(fields, terrainSettings.climate.riverMoistureRadius);
    const float baseFrequency = std::max(0.00001f, terrainSettings.noise.frequency);

    for (int z = 0; z < fields.depth; ++z) {
        const float latitude = fields.depth > 1
                                   ? static_cast<float>(z) / static_cast<float>(fields.depth - 1)
                                   : 0.5f;
        for (int x = 0; x < fields.width; ++x) {
            const size_t idx = fieldIndex(x, z, fields.width);
            const ClimateSample sample = computeClimateSample({
                fields.sampleXs[idx],
                fields.sampleZs[idx],
                (fields.heights[idx] - minHeight) * invHeightRange,
                fields.slopes[idx],
                static_cast<float>(riverDistance[idx]),
                std::clamp(fields.riverWeights[idx], 0.0f, 1.0f),
                std::clamp(fields.mountainWeights[idx], 0.0f, 1.0f),
                latitude,
                baseFrequency,
                terrainSettings.noise.lacunarity,
                terrainSettings.noise.gain,
                terrainSettings.climate,
                fbm,
            });
            fields.temperature[idx] = sample.temperature;
            fields.precipitation[idx] = sample.precipitation;
            fields.moisture[idx] = sample.moisture;
        }
    }
}

} // namespace terrain
