#include "provinces.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace terrain
{

namespace
{

struct ProvinceCenter
{
    float x = 0.0f;
    float z = 0.0f;
    uint16_t id = 0u;
};

uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float hash01(uint32_t seed, uint32_t a, uint32_t b)
{
    return static_cast<float>(hash32(seed ^ (a * 0x9e3779b9u) ^ (b * 0x85ebca6bu)) & 0xffffu) / 65535.0f;
}

} // namespace

void computeProvinceFields(TerrainFields& fields, const TerrainSettings& settings)
{
    if (fields.heights.empty())
    {
        return;
    }

    const float aspect = static_cast<float>(settings.width) / static_cast<float>(std::max(1, settings.depth));
    const int targetProvinceCount = std::clamp(
        static_cast<int>(std::round(std::sqrt(static_cast<float>(settings.width * settings.depth)) / 28.0f)),
        6,
        14);
    const int provinceGridX = std::max(2, static_cast<int>(std::round(std::sqrt(static_cast<float>(targetProvinceCount) * aspect))));
    const int provinceGridZ = std::max(2, (targetProvinceCount + provinceGridX - 1) / provinceGridX);

    const float worldWidth = static_cast<float>(settings.width - 1) * settings.horizontalScale;
    const float worldDepth = static_cast<float>(settings.depth - 1) * settings.horizontalScale;
    const float cellWidth = worldWidth / static_cast<float>(provinceGridX);
    const float cellDepth = worldDepth / static_cast<float>(provinceGridZ);

    std::vector<ProvinceCenter> centers;
    centers.reserve(static_cast<size_t>(provinceGridX * provinceGridZ));
    uint16_t nextId = 0u;
    for (int gz = 0; gz < provinceGridZ; ++gz)
    {
        for (int gx = 0; gx < provinceGridX; ++gx)
        {
            const float jitterX = hash01(settings.seed, static_cast<uint32_t>(gx), static_cast<uint32_t>(gz)) * 0.7f - 0.35f;
            const float jitterZ = hash01(settings.seed ^ 0x51f15e5du, static_cast<uint32_t>(gx), static_cast<uint32_t>(gz)) * 0.7f - 0.35f;
            ProvinceCenter center;
            center.x = (static_cast<float>(gx) + 0.5f + jitterX) * cellWidth;
            center.z = (static_cast<float>(gz) + 0.5f + jitterZ) * cellDepth;
            center.id = nextId++;
            centers.push_back(center);
        }
    }

    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        const float sampleX = fields.sampleXs[idx];
        const float sampleZ = fields.sampleZs[idx];

        float bestDistanceSq = std::numeric_limits<float>::max();
        uint16_t bestProvince = 0u;
        for (const ProvinceCenter& center : centers)
        {
            const float dx = sampleX - center.x;
            const float dz = sampleZ - center.z;
            const float distanceSq = dx * dx + dz * dz;
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestProvince = center.id;
            }
        }

        fields.provinceIds[idx] = bestProvince;
    }

    std::fill(fields.provinceEdgeDistance.begin(), fields.provinceEdgeDistance.end(), 255u);
    std::queue<size_t> q;
    for (int z = 0; z < fields.depth; ++z)
    {
        for (int x = 0; x < fields.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, fields.width);
            const uint16_t province = fields.provinceIds[idx];
            bool isEdge = false;
            if (x > 0 && fields.provinceIds[fieldIndex(x - 1, z, fields.width)] != province)
            {
                isEdge = true;
            }
            if (x + 1 < fields.width && fields.provinceIds[fieldIndex(x + 1, z, fields.width)] != province)
            {
                isEdge = true;
            }
            if (z > 0 && fields.provinceIds[fieldIndex(x, z - 1, fields.width)] != province)
            {
                isEdge = true;
            }
            if (z + 1 < fields.depth && fields.provinceIds[fieldIndex(x, z + 1, fields.width)] != province)
            {
                isEdge = true;
            }

            if (isEdge)
            {
                fields.provinceEdgeDistance[idx] = 0u;
                q.push(idx);
            }
        }
    }

    const int neighborDx[4] = {-1, 1, 0, 0};
    const int neighborDz[4] = {0, 0, -1, 1};
    while (!q.empty())
    {
        const size_t cur = q.front();
        q.pop();

        const uint8_t nextDistance = static_cast<uint8_t>(std::min<int>(255, fields.provinceEdgeDistance[cur] + 1));
        const int cx = static_cast<int>(cur % static_cast<size_t>(fields.width));
        const int cz = static_cast<int>(cur / static_cast<size_t>(fields.width));
        for (int i = 0; i < 4; ++i)
        {
            const int nx = cx + neighborDx[i];
            const int nz = cz + neighborDz[i];
            if (nx < 0 || nx >= fields.width || nz < 0 || nz >= fields.depth)
            {
                continue;
            }

            const size_t nidx = fieldIndex(nx, nz, fields.width);
            if (nextDistance >= fields.provinceEdgeDistance[nidx])
            {
                continue;
            }

            fields.provinceEdgeDistance[nidx] = nextDistance;
            q.push(nidx);
        }
    }
}

} // namespace terrain
