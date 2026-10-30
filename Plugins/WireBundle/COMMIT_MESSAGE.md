feat(wire-bundle): support backward obstacle routes and native spline directions

Route bundles travelling right-to-left through the same bounded obstacle search
used for forward paths. Reflect geometry around the snapped layout center and
restore source-to-target knot order, grid coordinates and native spline tangents.
Direction is inferred from connected pins; no new user setting is required.

- Preserve wheel-selected columns, spacing, detour limits and additional-bend
  budgets in both travel directions. Keep mixed-direction obstacle searches
  conservative: reject the bundle before editing instead of using a partial plan.
- Predict endpoint knot orientation from remaining neighbours and the proposed
  first/last route knot. Keep ordinary node pin tangents in their native direction.
- Store the orientation of every new knot, including outside gestures where an
  end knot can face differently from the overall route. Use these signs in preview.
- Include endpoint neighbour geometry in the route cache key. Expose the read-only
  wire snapshot to verify actual pin geometry in the separate test module.
- Add backward planner cases for ordinary pins and knot endpoints, asymmetric
  tangents, one/two/three columns, snapping, determinism and mixed-direction refusal.
- Exercise forward and backward multi-bend creation in Blueprint and Material
  windows. Compare preview positions and sample every committed native curve for
  clearance, requiring current pin geometry for all routed edges. Verify Undo/Redo.
- Install fixed-size test obstacles through each graph editor's node factory;
  Material nodes bypass global factories with their own visual widgets. Synchronize
  mirrored Material test expressions with graph positions.
- Wait for two input frames after pointer movement in UI tests, in addition to
  elapsed time, so a slow frame cannot leave handle hover state stale.

Validation:
- UE 5.5 isolated verification build succeeded.
- The final full run passed all 50 WireBundle tests, including the entire UI suite.
  An earlier run exposed two timing failures in existing Blueprint handle/drag
  tests; the frame-based settling change was included in the successful final run.
- Regenerated ToolsProject Visual Studio 2022 project files successfully.
- Canonical ToolsProjectEditor build succeeded and linked both plugin DLLs.
- git diff --check passed.

Limits: a shared obstacle search requires one travel direction per bundle.
Existing bounded search limits and fallback sizes for unlaid-out nodes still apply.
No Git commit was created by this task; existing staging was left untouched.
