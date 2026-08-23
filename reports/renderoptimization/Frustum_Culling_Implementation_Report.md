# Frustum Culling Implementation Report

Date: 2026-08-23

## Objective

Add CPU-side frustum culling to the render loop so meshes outside the active
camera volume do not generate main-pass draw calls. The checked-in grocery-store
level is represented by a small number of scene entities, but its imported Maya
group contains hundreds of child meshes, so culling only at the entity level is
not sufficiently granular.

## Final architecture

The renderer retains one `RenderCommand` per scene entity. Imported static meshes
inside that entity are tested individually immediately before their buffer is
drawn.

This preserves batching at the entity level:

1. `RenderManager::BuildRenderCommands` captures the entity mesh, shader, model
   matrix, entity ID, and aggregate world bounds.
2. `RenderManager::Flush` constructs the active camera frustum once.
3. An entity whose aggregate AABB is completely outside the camera frustum is
   rejected immediately.
4. A potentially visible static entity is passed to
   `OpenGLGraphicsDevice::DrawCulled`.
5. The camera frustum is transformed into the entity's local coordinate space
   once.
6. Every imported submesh AABB is tested against that local frustum.
7. Only visible submesh buffers reach `glDrawElements`.

Skinned entities use a conservative fallback and remain visible at the entity
level. Imported bind-pose bounds cannot safely describe all animated poses.

## Frustum math

`include/Engine/Core/Frustum.h` provides the frustum representation and AABB
intersection test.

- Six planes are extracted from the OpenGL view-projection matrix: left, right,
  bottom, top, near, and far.
- GLM's column-major matrix layout is handled by explicitly constructing rows.
- Planes are normalized.
- AABB intersection uses the positive-vertex method.
- Reversed bounds are canonicalized before testing.
- A small plane epsilon reduces boundary flicker.
- Invalid bounds and degenerate planes fail conservatively by keeping geometry
  visible.

## Imported submesh bounds

`ModelImporter` now calculates local minimum and maximum bounds for every Assimp
mesh while reading its vertices. These arrays preserve the same ordering used by
the imported face counts, material entries, and GPU buffer indices.

`Mesh` owns the resulting bounds and exposes them through:

- `SubMeshMinBounds(index)`
- `SubMeshMaxBounds(index)`

This lets a Maya group remain one selectable and serializable engine entity while
its child meshes are culled independently.

## Main-pass behavior

Main-pass culling occurs at two levels:

- Aggregate entity AABB rejection avoids entering the graphics-device draw path
  when the complete entity is outside the camera.
- Per-submesh rejection handles large grouped models whose aggregate AABB
  intersects the frustum even when most of their child meshes do not.

Before shader, lighting, and texture setup, the device performs a prepass across
the entity's submesh bounds. If no child mesh is visible, the draw returns early
and avoids all graphics-state setup for that entity.

If at least one child is visible, shader and lighting state are configured once
for the entity. Visible buffers are then drawn and culled buffers are skipped.

## Shadow-pass behavior

Camera-frustum culling is not applied directly to shadow casters because an
off-camera mesh can cast a shadow into the visible scene.

Instead, shadow casters are tested against the relevant light volume:

- Directional-shadow submeshes are tested against the directional light
  view-projection frustum.
- Point-light shadow submeshes are tested independently against each of the six
  cubemap-face frustums.
- Skinned shadow casters remain conservatively included.

The light frustum is transformed into entity-local space once per entity and
shadow pass. Submesh tests then use local bounds directly. This avoids repeatedly
transforming eight AABB corners for every mesh and light face.

## Performance corrections made during implementation

An early implementation emitted one complete `RenderCommand` per imported child
mesh. Although this enabled granular culling, it caused a severe performance
regression because shader uniforms, lighting state, texture bindings, skinning
state, and shadow setup were repeated for every child mesh. The point-light
shadow pass amplified this cost across six cubemap faces.

That command expansion was removed. The final implementation uses one command per
entity and performs submesh visibility tests within the existing batched draw.

A later shadow implementation transformed all eight AABB corners for every
submesh and every cubemap face each frame. This produced excessive flush time,
particularly in debug builds. It was replaced with the current local-frustum
approach: one matrix combination and plane extraction per entity/pass, followed
by inexpensive local AABB tests.

## Render Stats changes

The Render Stats window now distinguishes scene structure, visibility work, and
actual GPU submission:

- Scene objects
- Candidate mesh buffers
- Skipped objects
- Camera-visible mesh buffers
- Camera-frustum culled mesh buffers
- Main draw calls saved
- Camera draw calls actually issued
- Camera-independent shadow-map draw calls
- Total GPU draw calls
- Main, shadow, and total triangle counts

"Candidate mesh buffers" can remain high while looking at an empty area. It is
the number considered for visibility, not the number actually submitted. The
authoritative main-pass result is "Camera draw calls actually issued."

Shadow-map draw calls can remain nonzero when the camera sees no geometry because
shadow maps render from the lights' perspectives.

## Known limitations

### No occlusion culling

Frustum culling only rejects geometry outside the camera volume. It does not
reject an interior shelf, product, or fixture that is inside the frustum but
hidden behind the grocery-store exterior wall. Depth testing prevents hidden
fragments from reaching the final image, but the associated draw calls and vertex
processing have already occurred.

This can cause draw-call spikes when a small camera rotation brings many hidden
interior bounds into the frustum.

### Skinned mesh bounds

Per-submesh culling is disabled for skinned meshes until conservative animated
bounds are available. Using bind-pose bounds could incorrectly remove animated
limbs or other displaced geometry.

### CPU traversal

Every candidate static submesh is still tested each frame. The plane tests are
cheap, but a spatial hierarchy may eventually be needed for substantially larger
levels.

### Draw-call granularity

Every visible imported submesh remains a separate GPU draw. Frustum culling saves
off-screen submissions but does not combine visible meshes that share compatible
materials and state.

## Recommended next work

For the grocery-store level, coarse occlusion zones are the most direct next
optimization:

1. Divide the level into exterior, main interior, back-room, and optional aisle
   groups.
2. Associate doorways and windows with portals connecting those zones.
3. When the camera is outside, reject interior groups unless a connecting portal
   is visible.
4. Retain submesh frustum culling inside every visible zone.
5. Record zone-culled items and saved draw calls separately in Render Stats.

General-purpose alternatives include hierarchical-Z occlusion or batched GPU
occlusion queries. Queries should operate on coarse groups rather than individual
grocery items to avoid replacing draw overhead with query overhead and GPU/CPU
synchronization stalls.

After occlusion work, material-aware static batching or multi-draw indirect can
reduce the cost of the visible submeshes that remain.

## Verification

The `AquanactEngine` x64 debug target was rebuilt successfully after the final
main-pass, shadow-pass, statistics, and local-frustum changes.

Recommended runtime checks:

1. Look completely away from the level and confirm camera draw calls approach
   zero.
2. Compare candidate, visible, and culled mesh-buffer counts while rotating the
   camera.
3. Confirm point and directional shadows remain present near frustum boundaries.
4. Watch flush time while looking at empty space, the exterior wall, and the full
   store interior.
5. Verify static meshes do not pop at camera or light-frustum edges.
6. Confirm animated entities remain rendered throughout their full animation
   range.
