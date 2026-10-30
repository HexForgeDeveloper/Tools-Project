Wire Bundle 0.8
An editor plugin for bundling connections in Blueprints and Material Graphs using Unreal Engine’s standard reroute nodes.

An Unreal Engine editor plugin that bundles Blueprint and Material Graph connections using standard native reroute nodes.

## Controls

- Hold Ctrl+Alt+Left Mouse Button and cross the wires you want to bundle. Scroll down to decrease the number of reroute columns or up to increase it: 1, 2, 3, and more. The preview updates immediately; release the mouse button to create the selected layout. Outside an active gesture, the wheel behaves normally.
- Ctrl+Alt+B creates a bundle between selected connected nodes. With a registered bundle node selected, it selects the entire bundle instead of creating duplicates.
- Ctrl+Alt+A registers existing reroute chains. Select only the reroutes belonging to at least two complete chains.
- Ctrl+Alt+Arrow moves selected native reroutes. Add Shift for faster movement.
- Right-click a node and use the Wire Bundle section to create a bundle or register existing chains. Registered reroutes also offer Select entire bundle and Unbundle wires.

Unbundle wires restores direct connections and removes only the reroutes owned by that bundle. Unrelated connections are preserved. Creation, registration, and unbundling support Undo/Redo. Unbundling is unavailable when the recorded chain is damaged or has acquired extra branches. Commands validate the graph again when executed.

## Route preview and settings

During a Ctrl+Alt+Left Mouse Button gesture, the overlay shows the planned route and marks insertion points with diamonds. The preview and creation use the same layout planner for width, wire spacing, center offset, and grid snapping. Preview curves are guides; Unreal draws the final native splines.

Settings are available under Editor Preferences > Plugins > Wire Bundle. User-created Data Assets are not required.

- Movement > Layout > Reroute Columns sets the initial column count for each gesture.
- Max Reroute Columns limits new bundles to 8 columns by default and can be increased to 32.
- Gesture > Input > Wheel Changes Columns enables switching the column count with the wheel during a gesture.
- Appearance > Route Preview controls preview visibility, color, and thickness.

## Editing shapes with the mouse

Select two or more native reroutes in a Blueprint or material graph. Handles work with ordinary nodes regardless of who created them; plugin registration is not required.

Selecting exactly one reroute shows no handles or labels and preserves standard Unreal mouse movement. This also applies to nodes belonging to registered bundles.

The floating toolbar sits above the selection, stays within the graph viewport, and switches to a vertical layout in narrow panels. Buttons have opaque backgrounds, hover borders, action hints and enlarged rectangular hit targets. Its position stays fixed during a drag.

- The arrow button moves the group.
- The two width buttons resize its width. Intermediate nodes move proportionally, preserving the shape. Width handles are hidden when the group has no usable horizontal extent.
- The spacing button changes the spacing between separate selected chains or disconnected nodes around the center. It is hidden for a single chain.

Drag a handle with the left mouse button without modifiers. Dragging only previews the change; the graph is modified when the button is released inside the original graph. One Undo restores the entire operation. Esc, or the configured cancel key, loss of focus, and releasing outside the graph cancel the drag. Standard native node dragging remains available.

Movement > Mouse Editing controls handle availability and the movement axis constraint. Mouse editing uses the shared grid settings. Appearance > Shape Handles controls handle color and size. A drag is rejected if node positions or topology change before it is committed.

Handles support long chains and different chain lengths on different wires. Connections and internal bends are preserved. Ordinary selections form temporary groups without creating persistent bundle metadata; only selected nodes are edited. Shape handles always use exactly the current selection, including nodes from registered bundles. Use Select entire bundle explicitly when you want to edit the whole bundle. Ctrl+Alt+A registration is needed for persistent bundle recognition and Unbundle, not for displaying handles. The column limit for creation does not limit editing existing chains.

## Vertical alignment

Select two or more reroutes. Click the vertical-line toolbar button, or choose Wire Bundle > Align reroutes vertically from the context menu.

Only selected nodes are aligned. They share the group's average X position while retaining their original Y positions and connections. Grid snapping is respected. Alignment works with ordinary and registered reroutes in Blueprint and material graphs without requiring registration. One Undo restores the group. The icon is hidden for a single selected node.

## Collapsing a reroute chain

Select two or more connected reroutes in the same chain, then click the inward-arrow toolbar button or choose Wire Bundle > Collapse reroute chain.

One native reroute remains at the average center of the selection, respecting grid snapping. The original incoming connection and outgoing branches are preserved. The remaining reroute is selected; since only one node is selected, handles and labels disappear. One Undo restores the entire chain, its positions, and connections.

This works with ordinary reroutes and registered bundle wires in Blueprint and material graphs. A registered wire is updated to reference its shortened chain when possible. If an affected registration cannot describe the resulting graph, it is removed with a notification; native connections remain intact.

Independent chains, cycles, incompatible types, and branching paths inside the selection cannot be collapsed into one signal. Select one continuous chain at a time.

## Modules

- WireBundle contains graph commands, input and overlays, route planning, settings, persistent storage, and bundle validation.
- WireBundleTests contains automation tests and depends on WireBundle. The main module does not depend on the test module.

The main module exposes public interfaces for commands and tests. Module startup is separate from graph commands and context menu registration.

## Avoiding nodes when creating bundles

Enable **Movement > Routing > Avoid Nodes** in Editor Preferences > Plugins > Wire Bundle.
The setting is off by default. **Obstacle Clearance** controls the gap around node bodies;
**Max Route Detour** limits how far the bundle may move vertically from the gesture center.

The planner checks native spline curves, including their source/target approaches, and
first searches parallel corridors above and below blocking nodes. If none works and
**Allow Extra Bends** is enabled, it searches a forward path between staggered obstacles.
The wheel-selected columns are retained; extra native reroutes are inserted only where
needed. The preview hint reports the extra reroutes per wire. **Max Routing Reroutes**
limits the total number on each wire, including the requested columns (default 16).
Disable Allow Extra Bends to keep an exact column count.

Wire spacing is preserved at every bend. The planner removes unnecessary intermediate
points, respects grid snapping and validates the resulting curves again. Preview and
creation use the same plan. Repeated previews reuse a cached result only while geometry,
settings and native spline parameters remain unchanged. One Undo removes the whole route.

The search supports bundles travelling left-to-right or right-to-left and has bounded
work: up to 64 horizontal stations,
24 candidate heights and 32,768 edge attempts. It does not guarantee a solution for every
maze. With node obstacles present and avoidance enabled, a single bundle must have one travel direction;
a mixture of opposing directions is rejected without edits. If no candidate clears the obstacles within the routing
limits, the guide shows a blocked hint and releasing the gesture leaves nodes, connections,
metadata and Undo/Redo history unchanged.

Backward chains store reroutes in source-to-target order. Native knot orientation is
predicted from the remaining neighbours and the planned first/last knot. Ordinary node
pins retain their normal tangent direction; intermediate reroutes follow the route.
Preview and collision checking use these endpoint directions, including backward curls.
Neighbour geometry is part of the preview cache key.

Comments and reroute dots are not obstacles; source and target node bodies are excluded
for their own wire. Visible nodes use current Slate geometry; nodes without layout use
fallback size estimates. Existing bundles and ordinary node dragging are not automatically
rerouted.

The pure layout and obstacle planners live in `WireBundleRoutePlanner.cpp` and
`WireBundleObstacleRouting.cpp`. The graph adapter supplies geometry and spline math;
all automation and mouse-driven tests remain in the separate `WireBundleTests` module.

## Supported graphs

Persistent bundle recognition and unbundling are supported in Blueprint and root Material graphs. Material Functions and composite graphs do not yet support persistent bundle storage. Their native reroutes remain editable using Unreal's standard tools.

Planned work includes mixed-direction bundle routing and further shape editing improvements.

## Validation and building

The automation tests cover creation, gesture scope, window coordinates, settings, movement, route planning, registration, save/load and duplication, unbundling, shape editing, native selections, vertical alignment, and reroute chain collapse with Undo/Redo. Material tests also cover Apply, reopening assets, expression position synchronization, and long chains.

### Automation Driver UI suite

`WireBundle.UI` is a separate group in the `WireBundleTests` module. It sends
mouse and keyboard input through Unreal's Automation Driver into a real Slate
window containing an `SGraphEditor`. Blueprint and root Material graphs use the
same scenarios; Material fixtures also create an actual Material Editor.

The scenarios exercise single-node dragging, move/width/spacing handles with a
partially selected registered bundle, keyboard Undo/Redo, Escape cancellation,
release outside the graph, application deactivation, and Ctrl+Alt+drag creation
with wheel-controlled column count, plus routing around real obstacles with preview/commit
position checks, staggered obstacles requiring extra bends in both travel directions, sampled native
curve clearance after creation, and blocked-route history preservation. Assertions inspect node positions, selection,
connections and bundle records rather than relying on screenshots alone.

Single-node drag scenarios send native Slate button events at the grab spacer,
using the Driver for cursor movement, because the spacer center is covered by the pin
and fails the Driver's center-based visibility check.

Fixed-size obstacle widgets are installed through each test graph editor's node factory,
including Material graphs whose nodes provide their own default visual widgets.

The standalone graph editor registers for Undo/Redo refresh callbacks so restored pins
replace stale Slate widgets before painting. The fixture handles temporary graphs, window layout, focus, input cleanup and
settings restoration. `AutomationDriver` is a private dependency of the test
module only; the plugin's editor module does not depend on it. Test actions run
on a worker thread, while graph and Slate inspection runs on the game thread.

Run these tests in a disposable project with the plugin enabled. They create
temporary editor windows and reset the editor transaction buffer, so do not run
them in a working session whose Undo history you need to keep. Use a full editor
with rendering, not `NullRHI`. UI coverage does not validate overlay aesthetics,
all monitor/DPI arrangements, or every operating-system focus transition.

```powershell
& "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" "$TEST_PROJECT" `
  '-ExecCmds=Automation RunTests WireBundle.UI' `
  '-TestExit=Automation Test Queue Empty' `
  "-ReportExportPath=$REPORT_PATH" -unattended -nop4 -nosplash -RenderOffscreen
```

Set `UE_ROOT`, `TEST_PROJECT` and `REPORT_PATH` to your engine, disposable project
and report directory. Use `WireBundle.UI.Blueprint` or `WireBundle.UI.Material`
for one graph type, or `WireBundle` for the full suite. Check the exported JSON
report for failures and missing tests as well as the process exit code.

Close Unreal Editor and build ToolsProjectEditor when changing module structure. Live Coding does not replace a full build when adding a module.

Overlay appearance and usability at different zoom/DPI settings still benefit from a short manual review.

## Task workflow

Follow [AGENTS.md](AGENTS.md): implement the change, add and run regression tests, regenerate ToolsProject project files, then write the commit description.
