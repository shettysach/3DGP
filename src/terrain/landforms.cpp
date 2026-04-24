#include "landforms.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace terrain {

namespace {

LandformId classifyLandform(float elevationNorm, float temperature, float slope, float signal) {
    if (elevationNorm < 0.22f && slope < 0.09f && signal < 0.30f) {
        return LandformId::Lowland;
    }
    if (elevationNorm < 0.46f && slope < 0.14f && signal < 0.42f) {
        return LandformId::Valley;
    }
    if (elevationNorm > 0.88f && temperature < 0.32f) {
        return LandformId::Snowcap;
    }
    if (elevationNorm > 0.74f && (temperature < 0.42f || signal > 0.74f)) {
        return LandformId::Alpine;
    }
    if (signal > 0.67f || (elevationNorm > 0.60f && slope > 0.12f)) {
        return LandformId::Mountain;
    }
    if (signal > 0.42f || (elevationNorm > 0.42f && slope > 0.07f)) {
        return LandformId::Foothill;
    }
    return LandformId::Plain;
}

int maxAllowedLevel(float elevationNorm, float temperature, float signal) {
    int maxLevel = static_cast<int>(LandformId::Plain);
    if (elevationNorm < 0.46f && signal < 0.42f) {
        maxLevel = static_cast<int>(LandformId::Valley);
    }
    if (signal > 0.40f || elevationNorm > 0.38f) {
        maxLevel = static_cast<int>(LandformId::Foothill);
    }
    if (signal > 0.63f || elevationNorm > 0.58f) {
        maxLevel = static_cast<int>(LandformId::Mountain);
    }
    if (elevationNorm > 0.72f && (temperature < 0.45f || signal > 0.72f)) {
        maxLevel = static_cast<int>(LandformId::Alpine);
    }
    if (elevationNorm > 0.86f && temperature < 0.34f) {
        maxLevel = static_cast<int>(LandformId::Snowcap);
    }
    return maxLevel;
}

void blurScalarField(const std::vector<float>& src, std::vector<float>& dst, int width, int depth) {
    dst.resize(src.size());
    for (int z = 0; z < depth; ++z) {
        const int z0 = std::max(0, z - 1);
        const int z1 = std::min(depth - 1, z + 1);
        for (int x = 0; x < width; ++x) {
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(width - 1, x + 1);
            float sum = 0.0f;
            float weight = 0.0f;
            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const float sampleWeight = (nx == x && nz == z) ? 2.0f : 1.0f;
                    sum += src[fieldIndex(nx, nz, width)] * sampleWeight;
                    weight += sampleWeight;
                }
            }
            dst[fieldIndex(x, z, width)] = sum / std::max(0.0001f, weight);
        }
    }
}

} // namespace

void computeLandformFields(TerrainFields& fields) {
    if (fields.heights.empty()) {
        return;
    }

    const float invHeightRange = 1.0f / std::max(0.0001f, fields.maxHeight - fields.minHeight);
    const size_t cellCount = fields.size();

    std::vector<float> rawSignal(cellCount, 0.0f);
    for (size_t idx = 0; idx < cellCount; ++idx) {
        const float elevationNorm = (fields.heights[idx] - fields.minHeight) * invHeightRange;
        const float slope = std::clamp(fields.slopes[idx], 0.0f, 1.0f);
        const float valleyPull = std::clamp(fields.valleyWeights[idx], 0.0f, 1.0f);
        rawSignal[idx] = std::clamp(
            fields.mountainWeights[idx] * 0.72f +
                valleyPull * 0.30f +
                smoothstep(0.04f, 0.36f, slope) * 0.35f +
                smoothstep(0.35f, 0.82f, elevationNorm) * 0.38f,
            0.0f,
            1.0f);
    }

    std::vector<float> blurredSignal;
    blurScalarField(rawSignal, blurredSignal, fields.width, fields.depth);
    blurScalarField(blurredSignal, fields.landformSignal, fields.width, fields.depth);

    std::vector<uint8_t> levels(cellCount, static_cast<uint8_t>(LandformId::Plain));
    std::vector<float> elevationNorms(cellCount, 0.0f);
    for (size_t idx = 0; idx < cellCount; ++idx) {
        elevationNorms[idx] = (fields.heights[idx] - fields.minHeight) * invHeightRange;
        levels[idx] = static_cast<uint8_t>(classifyLandform(
            elevationNorms[idx],
            fields.temperature[idx],
            fields.slopes[idx],
            fields.landformSignal[idx]));
    }

    for (int iteration = 0; iteration < 2; ++iteration) {
        std::vector<uint8_t> nextLevels = levels;
        for (int z = 0; z < fields.depth; ++z) {
            const int z0 = std::max(0, z - 1);
            const int z1 = std::min(fields.depth - 1, z + 1);
            for (int x = 0; x < fields.width; ++x) {
                const int x0 = std::max(0, x - 1);
                const int x1 = std::min(fields.width - 1, x + 1);
                const size_t idx = fieldIndex(x, z, fields.width);

                int level = static_cast<int>(levels[idx]);
                int sum = level * 2;
                int count = 2;
                for (int nz = z0; nz <= z1; ++nz) {
                    for (int nx = x0; nx <= x1; ++nx) {
                        if (nx == x && nz == z) {
                            continue;
                        }
                        sum += static_cast<int>(levels[fieldIndex(nx, nz, fields.width)]);
                        ++count;
                    }
                }

                const int neighborAverage = (count > 0) ? (sum / count) : 0;
                if (level > neighborAverage + 1) {
                    --level;
                } else if (level < neighborAverage - 1) {
                    ++level;
                }

                level = std::min(level, maxAllowedLevel(elevationNorms[idx], fields.temperature[idx], fields.landformSignal[idx]));
                if (fields.valleyWeights[idx] > 0.45f && level > static_cast<int>(LandformId::Valley) && level < static_cast<int>(LandformId::Mountain)) {
                    level = static_cast<int>(LandformId::Valley);
                }
                if (level == static_cast<int>(LandformId::Lowland) &&
                    !(elevationNorms[idx] < 0.30f && fields.slopes[idx] < 0.12f)) {
                    level = static_cast<int>(LandformId::Plain);
                }
                nextLevels[idx] = static_cast<uint8_t>(std::clamp(level, 0, static_cast<int>(LandformId::Snowcap)));
            }
        }
        levels.swap(nextLevels);
    }

    fields.landformIds = levels;
}

} // namespace terrain
