// for removal
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

} // namespace terrain
