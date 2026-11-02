[CmdletBinding()]
param(
    [string]$ClangFormatPath,
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$stylePath = Join-Path $projectRoot '.clang-format'
if (-not (Test-Path -LiteralPath $stylePath -PathType Leaf)) {
    throw "Configuration not found: $stylePath"
}

if (-not $ClangFormatPath) {
    $command = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($command) {
        $ClangFormatPath = $command.Source
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $candidates = @(& $vswhere -latest -products '*' -find 'VC\Tools\Llvm\**\bin\clang-format.exe')
            $ClangFormatPath = $candidates | Where-Object { $_ -notmatch '[\\/]ARM64[\\/]' } | Select-Object -First 1
        }
    }
}
if (-not $ClangFormatPath -or -not (Test-Path -LiteralPath $ClangFormatPath -PathType Leaf)) {
    throw 'clang-format not found. Install LLVM or pass -ClangFormatPath with the executable path.'
}

# Prune generated and external directories before descending into them.
$excludedDirectories = @('.git', '.vs', '.idea', 'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', 'ThirdParty', 'node_modules')
$extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.ipp')
function Get-SourceFiles([string]$Directory) {
    foreach ($entry in Get-ChildItem -LiteralPath $Directory -Force) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if ($entry.PSIsContainer) {
            if ($entry.Name -notin $excludedDirectories) { Get-SourceFiles $entry.FullName }
        } elseif ($entry.Extension -in $extensions -and $entry.Name -notmatch '\.generated\.h$|\.gen\.cpp$') {
            $entry.FullName
        }
    }
}

$files = @(Get-SourceFiles $projectRoot | Sort-Object)
Write-Host "Formatter: $ClangFormatPath"
& $ClangFormatPath --version
if ($LASTEXITCODE -ne 0) { throw 'Could not execute clang-format.' }
Write-Host "Source files: $($files.Count)"
$failures = 0
foreach ($file in $files) {
    $formatArguments = @("--style=file:$stylePath", '--fallback-style=none')
    if ($Check) { $formatArguments += @('--dry-run', '--Werror') }
    else { $formatArguments += '-i' }
    & $ClangFormatPath @formatArguments -- $file
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "clang-format failed: $file"
        $failures++
    }
}
if ($failures -gt 0) { throw "clang-format reported failures in $failures file(s)." }
if ($Check) { Write-Host 'Formatting check passed.' }
else { Write-Host 'Formatting completed.' }
