#include "graph/compile.h"

#include <climits>
#include <stdexcept>
#include <unordered_map>

namespace graph {

CompiledGraph compile(const EditorGraph& editorGraph) {
    if (editorGraph.nodes.empty()) {
        throw std::runtime_error("Graph has no nodes");
    }

    std::unordered_map<NodeId, size_t> idToIndex;
    for (size_t i = 0; i < editorGraph.nodes.size(); ++i) {
        idToIndex[editorGraph.nodes[i].id] = i;
    }

    CompiledGraph compiled;
    compiled.nodes.resize(editorGraph.nodes.size());

    for (size_t i = 0; i < editorGraph.nodes.size(); ++i) {
        const auto& en = editorGraph.nodes[i];
        compiled.nodes[i].kind = en.kind;
        compiled.nodes[i].params = en.params;
        compiled.nodes[i].inputs.resize(nodeDefinition(en.kind).inputs.size(), UINT16_MAX);
    }

    for (const auto& link : editorGraph.links) {
        const size_t srcIdx = idToIndex.at(link.from.nodeId);
        const size_t dstIdx = idToIndex.at(link.to.nodeId);

        if (link.to.slot >= compiled.nodes[dstIdx].inputs.size()) {
            throw std::runtime_error("Link targets invalid input slot");
        }

        const NodeDef& srcDef = nodeDefinition(compiled.nodes[srcIdx].kind);
        const NodeDef& dstDef = nodeDefinition(compiled.nodes[dstIdx].kind);

        if (link.from.slot >= srcDef.outputs.size()) {
            throw std::runtime_error("Link connects from invalid output slot");
        }

        if (srcDef.outputs[link.from.slot].type != dstDef.inputs[link.to.slot].type) {
            throw std::runtime_error("Link connects incompatible pin types");
        }

        compiled.nodes[dstIdx].inputs[link.to.slot] = static_cast<uint16_t>(srcIdx);
    }

    for (auto& cn : compiled.nodes) {
        switch (cn.kind) {
        case NodeKind::Fbm:
        case NodeKind::RidgedFbm:
        case NodeKind::FractalPerlin:
        case NodeKind::Perlin:
        case NodeKind::Simplex:
            cn.hasCoordInput = (!cn.inputs.empty() && cn.inputs[0] != UINT16_MAX);
            break;
        default:
            break;
        }
    }

    return compiled;
}

} // namespace graph
