<p align="center">
  <img src="Docs/Images/ToolsProjectImage.jpg" alt="ToolsProject plugins inside Unreal Editor" width="1000">
</p>

<h1 align="center">ToolsProject</h1>

<p align="center">
  <strong>A focused collection of Unreal Engine 5.5 editor tools for faster, cleaner production workflows.</strong>
</p>

<p align="center">
  Nine standalone plugins for content management, editor productivity, validation, performance analysis, and graph organization.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Unreal%20Engine-5.5-303846" alt="Unreal Engine 5.5">
  <img src="https://img.shields.io/badge/Platform-Windows-0078D4" alt="Windows">
  <img src="https://img.shields.io/badge/Plugins-9-6D5BD0" alt="Nine plugins">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-2E9E5B" alt="MIT License"></a>
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> •
  <a href="#the-toolkit">The Toolkit</a> •
  <a href="Docs/Plugins.md">Full Documentation</a> •
  <a href="#testing">Testing</a>
</p>

---

## One workspace. Nine focused tools.

ToolsProject brings everyday Unreal Editor improvements into one coherent workspace. Use the complete project or take individual plugins into your own toolchain.

| Tool | Focus | Highlights |
| --- | --- | --- |
| **CppTemplateGenerator** | C++ workflow | Create native classes from a configurable set of approved parent templates. |
| **ContentBrowserToolkit** | Content audit | Search and triage assets with production-oriented Content Browser filters. |
| **DataAssetManager** | Data Assets | Browse, filter, validate, compare, and maintain Data Assets in bulk. |
| **OutlinerToolkit** | Scene organization | Add useful Outliner columns, actor batch actions, filters, and world audits. |
| **PerformanceInspector** | Performance | Capture frame metrics, inspect live graphs, add markers, and export reports. |
| **TextureChannelPacker** | Textures | Pack, repack, unpack, and preview texture channels through reusable presets. |
| **UNotepad** | Editing | Edit text, source code, JSON, CSV, and Verse without leaving Unreal Editor. |
| **ValidatorX** | Validation | Manage project validators from a searchable Data Validation dashboard. |
| **WireBundle** | Graphs | Organize Blueprint and Material wires with native reroute bundles. |

<p align="center">
  <a href="Docs/Plugins.md"><strong>Explore the complete plugin guide</strong></a>
</p>

## A closer look

<table>
  <tr>
    <td width="50%">
      <a href="Docs/Plugins.md#dataassetmanager"><img src="Plugins/DataAssetManager/Resources/DataAssetManagerImage.jpg" alt="Data Asset Manager"></a>
    </td>
    <td width="50%">
      <a href="Docs/Plugins.md#outlinertoolkit"><img src="Plugins/OutlinerToolkit/Resources/OutlinerToolkitImage.jpg" alt="Outliner Toolkit"></a>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <a href="Docs/Plugins.md#performanceinspector"><img src="Plugins/PerformanceInspector/PerformanceInspector/Resources/PIImage.jpg" alt="Performance Inspector"></a>
    </td>
    <td width="50%">
      <a href="Docs/Plugins.md#texturechannelpacker"><img src="Plugins/TextureChannelPacker/Resources/TextureChannelPackerImage.jpg" alt="Texture Channel Packer"></a>
    </td>
  </tr>
</table>

## Quick Start

### Requirements

- Unreal Engine **5.5**
- Windows
- Visual Studio toolchain compatible with Unreal Engine 5.5

### Open the workspace

```powershell
git clone https://github.com/HexForgeDeveloper/Tools-Project.git
```

1. Open `ToolsProject.uproject` in Unreal Engine 5.5.
2. Build the `ToolsProjectEditor` target in the **Development Editor** configuration.
3. Launch the project and open the required tools from the editor menus or toolbar.

Each plugin is self-contained under [`Plugins/`](Plugins/). See the [plugin guide](Docs/Plugins.md) for entry points, settings, controls, and limitations.

### Use individual plugins

The plugins are also synchronized to the separate [Tools-Plugins](https://github.com/HexForgeDeveloper/Tools-Plugins) repository. Copy the required plugin into your project's `Plugins` directory, regenerate project files, and build your editor target.

## Testing

The repository includes dedicated automation test modules and a project-wide test runner:

```powershell
powershell -ExecutionPolicy Bypass -File DevOps/Test-Project.ps1
```

The script builds `ToolsProjectEditor`, discovers the supported headless test suites, runs them, and writes reports to `Saved/CI`.

## Project Status

ToolsProject is under active development. The editor tools are usable, but APIs, UI details, and validation rules may continue to evolve.

Contributions are welcome. Keep changes focused and run the relevant automation tests for modified plugins.

## License

Released under the [MIT License](LICENSE).
