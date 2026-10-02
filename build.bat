@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  ABDAudioLab - Build and Compilation Script
echo ==============================================================================

:: Terminate running instance if open
taskkill /f /im ABDAudioLab.exe >nul 2>nul
taskkill /f /im ABDAudioLab_Tests.exe >nul 2>nul
timeout /t 1 /nobreak >nul 2>nul

:: 1. Detect Visual Studio Environment using vswhere
where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [Info] MSVC compiler not in PATH. Searching for Visual Studio installation...
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_PATH=%%i"
        )
    )

    if defined VS_PATH (
        echo [Info] Found Visual Studio at: !VS_PATH!
        set "CMAKE_GENERATOR_INSTANCE=!VS_PATH!"
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
::
:: WHAT THIS MUST NOT DO, AND WHY THE RULE IS PER PATH.
::
:: A junction here is only legitimate where nothing versioned is underneath.
:: `assets/models` and `assets/brands` are gitignored on purpose, so pointing
:: them at the sibling repo is exactly what they are for. `contracts/hardware`
:: is the opposite: the .gitignore says "contracts/ is deliberately NOT
:: ignored", and 40 contract JSONs are tracked there, because a clean clone
:: and the CI of this repo have no sibling to link to.
::
:: So the rule is not "never make a junction". The rule is "never replace a
:: path that git tracks with a junction", and it is asked per path, because
:: asking it once for the section would forbid the two legitimate links.
::
:: What the blind version did: it asked `if not exist` and nothing else. On a
:: machine where that directory was missing ?fresh clone, a stale working
:: copy, someone who deleted it to see what would happen? it created a link
:: with no message. Git still listed 40 files, the preflight compared the
:: source with itself through the link and reported them all identical, and the
:: test did the same. Green everywhere, nothing checked. Both of those are
:: fixed now and would still be green here.
::
:: An existing junction is only reported, never repaired. Deleting what
:: somebody may have on purpose, mid-build, is not this script's call. The
:: error is loud on purpose so it is visible before a broken build, and the
:: repair is one command that is printed.
set "SHARED_ASSETS=..\ABDSharedAssets"
if exist "!SHARED_ASSETS!" (
    call :avisarSiEsEnlace "contracts\hardware" "es una copia versionada que el preflight y el test de drift vigilan"
    call :crearEnlaceSiProcede "contracts/hardware" "contracts\hardware" "!SHARED_ASSETS!\contracts"
    call :crearEnlaceSiProcede "assets/models" "assets\models" "!SHARED_ASSETS!\models"
    call :crearEnlaceSiProcede "assets/brands" "assets\brands" "!SHARED_ASSETS!\brands"
)

:: 3. Configure with CMake (only when cache is missing or CMakeLists changed)
if not exist "build\CMakeCache.txt" (
    echo [Info] Configuring project with CMake...
    if defined VS_PATH (
        cmake -B build -G "Visual Studio 18 2026" -A x64 "-DCMAKE_GENERATOR_INSTANCE=!VS_PATH!" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ) else (
        cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )
    if %errorlevel% neq 0 (
        echo [Info] Trying fallback CMake configuration...
        if defined VS_PATH (
            cmake -B build "-DCMAKE_GENERATOR_INSTANCE=!VS_PATH!" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        ) else (
            cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        )
        if %errorlevel% neq 0 (
            echo [Error] CMake configuration failed.
            exit /b 1
        )
    )
)

:: 4. Target Selection and Build Configuration
set "BUILD_TARGET="
set "IS_TEST_ONLY=0"
set "RUN_PERF=0"
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
) else if /i "%1"=="perf" (
    rem Compila los tests y cronometra la suite. `perf` y no `tests` porque es
    rem OTRO trabajo: medirla cuesta minutos, y quien compila para iterar no los
    rem quiere gastar. Se deja aparte a proposito.
    set "BUILD_TARGET=--target ABDAudioLab_Tests"
    set "IS_TEST_ONLY=1"
    set "RUN_PERF=1"
    echo [Info] Performance mode: compiling ABDAudioLab_Tests and timing the suite.
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

:: ------------------------------------------------------------------ cronometro
::
:: El tiempo de la suite se mide DESPUES de compilar, y solo si el binario esta:
:: cronometrar una compilacion fallida no dice nada del rendimiento, dice del
:: error, y el error ya se ha visto justo encima.
::
:: Se busca `node` porque el cronometro es el que es. Si no esta, se avisa y se
:: sigue: medir el tiempo es una comprobacion MAS, no la unica, y un pipeline
:: que se para porque no hay node se queda sin las comprobaciones de verdad.
if "!RUN_PERF!"=="1" (
    if not exist "build\Release\ABDAudioLab_Tests.exe" (
        echo [Warn] No hay binario de tests que cronometrar.
    ) else (
        where node >nul 2>nul
        if errorlevel 1 (
            echo [Warn] No hay node en el PATH: se salta el cronometro.
            echo [Warn] Se mide con:  node tools\duraciones-suite.mjs
        ) else (
            rem
            rem EL CRONOMETRO SE COMPRUEBA A SI MISMO ANTES DE QUE SE LE CREA QUE
            rem EL RESULTADO. Un tool roto no da una medicion mala: da una
            rem medicion que no existe con forma de medicion, que es peor que no
            rem medir. Y esto cuesta 0,7 s, de modo que el precio de comprobarlo
            rem es cero y el de no comprobarlo es que el guard se quede muda.
            rem
            rem Los tests van ANTES y no despues a proposito: despues de medir ya
            rem no se puede deshacer la confianza en un resultado que salio de un
            rem tool que no funciona. Con un solo caso de rojo se pierde la
            rem medicion entera, y con el orden al reves solo se pierde el paso.
            node tools\test_duraciones_suite.mjs
            if errorlevel 1 (
                echo [Error] The timing tool's own tests are red. Nothing was measured.
                echo [Error] A tool that fails its own tests does not get to say anything
                echo [Error] about how long the suite takes. Fix it first:
                echo [Error]   node tools\test_duraciones_suite.mjs
            ) else (
                echo ==============================================================================
                echo  Timing the suite. It takes minutes; that is not a hang.
                echo ==============================================================================
                node tools\duraciones-suite.mjs
                set "PERF_EXIT=!errorlevel!"
                rem
                rem El codigo de salida del cronometro son TRES clases, no una. Antes
                rem todo lo que no era 0 caia en el mismo `else`, y ahi conviven dos
                rem cosas que no se parecen en nada:
                rem
                rem   0 = se midio, y no hay nada que decir.
                rem   1 = se midio, y hay algo que mirar. Un test lento, una regresion
                rem       contra la referencia, o una medicion que no llego a
                rem       terminar --cuelgue, XML truncado, casos de la referencia que
                rem       no se midieron--. La medicion existe y es la que dice algo.
                rem   2 = NO SE MIDIO. El binario de tests o el XML que se le apunto no
                rem       estaba donde se buscaba. Aqui no hay ningun resultado que
                rem       leer, y por lo tanto tampoco hay ningun resultado de tiempos.
                rem
                rem Decir "tu suite no se ha puesto lenta" cuando no se ha medido nada
                rem es un verde falso con forma de aviso, que es la clase de mentira
                rem que un cronometro no deberia tener. Un 2 es un problema de entorno,
                rem y se dice como tal para que nadie lo lea como lentitud.
                rem
                rem Lo que NO se puede distinguir desde aqui: un error del propio tool
                rem sale con 1, porque es el codigo que Node usa para lo que no se
                rem captura. Un 1 es, por lo tanto, "algo va mal", no "algo va lento".
                if "!PERF_EXIT!"=="0" (
                    echo [Info] Suite timings: no slow test, no regression vs reference.
                ) else if "!PERF_EXIT!"=="1" (
                    echo [Warn] Suite timings: a slow test, a regression, or a run that did not finish.
                    echo [Warn] See the table above. An unfinished run means truncated XML,
                    echo [Warn] or reference cases that were not measured.
                ) else if "!PERF_EXIT!"=="2" (
                    echo [Error] The suite was NOT timed. That is not a performance result.
                    echo [Error] Exit 2 = the tool could not start: the test binary, or the XML it
                    echo [Error] was pointed at, is not where it was looking. There is no timing
                    echo [Error] above to read. The tool prints the full path it tried, right
                    echo [Error] before this line; that is the path to check.
                ) else (
                    echo [Error] The timing tool exited with !PERF_EXIT!, a code it does not use.
                    echo [Error] That is a failure of the tool itself, not a slow suite.
                )
            )
        )
    )
)

:: ------------------------------------------------------------------ junctions
::
:: %1 = path as git knows it, relative, with forward slashes.
:: %2 = path as Windows sees it, relative, with backslashes.
:: %3 = where the link should point.
::
:: A tracked path is never replaced. The check is `git ls-files`, not a guess
:: about which paths "look like" contracts: the question is not what this
:: directory is called, it is whether git has files recorded inside it.
:crearEnlaceSiProcede
set "Rastreado=NO"
set "GitResponde=NO"
set "LISTA=%TEMP%\abdl_git_ls.txt"

rem `-c safe.directory` esta aqui por una razon concreta: en una maquina donde
rem el repo es de otro usuario, `git` responde "dubious ownership", sale con
rem codigo 129 y no lista NADA. Leido sin mirar el codigo, eso es
rem indistinguible de "no hay ficheros versionados aqui", que es exactamente
rem la respuesta que abre la puerta que esta subroutine cierra.
rem
rem Por eso va `-c safe.directory=*` y no la ruta concreta: el `%~dp0` de batch
rem sale con barras invertidas y barra final, y git no lo reconoce como el
rem mismo sitio, asi que seguira respondiendo dubious ownership. Entre
rem desactivar la comprobacion y no poder preguntar, se desactiva.
rem
rem Y el punto ese detras de `%~dp0.` no es un descuido. Medido: con la
rem barra final, `-C "%~dp0"` hace que git se coma el resto de la linea y
rem salga con 128, que es indistinguible de "no hay ficheros versionados".
rem
rem Y de ahi el segundo punto, que es el importante: si git no responde, NO se
rem enlaza. "No se" no es "si": no saber si un path esta versionado no es un
rem motivo para sustituirlo, porque el coste de equivocarse es un guard que
rem miente en verde y el derangarse es un aviso.
git -c safe.directory=* -C "%~dp0." ls-files -- "%~1" >"!LISTA!" 2>nul
if not errorlevel 1 set "GitResponde=SI"
for /f "usebackq delims=" %%f in ("!LISTA!") do set "Rastreado=SI"
del "!LISTA!" >nul 2>nul

if exist "%~2" goto :eof

if "!GitResponde!"=="NO" (
    echo [Aviso] %~2 NO se enlaza: git no ha podido decir si esta versionado.
    echo         Se forego el enlace porque "no se" no es "si". Con git disponible:
    echo           git -c safe.directory=* -C "%~dp0." ls-files -- "%~1"
    goto :eof
)

if "!Rastreado!"=="SI" (
    echo [Aviso] %~2 NO se enlaza: git rastrea ficheros dentro de el.
    echo         Es una copia versionada a proposito, no un directorio de trabajo.
    echo         Un enlace aqui haria que todo lo que vigila esa copia comparase
    echo         el origen consigo mismo y saliese verde sin comprobar nada.
    goto :eof
)

rem El padre, nunca el destino. `mklink /J` falla si el path ya existe,
rem y existe aqui porque este mismo script lo acaba de crear. Eso fue un
rem fallo de verdad: la junction no se creaba y el aviso decia que no se
rem habia podido crear, que era cierto pero no decia por que.
for %%d in ("%~2") do if not exist "%%~dpd" mkdir "%%~dpd" 2>nul
mklink /J "%~2" "%~3" >nul 2>nul
if errorlevel 1 (
    echo [Aviso] No se ha podido crear el enlace a %~2. Se sigue con una copia vacia.
)
goto :eof

:: Only reports. `fsutil` answers "not a reparse point" for a normal directory
:: and does so as a normal user, which is all this needs: the point is to say
:: it out loud, not to fix it.
:avisarSiEsEnlace
rem Solo para el path VIGILADO. En assets/models y assets/brands una
rem junction es justo lo que esta ahi para que exista, asi que avisar
rem de ella en cada build seria gritar a alguien que ha hecho bien su
rem trabajo. Lo que se avisa es el enlace en el sitio donde no puede
rem estar, y por eso el segundo parametro dice que se rompe.
if not exist "%~1" goto :eof
fsutil reparsepoint query "%~1" >nul 2>nul
if errorlevel 1 goto :eof
echo [Aviso] %~1 YA es una junction, y %~2.
echo         El build sigue, pero con un enlace no se esta comparando
echo         nada: todo lo que vigila esa ruta se compara consigo mismo.
echo         Para dejar de verlo:
echo           cmd /c rmdir "%~1"
goto :eof

:end
endlocal
