fix(wire-bundle): restrict shape editing to the current selection

Registered reroutes previously expanded a partial selection into the entire
bundle's shape snapshot, allowing an unselected fourth reroute to move with
three selected nodes. Cached handle positions could also intercept ordinary
node clicks, producing different behavior for top, middle, and bottom nodes.

- Capture exactly the selected reroutes without implicit bundle expansion.
- Invalidate cached overlays and drags when selection membership changes,
  including replacement selections with the same node count.
- Reject commits whose snapshot does not match the current selection.
- Defer native graph node and pin clicks to Unreal instead of capturing them
  through overlapping handles. Keep single-node movement native and hide
  group handles and labels for single selections.
- Keep Select entire bundle as an explicit command.
- Add Blueprint selection-isolation and native Slate widget-path regressions,
  plus material subset movement and expression position checks.
- Record the required implementation, tests, project regeneration, and commit
  description workflow in AGENTS.md. Keep README documentation in English.

Validation:
- UE 5.5 Win64 plugin build succeeded in the isolated verification project.
- All 22 WireBundle automation tests passed, including move, width, spacing,
  Undo, stale-selection rejection, top/middle/bottom single-node selection,
  native click-path handling, and material selection isolation.
- Regenerated ToolsProject Visual Studio 2022 project files successfully;
  the new WireBundleSelectionTest.cpp is included in ToolsProject.vcxproj.

The ToolsProject editor was open during validation, so its loaded plugin DLL
was not replaced. Close the editor and rebuild ToolsProjectEditor to load the
fix. Real mouse interaction in the running editor still needs manual QA.
