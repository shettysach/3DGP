# Graph View Spec

## Goal

Build an in-app terrain graph editor using Dear ImGui so the project can:

- drag nodes around on a canvas
- connect node outputs to node inputs
- edit node parameters live
- regenerate terrain from the graph
- preview the result immediately in the existing SDL/OpenGL renderer

The first version should fit the current app instead of becoming a separate tool. The graph view should sit on top of the current demo loop in [src/renderer/demo.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/demo.cpp#L14-L215) and continue using the existing mesh renderer in [src/renderer/core.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/core.cpp#L416-L516).

## Stack Choice

Use these libraries:

- Dear ImGui for the editor UI, property panels, menus, status bars, and dockable windows
- imnodes for the node canvas, draggable nodes, pins, and links
- SDL2 + OpenGL backend bindings from Dear ImGui for integration with the current app

Use `imnodes` instead of `imgui-node-editor` for the first pass because it is smaller, easier to wire into a project this size, and good enough for the initial graph features.

## Why This Fits

This repo already has:

- an SDL event loop in [src/renderer/demo.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/demo.cpp#L97-L213)
- an OpenGL renderer in [src/renderer/core.h](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/core.h#L39-L157)
- a single executable defined in [CMakeLists.txt](file:///home/sword/Desktop/USC/3d/hws/PROJECT/CMakeLists.txt#L1-L71)
- a stable terrain output boundary in [src/terrain.h](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.h#L118-L147)

That means the cleanest path is to embed the graph editor into the existing application and keep terrain execution in C++.

## Scope

The first graph editor version should cover:

- graph canvas with node dragging and link creation
- node palette for adding terrain-related nodes
- inspector panel for parameters
- save and load graph files
- compile graph into terrain generation inputs
- live terrain rebuild from graph changes
- graph-driven base terrain synthesis only

The first version should not cover:

- full undo and redo history
- multi-document editing
- collaborative editing
- scripting support
- graph-driven climate, biome, or river passes

## MVP Simplicity Rules

This spec intentionally keeps the system narrow.

- the graph drives only base terrain synthesis, not the later climate or biome passes
- every link carries the same value kind: a `float` raster field with one sample per terrain cell
- nodes may expose float control inputs such as frequency, gain, thresholds, and blend weights, but in the MVP those controls are stored as node params edited in the inspector rather than linkable scalar graph pins
- pin layouts are fixed by `NodeKind`; the editor does not persist separate pin objects in the graph file
- nodes do not add or remove pins dynamically in the MVP
- one graph is open at a time
- invalid graphs do not partially execute; the app keeps the last valid compiled graph and last valid terrain

These rules are the main guardrail against building an overly generic system too early.

## Architecture

Split the system into four layers.

### UI Layer

The ImGui layer should only handle interaction and presentation:

- node canvas
- panels
- menus
- hotkeys
- graph selection state

Recommended files:

- `src/editor/ui.h`
- `src/editor/ui.cpp`
- `src/editor/node_canvas.h`
- `src/editor/node_canvas.cpp`
- `src/editor/inspector.h`
- `src/editor/inspector.cpp`

### Graph Model

The graph model should represent editable graph data, not execution details.

It should store:

- node ids
- node type
- links
- parameter values
- node positions on the canvas
- editor metadata like collapsed state or selection

Recommended files:

- `src/editor/graph_model.h`
- `src/editor/graph_model.cpp`

### Graph Compiler

The compiler should validate the editor graph and turn it into an executable terrain graph.

It should handle:

- node type validation
- pin layout validation from the static node definition table
- cycle checks
- missing input detection
- default parameter fallback
- compilation into an execution-friendly graph representation

Recommended files:

- `src/terrain/graph.h`
- `src/terrain/graph.cpp`
- `src/terrain/graph_compile.h`
- `src/terrain/graph_compile.cpp`

### Graph Execution

The executor should evaluate the compiled graph and write into the existing terrain field buffers.

It should own:

- field allocation
- node evaluation order
- caching for node outputs during a single build
- output binding into [TerrainFields](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain/fields.h#L12-L79)

The renderer should not know about the graph. It should still consume [TerrainMesh](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.h#L139-L147) only.

## Core Definitions

These definitions are the MVP contract.

### Runtime Concepts

- `FieldBuffer` means `std::vector<float>` with exactly `width * depth` elements
- a field sample at `(x, z)` corresponds to the same terrain cell index used by [fieldIndex()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain/fields.h#L77-L79)
- all graph node outputs are field buffers
- graph params are per-node constants, not per-cell values
- a node may have linkable field inputs and non-linkable float control inputs at the same time
- in the MVP, float control inputs are edited in the node inspector and serialized as params

### Type Model

The graph is a scalar-field graph.

- `Noise` outputs a scalar field
- `UnaryFieldOp` and `BinaryFieldOp` operate on scalar fields
- `BinaryFieldOp::Lerp` blends two scalar fields using a node float control
- `Warp2D` is a special multi-output node, not a generic vector type
- `TerrainSynthesis` consumes named scalar fields and produces named scalar fields

This means the MVP graph is not a generic `vec2` or `vec3` graph. It is a terrain field graph with one special-case node for warped sample coordinates.

### Id Types

```cpp
using NodeId = int32_t;
using LinkId = int32_t;
```

### Node Kinds

```cpp
enum class NodeKind : uint8_t {
    WorldX,
    WorldZ,
    Noise,
    Warp2D,
    UnaryFieldOp,
    BinaryFieldOp,
    TerrainSynthesis,
    GraphOutput,
};
```

### Output Slots

`GraphOutput` nodes bind a field into one required terrain slot.

```cpp
enum class TerrainFieldSlot : uint8_t {
    Height,
    SampleX,
    SampleZ,
    MountainWeight,
    ValleyWeight,
    PlateauWeight,
};
```

### Pin References

Pins are derived from node type and slot index. They are not stored as standalone objects.

```cpp
struct PinRef {
    NodeId nodeId = 0;
    uint8_t slot = 0;
};
```

### Params

Each node kind has a fixed param struct. The graph editor stores only the params for the node's kind.

```cpp
enum class NoiseMode : uint8_t {
    Fbm,
    Ridged,
};

struct NoiseParams {
    NoiseMode mode = NoiseMode::Fbm;
    float frequency = 0.007f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float sharpness = 2.0f;
    float xOffset = 0.0f;
    float zOffset = 0.0f;
    bool remapToUnit = true;
};

enum class UnaryFieldOpKind : uint8_t {
    Clamp,
    Smoothstep,
    Abs,
    OneMinus,
};

struct UnaryFieldOpParams {
    UnaryFieldOpKind op = UnaryFieldOpKind::Clamp;
    float a = 0.0f;
    float b = 1.0f;
};

enum class BinaryFieldOpKind : uint8_t {
    Add,
    Multiply,
    Min,
    Max,
    Lerp,
};

struct BinaryFieldOpParams {
    BinaryFieldOpKind op = BinaryFieldOpKind::Add;
    float control = 0.5f;
};

struct TerrainSynthesisParams {
    float verticalScale = 80.0f;

    float mountainSignalContinental = 0.55f;
    float mountainSignalSlopeHint = 0.35f;
    float mountainSignalRangeMask = 0.45f;
    float mountainHeightContinental = 0.52f;
    float mountainHeightRidges = 0.38f;
    float mountainHeightDetail = 0.10f;

    float valleySignalBasin = 0.60f;
    float valleySignalAntiContinental = 0.22f;
    float valleySignalSlopeHint = 0.28f;

    float plainsContinental = 0.46f;
    float plainsBaseWeight = 0.18f;
    float plainsDetailWeight = 0.08f;

    float plateauMountainSuppress = 0.75f;
    float finalDetailAmount = 0.035f;
};

struct GraphOutputParams {
    TerrainFieldSlot slot = TerrainFieldSlot::Height;
};
```

The first version does not need a large generic `NodeParams` system beyond a `std::variant` over the supported param structs.

## Library Integration

Use a vendored dependency layout so the build stays deterministic and does not depend on network access during `cmake`.

Recommended layout:

- `third_party/imgui/`
- `third_party/imnodes/`

Add these Dear ImGui sources to the build:

- `imgui.cpp`
- `imgui_draw.cpp`
- `imgui_tables.cpp`
- `imgui_widgets.cpp`
- `imgui_demo.cpp` only if development helpers are useful
- `backends/imgui_impl_sdl2.cpp`
- `backends/imgui_impl_opengl3.cpp`

Add this imnodes source to the build:

- `imnodes.cpp`

Update [CMakeLists.txt](file:///home/sword/Desktop/USC/3d/hws/PROJECT/CMakeLists.txt#L27-L66) to:

- include Dear ImGui and imnodes headers
- compile their sources into the main executable or a small internal static library
- keep all editor-specific code optional behind a project option if desired

Recommended CMake option:

```cmake
option(TERRAIN_ENABLE_GRAPH_EDITOR "Enable Dear ImGui graph editor" ON)
```

That makes it easy to disable the editor if needed later.

## App Integration

The current event loop and terrain lifecycle are both in [src/renderer/demo.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/demo.cpp#L14-L215). Before adding the graph canvas, split the demo into app-style pieces.

Recommended refactor:

- keep `Renderer` focused on rendering only
- move interaction state out of `runDemo()` into an editor or app session object
- create a top-level app coordinator that owns renderer, terrain generator, graph state, and UI state

Recommended files:

- `src/app/session.h`
- `src/app/session.cpp`
- `src/app/editor_mode.h`
- `src/app/editor_mode.cpp`

The goal is for the main loop to look like this:

```cpp
while (!app.shouldClose()) {
    app.pollEvents();
    app.beginFrame();
    app.updateUi();
    app.maybeRebuildTerrain();
    app.renderScene();
    app.endFrame();
}
```

That separation matters because ImGui needs a well-defined frame boundary and event forwarding path.

## ImGui Frame Flow

Each frame should follow this order:

1. poll SDL events
2. feed events into ImGui backend
3. update camera controls only when ImGui is not actively capturing mouse or keyboard
4. begin ImGui frame
5. draw graph canvas and panels
6. detect graph edits and mark terrain dirty
7. rebuild terrain if needed
8. render the 3D scene
9. render ImGui draw data
10. swap buffers

This prevents camera orbit and node dragging from fighting each other.

## Window Layout

Use docking and split the UI into these windows.

### Viewport

This remains the terrain preview using the existing renderer. It should stay visually dominant.

### Graph

This is the node canvas. It should support:

- pan
- zoom
- selection
- multi-link display
- link creation
- link deletion
- drag and drop node placement

### Inspector

This shows the selected node or graph output and allows direct editing of parameters.

### Palette

This lists node types that can be created.

### Build Status

This shows:

- graph validation errors
- terrain rebuild timing
- terrain dimensions
- currently selected preset or file

## Node Definitions

These are the only required node kinds for the MVP.

### `WorldX`

- inputs: none
- outputs:
  - slot `0`: `field`
- behavior: output world-space `x` coordinate for every cell

### `WorldZ`

- inputs: none
- outputs:
  - slot `0`: `field`
- behavior: output world-space `z` coordinate for every cell

### `Noise`

- inputs:
  - slot `0`: `x`
  - slot `1`: `z`
- outputs:
  - slot `0`: `field`
- params: `NoiseParams`
- behavior: sample seeded noise using the shared terrain noise backend
- mode behavior:
  - `Fbm`: standard fractal Brownian motion
  - `Ridged`: ridged fBm using `sharpness`

The editor palette may expose friendly entries like `FBm Noise` and `Ridged Noise`, but they should both instantiate the same `Noise` node kind with different `mode` defaults.

### `Warp2D`

- inputs:
  - slot `0`: `x`
  - slot `1`: `z`
  - slot `2`: `warpX`
  - slot `3`: `warpZ`
- outputs:
  - slot `0`: `sampleX`
  - slot `1`: `sampleZ`
- behavior: output `x + warpX` and `z + warpZ`

### `UnaryFieldOp`

- inputs:
  - slot `0`: `value`
- outputs:
  - slot `0`: `value`
- params: `UnaryFieldOpParams`
- behavior: apply a unary field operation
- op behavior:
  - `Clamp`: clamp `value` to `[a, b]`
  - `Smoothstep`: apply smoothstep with `edge0 = a` and `edge1 = b`
  - `Abs`: absolute value, ignore `a` and `b`
  - `OneMinus`: compute `1.0 - value`, ignore `a` and `b`

### `BinaryFieldOp`

- inputs:
  - slot `0`: `a`
  - slot `1`: `b`
- outputs:
  - slot `0`: `value`
- params: `BinaryFieldOpParams`
- behavior: apply a binary field operation
- op behavior:
  - `Add`: `a + b`
  - `Multiply`: `a * b`
  - `Min`: `min(a, b)`
  - `Max`: `max(a, b)`
  - `Lerp`: `lerp(a, b, control)` where `control` is the node's float control input

For `Lerp`, the node takes two field inputs and one float control value owned by the node. This matches the intended UX where a blend node is fed by noise fields but the blend weight is edited as a float control on the node itself.

### `TerrainSynthesis`

- inputs:
  - slot `0`: `continental`
  - slot `1`: `ridges`
  - slot `2`: `detail`
  - slot `3`: `rangeMask`
  - slot `4`: `basin`
  - slot `5`: `detailBand`
  - slot `6`: `rimMask`
  - slot `7`: `plainsBase`
  - slot `8`: `macroRelief`
  - slot `9`: `hilliness`
  - slot `10`: `basinNoise`
  - slot `11`: `plateauMask`
  - slot `12`: `falloff`
  - slot `13`: `sampleX`
  - slot `14`: `sampleZ`
- outputs:
  - slot `0`: `height`
  - slot `1`: `mountainWeight`
  - slot `2`: `valleyWeight`
  - slot `3`: `plateauWeight`
  - slot `4`: `sampleX`
  - slot `5`: `sampleZ`
- params: `TerrainSynthesisParams`
- behavior: implement the current terrain recipe at a higher level by computing mountain, valley, plains, plateau, and final blend contributions from the named input fields

This node is intentionally composite. It is the main mechanism for keeping complexity down while still making the terrain recipe graph-driven.

### `GraphOutput`

- inputs:
  - slot `0`: `value`
- outputs: none
- params: `GraphOutputParams`
- behavior: bind the incoming field to one terrain field slot such as `Height` or `MountainWeight`

### Required Outputs

Every valid base terrain graph must provide exactly one `GraphOutput` node for each of these slots:

- `Height`
- `SampleX`
- `SampleZ`
- `MountainWeight`
- `ValleyWeight`
- `PlateauWeight`

Those six outputs are the contract with the rest of the existing terrain pipeline in [TerrainFields](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain/fields.h#L17-L36).

## Graph Data Model

Use an editor model that persists both logic and UI layout.

The editor model should not persist pin objects. Pin layouts are fixed by `NodeKind` and resolved from a node-definition table in code.

Recommended shape:

```cpp
struct EditorNode {
    NodeId id;
    NodeKind kind;
    float posX;
    float posY;
    std::string title;
    NodeParams params;
};

struct EditorLink {
    LinkId id;
    PinRef from;
    PinRef to;
};

struct EditorGraph {
    int version = 1;
    std::vector<EditorNode> nodes;
    std::vector<EditorLink> links;
};
```

Persist the node positions so reopening a graph restores the same layout.

## Compiled Graph Spec

Compilation should convert the editor graph into a compact execution graph.

Recommended shape:

```cpp
struct InputBinding {
    uint16_t sourceNodeIndex = 0;
    uint8_t sourceOutputSlot = 0;
};

struct CompiledNode {
    NodeKind kind;
    NodeParams params;
    std::vector<InputBinding> inputs;
};

struct OutputBinding {
    TerrainFieldSlot slot;
    uint16_t sourceNodeIndex = 0;
    uint8_t sourceOutputSlot = 0;
};

struct CompiledGraph {
    std::vector<CompiledNode> nodes;
    std::vector<OutputBinding> outputs;
};
```

The first version does not need:

- subgraphs
- function nodes
- generic type inference
- partial recompilation
- cross-graph references

## Graph Compilation

The editable graph should compile into a terrain execution graph.

The compiler should:

- resolve pin-to-pin links into node dependencies
- reject invalid type pairings
- reject incomplete required inputs
- assign topological execution order
- map graph outputs into terrain field slots

The compiler should also enforce these rules:

- each input pin accepts at most one incoming link
- output pins may fan out to many links
- links may only go from output pins to input pins
- `WorldX` and `WorldZ` may not have incoming links
- `GraphOutput` may not have outgoing links
- all required `GraphOutput` slots must be present exactly once
- cycles are invalid
- unknown node kinds or param schemas are invalid

A failed compile should not crash the app. It should keep the previous valid terrain and show errors in the build status panel.

## Execution Rules

The executor should evaluate nodes in compiled order and materialize field buffers for every node output used later in the graph.

The first version may simply keep all intermediate node outputs alive for the duration of one terrain build. Memory reuse can be added later if needed.

Execution rules:

- one terrain build evaluates the whole graph from scratch
- all field buffers have the same dimensions as the active terrain settings
- node params are read-only during execution
- output bindings write directly into the corresponding fields on `TerrainFields`
- after graph execution, the existing post-passes run unchanged

This means the graph replaces only the current logic in [buildBaseTerrainFields()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.cpp#L274-L416), while [generateMesh()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.cpp#L423-L458) keeps its later stages.

## Terrain Integration

The graph should replace the current hardcoded logic inside [buildBaseTerrainFields()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.cpp#L274-L416), not the whole terrain pipeline.

Recommended generator API additions:

```cpp
void setBaseGraph(std::shared_ptr<const TerrainGraph> graph);
const TerrainGraph* baseGraph() const;
```

Recommended execution boundary:

- graph builds the base scalar fields
- existing smoothing, rivers, climate, landforms, biomes, and mesh packing stay unchanged

That keeps [computeClimateFields()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain/climate.cpp#L62-L153), [computeLandformFields()](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain/landforms.cpp#L85-L168), and the renderer stable while the editor evolves.

## Live Rebuild Behavior

The editor needs a predictable terrain rebuild policy.

Recommended behavior:

- structural graph changes mark terrain dirty immediately
- parameter changes mark terrain dirty immediately
- rebuild on the next frame if auto-update is enabled
- support a manual rebuild toggle for expensive graphs later
- debounce rebuilds slightly for slider drags if terrain generation gets expensive

Recommended UI controls:

- `Auto Rebuild` checkbox
- `Rebuild Now` button
- `Reset Camera` button
- `Use Legacy Graph Preset` button

## Persistence

Store graph files separately from code so the editor becomes useful without recompiling.

Recommended layout:

- `graphs/default.json`
- `graphs/mountains.json`
- `graphs/plateaus.json`

Each graph file should persist:

- nodes
- links
- parameter values
- editor positions
- graph version number

Version the schema from the beginning so node definitions can evolve safely.

Recommended JSON shape:

```json
{
  "version": 1,
  "nodes": [
    {
      "id": 1,
      "kind": "WorldX",
      "title": "World X",
      "pos": [80.0, 120.0],
      "params": {}
    },
    {
      "id": 10,
      "kind": "Noise",
      "title": "Continental",
      "pos": [420.0, 120.0],
      "params": {
        "mode": "Fbm",
        "frequency": 0.0052,
        "octaves": 6,
        "lacunarity": 2.0,
        "gain": 0.5,
        "sharpness": 2.0,
        "xOffset": 0.0,
        "zOffset": 0.0,
        "remapToUnit": true
      }
    }
  ],
  "links": [
    {
      "id": 100,
      "from": {"nodeId": 1, "slot": 0},
      "to": {"nodeId": 10, "slot": 0}
    }
  ]
}
```

Use strings for `kind` in JSON to keep files readable.

## Default Graph Spec

The app should ship with one default graph that approximates the current terrain recipe in [src/terrain.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/terrain.cpp#L290-L412).

The default graph should include at least these conceptual stages:

- `WorldX` and `WorldZ`
- noise nodes to produce `warpX` and `warpZ`
- one `Warp2D` node to produce `sampleX` and `sampleZ`
- separate `Noise` nodes for `continental`, `detail`, `ridges`, `rangeMask`, `basin`, `detailBand`, `rimMask`, `plainsBase`, `macroRelief`, `hilliness`, `basinNoise`, `plateauMask`, and `falloff`
- one `TerrainSynthesis` node
- six `GraphOutput` nodes for the required terrain field slots

The default graph does not need to exactly reproduce every constant expression from the legacy implementation on day one, but it should be structurally equivalent and should use similar defaults.

## Editor UX

The first version should support these workflows.

### Create Graph

- open default graph
- add noise node from palette
- drag it onto the canvas
- connect it into `TerrainSynthesis`
- see terrain rebuild automatically

### Inspect Node

- click a node
- edit floats in inspector
- rename node for readability
- view validation warnings if inputs are missing

### Manage Links

- drag from an output pin to an input pin
- reject invalid links visually
- allow deleting links from the canvas

### Save and Load

- save current graph to a file
- reload from disk
- keep recent graph list later if useful

## Camera And Input Rules

ImGui should take priority when the pointer is over the graph or inspector.

Rules:

- if ImGui wants mouse input, disable terrain orbit and pan for that frame
- if ImGui wants keyboard input, disable terrain hotkeys for that frame
- keep viewport controls active only when the viewport is focused

This is important because the current controls in [src/renderer/demo.cpp](file:///home/sword/Desktop/USC/3d/hws/PROJECT/src/renderer/demo.cpp#L105-L213) assume the whole window belongs to the renderer.

## Milestones

### Milestone 1

Add Dear ImGui and imnodes to the build.

Deliverables:

- dependencies vendored
- CMake updated
- ImGui frame bootstrapped in SDL/OpenGL app
- one debug window visible

### Milestone 2

Split app loop responsibilities out of `runDemo()`.

Deliverables:

- app session object
- scene rendering still works
- ImGui and camera input coexist cleanly

### Milestone 3

Implement editable graph model and graph canvas.

Deliverables:

- nodes can be added
- nodes can be moved
- links can be created and deleted
- selected node shows in inspector

### Milestone 4

Implement graph compiler and terrain synthesis wiring.

Deliverables:

- graph compiles into executable terrain graph
- `TerrainSynthesis` node works
- terrain rebuilds from graph changes

### Milestone 5

Add persistence and default preset graphs.

Deliverables:

- save and load graph JSON
- load default graph at startup
- legacy terrain recipe represented as a default graph

### Milestone 6

Polish usability.

Deliverables:

- docking layout
- palette search
- error display
- rebuild toggle
- simple graph validation hints

## Risks

### App Loop Risk

The current demo loop mixes rendering, input, and terrain lifecycle in one place. If ImGui is added directly without refactoring that flow first, input conflicts will be messy.

### Build Risk

Adding editor dependencies directly into the executable will increase build complexity. Vendoring the libraries and keeping the integration small reduces risk.

### Performance Risk

Terrain regeneration is not free. Rebuilding on every parameter drag could become noisy at larger resolutions. Auto-rebuild should remain optional.

### Scope Risk

It is easy to turn the editor into a full tool before the terrain graph runtime is stable. The UI should stay thin and defer graph meaning to the compiler and executor.

## Acceptance Criteria

The first release of graph view is successful when all of these are true.

- the app opens with an ImGui-based graph canvas
- nodes can be created, moved, and linked visually
- at least one default graph reproduces the current terrain recipe at a high level
- editing graph parameters can rebuild terrain live
- graph files can be saved and loaded
- the renderer still consumes only `TerrainMesh`
- normal terrain preview controls still work when the viewport is focused
- graph editing never crashes the app on invalid links or missing inputs

## Recommended First Implementation Order

If this is built in one branch, do the work in this order:

1. vendor Dear ImGui and imnodes
2. wire ImGui into SDL2 and OpenGL frame lifecycle
3. extract the app loop from `runDemo()` into a session object
4. add empty graph, inspector, and status windows
5. implement editor graph model and canvas interactions
6. implement a non-executing `TerrainSynthesis` node in UI only
7. implement graph compilation
8. connect compiled graph to terrain generation
9. add save and load support
10. polish interaction and validation

## Nice Follow-Ups

After the first version is working, the best next upgrades are:

- graph presets browser
- mini-map overview of the graph canvas
- node grouping and comments
- debug field preview for intermediate noises
- side-by-side legacy and graph output comparison
- optional undo and redo stack
