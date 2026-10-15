# Roadmap: work that is planned but not in v5.1.0

## Multiplayer (staged plan)
1. **Split state from presentation**: move rendering-only state out of `Entity`/`G`; make `game_tick` a pure function of
   world + player inputs (it already runs at a fixed 60 Hz, which helps).
2. **Input as commands**: the player sends `PlayerInput` (move vector, look, actions) each tick instead of touching the
   world directly; single player loops it back locally.
3. **Integrated server**: run world/entity ticking on a server thread; the client keeps a replicated view (columns sent
   on load, block changes and entity snapshots as deltas, 20 Hz, client-side interpolation).
4. **LAN** (UDP discovery + TCP/ENet transport), then a **dedicated server** binary (no GL), permissions, whitelist.
Risks: save format and AI assume one player (`player_ent()` is used in ~200 places) – step 1 needs a `players[]` list.

## Performance
* **Paletted column storage**: columns hold 384×16×16 `u16` (≈ 196 KB blocks + light). Plan: per-16³ section palette
  (1/2/4/8/16-bit indices, uniform sections stored as one value) – saves already use a key:variant palette.
* **Threaded meshing and generation**: worker pool for `worldgen_column` and section meshing with immutable snapshots
  of the 3×3 neighbourhood; the main thread uploads finished meshes.
* Already in v5: simulation distance, graphics presets, frame cap, update-queue de-duplication, fire/fluid tick fixes.

## Other
Piece-based rebuilds of the older structures (villages, mansions, bastions, trial chambers), animated moving pistons,
an opt-in crash reporter, code signing for Windows/macOS.
