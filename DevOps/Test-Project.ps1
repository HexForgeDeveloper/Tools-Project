[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ROOT
)

$ErrorActionPreference = 'Stop'
if (-not $EngineRoot) { $EngineRoot = 'C:\Program Files\Epic Games\UE_5.5' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'ToolsProject.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($required in @($build, $editor, $project)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Not found: $required" }
}

# A unique report directory prevents old results from satisfying a new run.
$reportRoot = Join-Path $projectRoot ('Saved\CI\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
& $build ToolsProjectEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE 2>&1 |
    Tee-Object -FilePath (Join-Path $reportRoot 'Build.log')
if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $LASTEXITCODE" }

$groups = @('ContentBrowserToolkit', 'CppTemplateGenerator', 'DataAssetManager', 'OutlinerToolkit', 'PerformanceInspector', 'TextureChannelPacker', 'UNotepad', 'ValidatorX', 'WireBundle')
$namePattern = '(?:IMPLEMENT_\w*AUTOMATION_TEST|BEGIN_DEFINE_SPEC)\s*\(\s*\w+\s*,\s*"((?:' + ($groups -join '|') + ')\.[A-Za-z0-9_.]+)"'
$names = @(Get-ChildItem (Join-Path $projectRoot 'Plugins') -Recurse -File -Filter '*.cpp' |
    Where-Object { $_.FullName -notmatch '[\\/](Intermediate|Binaries)[\\/]' } |
    ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName)
        foreach ($match in [regex]::Matches($text, $namePattern)) { $match.Groups[1].Value }
    } | Where-Object { $_ -notmatch '\.(h|cpp)$' -and $_ -notmatch '\.UI' -and $_ -notmatch '^WireBundle\..*Material' } | Sort-Object -Unique)
foreach ($group in $groups) {
    if (-not @($names | Where-Object { $_.StartsWith("$group.") }).Count) {
        throw "No headless tests discovered for $group"
    }
}
$names | Set-Content -LiteralPath (Join-Path $reportRoot 'SelectedTests.txt')
$filter = $names -join '+'
& $editor $project '-unattended' '-nopause' '-nosplash' '-nosound' '-NullRHI' `
    "-ExecCmds=Automation RunTests $filter" '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$reportRoot" "-abslog=$(Join-Path $reportRoot 'Automation.log')"
if ($LASTEXITCODE -ne 0) { throw "Automation process failed: $LASTEXITCODE" }

$reportPath = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportPath)) { throw 'Automation did not produce index.json.' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$results = @($report.tests)
if (-not $results.Count) { throw 'Automation report contains no tests.' }
foreach ($name in $names) {
    # Complex tests and specs report child cases under the declared prefix.
    $result = @($results | Where-Object { $_.fullTestPath -eq $name -or $_.fullTestPath.StartsWith("$name.") })
    if (-not $result.Count -or @($result | Where-Object { $_.state -ne 'Success' }).Count) {
        throw "Test failed, missing, or incomplete: $name (see $reportPath)"
    }
}
if (@($results | Where-Object { $_.state -ne 'Success' }).Count) {
    throw "Automation report contains unsuccessful tests: $reportPath"
}
Write-Host "Passed $($results.Count) tests. Reports: $reportRoot"
