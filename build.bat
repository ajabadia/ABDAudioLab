@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  ABDAudioLab - Build and Compilation Script
echo ==============================================================================

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

:: 3. Configure with CMake
echo [Info] Configuring project with CMake...
cmake -B build -G "Visual Studio 18 2026" -A x64
if %errorlevel% neq 0 (
    echo [Info] Trying fallback CMake configuration...
    cmake -B build
    if %errorlevel% neq 0 (
        echo [Error] CMake configuration failed.
        exit /b 1
    )
)

:: 4. Build Project
echo [Info] Building ABDAudioLab (Release)...
cmake --build build --config Release --parallel
if %errorlevel% neq 0 (
    echo [Error] Build failed.
    exit /b 1
)

echo ==============================================================================
echo  Build Successful!
echo  Executable output: build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe
echo ==============================================================================

if /i "%1"=="run" (
    echo [Info] Launching ABDAudioLab...
    start "" "build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe"
)

:end
endlocal
