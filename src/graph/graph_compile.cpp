#include "graph/graph_compile.h"

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
        compiled.nodes[i].inputs.resize(nodeDefinition(en.kind).inputs.size());
        compiled.nodes[i].channelCount = static_cast<uint8_t>(outputChannelCount(en.kind));
    }

    for (const auto& link : editorGraph.links) {
        compiled.nodes[idToIndex.at(link.to.nodeId)].inputs[link.to.slot] =
            static_cast<uint16_t>(idToIndex.at(link.from.nodeId));
    }

    return compiled;
}

} // namespace graph
