# Development scripts

## Build and automation tests

`Test-Project.ps1` builds `ToolsProjectEditor` for Win64 Development and runs headless tests from the eight existing plugin automation groups with NullRHI. Test names containing `.UI` and WireBundle Material tests are excluded because they can require native editor windows; these scenarios require a separate run with rendering. A missing report, missing selected test, unsuccessful test, or build error fails validation. Reports and logs are written to a unique directory under `Saved/CI`.

```powershell
.\DevOps\Test-Project.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.5'
```

The `Sync plugins` workflow requires a dedicated Windows x64 self-hosted runner with the custom label `unreal-5.5`, PowerShell 7 (`pwsh`), Git, UE 5.5, Visual Studio C++ tools, and the Windows SDK. Register the runner through repository Settings > Actions > Runners > New self-hosted runner. Keep it online for validation jobs. Set the repository Actions variable `UE_ROOT` if the engine is outside `C:\Program Files\Epic Games\UE_5.5`.

Pushes to `5.5` trigger this workflow. For manual dispatch, select the `5.5` branch; runs selected on other branches are skipped. Do not enable untrusted pull-request execution on this runner. Use an isolated runner machine or VM rather than a workstation holding unrelated credentials. Synchronization waits for successful validation and uses exactly the triggering commit, rather than a later revision of another branch. Until a matching runner is online, jobs wait in the queue and synchronization does not run.

The UE 5.5 synchronization chain is `Tools-Project/5.5` -> `Tools-Plugins/5.5` ->
the standalone plugin repositories' `5.5` branches. Pushes to `main` do not export
plugin changes. Other engine versions need a separate explicit branch mapping.

## Formatting

Double-click `Format-Code.bat` or run it from a terminal. It launches the companion PowerShell script with a process-local execution policy; no administrator rights or permanent policy change is needed. The window stays open to show the result. Set `FORMAT_CODE_NO_PAUSE=1` to disable the pause for automation.

Run from PowerShell (any working directory):

```powershell
& C:\Git\Tools-Project\DevOps\Format-Code.bat
```

The script modifies C/C++ source and header files throughout the project using the root `.clang-format`. Generated, build, cache, and ThirdParty directories are excluded. Symbolic links and junctions are not followed. C#, JSON, and Unreal assets are not formatted.

Check without modifying files (a failed check returns an error):

```powershell
.\DevOps\Format-Code.ps1 -Check
```

The formatter is discovered through PATH or Visual Studio Installer. To select a specific LLVM version:

```powershell
.\DevOps\Format-Code.ps1 -ClangFormatPath 'C:\Program Files\LLVM\bin\clang-format.exe'
```

Use the same clang-format version across the team for consistent results. Review the Git diff after formatting.
