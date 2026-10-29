feat(wire-bundle): add bounded obstacle routing with extra bends

Enable Movement > Routing > Avoid Nodes to route new bundles around graph
nodes. Try an unchanged layout and a shared upper/lower corridor first. When
neither clears the graph, Allow Extra Bends searches a forward path between
staggered obstacles and inserts the necessary native reroutes.

- Keep the pure obstacle search in WireBundleObstacleRouting.cpp, separate
  from parallel layout and the Slate/graph adapter. Keep all tests in the
  WireBundleTests module.
- Preserve every wheel-selected column and wire spacing at every bend;
  report additional routing knots per wire in the preview hint.
- Add Allow Extra Bends and Max Routing Reroutes to plugin preferences.
  Avoid Nodes remains opt-in; Allow Extra Bends defaults on within that mode.
- Bound candidate heights, horizontal stations and edge attempts. Prune
  redundant stations without discarding requested columns. Validate native
  cubic splines and clearance again after pruning.
- Share the same plan between preview and commit. Cache repeated planning
  only when panel, geometry, settings and spline parameters match exactly.
- Reject blocked or over-budget routes before opening a transaction,
  preserving connections, metadata and Redo history.
- Preserve native graph node and pin interaction, including single-reroute
  dragging. Comments and reroute dots are not obstacles; each wire excludes
  its own endpoint bodies.
- Refresh the standalone UI test graph after Undo/Redo, fixing stale pin
  widget painting after the wheel-creation test's Redo. Send native Slate
  button events at the actual knot grab spacer for single-node drag tests.
- Add deterministic staggered-obstacle planner regressions for one, two and
  three columns, dense curve clearance, snapping, pruning and routing limits.
- Add Blueprint and Material mouse tests for multi-bend preview/commit
  position agreement, native chain counts, persistent topology and Undo/Redo.
  Test-only fixed-size Slate walls make obstacle geometry repeatable.

Validation:
- UE 5.5 isolated verification build succeeded.
- All 47 WireBundle automation tests passed together, including the full UI
  suite, wheel creation, native single-node dragging and multi-bend routing.
- Regenerated ToolsProject Visual Studio 2022 project files successfully.
- Canonical ToolsProjectEditor build succeeded; both plugin DLLs were linked.

Limits: routing currently supports forward chains, up to 64 horizontal
stations, 24 candidate heights and 32,768 edge attempts. It does not guarantee
finding every geometrically possible path. Missing layout uses fallback node
sizes; existing bundles and normal dragging are not rerouted automatically.

No Git commit was created; the user's staging area was preserved.
