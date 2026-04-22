# Render Refactor Plan

## Goals

- Replace the fixed-function OpenGL 2.1 renderer with a modern OpenGL + GLSL pipeline.
- Keep the existing terrain generation code and mesh data flow intact where possible.
- Improve visual quality in the specific weak areas called out so far: fog, lighting, materials, water, shadows, and atmosphere.
- Keep the renderer performant on a desktop GPU for the current terrain scale.
- Refactor in phases so the project stays runnable between milestones.

## Non-Goals

- Do not turn this into a full engine.
- Do not rewrite the terrain generator unless a rendering feature absolutely requires extra data.
- Do not add speculative rendering systems that are not directly useful for the terrain demo.
- Do not chase film-quality volumetrics or full PBR feature parity on the first pass.

## Current Renderer Summary

The current renderer in `src/renderer.cpp` is limited by the pipeline itself rather than the terrain mesh.

- OpenGL context is requested as 2.1 and uses fixed-function lighting/fog.
- Terrain is rendered with client-side arrays and CPU-built color buffers.
- Fog is linear distance fog, which reads as fake in large outdoor scenes.
- Lighting is a single fixed-function directional light with constant ambient.
- Detail materials are alpha-blended overlays with tiny procedural textures.
- Water is flat-normal translucent geometry with a scrolling texture.
- Shadows are a light CPU approximation multiplied into vertex colors.

The terrain generation side is in better shape than the renderer. The main refactor target is the rendering pipeline.

## Target Architecture

Keep SDL for the window, input, and swapchain. Replace the rest of the rendering path with a shader-driven renderer.

### Core Decisions

- Move to OpenGL 3.3 core profile.
- Keep SDL2.
- Remove GLU and fixed-function OpenGL usage.
- Add a GL function loader such as vendored GLAD.
- Use VBOs, IBOs, and VAOs for terrain and water.
- Use GLSL shaders for every visual pass.
- Keep terrain generation CPU-side and upload mesh data only when terrain changes.

### Proposed Renderer Layout

```text
terrain::TerrainGenerator
  -> terrain::TerrainMesh
  -> renderer::MeshUpload
  -> render passes
     - sky/atmosphere
     - shadow map
     - terrain opaque
     - water transparent
     - optional post-process
```

### Suggested File Structure

This is a logical target, not a required one-shot change.

```text
src/
  renderer/
    renderer.h
    renderer.cpp
    gl_loader.*
    shader.h
    shader.cpp
    program.h
    program.cpp
    mesh_buffer.h
    mesh_buffer.cpp
    texture.h
    texture.cpp
    framebuffer.h
    framebuffer.cpp
    camera.h
    camera.cpp
    atmosphere.h
    atmosphere.cpp
    materials.h
    materials.cpp
    shaders/
      terrain.vert
      terrain.frag
      water.vert
      water.frag
      shadow.vert
      shadow.frag
      sky.vert
      sky.frag
      post.vert
      post.frag
```

If this ends up being too much structure for the project size, collapse the helpers into fewer files. The important change is the render architecture, not the file count.

## Rendering Pipeline

## Pass Order

1. Shadow map pass
2. Sky/atmosphere background pass
3. Terrain opaque pass
4. Water transparent pass
5. Optional full-screen post pass

## Pass Details

### 1. Shadow Map Pass

- Render terrain depth from the sun's point of view.
- Start with one directional shadow map.
- Use a fixed shadow map size first, for example `2048x2048`.
- Use slope-scale bias and normal bias to reduce acne.
- Use simple PCF filtering in the terrain shader.

This replaces the current CPU shadow darkening as the main direct-light shadow source. The CPU shadow term can remain temporarily as a cheap ambient occlusion-style multiplier during migration if helpful.

### 2. Sky And Atmosphere Pass

- Render a sky gradient or procedural sky dome before terrain.
- Match the sky palette to the fog and sun direction.
- Include horizon brightening and a warm sun-facing tint.
- Keep this cheap; a full-screen triangle or low-cost sky dome is enough.

This is important because outdoor scenes never look convincing when the clear color is doing all the work.

### 3. Terrain Opaque Pass

- Render terrain with a GLSL shader.
- Use per-fragment lighting.
- Blend macro biome color with tiled material detail.
- Apply aerial perspective and height fog here.
- Apply shadow map sampling here.

This pass should become the visual backbone of the demo.

### 4. Water Pass

- Render water after terrain with blending enabled.
- Use Fresnel-like reflectance behavior.
- Use two scrolling normal/detail layers instead of a single flat scroll pattern.
- Add shallow/deep color variation.
- Add shoreline foam where river weight and slope imply turbulence.

### 5. Optional Post Pass

- Start without a heavy post stack.
- If needed, add a small final pass for tone shaping, gamma, contrast, and subtle color grading.
- Skip bloom at first unless the scene genuinely benefits from it.

## Visual Feature Plan

## Lighting

### Phase 1 Lighting Model

Start with a simple but credible outdoor model:

- One directional sun light.
- Sky ambient from above.
- Ground bounce from below.
- Per-fragment Lambert or wrapped diffuse.
- Soft specular only where it makes sense.

Suggested terrain lighting model:

- `sun = max(dot(normal, sunDir), 0.0)`
- `skyAmbient = mix(groundColor, skyColor, normal.y * 0.5 + 0.5)`
- `shadowedSun = sun * shadowFactor`
- `final = albedo * (skyAmbient + shadowedSun * sunColor)`

This will already look much better than the current fixed-function setup.

### Phase 2 Lighting Improvements

- Add backlighting tint for low sun angles.
- Add a view-dependent rim term for haze near the horizon.
- Slightly vary ambient by altitude to avoid flat valleys.
- Add optional wetness darkening near rivers.

## Fog And Atmosphere

Replace linear fog with height-aware aerial perspective.

### Minimum Viable Fog

- Exponential distance fog.
- Height falloff so low-lying areas gather more haze.
- Fog color derived from sky color, not hard-coded in isolation.

Suggested shader inputs:

- `fogDensity`
- `fogHeightFalloff`
- `fogBaseHeight`
- `fogColorZenith`
- `fogColorHorizon`
- `sunDir`

Suggested behavior:

- More haze with distance.
- More haze in valleys and near water.
- Slight forward-scattering warmth toward the sun.
- Cooler haze away from the sun.

### Atmosphere Goals

- A believable horizon.
- No hard fog wall.
- Terrain recedes with depth naturally.
- The sky, fog, and sun feel like one lighting environment.

## Terrain Materials

The current overlay approach is a useful starting signal source, but the final material system should move into shader logic.

### Material Inputs Already Available

The terrain mesh already contains useful data in `terrain::TerrainVertex`:

- normal
- slope
- mountain weight
- river weight
- moisture
- temperature
- precipitation
- biome ids and blend weights

These values are enough to drive a first good-looking terrain material without changing the generator.

### Target Terrain Material Model

Blend four kinds of detail instead of relying mostly on vertex color:

- grass/soil
- rock
- sand/sediment
- snow

Each material should support:

- albedo
- normal detail if available
- roughness scalar or roughness texture later

### Blend Rules

- Rock increases with slope and mountain weight.
- Snow increases with altitude and colder climate.
- Sand increases near rivers, flatter lowlands, and drier regions.
- Grass/soil fills the remaining terrain.

### Mapping Strategy

- Use world-space UVs for low-slope surfaces.
- Use triplanar mapping on steep slopes and cliffs.
- Blend a low-frequency macro color map with high-frequency detail maps.

This prevents the terrain from looking either flat from far away or noisy up close.

### Texture Plan

Short term:

- Replace `64x64` material textures with at least `256x256` or `512x512`.
- Generate mipmaps.
- Enable anisotropic filtering when available.

Medium term:

- Swap procedural textures for authored terrain textures if needed.
- Add normal maps for rock and ground detail.

## Water

Water needs a dedicated shader rather than a color-plus-scroll overlay.

### Water Goals

- Read as flowing water, not tinted glass.
- Look better at both shallow river edges and deeper centers.
- Pick up sky color and view-angle changes.
- Stay inexpensive enough for the whole river network.

### Water Shader Features

- Fresnel term for view-angle response.
- Two scrolling normal/detail layers moving at different speeds.
- Depth or shore fade using river weight and terrain proximity.
- Foam near banks and turbulent sections.
- Slight specular highlight from the sun.
- Optional soft absorption tint for deeper water.

### Water Geometry

Keep the current generated water mesh first.

Later improvements, if needed:

- Slight vertex displacement in the shader for motion.
- Better bank conformity.
- Per-segment flow direction if river data exposes it.

## Shadows

The project needs real direct-light shadows before more advanced effects.

### Phase 1 Shadows

- Single directional shadow map.
- PCF filtering.
- Stable light matrix tied to the camera target.
- Conservative bias tuning.

### Phase 2 Shadows

- Consider cascaded shadow maps only if needed.
- Consider contact-darkening or SSAO-style tricks only after base shadows are solid.

Do not start with cascades. A good single shadow map is enough for a terrain demo at this scale.

## Performance Strategy

Visual upgrades should not come from per-frame CPU work.

### General Rules

- Terrain generation remains CPU-side and event-driven.
- Mesh uploads happen only when the terrain changes.
- Per-frame rendering uses GPU-side vertex/index buffers.
- Avoid rebuilding color/material buffers every frame on the CPU.
- Move blend logic from CPU vectors into the shaders.

### Geometry Budget

Current settings generate a `512x512` terrain, which is already a meaningful amount of geometry. Treat this as a desktop target and optimize around it.

Initial performance plan:

- One static terrain VBO/IBO.
- One static water VBO/IBO.
- One shadow pass.
- One terrain pass.
- One water pass.

Only introduce chunking or LOD if profiling shows the need.

### Material Performance

- Keep texture count modest.
- Pack related scalar maps together when convenient.
- Avoid branching-heavy shader code when simple blends will do.
- Prefer a few good detail layers over many weak ones.

### Shadow Performance

- Start with one shadow map.
- Use a moderate PCF kernel.
- Increase resolution only after measuring aliasing.

### Water Performance

- Avoid real reflections initially.
- Fake reflections using sky color and Fresnel first.
- Skip refraction unless the benefit is obvious.

## Data And API Changes

## Renderer Interface

Keep the public renderer API close to the current one where useful, but separate responsibilities internally.

Suggested high-level API:

```cpp
class Renderer {
  public:
    bool init();
    void shutdown();
    void uploadTerrain(const terrain::TerrainMesh& mesh);
    void render(const CameraState& camera);
    void invalidateTerrain();
};
```

Even if `render(const terrain::TerrainMesh&)` remains during transition, the long-term goal is to upload terrain once and render from GPU-owned resources.

## GPU Mesh Format

Avoid sending more data than each shader actually needs.

Possible terrain vertex payload:

- position
- normal
- slope
- mountain weight
- river weight
- moisture
- biome weights or precomputed material weights

Possible water vertex payload:

- position
- normal
- river weight
- local flow or foam factor later

If attribute count gets awkward, pack some scalar values into `vec4` groups.

## Uniforms And State

Use explicit shader uniforms for:

- camera matrices
- sun direction and colors
- fog parameters
- shadow matrix
- texture bindings
- global atmosphere settings
- time for animated water

Keep naming consistent across shaders.

## Migration Plan

## Phase 0: Rendering Infrastructure

- Request an OpenGL 3.3 core context.
- Add GLAD or equivalent loader.
- Remove GLU-dependent matrix setup.
- Add a small matrix math layer if needed.
- Add shader compile and error logging utilities.
- Add VAO/VBO/IBO setup utilities.

Exit criteria:

- App starts with OpenGL 3.3 core.
- A basic shader can draw the terrain mesh.

## Phase 1: Baseline Terrain Shader

- Upload terrain mesh into GPU buffers.
- Replace client arrays with VAO/VBO/IBO.
- Render terrain with a vertex and fragment shader.
- Port the current biome color logic either to GPU or to a static uploaded buffer.

Exit criteria:

- Terrain renders fully through shaders.
- Fixed-function lighting and fog are no longer used.

## Phase 2: Better Fog And Atmosphere

- Add sky pass.
- Add exponential height fog in the terrain shader.
- Tune fog colors from horizon/zenith values.

Exit criteria:

- Distance reads naturally.
- Valleys and far terrain fade plausibly.

## Phase 3: Terrain Materials

- Replace overlay-only detail with shader material blending.
- Add larger albedo textures.
- Add mipmaps and anisotropic filtering.
- Add triplanar mapping for steep slopes.

Exit criteria:

- Ground looks believable from both far and near distances.
- Rock, snow, sand, and ground read as different materials.

## Phase 4: Water Shader

- Replace current water rendering with a dedicated shader pass.
- Add Fresnel, foam, and dual scrolling detail.
- Tune shoreline behavior.

Exit criteria:

- Water reads as water instead of a transparent overlay.

## Phase 5: Shadow Map

- Add directional shadow map framebuffer.
- Render shadow depth pass.
- Sample shadow map in terrain and water shaders.

Exit criteria:

- Sun shadows ground the terrain shapes.
- Artifacts are controlled well enough for normal camera movement.

## Phase 6: Performance And Polish

- Profile CPU and GPU cost.
- Reduce redundant state changes.
- Tune texture sizes and shadow resolution.
- Evaluate whether chunking or LOD is needed.
- Add lightweight post-process if the frame still feels flat.

Exit criteria:

- Visual target is met without obvious frame drops on the target desktop setup.

## Implementation Priorities

If time is limited, do these in order:

1. Shader terrain pass
2. Height fog + sky
3. Material blending + better textures
4. Water shader
5. Shadow map
6. Final tuning

This order gives the biggest visual payoff fastest.

## Validation Checklist

## Visual Validation

- Does the far terrain fade gradually instead of hitting a fog wall?
- Do valleys gather more haze than peaks?
- Does the terrain still read well at noon-like lighting and lower sun angles?
- Do rock, snow, sand, and vegetation-like surfaces feel materially different?
- Does water change with view angle and bank depth?
- Do shadows help shape the land without crawling or heavy acne?

## Performance Validation

- Terrain regeneration should remain the main CPU cost, not frame rendering.
- Camera movement should not trigger CPU mesh rebuilds.
- Shader complexity should stay reasonable at `512x512` terrain resolution.
- Texture bandwidth should remain controlled.
- Shadow pass should not dominate the frame.

## Immediate First Refactor

The first implementation pass should be intentionally small:

- Switch to OpenGL 3.3 core.
- Add GLAD.
- Upload terrain and water into buffers.
- Draw terrain with a basic shader.
- Add sky gradient and exponential height fog.

That is the smallest change that meaningfully improves realism and opens the door for everything else.

## Final Recommendation

Treat this as a renderer modernization, not a renderer replacement.

The terrain system already produces enough data for a good-looking result. The highest-leverage path is:

- keep the terrain generator
- keep SDL
- move fully to GLSL
- add atmosphere before fancy effects
- add real shadows before more post-processing
- profile before introducing chunking or LOD

If the refactor stays disciplined, this should produce a much better-looking terrain demo without the weight of a full engine.
