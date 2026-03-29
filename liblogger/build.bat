@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%out\build"
set "GENERATOR=Visual Studio 17 2022"

pushd "%SCRIPT_DIR%" >nul || exit /b 1

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64
if errorlevel 1 (
	set "BUILD_EXIT_CODE=10"
	goto :done
)

cmake --build "%BUILD_DIR%" --config Release --target liblogger_windows
if errorlevel 1 (
	set "BUILD_EXIT_CODE=20"
	goto :done
)

set "BUILD_EXIT_CODE=0"

:done
popd >nul
exit /b %BUILD_EXIT_CODE%