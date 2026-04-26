#include "graph/graph_compile.h"

#include <stdexcept>
#include <unordered_map>

namespace graph {

CompiledGraph compile(const EditorGraph& editorGraph) {
    if (editorGraph.nodes.empty()) {
        throw std::runtime_error("Graph has no nodes");
    }

    // Map editor NodeId → compiled index (position in array)
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
        compiled.nodes[i].inputs.resize(nodeDefinition(en.kind).inputs.size());
    }

    for (const auto& link : editorGraph.links) {
        InputBinding binding;
        binding.sourceNodeIndex = static_cast<uint16_t>(idToIndex.at(link.from.nodeId));
        binding.sourceOutputSlot = link.from.slot;
        compiled.nodes[idToIndex.at(link.to.nodeId)].inputs[link.to.slot] = binding;
    }

    for (size_t i = 0; i < compiled.nodes.size(); ++i) {
        if (compiled.nodes[i].kind == NodeKind::TerrainSynthesis) {
            const NodeDef& def = nodeDefinition(NodeKind::TerrainSynthesis);
            for (uint8_t slot = 0; slot < static_cast<uint8_t>(def.outputs.size()); ++slot) {
                compiled.outputs.push_back({
                    static_cast<FieldSlot>(slot),
                    static_cast<uint16_t>(i),
                    slot});
            }
            break;
        }
    }

    return compiled;
}

} // namespace graph
