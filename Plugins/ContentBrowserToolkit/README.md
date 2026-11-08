# Content Browser Toolkit

## Folder reports

Right-click a single folder in the Content Browser to open one of these reports.
Both include subfolders and work without a map selection.

- **Folder Inventory...** lists assets from largest to smallest and summarizes
  asset counts and disk usage by class and immediate subfolder. Totals count each
  package once within each group and include separate export and bulk data files.
  Packages whose files cannot be measured are reported separately.
- **Folder Reference Audit...** shows incoming hard and soft package references
  from outside the selected folder. Select an asset to see its referring
  packages. A sibling such as `/Game/PropsExtra` is outside `/Game/Props`.
  The report uses saved Asset Registry data and cannot detect every runtime
  reference or prove an asset is unused.

**Export CSV** writes all rows and groups under
`Saved/ContentBrowserToolkit/Audits`. The commands inspect project content
without changing assets.

## Map Asset Usage Audit (UE 5.5)

Select assets in Content Browser, right-click and choose **Asset Audit to Map...**.
Choose a World map and click **Scan**. Only the selected assets appear in results.
Right-click a folder to scan its contents, including subfolders. No manual path
entry is required. **Window > Asset Audit to Map** scans `/Game`.

Results use compact Asset, Type, Usage and Size columns. Search by name/path or
filter by usage status. Select one row to read its reference evidence in the
bottom panel; double-click it to locate the asset in Content Browser. CSV export
includes all scanned results, even when the table is filtered.

The audit is read-only: it does not delete, rename, save or unload assets, or
switch the current editor level. Loading the selected map for inspection may
increase memory usage and execute normal asset load hooks. Save your work before
scanning; unsaved state can affect reference information.

The scanner refreshes the Asset Registry, follows transitive package, soft,
searchable-name and management dependencies, inspects loaded map object
references, and includes external actors/objects of the selected map and its
reachable saved sublevels/level instances. It also checks incoming references
from other registered assets and literal package/folder paths in project Source,
Config, DevOps and project plugin source/config/scripts.

Results include package size (main file plus bulk sidecars) and evidence:

| Status | Meaning |
| --- | --- |
| Used by selected map | Reachable dependency or loaded object reference; the chain is shown. |
| Other references found | Another asset or a source/config literal references it. Keep it for review even if it is outside this map's dependency closure. |
| Protected / incomplete | Special package, unsaved/missing data or incomplete scan. |
| No tracked references - REVIEW ONLY | These checks found no reference. This is a review candidate, never proof that deletion is safe. |

Maps, map build data, redirectors, primary assets/labels, external packages,
dirty packages and missing files are protected when no stronger evidence exists.
Cancellation or unresolved coverage protects all otherwise unreferenced results.
Cancel is cooperative; an individual synchronous engine load or registry refresh
must finish before cancellation is checked.

**Export CSV** writes a new UTF-8 report to
`Saved/ContentBrowserToolkit/Audits/Usage-<guid>.csv`, including the selected map,
folder, status, bytes, evidence and limitations. Changing the folder or map
invalidates previous results. No delete operation is provided.

Dynamic path construction, custom runtime loading, external consumers, code
outside the scanned project directories and unavailable content cannot be
exhaustively detected. The selected map is only one usage root: test other maps
and game modes and inspect cooking/Asset Manager rules before deciding what to
remove. Nothing in this report grants deletion approval.

## Validation

`ContentBrowserToolkit.Audit.TransitiveCycles` checks indirect dependencies and
cycles; `ContentBrowserToolkit.Audit.Cancellation` checks partial traversal;
`ContentBrowserToolkit.Audit.SavedMap` scans the saved engine Entry map, verifies
its protection and package size, exports a CSV, and rejects invalid input.
The Tools-Project CI script discovers this test group.

`ContentBrowserToolkit.FolderReports.SavedPackages` saves test packages and
checks recursive inventory, external references across a folder boundary and CSV export.


Automation implementations live in `Source/ContentBrowserToolkitTests`, a dedicated Editor test module inside this plugin. The production plugin has no test-module dependency. See the test module policy in the Tools-Project DevOps documentation.
