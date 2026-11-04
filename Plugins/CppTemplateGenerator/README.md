# CppTemplateGenerator

UE 5.5 editor plugin providing a configurable shortcut to Unreal's standard C++ class wizard.

## Requirements and installation

Validated configuration: Unreal Engine 5.5, Windows x64, and a compatible Visual Studio C++ toolchain with Windows SDK. Other engine versions and platforms have not been verified. This is an editor-only plugin.

Close Unreal Editor, copy the complete `CppTemplateGenerator` folder into `<YourProject>/Plugins/`, then reopen the project and enable **C++ Template Generator** under Edit > Plugins. Restart when prompted. Keep only one installed copy of the plugin across the project and engine directories.

For a source checkout, regenerate project files and build the project's Editor target in Development configuration. A packaged Win64 plugin includes compiled editor modules for the matching engine version.

## Supported behavior

The initial list contains Actor, ActorComponent, Pawn, Character, GameModeBase and HUD. Configure the list in Project Settings > Plugins > C++ Template Generator, or use Template Settings in the submenu. Native subclasses of Actor and ActorComponent are supported; final class eligibility is checked by Unreal's wizard.

Adding the first C++ class to a Blueprint-only project creates the project's source module. If Unreal asks you to close the editor and build from the IDE, close it, build `<YourProject>Editor` for Win64 Development, then reopen the project. The class becomes available after successful compilation.

## Usage

1. Enable CppTemplateGenerator and restart Unreal Editor.
2. Open **Tools → Programming → New C++ Template… → Template Settings…**.
3. Configure **Template Classes**. Native Actor and ActorComponent subclasses are supported; duplicates, deprecated/replaced classes and Blueprint-generated classes are rejected. An empty slot is allowed while editing settings.
4. Choose a class in **New C++ Template…**, then choose the new class name and module in Unreal's wizard.
5. Complete the wizard and compile the generated class using the project's normal C++ workflow.

The plugin selects a parent class; it does not supply arbitrary file templates or copy the selected class's implementation. A working C++ toolchain is required by Unreal's wizard. Unreal performs the final module/export and class-name checks.

## Verification

Run the `CppTemplateGenerator` Automation group. It covers settings sanitization, empty slots during editing, unsupported classes and menu registration/cleanup/re-registration.

Before release, in an interactive editor verify: create and compile one Actor subclass and one ActorComponent subclass, edit the configured list and reopen the submenu, open settings through the menu, cancel the wizard, then restart the editor and check settings persistence. Automated menu tests do not replace these interactive checks.

## Troubleshooting

- Missing menu: confirm that the plugin is enabled and the editor was restarted.
- Missing class entry: check that the configured class is native, supported and not deprecated or replaced.
- Compilation failure: inspect the UnrealBuildTool output and verify the C++ toolchain and selected module. Class generation and compilation are handled by Unreal.
- Diagnostic category: `LogCppTemplateGenerator` in Output Log.

## License

MIT; see the included [LICENSE](LICENSE).
