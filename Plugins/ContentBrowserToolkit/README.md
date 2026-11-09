# Content Browser Toolkit

Content Browser Toolkit adds filters and read-only reports to the Unreal Editor Content Browser. Use it to trace references, review a folder before moving it, or compare the settings of selected assets.

![Content Browser Toolkit in Unreal Editor](Resources/ContentBrowserToolkitImage.jpg)

## Getting started

This source plugin has been verified with **Unreal Engine 5.5**. It runs in the editor.

1. Copy the `ContentBrowserToolkit` folder into your project's `Plugins` directory so the descriptor is at `Plugins/ContentBrowserToolkit/ContentBrowserToolkit.uplugin`.
2. Build the project editor target, then open the project. Enable **Content Browser Toolkit** in **Edit > Plugins** if needed and restart the editor when prompted.
3. Right-click an asset or folder and look for the **Content Browser Toolkit** section. In a mixed asset selection, each report uses the asset types it supports.

For quick triage, open **Filters > Content Browser Toolkit**. Examples include **Missing Source File**, **Data-Only Blueprints**, **Textures With No Mips**, **Static Meshes No Collision** and **Audit: References Plugin**. Filters based on Asset Registry tags depend on the metadata available for each asset.

## Common workflows

| Goal | Steps |
| --- | --- |
| Find why a map uses a texture | Run **Show Usage Chains...** on the texture and select a row to read the map-to-package chain. For a deeper scan of a particular level, use **Asset Audit to Map...** and select that map. |
| Plan a folder move | Run **Folder Migration Preview...**, choose the destination, and review **Destination conflict**, **External dependency**, and **External referencer** rows before moving assets. |
| Review a possible duplicate | Run **Find Exact Texture Duplicates...** on a 2D texture. Compare sRGB, compression, and mip settings, then run **Show Usage Chains...** on the candidate before choosing a replacement. |
| Review a material change | Run **Find Material Instances...** on the parent. Select the instances you want to compare and run **Compare Instance Overrides...** to see which values they inherit or override. |
| Check imported content | Run **Check Texture Sets...** on a member of a texture set, **Mesh Setup Audit...** on static meshes, or **Audio Import Audit...** on sound waves. Run **Folder Naming Audit...** to check naming across the whole folder. |

## Content Browser filters

| Area | Filters |
| --- | --- |
| Import and ownership | Imported assets, missing source files, sources outside the project, and primary or non-primary assets. |
| Size and naming | Large or tiny packages, known prefix violations, spaces in names, and names starting with a lowercase letter. |
| Blueprints | Data-only or scripted Blueprints, component counts, native or project parent classes, interfaces, Find in Blueprints data, missing descriptions, and missing categories. |
| Textures | Large textures, invalid source data, missing mips, and power-of-two adjustments. |
| Static meshes | Nanite state, high triangle counts, no collision, single LOD, material slot and UV channel counts, and complex collision. |
| Sound waves | Looping, multichannel, low sample rate, low compression quality, cloud streaming, and mature audio. |
| References | Assets with referencers or dependencies, and references to engine, project, or plugin content. Turn on **Enable Cached Audit Filters** in project settings to show these filters. |

Use **Folder Naming Audit...** when you want to apply the project's configured prefixes. The **Name Violates Prefix** filter uses built-in conventions for common asset classes.

## Trace map usage

Choose **Asset Audit to Map...** from an asset or folder context menu, pick a map, and click **Scan**. An asset selection limits the rows to those assets. A folder selection includes its subfolders. To scan project content under `/Game`, open **Window > Asset Audit to Map**.

Search the results by name or path, filter by usage status, and select a row to read its evidence. Double-click a row to locate the asset in the Content Browser. **Export CSV** includes every result even when the table is filtered.

| Status | How to read it |
| --- | --- |
| **Used by selected map** | The audit found a dependency chain or a loaded object reference from the map. |
| **Other references found** | The audit found an incoming asset reference or a literal reference in project files. |
| **Protected / incomplete** | A protected package type, unsaved or missing data, or an incomplete scan requires review. |
| **No tracked references - REVIEW ONLY** | The audit found no reference in the sources it checked. Review other maps, Asset Manager and cook rules, and runtime loading before removing an asset. |

The map audit combines Asset Registry dependencies with references from loaded map objects, saved external actors and objects, and literal paths in project source, configuration, and scripts. Save relevant assets before scanning. Loading a map for inspection can use additional memory. Cancellation takes effect between synchronous editor operations.

Use **Show Usage Chains...** for the reverse question. Starting from a selected asset, it shows one shortest saved package reference chain from each map it finds. It uses Asset Registry package edges, so its coverage differs from the map audit above.

## Review a folder

Right-click one folder to open these reports. They include subfolders.

| Command | What to look for |
| --- | --- |
| **Folder Inventory...** | Asset counts and on-disk package sizes grouped by class and immediate subfolder. Missing size data is reported separately. |
| **Folder Reference Audit...** | Incoming saved package references from outside the folder. Select a row to see the referring packages. |
| **Folder Migration Preview...** | Pick a destination to see existing package conflicts, dependencies outside the source folder, and direct references into it. The preview preserves relative subfolder paths. For example, moving `/Game/Props/Chairs/SM_Chair` to `/Game/Furniture` checks for a conflict at `/Game/Furniture/Chairs/SM_Chair`. |
| **Find Cleanup Candidates...** | Redirectors, registry folders without saved assets, and non-map assets with no recorded referencers. Treat these as candidates for review. |
| **Folder Naming Audit...** | Names that violate the configured class prefixes. |

The map audit, folder inventory, and folder reference audit export CSV reports under `Saved/ContentBrowserToolkit/Audits/`.

## Inspect and compare assets

| Select | Command | What it shows |
| --- | --- | --- |
| Assets | **Find Replacement Candidates...** | Assets of the same class in the same folder, with comparison details for textures, animations, and meshes. |
| Textures | **Texture Settings Audit...** | Source dimensions and format, compression, sRGB, and mip generation. For layered or tiled sources, size and format describe the base block and first layer. |
| 2D textures | **Check Texture Sets...** | Missing or duplicate roles and mismatched source dimensions for textures with a shared stem in the same folder. Recognizes `_BaseColor` or `_Albedo`, `_Normal` or `_N`, and `_ORM`. |
| 2D textures | **Find Exact Texture Duplicates...** | Matching top-mip source pixels among same-folder textures with one block, layer, and slice. The report shows sRGB, compression, and mip settings separately. |
| Materials or instances | **Material Parent Chain...** | Each instance's path to its base material, with missing parents or cycles flagged. |
| Materials or instances | **Find Material Instances...** | Direct and indirect instances found in saved registry data. |
| Material instances | **Compare Instance Overrides...** | Effective scalar, vector, texture, and static switch values, with local overrides and values that match the parent marked. |
| Blueprints | **Blueprint Parent Audit...** | The resolved parent class for each Blueprint. |
| Blueprints | **Find Child Blueprints...** | Direct and indirect subclasses found in saved registry data. |
| Blueprints | **Blueprint Dependency Audit...** | Direct hard and soft references, external mounts, and shortest saved dependency chains to up to ten of the largest reachable package files. |
| Animation assets | **Animation Skeleton Audit...** | Skeleton paths and playback lengths where available. |
| Skeletons or animation assets | **Find Animations for Skeleton...** | Saved animations that reference the exact same skeleton asset. |
| Animation sequences | **Animation Set Audit...** | Duration, sampling rate, root motion, and missing required notify or float curve names. |
| Static meshes | **Mesh Setup Audit...** | Collision complexity and shape count, LODs, Nanite, and material slots. |
| Skeletal meshes | **Character Asset Audit...** | Skeleton, Physics Asset, LODs, material slots, and morph targets. |
| Sound waves | **Audio Import Audit...** | Duration, channel count, and imported sample rate compared with the matching folder profile. |

Select a report row to see its evidence, such as the package path, matching settings, or a reference chain. Property reports may load assets and read their current editor values. Relationship, migration, and cleanup reports depend on saved registry data. If the registry is still scanning, run the report again after discovery finishes. Dynamically built paths and runtime loading can escape saved reference checks.

## Project settings

Open **Project Settings > Content Browser Toolkit**.

| Threshold | Default |
| --- | --- |
| Large asset / tiny asset | 50 MB / 4 KB |
| Large texture maximum-size threshold | 4096 |
| High triangle count | 100,000 |
| Material slots | 8 |
| UV channels | 4 |
| Low audio compression quality | 35 |

- **Thresholds** controls filter cutoffs for asset size, texture size, triangle count, material slots, UV channels, and audio quality.
- **Audit** enables cached dependency and referencer filters.
- **Naming Audit** maps full class paths such as `/Script/Engine.StaticMesh` to case-sensitive prefixes such as `SM_`. The most specific available class rule wins. An empty prefix disables the rule for that class.
- **Animation Audit** defines required notify and float curve names for selected sequences. Enter notify names as they appear in the editor, for example `HitWindow`.
- **Audio Audit** defines channel and imported sample rate expectations by package folder, for example `/Game/Audio/Dialogue`. The closest folder profile wins. A zero value leaves that property unrestricted.

## Automation

Run the `ContentBrowserToolkit` group in Unreal's Automation window. Tests live in the separate `ContentBrowserToolkitTests` editor module and cover dependency traversal, map and folder reports, saved asset relationships, texture checks, naming, migration conflicts, cleanup candidates, and animation rules.
