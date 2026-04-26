#ifndef GRAPH_COMPILE_H
#define GRAPH_COMPILE_H

#include "graph/types.h"

namespace graph {

// Validates the editor graph and produces a topologically sorted
// execution graph. Throws std::runtime_error on invalid input.
CompiledGraph compile(const EditorGraph& editorGraph);

} // namespace graph

#endif // GRAPH_COMPILE_H
