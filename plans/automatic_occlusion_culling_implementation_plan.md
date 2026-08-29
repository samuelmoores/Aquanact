# Automatic Occlusion Culling Implementation Plan

Date: 2026-08-29

## Goal

Add conservative main-camera occlusion culling without requiring artists to put
meshes into zones or groups. The engine will build spatial clusters from the
world-space bounds it already owns, choose useful opaque occluders automatically,
and use asynchronous OpenGL occlusion queries to avoid drawing hidden clusters.

The existing camera-frustum and light-frustum culling remain in place. Camera
occlusion must never be applied to shadow passes because an off-camera or hidden
object can still cast a visible shadow.

## Observed performance and reason for prioritizing this work

Runtime measurements taken before implementation show:

- game-mode frame average: approximately 10 ms;
- game-mode render average: approximately 7 ms;
- game-mode flush average: approximately 6 ms, with spikes near 10 ms;
- shadow pass: approximately 3.2 ms;
- ordinary main pass: approximately 2.6 ms;
- editor flush: approximately 12 ms;
- editor debug overlay: approximately 2.6 ms.

The high-flush camera positions also show a rapid increase in main draw calls at
the same time that the frustum-culled mesh count plummets. Much of the newly
frustum-visible environment remains hidden behind walls, shelving, or adjacent
rooms. This correlation is the expected signature of missing occlusion culling,
so reducing the view-dependent main-pass spikes is the primary objective of this
work.

At 120 FPS the complete frame budget is 8.33 ms. Occlusion culling cannot reduce
the approximately 3.2 ms shadow baseline and will add its own prepass/query cost,
so it must not be presented as a standalone guarantee of 120 FPS. Success means
that obstructed views no longer cause main draw calls and flush time to grow
rapidly. Shadow optimization and static batching may still be required to hold
120 FPS in views where most geometry is genuinely visible.

## Intended first version

The first implementation should use an automatically generated BVH and batched
OpenGL occlusion queries. It is a lower-risk bridge to a future hierarchical-Z
GPU implementation because the BVH, stable item identifiers, visibility history,
debug views, and statistics can all be retained when the query backend is
replaced.

The implementation must be optional and default to conservative behavior:

- unsupported hardware, missing bounds, unavailable query results, moving
  objects, and invalid state all mean visible;
- queries operate on BVH nodes rather than individual grocery products;
- query results are consumed asynchronously and must never block the CPU;
- large camera discontinuities invalidate prior visibility;
- skinned meshes remain visible in the first version;
- alpha-blended, cutout, or otherwise uncertain surfaces are not occluders;
- the feature affects only the main camera pass.

## Proposed frame flow

1. Build normal entity-level `RenderCommand` objects.
2. Update or rebuild the automatic BVH when scene membership or static transforms
   change.
3. Build the camera frustum once and reject out-of-frustum commands as today.
4. Render a depth-only prepass containing automatically selected large opaque
   occluders.
5. Poll only query results already reported available by OpenGL. Never wait.
6. Traverse the BVH front to back using accepted historical results. Treat new,
   stale, invalid, or uncertain nodes as visible.
7. Draw visible commands through the existing `DrawCulled` path so per-submesh
   frustum culling is preserved.
8. With color and depth writes disabled, issue bounding-box queries for selected
   BVH nodes against the prepass depth for use by later frames.
9. Restore all OpenGL state before selection outlines and GUI rendering.

This ordering provides useful depth before the queries run and keeps GPU results
asynchronous. It does add an occluder depth pass, so the statistics must show its
cost separately.

## Step-by-step implementation

### 1. Establish a baseline and feature boundary

1. Preserve the measurements above as the initial coarse baseline, then capture
   Render Stats for the grocery-store test views: outside facing a wall,
   outside looking through an entrance, full interior, back room, and empty sky.
2. Record camera draw calls, candidate and frustum-culled buffers, triangles,
   main visibility time, shadow time, main-pass time, flush time, and complete
   frame time for each view. The new game-mode `Flush Timings` window provides
   the CPU stage breakdown.
3. Record average, 95th-percentile, and 99th-percentile frame and flush times so
   the current 10 ms flush spikes cannot be hidden by a 6 ms average.
4. Add GPU timestamp queries for the shadow and main passes, or explicitly label
   existing values as CPU wall-clock timings around OpenGL submission. Driver
   buffering can otherwise attribute GPU work to a later call or stage.
5. Add a runtime `Occlusion culling` toggle, defaulting off during development.
6. Confirm that disabling the feature follows the existing render path exactly.
7. Define the first-version scope as static, opaque, bounded scene geometry only.

Deliverable: reproducible before-data and a zero-behavior-change disabled path.

### 2. Introduce stable occlusion items

Add an `OcclusionItem` representation owned by a new renderer-side
`OcclusionCullingSystem`. Each item should contain:

- entity ID;
- submesh index, with `-1` representing a whole-entity item when granular bounds
  are unavailable;
- current world AABB;
- static/moving classification;
- opaque/occluder eligibility;
- an estimate of triangle or draw-call cost;
- the scene revision in which it was generated.

Generate items automatically from `RenderCommand` and `Mesh` data. Transform the
eight corners of each imported local submesh AABB by `modelMatrix`, then
canonicalize the resulting world AABB. Keep the current whole-entity fallback
for missing or invalid submesh bounds.

Use `(entityId, subMeshIndex)` as the stable key. Do not store raw pointers in
persistent visibility history because scene reloads can invalidate them.

Deliverable: a testable flat list whose union matches the bounded static scene
geometry and requires no project-file changes.

### 3. Build an automatic BVH

1. Add `OcclusionBvhNode` containing a world AABB, child indices, a contiguous
   item range for leaves, aggregate draw-call/triangle cost, and depth used for
   debugging.
2. Build the tree by recursively splitting item centers along the longest axis
   near the median. Start with a configurable leaf target such as 8-16 items.
3. Store nodes and item indices in contiguous vectors to keep traversal cheap.
4. Rebuild on scene load/unload and when static scene membership changes.
5. For the first version, rebuild after a static transform changes. Do not rebuild
   every frame.
6. Exclude moving and skinned items from the BVH and render them conservatively.
7. Add unit tests covering empty input, one item, identical centers, reversed
   bounds, invalid bounds, deterministic construction, and parent bounds
   containing every descendant.

Deliverable: an engine-generated hierarchy independent of artist-authored groups.

### 4. Select occluders automatically

An occluder must be opaque, static, have valid bounds, and exceed configurable
world-space or projected-screen-size thresholds. Favor large exterior walls,
floors, ceilings, and fixtures. Reject transparent and alpha-tested materials in
the first version.

Rank candidates using projected AABB area multiplied by a simple solidity/cost
score. Apply a maximum occluder count or triangle budget so the prepass cannot
grow without bound. Re-evaluate projected ranking per frame, but reuse allocated
storage.

Initially draw the selected source geometry in the depth prepass rather than
their AABB proxies: box proxies can write depth where the real mesh has empty
space and incorrectly hide visible objects. Add a minimal depth-only shader and
reuse the mesh VAOs/index buffers.

Deliverable: automatic, budgeted occluder selection that cannot treat transparent
surfaces or empty AABB volume as solid.

### 5. Add the depth-only occluder prepass

1. Add `RenderOccluderDepth` to `OpenGLGraphicsDevice`.
2. Render after shadow maps and before main-pass occlusion decisions.
3. Disable color writes, enable depth testing and depth writes, use the camera
   view-projection matrix, and draw only selected occluder geometry.
4. Clear camera depth before the prepass. The later main pass should reuse that
   depth and use a compatible depth comparison; avoid clearing it again.
5. Ensure selected objects and the outline pass still behave correctly when
   their depth already exists.
6. Restore color masks, culling, polygon mode, shader, VAO, and other changed
   state explicitly.
7. Track prepass draw calls, triangles, and milliseconds separately.

Deliverable: a correct depth buffer representing real opaque occluder surfaces.

### 6. Add query-object lifetime management

Create a fixed-capacity query pool owned by `OpenGLGraphicsDevice` or the
occlusion system. Each slot needs a GL query ID, node identity/generation, issue
frame, pending flag, and last completed result.

1. Generate query IDs at startup or pool growth and delete them at shutdown.
2. Prefer `GL_ANY_SAMPLES_PASSED`; use a conservative variant only after checking
   runtime support. If the required query target is unavailable, disable the
   feature and report the reason in Render Stats.
3. Poll `GL_QUERY_RESULT_AVAILABLE`. Read `GL_QUERY_RESULT` only when available.
4. Never call a blocking query-result read on a pending query.
5. Do not reuse a slot until its prior result is complete or deliberately
   invalidated after a safe context reset.
6. Cap queries per frame and pending queries globally.

Deliverable: leak-free, nonblocking query management with graceful fallback.

### 7. Render conservative BVH query proxies

1. Add a unit-cube VAO and a minimal position-only query shader.
2. Draw each queried node's world AABB as a box with color writes and depth writes
   disabled and depth testing enabled.
3. Disable face culling for the box or use a known winding configuration.
4. Wrap the proxy draw in `glBeginQuery`/`glEndQuery`.
5. Expand query AABBs by a small configurable epsilon to reduce boundary errors.
6. Skip nodes outside the camera frustum without issuing a query.
7. Prefer higher-cost and nearer nodes when the per-frame query budget is full.
8. Restore every modified GL state after the query pass.

Deliverable: bounded-cost node visibility requests evaluated against real scene
depth without CPU readback stalls.

### 8. Add temporal visibility state and conservative invalidation

Track for every stable BVH node:

- `Unknown`, `Visible`, or `Occluded`;
- last result frame and camera generation;
- consecutive visible and occluded result counts;
- pending-query identity;
- last tested bounds revision.

Apply these rules:

1. Unknown nodes render.
2. Pending results do not change visibility.
3. Require at least two consecutive occluded results before suppressing a node
   during initial tuning.
4. Visible results take effect immediately.
5. Periodically retest occluded nodes, with nearer/larger nodes refreshed more
   often.
6. Invalidate occluded state after scene reload, viewport/projection change,
   static-transform change, teleport, camera-mode switch, or a configured camera
   translation/rotation discontinuity.
7. On invalidation, render uncertain nodes until fresh results arrive.
8. Add one-frame grace visibility for newly created items.

Asynchronous results are inherently based on an earlier camera pose. These rules
favor false visibility (extra work) over false occlusion (missing geometry). If
testing still reveals popping during ordinary motion, keep occlusion enabled
only below stricter camera-motion thresholds or move directly to the Hi-Z phase.

Deliverable: stable temporal behavior with no intentional synchronous waits.

### 9. Integrate BVH traversal into `RenderManager::Flush`

Refactor `Flush` into explicit stages while preserving shadow behavior:

1. count candidates;
2. render shadow maps from the complete command list;
3. perform aggregate camera-frustum rejection;
4. run the occluder depth prepass when enabled;
5. consume available query results;
6. traverse the BVH front to back and create a per-item visibility decision;
7. render visible commands/submeshes;
8. issue this frame's node queries;
9. clear frame-local command storage.

Avoid expanding the main command buffer to one full `RenderCommand` per
submesh—the earlier frustum implementation established that this repeats costly
shader and lighting setup. Instead, pass a compact submesh visibility mask or
visible-index span into a new `DrawCulled` overload so each entity still sets
shared render state once.

Commands or submeshes not represented in the current BVH must render. Selected
entities should also render conservatively so editor interaction cannot make a
selection disappear.

Deliverable: occluded items skip main-pass buffer submission while existing
entity batching and frustum filtering remain intact.

### 10. Add statistics and debugging tools

Extend Render Stats with:

- occlusion enabled/supported state and fallback reason;
- BVH node, leaf, and item counts;
- occluder-prepass draws, triangles, and milliseconds;
- queries issued, pending, completed, and budget-skipped;
- query-result polling milliseconds;
- occlusion-visible and occlusion-culled mesh buffers;
- occlusion-saved main draw calls and triangles;
- stale/unknown nodes rendered conservatively;
- BVH rebuild count and last rebuild milliseconds.

Keep frustum-culled and occlusion-culled values separate. Update
`Main draw calls saved` to present a clearly labeled total rather than silently
changing its meaning.

Add optional debug rendering for BVH nodes with distinct colors for unknown,
visible, occluded, pending, and selected-as-occluder states. The debug geometry
must not participate in depth or queries.

Deliverable: enough information to distinguish useful culling from query or
prepass overhead.

### 11. Verify correctness

Test these cases with the feature both enabled and disabled:

1. Exterior wall hides store contents without hiding the wall itself.
2. Looking through doors and windows preserves all visible interior geometry.
3. Walking rapidly through an entrance does not produce missing geometry.
4. Camera teleport, editor/game camera switching, FOV changes, and window resize
   invalidate stale decisions.
5. Moving and animated objects never disappear because of stale BVH bounds.
6. Transparent surfaces never hide objects behind them.
7. Selected objects and outlines remain visible and pickable.
8. Directional and point-light shadows match the feature-disabled result.
9. Empty scenes, missing bounds, scene reload, and shutdown do not leak or crash.
10. Query results delayed for many frames do not stall or corrupt the pool.
11. Disabling occlusion restores the original draw counts and rendering.

Add a debug option that freezes the camera and alternates occlusion on/off. Use
image comparisons to detect false-negative culling; extra geometry is acceptable,
missing pixels are not.

Deliverable: visual parity for visible pixels and robust lifecycle behavior.

### 12. Measure and tune before enabling by default

Repeat the baseline views and compare complete frame time, not just draw calls.
Pay particular attention to the known camera transitions where frustum-culled
buffers fall and main draw calls rise. For each view, compare occlusion disabled
and enabled using average, 95th-percentile, and 99th-percentile values.
Tune:

- BVH leaf size and maximum depth;
- occluder projected-area and triangle thresholds;
- occluder prepass budget;
- queries issued per frame and maximum pending queries;
- number of consecutive occluded results;
- occluded-node refresh interval;
- camera-motion invalidation thresholds;
- AABB expansion epsilon.

Enable the feature by default only if representative scenes show a consistent
frame-time improvement and the correctness suite finds no missing geometry. If
the prepass plus query cost exceeds the saved main-pass work, leave it opt-in and
proceed to hierarchical-Z.

The first performance gate is that obstructed views keep a relatively stable
main draw-call count and no longer produce the observed main-pass/flush spike.
The second gate is a net reduction in complete frame time after including the
depth prepass and query overhead. A 120 FPS result requires the complete frame,
not merely flush, to stay below 8.33 ms; report that separately from the
occlusion-specific pass/fail result.

Deliverable: recorded before/after results and defensible defaults.

## Future Hi-Z migration

The long-term backend can replace OpenGL query proxies with a depth pyramid and
GPU AABB tests:

1. Generate mip levels from the camera depth buffer using the correct reduction
   rule for the engine's depth convention.
2. Upload BVH-node or item bounds to an SSBO.
3. Project bounds, choose a mip from screen-space size, and test them in a compute
   shader.
4. Compact visible items into indirect draw commands.
5. Use indirect-count or a conservative fixed command range when supported.

Do this only after the query implementation has validated the visibility model.
Hi-Z is most valuable when paired with indirect rendering; reading GPU visibility
back to the CPU would reintroduce synchronization and reduce its benefit.

## Suggested file layout

- `include/Engine/Core/OcclusionCullingSystem.h`
- `src/Engine/Core/OcclusionCullingSystem.cpp`
- `include/Engine/Core/OcclusionBvh.h`
- `src/Engine/Core/OcclusionBvh.cpp`
- `shaders/OccluderDepth.vert` and a minimal fragment shader if required by the
  supported OpenGL profile
- `shaders/OcclusionBounds.vert` and a minimal fragment shader
- focused BVH and visibility-history tests under `tests/`

Keep OpenGL resource creation and query calls behind `OpenGLGraphicsDevice`.
Keep hierarchy construction, stable keys, history policy, and traversal free of
OpenGL dependencies so they can be unit tested and reused by a future backend.

## Completion criteria

The implementation is complete when:

- no artist-authored grouping or project migration is required;
- unsupported or uncertain cases render conservatively;
- the CPU never waits for an occlusion query;
- visible output matches occlusion-disabled output in the correctness suite;
- shadow rendering remains camera-independent;
- Render Stats separates frustum savings, occlusion savings, and occlusion cost;
- the known obstructed camera positions no longer cause main draw calls to grow
  rapidly when the frustum-culled count falls;
- representative obstructed views show lower 95th- and 99th-percentile flush and
  complete-frame times after all occlusion overhead is included;
- results state explicitly whether the complete frame meets the 8.33 ms 120 FPS
  budget and do not infer that from flush time alone;
- all new unit tests and the normal engine build pass.
