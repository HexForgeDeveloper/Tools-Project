@echo off
setlocal
rem Run the companion script without requiring a change to the user's execution policy.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Format-Code.ps1" %*
set "FormatExitCode=%ERRORLEVEL%"
echo.
if not "%FormatExitCode%"=="0" (
    echo Formatting failed. See the error above.
) else (
    echo Done.
)
rem Keep results visible when launched from Explorer. Set FORMAT_CODE_NO_PAUSE=1 for CI.
if not defined FORMAT_CODE_NO_PAUSE pause
exit /b %FormatExitCode%
