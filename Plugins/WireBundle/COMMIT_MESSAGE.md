fix(wire-bundle): move shape controls into a visible floating toolbar

Move, width, spacing, alignment and collapse controls previously overlapped
reroute nodes and wires. Native node hit testing correctly took priority,
but this made the move arrow appear unresponsive.

- Place all controls in a floating toolbar above the selection, clamp it to
  the graph viewport and stack it vertically in narrow panels.
- Give buttons opaque backgrounds, hover outlines, short action hints and
  larger rectangular hit targets shared by painting and input handling.
- Freeze the toolbar position during a drag. Keep ordinary node and pin
  interaction native; allow toolbar actions over graph comment backgrounds.
- Preserve exact-selection editing and the single-reroute native workflow.
- Add placement and hit-target regression coverage for viewport edges,
  narrow panels and floating windows on positive and negative desktop origins.
- Update existing mouse-driven UI scenarios to locate the toolbar rather
  than relying on the previous inline handle coordinates.

Validation:
- UE 5.5 isolated verification build passed.
- All 7 WireBundle.Shape tests passed.
- All 14 focused Blueprint/Material mouse-driven toolbar scenarios passed:
  move, entry/exit width, spacing, Undo/Redo, Escape, release outside and
  application deactivation.
- The broader UI run crashed in Blueprint.GestureWheelCreatesThreeColumnsUndoRedo
  with an engine access violation. That separate scenario was excluded from
  the focused toolbar run; the full UI suite is not claimed as passing.
- Regenerated ToolsProject Visual Studio project files successfully.
- Built the canonical ToolsProjectEditor target successfully, including both
  WireBundle and WireBundleTests DLLs.

No Git commit was created and the user's staging area was preserved.
