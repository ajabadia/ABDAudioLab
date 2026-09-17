@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  ABDAudioLab - Build and Compilation Script
echo ==============================================================================

:: Terminate running instance if open
taskkill /f /im ABDAudioLab.exe >nul 2>nul

:: 1. Detect Visual Studio Environment using vswhere
where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [Info] MSVC compiler not in PATH. Searching for Visual Studio installation...
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -property installationPath`) do (
            set "VS_PATH=%%i"
        )
    )

    if defined VS_PATH (
        echo [Info] Found Visual Studio at: !VS_PATH!
        if exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
        ) else if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64
        )
    ) else (
        echo [Warning] Visual Studio installation path could not be determined automatically.
    )
)

:: 2. Handle Arguments (e.g. clean)
if /i "%1"=="clean" (
    echo [Info] Cleaning build directory...
    if exist build (
        rmdir /s /q build
    )
    echo [Info] Clean completed.
    if "%2"=="" goto end
)

:: 2.5. Link Shared Assets from ABDSharedAssets via NTFS Junctions (Zero-Copy)
set "SHARED_ASSETS=..\ABDSharedAssets"
if exist "!SHARED_ASSETS!" (
    if not exist "contracts\hardware" (
        if not exist "contracts" mkdir "contracts"
        mklink /J "contracts\hardware" "!SHARED_ASSETS!\contracts" >nul 2>nul
    )
    if not exist "assets\models" (
        if not exist "assets" mkdir "assets"
        mklink /J "assets\models" "!SHARED_ASSETS!\models" >nul 2>nul
    )
    if not exist "assets\brands" (
        if not exist "assets" mkdir "assets"
        mklink /J "assets\brands" "!SHARED_ASSETS!\brands" >nul 2>nul
    )
)

:: 3. Configure with CMake (only when cache is missing or CMakeLists changed)
if not exist "build\CMakeCache.txt" (
    echo [Info] Configuring project with CMake...
    cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if %errorlevel% neq 0 (
        echo [Info] Trying fallback CMake configuration...
        cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        if %errorlevel% neq 0 (
            echo [Error] CMake configuration failed.
            exit /b 1
        )
    )
)

:: 4. Target Selection and Build Configuration
set "BUILD_TARGET="
set "IS_TEST_ONLY=0"
if /i "%1"=="tests" (
    set "BUILD_TARGET=--target ABDAudioLab_Tests"
    set "IS_TEST_ONLY=1"
    echo [Info] Fast build mode: compiling ABDAudioLab_Tests only.
) else if /i "%1"=="test" (
    set "BUILD_TARGET=--target ABDAudioLab_Tests"
    set "IS_TEST_ONLY=1"
    echo [Info] Fast build mode: compiling ABDAudioLab_Tests only.
) else if /i "%1"=="app" (
    set "BUILD_TARGET=--target ABDAudioLab"
    echo [Info] Compiling ABDAudioLab app only.
)

:: 5. Auto-increment build number in src/BuildVersion.h (only for full app builds)
if "!IS_TEST_ONLY!"=="0" (
    powershell -NoProfile -Command "$file = 'src\BuildVersion.h'; if (Test-Path $file) { $c = Get-Content $file -Raw; if ($c -match 'kBuildNumber = (\d+);') { $b = [int]$matches[1] + 1; $c = $c -replace 'kBuildNumber = \d+;', ('kBuildNumber = ' + $b + ';'); Set-Content $file $c; Write-Host ('[Info] Incremented build number to: ' + $b) } }"
)

:: 6. Build Project with Parallel Multiprocessor Execution
echo [Info] Building ABDAudioLab Release !BUILD_TARGET!...
cmake --build build --config Release !BUILD_TARGET! --parallel
if %errorlevel% neq 0 (
    echo [Error] Build failed.
    exit /b 1
)

:: 7. Ensure Worker and ReferenceSynth are alongside ABDAudioLab.exe for portable/isolated execution (only when building app/all)
if "!IS_TEST_ONLY!"=="0" (
    if exist "build\Release\ABDAudioLab_PluginWorker.exe" (
        if not exist "build\ABDAudioLab_artefacts\Release" mkdir "build\ABDAudioLab_artefacts\Release"
        copy /y "build\Release\ABDAudioLab_PluginWorker.exe" "build\ABDAudioLab_artefacts\Release\" >nul
        echo [Info] Synced ABDAudioLab_PluginWorker.exe to artefacts directory.
    )
    if exist "build\ReferenceSynth_artefacts\Release\VST3\ReferenceSynth.vst3" (
        if not exist "build\ABDAudioLab_artefacts\Release" mkdir "build\ABDAudioLab_artefacts\Release"
        xcopy /y /e /i /q "build\ReferenceSynth_artefacts\Release\VST3\ReferenceSynth.vst3" "build\ABDAudioLab_artefacts\Release\ReferenceSynth.vst3" >nul
        echo [Info] Synced ReferenceSynth.vst3 to artefacts directory.
    )
)

echo ==============================================================================
echo  Build Successful!
if "!IS_TEST_ONLY!"=="1" (
    echo  Test executable ready: build\Release\ABDAudioLab_Tests.exe
) else (
    echo  Executable output: build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe
)
echo ==============================================================================

if /i "%1"=="run" (
    echo [Info] Launching ABDAudioLab...
    start "" "build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe"
)

:end
endlocal
