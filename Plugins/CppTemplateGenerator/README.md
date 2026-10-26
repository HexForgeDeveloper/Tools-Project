# CppTemplateGenerator

UE 5.5 editor plugin providing a configurable shortcut to Unreal's standard C++ class wizard.

## Usage

1. Enable CppTemplateGenerator and restart Unreal Editor.
2. Open **Tools → Programming → New C++ Template… → Template Settings…**.
3. Configure **Template Classes**. Native Actor and ActorComponent subclasses are supported; duplicates, deprecated/replaced classes and Blueprint-generated classes are rejected. An empty slot is allowed while editing settings.
4. Choose a class in **New C++ Template…**, then choose the new class name and module in Unreal's wizard.
5. Complete the wizard and compile the generated class using the project's normal C++ workflow.

The plugin selects a parent class; it does not supply arbitrary file templates or copy the selected class's implementation. A working C++ toolchain is required by Unreal's wizard. Unreal performs the final module/export and class-name checks.

## Verification

Run the `CppTemplateGenerator` Automation group. It covers settings sanitization, unsupported classes and menu registration/cleanup/re-registration.

Before release, in an interactive editor verify: create and compile one Actor subclass and one ActorComponent subclass, edit the configured list and reopen the submenu, open settings through the menu, cancel the wizard, then restart the editor and check settings persistence. Automated menu tests do not replace these interactive checks.
