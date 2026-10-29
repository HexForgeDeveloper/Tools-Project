Wire Bundle 0.8
An editor plugin for bundling connections in Blueprints and Material Graphs using Unreal Engine’s standard reroute nodes.

Commands
While holding Ctrl+Alt+Left Mouse Button (LMB), scrolling the mouse wheel down decreases the number of reroute node columns, while scrolling up increases it (1, 2, 3, etc.). The preview updates in real-time; releasing the LMB applies the selected layout. Outside of this gesture, the mouse wheel functions as usual.
Ctrl+Alt+B — Create a bundle between selected connected nodes; when used on a node that is part of an existing bundle, it selects the entire bundle.
Ctrl+Alt+A — Register existing chains. Select only the reroute nodes belonging to at least two complete chains.
Ctrl+Alt+Arrow Keys — Move selected reroute nodes; holding Shift increases the movement speed.
Right-click a node → Wire Bundle: Create bundle / Register chains. When used on a registered reroute node, options include selecting the entire bundle or "Unbundle wires."
"Unbundle wires" restores direct connections and removes only the reroute nodes associated with that bundle; any unrelated connections remain intact. Unbundling, creating, and registering bundles all support Undo/Redo. If a chain has been damaged or has acquired additional branches, the unbundle function will be unavailable. Commands re-verify the graph upon execution.

During the Ctrl+Alt+LMB gesture, the intended route and the insertion points for reroute nodes (displayed as diamonds) are shown. The default mode is configured under Movement → Layout → Reroute Columns. The maximum number of columns during creation is defined by "Max Reroute Columns" (default is 8, up to a maximum of 32). The ability to toggle the number of columns using the mouse wheel can be enabled via Gesture → Input → Wheel Changes Columns. Width, spacing, offset, and grid snapping are calculated by the same layout engine used during the initial creation process. The color, thickness, and visibility of the preview can be configured under Appearance → Route Preview. The rendered curves serve as a visual guide; the final splines are drawn by Unreal itself.

Settings can be found in Editor Preferences → Plugins → Wire Bundle. No custom Data Assets are required.

Modifying shape via mouse
Select one or more standard reroute nodes within a Blueprint or material. Handles appear directly on the selection, regardless of how the nodes were created—no prior registration with the plugin is necessary. For a bundle that has already been registered, selecting just one of its nodes is sufficient to edit the entire bundle.

The central handle with arrows moves the entire bundle.
The diamond-shaped handles on the sides adjust the width of the bundle; intermediate nodes shift proportionally, preserving the overall shape. These width handles are hidden if the group lacks a horizontal dimension.
The diamond handle at the bottom adjusts the spacing between individual selected chains or unconnected nodes, relative to the center. It remains hidden when only a single chain is selected.
Drag a handle using the left mouse button (without any modifier keys). Only a preview is displayed while dragging; the graph itself remains unchanged. Releasing the mouse button within the graph applies the changes as a single undoable operation. Pressing Esc (or a custom-assigned cancel key), losing focus, or releasing the mouse button outside the graph cancels the action. Standard drag-and-drop functionality for regular nodes remains fully available.

The Movement → Mouse Editing settings allow you to toggle the handles and restrict movement to specific axes. The plugin respects global grid-snapping settings. The color and size of the handles can be customized under Appearance → Shape Handles. If the topology or node positions change while a drag operation is in progress, the changes will not be applied.

The handles support both standard and registered reroute nodes, long chains, and configurations where different wires contain varying numbers of nodes. Existing connections and internal curves are preserved. For a standard selection, no bundle record is created; only the selected nodes are edited. Registration via Ctrl+Alt+A is required solely for persistent recognition and the "Unbundle" command—it is not needed for the appearance of control handles. Limiting the number of columns during creation does not restrict the editing of existing bundles.

Modules
WireBundle: handles commands, input and rendering, route planning, settings, and the storage/validation of bundles.
WireBundleTests: contains automated tests. The dependency points toward WireBundle; the core module does not depend on the tests.
Public headers in the core module provide interfaces for commands and tests. Module initialization is decoupled from the command system and the context menu integration.

Support
Saving and parsing of registered bundles are supported within Blueprints and standard Material Graphs. Material Functions and composite graphs do not yet support bundle storage; their standard reroute nodes remain editable using native Unreal tools.

Next steps: obstacle avoidance and further expansion of shape-editing capabilities.

Verification
18 automated tests cover: creation, gesture zones, window coordinates, settings and movement, route planning, registration, save/load/duplicate operations, parsing, and shape modification (including Undo/Redo). These tests can be found under the "WireBundle" category in the Automation window. Material parsing is also verified after applying changes and reopening the asset.

To modify the module structure, close the Unreal Editor and rebuild the ToolsProjectEditor. Live Coding cannot replace a full build when adding a new module.

Features such as mouse-wheel switching, control handle behavior during actual mouse input, and visual appearance require manual verification within the editor.
