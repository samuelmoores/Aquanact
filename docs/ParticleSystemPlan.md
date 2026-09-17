# Particle System Implementation Plan

## Milestone 1: procedural spark particle (implemented)

- Add an engine-owned `ParticleSystem` component.
- Simulate position, velocity, gravity, age, lifetime, size, and color on the CPU.
- Support looping emission, bursts, restart, and a configurable particle cap.
- Submit particles through a renderer-specific batch path.
- Render point instances as camera-facing point sprites.
- Generate a soft radial glow in the fragment shader; no texture asset is required.
- Register the component in both the in-tree and externally generated component registries.
- Persist the component configuration in project state files.
- Expose the core controls in the entity inspector.

The default spark size is 12.5 world units, tuned for the current scene scale.

## Milestone 2: authoring and polish

- Add color controls and a preview/reset workflow to the inspector. (implemented)
- Add editable coordinate-effect presets for flames, embers, magic, healing,
  poison, sparks, fountains, smoke, explosions, and trails. (implemented)
- Add point, sphere, and box emission shapes with configurable velocity spread
  and radial speed. (implemented)
- Add size-over-life, local/world simulation, and additive/alpha blending.
  (implemented)
- Keep coordinate effects as entity components and move rain/snow into a
  scene-owned world-space weather pool replenished around the active camera
  and configured from the Particle System window. (implemented)
- Move emitter settings into a project-relative particle asset format.
- Add depth-aware soft particles and additional texture/blend options.
- Add per-particle sorting if alpha-blended effects require it; alpha emitters
  are currently sorted back-to-front as a group.

## Milestone 3: production features

- Add rotation variance, general curves, and sub-emitters.
- Add GPU instancing or compute simulation for large particle counts.
- Add collision, ribbons, animation sheets, and optional lighting/shadows.
