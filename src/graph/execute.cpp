#include "graph/execute.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <variant>
#include <vector>

#include "../terrain/blending.h"
#include "../terrain/fields.h"
#include "../terrain/mountains.h"
#include "../terrain/plains.h"
#include "../terrain/plateaus.h"
#include "../terrain/util.h"
#include "../terrain/valleys.h"
#include "graph/types.h"

namespace graph {

using terrain::BlendResult;
using terrain::blendTerrain;
using terrain::computeMountain;
using terrain::computePlainsHeight;
using terrain::computePlateau;
using terrain::computeValley;
using terrain::fieldIndex;
using terrain::MountainResult;
using terrain::PlateauResult;
using terrain::smoothstep;
using terrain::ValleyResult;

using NodeOutput =
    std::variant<std::monostate, std::vector<float>, std::vector<Vec2>>;

terrain::TerrainFields execute(
    const CompiledGraph& compiled,
    const terrain::TerrainSettings& settings,
    const terrain::NoiseContext& noiseContext
) {
    const size_t N = compiled.nodes.size();
    const int w = settings.width;
    const int d = settings.depth;
    const float hScale = settings.horizontalScale;
    const size_t cellCount = static_cast<size_t>(w) * static_cast<size_t>(d);

    size_t rootIdx = 0;
    for (size_t i = 0; i < N; ++i) {
        if (compiled.nodes[i].kind == NodeKind::TerrainSynthesis) {
            rootIdx = i;
            break;
        }
    }

    std::vector<NodeOutput> nodeOutputs(N);
    std::vector<bool> visited(N, false);
    terrain::TerrainFields fields(w, d);

    const float centerX = static_cast<float>(w - 1) * 0.5f * hScale;
    const float centerZ = static_cast<float>(d - 1) * 0.5f * hScale;
    const float maxRadius =
        std::min(static_cast<float>(w - 1), static_cast<float>(d - 1)) * 0.5f
        * hScale;
    std::function<void(size_t)> eval = [&](size_t ni) {
        if (visited[ni])
            return;

        const CompiledNode& cn = compiled.nodes[ni];

        for (const auto& src : cn.inputs) {
            if (src.has_value())
                eval(*src);
        }

        switch (cn.kind) {
            case NodeKind::Fbm:
            case NodeKind::RidgedFbm:
            case NodeKind::FractalPerlin:
            case NodeKind::Perlin:
            case NodeKind::Simplex: {
                const auto& np = std::get<NoiseParams>(cn.params);

                auto noiseVal = [&](float wx, float wz) -> float {
                    switch (cn.kind) {
                        case NodeKind::Fbm:
                            return noiseContext.fbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.frequency
                            );
                        case NodeKind::RidgedFbm:
                            return noiseContext.ridgedFbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.sharpness,
                                np.frequency
                            );
                        case NodeKind::FractalPerlin:
                            return noiseContext.perlinFbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.frequency
                            );
                        case NodeKind::Perlin:
                            return noiseContext.perlin2D(
                                wx * np.frequency,
                                wz * np.frequency
                            );
                        default: // Simplex
                            return noiseContext.simplex2D(
                                wx * np.frequency,
                                wz * np.frequency
                            );
                    }
                };

                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);

                if (!cn.inputs.empty() && cn.inputs[0].has_value()) {
                    const auto& coordBuf =
                        std::get<std::vector<Vec2>>(nodeOutputs[*cn.inputs[0]]);
                    for (int z = 0; z < d; ++z) {
                        for (int x = 0; x < w; ++x) {
                            const size_t idx = fieldIndex(x, z, w);
                            const float wx = coordBuf[idx].x + np.xOffset;
                            const float wz = coordBuf[idx].y + np.zOffset;
                            out[idx] = 0.5f * (noiseVal(wx, wz) + 1.0f);
                        }
                    }
                } else {
                    for (int z = 0; z < d; ++z) {
                        for (int x = 0; x < w; ++x) {
                            const size_t idx = fieldIndex(x, z, w);
                            const float wx =
                                static_cast<float>(x) * hScale + np.xOffset;
                            const float wz =
                                static_cast<float>(z) * hScale + np.zOffset;
                            out[idx] = 0.5f * (noiseVal(wx, wz) + 1.0f);
                        }
                    }
                }
                break;
            }

            case NodeKind::Position: {
                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        out[idx] = Vec2 {
                            static_cast<float>(x) * hScale,
                            static_cast<float>(z) * hScale
                        };
                    }
                }
                break;
            }

            case NodeKind::CreateVec2: {
                const auto& cp = std::get<CreateVec2Params>(cn.params);
                const bool hasX = cn.inputs[0].has_value();
                const bool hasY = cn.inputs[1].has_value();
                const float* px = nullptr;
                const float* py = nullptr;
                if (hasX)
                    px =
                        std::get<std::vector<float>>(nodeOutputs[*cn.inputs[0]])
                            .data();
                if (hasY)
                    py =
                        std::get<std::vector<float>>(nodeOutputs[*cn.inputs[1]])
                            .data();

                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = Vec2 {
                        px ? px[i] + cp.x : cp.x,
                        py ? py[i] + cp.y : cp.y
                    };
                }
                break;
            }

            case NodeKind::Add2: {
                const auto& a =
                    std::get<std::vector<Vec2>>(nodeOutputs[*cn.inputs[0]]);
                const auto& b =
                    std::get<std::vector<Vec2>>(nodeOutputs[*cn.inputs[1]]);
                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = Vec2 {a[i].x + b[i].x, a[i].y + b[i].y};
                }
                break;
            }

            case NodeKind::TerrainSynthesis: {
                const auto& tsp = std::get<TerrainSynthesisParams>(cn.params);
                const float vertScale = tsp.verticalScale;

                const std::vector<float>& continental =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[0]]);
                const std::vector<float>& ridges =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[1]]);
                const std::vector<float>& detailIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[2]]);
                const std::vector<float>& rangeMaskIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[3]]);
                const std::vector<float>& basinIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[4]]);
                const std::vector<float>& detailBandIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[5]]);
                const std::vector<float>& rimMaskIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[6]]);
                const std::vector<float>& plainsBaseIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[7]]);
                const std::vector<float>& macroReliefIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[8]]);
                const std::vector<float>& hillinessIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[9]]);
                const std::vector<float>& basinNoiseIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[10]]);
                const std::vector<float>& plateauMaskIn =
                    std::get<std::vector<float>>(nodeOutputs[*cn.inputs[11]]);

                fields.heights.assign(cellCount, 0.0f);
                fields.mountainWeights.assign(cellCount, 0.0f);
                fields.valleyWeights.assign(cellCount, 0.0f);
                fields.plateauWeights.assign(cellCount, 0.0f);
                fields.sampleXs.assign(cellCount, 0.0f);
                fields.sampleZs.assign(cellCount, 0.0f);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float worldX = static_cast<float>(x) * hScale;
                        const float worldZ = static_cast<float>(z) * hScale;

                        fields.sampleXs[idx] = worldX;
                        fields.sampleZs[idx] = worldZ;

                        const float detail = detailIn[idx];
                        const float rangeMask =
                            smoothstep(0.42f, 0.72f, rangeMaskIn[idx]);
                        const float basin = basinIn[idx];
                        const float detailBand = detailBandIn[idx];
                        const float rimMask =
                            smoothstep(0.38f, 0.74f, rimMaskIn[idx]);
                        const float plainsBase = plainsBaseIn[idx];
                        const float macroRelief = macroReliefIn[idx];
                        const float hilliness = hillinessIn[idx];
                        const float basinNoise = basinNoiseIn[idx];
                        const float plateauMask = plateauMaskIn[idx];

                        const float slopeHint = std::clamp(
                            (ridges[idx] - 0.35f) * 1.55f + detail * 0.2f,
                            0.0f,
                            1.0f
                        );

                        const MountainResult mountain = computeMountain(
                            {continental[idx],
                             ridges[idx],
                             detail,
                             slopeHint,
                             rangeMask,
                             vertScale}
                        );

                        const float valleySlopeHint = std::clamp(
                            (0.62f - basin) * 1.35f + detail * 0.22f,
                            0.0f,
                            1.0f
                        );

                        const ValleyResult valley = computeValley(
                            {continental[idx],
                             basin,
                             detailBand,
                             valleySlopeHint,
                             rimMask,
                             vertScale}
                        );

                        const float plainsHeight = computePlainsHeight(
                            {continental[idx],
                             plainsBase,
                             macroRelief,
                             hilliness,
                             basinNoise,
                             detail,
                             vertScale}
                        );

                        const PlateauResult plateau = computePlateau(
                            {continental[idx], plateauMask, detail, vertScale}
                        );

                        const BlendResult blend = blendTerrain(
                            {mountain.height,
                             mountain.weight,
                             plainsHeight,
                             plateau.height,
                             plateau.weight,
                             valley.depth,
                             detail,
                             vertScale}
                        );

                        fields.heights[idx] = blend.height;
                        fields.mountainWeights[idx] = blend.mountainWeight;
                        fields.valleyWeights[idx] = valley.weight;
                        fields.plateauWeights[idx] = plateau.weight;
                    }
                }
                break;
            }
        }
        visited[ni] = true;
    };

    eval(rootIdx);
    return fields;
}

} // namespace graph
