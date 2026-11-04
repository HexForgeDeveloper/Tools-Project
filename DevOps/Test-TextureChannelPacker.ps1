[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
if (-not $EngineRoot) { $EngineRoot = 'C:\Program Files\Epic Games\UE_5.5' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'ToolsProject.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($required in @($project, $build, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Not found: $required" }
}
$reportRoot = Join-Path $projectRoot ('Saved\TextureChannelPackerTests\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
if (-not $SkipBuild) {
    & $build ToolsProjectEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE 2>&1 |
        Tee-Object -FilePath (Join-Path $reportRoot 'Build.log')
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $LASTEXITCODE" }
}

# UI assertions require a real rendering device. Render offscreen to avoid stealing the desktop.
& $editor $project -unattended -nopause -nosplash -nosound -RenderOffscreen `
    '-ExecCmds=Automation RunTests TextureChannelPacker' '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$reportRoot" "-abslog=$(Join-Path $reportRoot 'Editor.log')"
if ($LASTEXITCODE -ne 0) { throw "Editor exited with code $LASTEXITCODE. See $reportRoot" }
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "Automation report missing: $reportFile" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$tests = @($report.tests | Where-Object { $_.fullTestPath.StartsWith('TextureChannelPacker.') })
if (-not $tests.Count) { throw 'No TextureChannelPacker tests ran.' }
$failed = @($tests | Where-Object { $_.state -ne 'Success' })
if ($failed.Count -or $report.failed -or $report.notRun -or $report.inProcess) {
    $failed | ForEach-Object { Write-Host "$($_.fullTestPath): $($_.state)" }
    throw "Automation did not complete successfully. See $reportRoot"
}
foreach ($group in @('Patterns', 'Paths', 'Presets', 'Preview', 'SourceFormats', 'Processing', 'Assets', 'UI')) {
    if (-not @($tests | Where-Object { $_.fullTestPath.StartsWith("TextureChannelPacker.$group.") }).Count) {
        throw "Required test group was not run: $group"
    }
}
Write-Host "TextureChannelPacker: $($tests.Count) tests passed. Report: $reportRoot"
