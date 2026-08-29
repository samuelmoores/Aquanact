# Automatic Static Environment Batching Plan

## Goal

Reduce main-pass and directional-shadow draw-call overhead by automatically
combining compatible static environment geometry. Artists should not need to
manually create mesh groups.

## Design principles

- Preserve movable, skinned, transparent, and independently selectable objects.
- Batch only geometry that is safe to transform into a shared world-space buffer.
- Keep material/texture compatibility as the primary batching boundary.
- Build batches outside the per-frame render loop and reuse them until the scene
  or relevant object state changes.
- Keep an opt-out path for objects that require individual rendering or shadows.

## Implementation steps

1. **Inventory existing render data**
   - Document `RenderCommand`, `Mesh`, `SubMeshMaterial`, texture ownership, and
     entity static/movable flags.
   - Identify how selection, editor gizmos, transparency, skinning, and shadow
     casting depend on individual entities.

2. **Define batching eligibility**
   - Add or reuse an entity/object `static` flag.
   - Exclude skinned, animated, transparent/cutout, physics-driven, selected,
     individually scripted, and unsupported-material objects.
   - Add explicit `batching disabled` and `casts directional shadow` overrides.

3. **Create a batch compatibility key**
   - Key batches by shader variant, material parameters, base/specular/normal
     textures, vertex layout, and shadow settings.
   - Ensure texture/material lifetime is owned safely by the batch or referenced
     through stable asset handles.

4. **Implement batch construction**
   - Add a `StaticMeshBatch` builder that collects eligible submeshes.
   - Transform vertex positions and normals into world space using each object’s
     model matrix; preserve UVs, tangents, and material attributes.
   - Rebase/merge indices and create one or more GPU vertex/index buffers per
     compatibility key.
   - Compute batch AABBs and per-batch triangle/index counts.
   - Split batches when index limits, buffer-size limits, or material changes are
     reached.

5. **Integrate batch lifetime and invalidation**
   - Build batches after scene load/import and before rendering.
   - Rebuild only when static membership, transforms, meshes, materials, or
     relevant asset versions change.
   - Dispose GPU resources safely when scenes unload or batches are replaced.

6. **Integrate rendering**
   - Submit one render command per batch to the main pass.
   - Keep original commands for excluded or editor-selected objects.
   - Preserve frustum culling using batch AABBs.
   - Route batches through the existing material/shader setup without changing
     visual output.

7. **Integrate directional shadows**
   - Submit only shadow-casting batches to the directional shadow pass.
   - Use batch AABBs for light-frustum culling.
   - Support a separate shadow-only batch or opt-out when material/alpha rules
     require individual rendering.

8. **Editor and runtime controls**
   - Add a diagnostic toggle for static batching.
   - Display source objects, batch count, merged buffers, and draw calls saved.
   - Make selection map from a batch subrange back to the source entity.
   - Ensure disabling batching restores the original render-command path.

9. **Validation**
   - Compare images and lighting against the unbatched path.
   - Test opaque, textured, missing-texture, scaled/rotated, and large scenes.
   - Verify selection, picking, frustum culling, shadows, scene reload, and asset
     hot-reload behavior.
   - Measure main-pass and shadow-pass draw calls, CPU submission time, GPU frame
     time, memory usage, and frame-time spikes.

10. **Rollout strategy**
    - Start with opaque static environment meshes sharing the same shader and
      material textures.
    - Add broader material compatibility and shadow batching incrementally.
    - Keep the feature experimental until visual and performance validation is
      complete.

## Success criteria

- Static environment draw calls and shadow draw calls decrease substantially.
- Main and directional-shadow pass times improve in imported scenes.
- No visible differences for supported materials.
- Moving, skinned, transparent, selected, and opted-out objects remain correct.
- Batch rebuilds do not occur during steady-state frames.
