# Development scripts

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
