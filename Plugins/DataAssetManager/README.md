# Data Asset Manager

An Unreal Editor plugin for browsing, editing and managing Data Asset instances in one workspace.

**Status:** beta. Development and automated checks currently target Unreal Engine 5.5 on Windows. Compatibility with later engine versions has not yet been verified.

## Features

- Browse Data Assets from configured project directories and enabled project plugin content roots.
- Search asset names and combine class, plugin, modified-only and invalid-only filters.
- Edit selected assets in an embedded Details panel or open the Property Matrix.
- Create, rename, duplicate, move and delete assets through Unreal Editor tools.
- Save selected assets or dirty Data Assets within the manager scan scope.
- Validate assets through their `IsDataValid` implementation and inspect results in Message Log.
- Inspect package dependencies and referencers with Reference Inspector.
- Compare editable top-level properties of two assets of the same class and copy values between them.
- Import and export property values as JSON from the Details header menu.
- Reset editable properties to class defaults, randomize supported numeric/color properties and reorder array elements.
- Open Unreal Reference Viewer, Size Map, Asset Audit and source control tools for selected assets.

## Installation

1. Close Unreal Editor.
2. Copy this complete folder to `<YourProject>/Plugins/DataAssetManager`.
3. Generate project files and build the Editor target for your project. Source distributions require the Unreal C++ build toolchain.
4. Enable **DataAssetManager** in the Plugins window and restart if requested.
5. Open **Tools > Data Asset Manager**.

The plugin declares dependencies on **Editor Scripting Utilities** and **Asset Manager Editor**. Both are editor plugins; the manager has no runtime module.

## First use

1. Open **Project Settings > Plugins > DataAssetManager**.
2. Configure **Scanned Asset Directories** using Unreal package paths such as `/Game/Data`. The default is `/Game`.
3. Configure **Excluded Scan Asset Types** if some classes should be hidden.
4. Open the manager and select an asset to edit its properties.
5. Save the selected assets after editing. **Save All** applies to dirty Data Assets in the manager scan scope, including entries hidden by filters.

Enabled project plugin content roots are also scanned. The Documentation menu opens the configurable **Documentation URL**; it does not open a page when that setting is empty.

## Comparison and references

Select two assets of the same class and choose **Data Asset Diff**. The comparison shows editable top-level property differences. Copy buttons apply a property value to the opposite asset and refresh the comparison.

Reference Inspector displays package dependencies, referencers and unresolved dependency packages. A "potentially unused" result is an inspection hint, not proof that an asset is safe to delete: dynamically constructed paths and external consumers may not be represented by registry references.

## Undo, saving and JSON

Property copy, reset, JSON import, color randomization and array reordering have automated Undo/Redo coverage. These checks do not establish undo support for every property type or every editor operation.

Rename and move retain standard Unreal Asset Tools behavior; the plugin does not guarantee Ctrl+Z recovery for these operations. Deletion uses Unreal's confirmation workflow and may invalidate transaction history. Saving writes packages to disk; Undo changes editor state and does not restore a previous file revision.

JSON is a property interchange format, not a complete package backup or asset migration format. Referenced assets must be available in the receiving project. Numeric integer randomization accepts only finite intervals and restricts double-based bounds to exactly representable integers within `+/- (2^53 - 1)` and the property type limits.

## Source layout

- `Source/DataAssetManager/Public` and `Private`: editor module API and implementation.
- `UI`: main manager and independent creation, comparison and reference inspection windows.
- `Services`: asset operations, registry subscription ownership and integer randomization.
- `Models`: asset list sorting and filtering.
- `Customization`: Details panel commands and property extensions.
- `Source/DataAssetManagerTests`: dedicated editor automation module. Production code does not depend on this module.

## Tests

In Unreal Editor, open the Automation tab in Session Frontend, select the `DataAssetManager` group and run the tests. The same group can be run from a terminal:

```powershell
& '<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' '<Project>.uproject' `
    -unattended -nopause -nosplash -nosound -NullRHI `
    '-ExecCmds=Automation RunTests DataAssetManager' `
    '-TestExit=Automation Test Queue Empty' `
    '-ReportExportPath=<AbsoluteReportDirectory>'
```

Replace the engine, project and report paths. Inspect the exported report; a zero process exit code alone does not prove all requested tests ran successfully. NullRHI tests exercise logic and widget construction, not visual rendering or physical mouse/keyboard interactions.

Test assets are generated in process-specific package directories. Keep generated artifacts, `Binaries`, `Intermediate` and test reports out of a source release.

## License

See [LICENSE](LICENSE). Unreal Engine and its components remain subject to their own terms.
