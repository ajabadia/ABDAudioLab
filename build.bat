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
rem Lo que un chequeo de esta seccion deja sin PODERSE FIAR. Se inicializa
rem AQUI, antes de los `call` de mas abajo, y no junto a PERF_FATAL mas
rem abajo: estos `call` se ejecutan ANTES de ahi. Un `set ...=0` puesto mas
rem abajo BORRABA el fallo que estos acaban de marcar, y asi el guard de
rem junctions era verde siempre. No es hipotetico: nacia en la linea 119.
set "BUILD_FATAL=0"

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
rem Lo que el cronometro deja sin PODERSE FIAR. Distinto de que el cronometro
rem tenga algo que decir: eso es un aviso, esto es no tener medicion. Se acumula
rem y se devuelve al final del script porque todo lo que se imprime a partir
rem de aqui es largo, y quien lee un log se para en el final.
rem El de los junctions NO vive aqui: sus `call` estan mas ARRIBA, y por eso
rem su acumulador tambien. Ver el comentario de su propio `set`.
set "PERF_FATAL=0"
rem Y una tercera clase, que no es la misma que ninguna de las dos: la
rem autocomprobacion del cronometro en rojo. PERF_FATAL es "no he medido".
rem Aqui el instrumento esta roto: sus propios tests fallan, de modo que no se
rem ha medido Y no se creeria lo que se hubiera medido. Sale con codigo PROPIO
rem --3-- para que quien solo mire el exit sepa cual de las dos cosas ha
rem pasado, sin tener que leer el log entero.
set "SELFTEST_FATAL=0"
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
:: El tiempo de la suite se mide DESPUES de compilar. Si la compilacion ha
:: fallado, ni se cronometra ni se dice nada: el error ya se ha visto justo
:: encima y volver a compilar por cronometrarlo solo daria otro numero.
::
:: Se busca `node` porque el cronometro es el que es, y no se busca antes.
::
:: QUE PASA SI NO ESTA, QUE ANTES DE AQUI DECIA QUE SE AVISABA Y SE SEGUIA, Y
:: YA NO ES ESO. En modo `perf` no hay node es un build que se pidio medir y
:: no ha medido, y sale con codigo de fallo: es la misma clase de mentira que un
:: 2 del cronometro, solo que se ve antes de empezar. Un build normal NO LLEGA
:: aqui --todo este bloque corre solo con RUN_PERF-- asi que "sin node se salta
:: el cronometro" sigue siendo cierto para el que no ha pedido medir. Lo que no
:: es cierto es que pedirlo y no hacerlo salga con verde.
::
:: Igual con el binario de tests: si `perf` ha compilado y no hay exe, no es
:: que no hubiera nada que medir, es que la compilacion no ha entregado lo que
:: habia que medir. El fallo de compilacion ya se ha impreso justo encima.
if "!RUN_PERF!"=="1" (
    if not exist "build\Release\ABDAudioLab_Tests.exe" (
        rem Un aviso aqui es un build que se pedia medir y no ha medido. El
        rem binario se compila justo encima, asi que su ausencia no es "no habia
        rem nada que medir": es que la compilacion no ha dejado lo que habia que
        rem medir, y eso ya se ha informado arriba como fallo de compilacion.
        echo [Error] `perf` was asked to time the suite and there is no binary.
        echo [Error] It should have been built just above. Nothing was measured,
        echo [Error] and that is not a performance result. The build will FAIL.
        set "PERF_FATAL=1"
    ) else (
        where node >nul 2>nul
        if errorlevel 1 (
rem Sin node el cronometro no se puede ni intentar. El aviso decia "se salta"
rem como si saltar fuera una opcion mas: en `perf` es la unica razon por la que
rem se ha pedido este build, y un build que pide medir y no mide sale con el
rem codigo de uno que no ha mirado. Un build normal no llega aqui: este bloque
rem solo corre con RUN_PERF.
echo [Error] `perf` needs node and there is none in PATH. Nothing was measured.
echo [Error] Install Node 18+ or run `build.bat tests`. The build will FAIL.
set "PERF_FATAL=1"
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
            rem
            rem EL REPARTO DE CODIGOS TAMBIEN SE COMPRUEBA A SI MISMO, y antes
            rem que el cronometro. El build convierte el codigo del cronometro en
            rem codigo de salida del build, y ese reparto son cuatro filas escritas
            rem en un sitio: si derivan, el build deja de distinguir "tu maquina va
            rem lenta" de "no he medido nada", y las dos cosas salen por el mismo
            rem sitio. Un reparto que solo se lee no esta comprobado.
            rem
            rem Los dos tests van juntos porque son la misma promesa: que lo que dice
            rem el cronometro sea lo que lee el build.
            node tools\test_build_bat_perf.mjs
            if errorlevel 1 (
                echo [Error] The build cannot be trusted to read a timing result. Its own
                echo [Error] exit-code contract is red, so anything it says about the
                echo [Error] suite is unverifiable. Fix it first:
                echo [Error]   node tools\test_build_bat_perf.mjs
                echo [Error] The build will FAIL at the end.
                set "PERF_FATAL=1"
            ) else (
                node tools\test_duraciones_suite.mjs
                if errorlevel 1 (
                    echo [Error] The timing tool's own tests are red. Nothing was measured.
                    echo [Error] A tool that fails its own tests does not get to say anything
                    echo [Error] about how long the suite takes. Fix it first:
                    echo [Error]   node tools\test_duraciones_suite.mjs
                    echo [Error] The build will FAIL with code 3 at the end, which is NOT
                    echo [Error] the 1 of a slow suite. An unchecked timing tool is not a
                    echo [Error] slow suite: it is a guard that is no longer guarding.
                    set "SELFTEST_FATAL=1"
                ) else (
                    echo ==============================================================================
                    echo  Timing the suite. It takes minutes; that is not a hang.
                    echo ==============================================================================
                    rem La salida se captura a un temporal para poder leer el estado
                    rem de la linea de veredicto, y se reimprime tal cual justo
                    rem despues: el log del build tiene que ser el mismo de siempre,
                    rem con la tabla y con los mensajes. Un temporal y no una segunda
                    rem invocacion porque el cronometro tarda minutos, y una vuelta de
                    rem mas mediria otra vez: con otra carga, otro resultado y un
                    rem codigo de salida que se tiraria.
                    set "PERF_LOG=%TEMP%\abdl_perf.txt"
                    node tools\duraciones-suite.mjs >"!PERF_LOG!"
                    set "PERF_EXIT=!errorlevel!"
                    type "!PERF_LOG!"
                    rem El estado es la segunda palabra de la linea que empieza por el
                    rem prefijo. Lo que se captura es stdout, que es donde va el
                    rem veredicto; stderr se ha ido a la consola sin tocar, asi que los
                    rem mensajes de error se siguen viendo igual.
                    set "PERF_ESTADO=desconocido"
                    rem El estado va con %%~c y con `delims=:,{} `, y no con %%e ni con %%f.
                    rem Los delims por defecto de `for /f` incluyen la COMA, asi que %%f se cortaba
                    rem en el primer separador del JSON y %%e era el prefijo: ninguno de los dos era
                    rem el estado. Con estos delims el token 3 de la linea de veredicto es el valor
                    rem del campo estado, y %%~c le quita las comillas.

                    rem Y la comilla de cierre del `set` es lo que hace que esto ocurra: sin ella cmd
                    rem empareja las comillas cruzando lineas dentro de este bloque, el `for /f` deja
                    rem de ejecutar el comando y busca un FICHERO llamado `findstr /b /c:...`. Medido:
                    rem el estado se quedaba en desconocido en silencio, y con el estado muerto la
                    rem rama de fallo-del-tool no podia dispararse nunca.
                    for /f "tokens=3 delims=:,{} " %%c in ('findstr /b /c:"ABD-VEREDICTO " "!PERF_LOG!"') do set "PERF_ESTADO=%%~c
                    del "!PERF_LOG!" >nul 2>nul
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
                    rem
                    rem LO QUE ANTES NO SE PODIA DISTINGUIR, Y QUE AHORA SI. El codigo
                    rem de salida son tres clases, y las tres estan aqui. Lo que antes
                    rem no se podia distinguir era un fallo del PROPIO cronometro, que
                    rem salia con 1 porque es lo que usa Node para lo que no captura, y
                    rem se leia como una suite lenta: un fallo de herramienta anunciado
                    rem como lentitud, que es la clase de mentira que este cronometro no
                    rem deberia tener. El tool ahora lo distingue solo --sale con 2 y
                    rem con estado fallo-del-tool-- asi que no cae en la rama de
                    rem entorno, sino en la suya, que esta antes.
                    rem
                    rem El estado NO DECIDE el exit, solo lo explica. El exit lo decide
                    rem el codigo, que es lo que un pipeline sabe mirar. Lo que hace el
                    rem estado es que el log diga cual de las tres cosas que caben en un
                    rem 1 es la que ha pasado, en vez de repetir las tres.

                    rem
                    rem Y EL EXIT DE ESTE SCRIPT, QUE NO ES EL DEL TOOL. Son tres, y no son
                    rem los mismos tres:
                    rem
                    rem   0 = todo se ha hecho, y el 1 del cronometro, si lo hubo, era un aviso.
                    rem   1 = un chequeo no se ha podido hacer: las junctions, o el cronometro que
                    rem       no ha producido medicion. No hay nada que leer de tiempos.
                    rem   3 = la autocomprobacion del cronometro en rojo. Es el UNICO que quiere
                    rem       decir que las OTRAS dos respuestas no son de fiar, asi que por eso
                    rem       no comparte codigo con ellas.
                    rem
                    rem El 1 del cronometro NO aparece aqui, y esa es la parte que cuesta
                    rem defender ante alguien con prisa: un 1 es una medicion que existe y que
                    rem dice algo malo, y por eso avisa y sale con 0. Fallar por eso seria
                    rem convertir el cronometro en un aviso que nadie escucha.
                    if "!PERF_ESTADO!"=="fallo-del-tool" (
                        echo [Error] THE TIMING TOOL ITSELF FAILED. This is not a performance result.
                        echo [Error] State: fallo-del-tool. The tool could not finish, and it
                        echo [Error] said so. Its error and stack are in the log above.
                        echo [Error] Do NOT read this as a slow suite: a slow suite would have
                        echo [Error] been measured. The build will FAIL at the end.
                        set "PERF_FATAL=1"
                    ) else if "!PERF_ESTADO!"=="medicion-incompleta" (
                    rem La clase que el codigo 1 esconde. El 1 del cronometro son DOS cosas que
                    rem no se parecen: una medicion que EXISTE y dice que algo va mal, y una
                    rem medicion que no llego a existir. Esta es la segunda, y por eso se decide
                    rem por el ESTADO y no por el codigo: el estado es lo unico que las separa.
                    rem
                    rem Un build verde aqui no es un build que ha comprobado que la suite no se
                    rem cuelga: es un build que no ha comprobado nada y dice que si.
                        echo [Error] THE SUITE DID NOT FINISH. There is no measurement to read.
                        echo [Error] State: medicion-incompleta. The tool says what stopped it, in
                        echo [Error] the lines above: a hang, a crash, a truncated XML, or reference
                        echo [Error] cases that were never measured.
                        echo [Error] Do NOT read this as a slow suite and not as a regression: both
                        echo [Error] of those need a measurement, and there is not one.
                        echo [Error] The build will FAIL at the end.
                        set "PERF_FATAL=1"
                    ) else if "!PERF_EXIT!"=="0" (
                        echo [Info] Suite timings: no slow test, no regression vs reference.
                    ) else if "!PERF_EXIT!"=="1" (
                        echo [Warn] Suite timings: a slow test, or a regression vs reference.
                        echo [Warn] or reference cases that were not measured.
                        echo [Warn] State: !PERF_ESTADO!.
                    ) else if "!PERF_EXIT!"=="2" (
                        echo [Error] The suite was NOT timed. That is not a performance result.
                        echo [Error] Exit 2 = the tool could not start: the test binary, or the XML it
                        echo [Error] was pointed at, is not where it was looking. There is no timing
                        echo [Error] above to read. The tool prints the full path it tried, right
                        echo [Error] before this line; that is the path to check.
                        echo [Error] State: !PERF_ESTADO!.
                        echo [Error] The build will FAIL at the end: no measurement is not a slow
                        echo [Error] suite, and letting a build pass with the timing guard unable
                        echo [Error] to run is how a gate stops being one without anyone saying so.
                        set "PERF_FATAL=1"
                    ) else (
                        echo [Error] The timing tool exited with !PERF_EXIT!, a code it does not use.
                        echo [Error] That is a failure of the tool itself, not a slow suite.
                        echo [Error] The build will FAIL at the end.
                        set "PERF_FATAL=1"
                    )
                )
            )
        )
    )
)


rem --------------------------------------------------------------------- fin
rem
rem Aqui TERMINA el flujo principal, y el salto de abajo no es decorativo.
rem Sin el, batch sigue leyendo linea tras linea y CAE dentro de
rem :crearEnlaceSiProcede, que esta mas abajo en este mismo fichero. Ahi ya
rem no hay un `call` delante, de modo que %1 es el modo del build ("perf") y
rem %2 y %3 estan vacios: `mklink /J "" ""` falla y el build se quejaba de un
rem enlace con la ruta vacia en CADA ejecucion, sin que nadie lo pidiera.
rem
rem Y debajo de ese ruido hay algo mas grave. El `goto :eof` de esa
rem subrutina, sin un `call` que lo contenga, no devuelve al que llama:
rem termina el script entero. Con el se iban los dos guards de la cola de
rem :end, que es donde vive el `exit /b 1`. Medido antes de este arreglo:
rem con PERF_FATAL=1 puesto, el script salia con 0 y sin imprimir nada de la
rem cola. Los tests no lo veian porque el banco se reensambla sus propios
rem fragmentos en el orden correcto, que es justo el que faltaba aqui.
rem
rem El salto va aqui y no un `exit /b` al final del fichero, porque :end
rem tiene que seguir siendo alcanzable: es la cola que devuelve el fallo.
goto :end

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
rem motivo para sustituirlo. Lo que se hace con esa duda es lo que ha cambiado: antes
rem avisar y seguir, y ahora marcar el build como fallido. El enlace no se
rem crea igual, que sigue siendo lo correcto; lo que no se sostiene es salir
rem con verde sin haber podido hacer la comprobacion que lo protege.
git -c safe.directory=* -C "%~dp0." ls-files -- "%~1" >"!LISTA!" 2>nul
if not errorlevel 1 set "GitResponde=SI"
for /f "usebackq delims=" %%f in ("!LISTA!") do set "Rastreado=SI"
del "!LISTA!" >nul 2>nul

if exist "%~2" goto :eof

if "!GitResponde!"=="NO" (
    echo [Error] %~2 NO se enlaza: git no ha podido decir si esta versionado.
    echo         Se forego el enlace porque "no se" no es "si". Con git disponible:
    echo           git -c safe.directory=* -C "%~dp0." ls-files -- "%~1"
    rem El enlace NO se crea, que es lo correcto. Lo que no lo es es seguir.
    rem La comprobacion que protege el path versionado no se ha hecho, asi que
    rem el build no puede decir que ese path este bien. Antes avisaba y seguia.
    echo [Error] The check that protects this path did NOT run. The build will FAIL.
    set "BUILD_FATAL=1"
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
    echo [Error] Could not create the link to %~2.
    rem El mensaje de antes decia "se sigue con una copia vacia", y no era
    rem cierto: si no se ha enlazado no hay copia, ni vacia ni de otro tipo, y
    rem el build sigue como si los assets estuvieran donde deben. Con assets
    rem que no estan, todo lo que los mide compara contra nada.
    echo [Error] The assets are NOT linked. The build will FAIL.
    set "BUILD_FATAL=1"
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
rem Aqui no hay fallo de comprobacion: el junction existe y el aviso es
rem cierto. Lo que no es cierto es salir con verde. Todo lo que vigila esa
rem ruta se compara consigo mismo y sale verde sin comprobar nada, que es la
rem razon por la que esta rutina SOLO se llama para el path vigilado: en
rem assets/models y assets/brands un junction es justo lo que tiene que haber
rem ahi, y ahi no se llama nunca.
echo [Error] %~1 YA is a junction, and %~2.
echo         Everything that checks that path is comparing it with itself and
echo         passing without checking. The build will FAIL. To undo it:
echo           cmd /c rmdir "%~1"
set "BUILD_FATAL=1"
goto :eof

:end
:: El fallo se devuelve aqui y no en el bloque donde se detecta, porque todo
:: lo que se imprime a partir de ahi es largo y quien lee un log se para en el
:: final. Un build que montase los enlaces a medias y ademas fallara dejaria
:: el arbol peor que uno que no llego a empezar.
if "!SELFTEST_FATAL!"=="1" (
    rem El primero de los tres, y no por orden de gravedad sino por otra cosa:
    rem es el UNICO que significa que las otras respuestas no son de fiar. Si el
    rem tool no pasa sus propios tests, cualquier medicion que hubiera dado queda
    rem sin comprobar, y un 1 aqui no diria "no he medido" sino "no se que ha
    rem pasado". Por eso lleva codigo propio y no comparte el 1.
    echo [Error] Build failed: the timing tool failed its own self-check.
    echo [Error] Its own tests are red, so the instrument is broken: it did not
    echo [Error] measure the suite, and it would not have been believable if it had.
    echo [Error] Exit code 3. The 1 of this build is a check that could not run or a
    echo [Error] measurement that was not produced; the 3 is the tool not working.
    echo [Error] Nothing above this line is a performance result.
    endlocal & exit /b 3
)
if "!BUILD_FATAL!"=="1" (
    rem El motivo se dice aqui y no en el punto de fallo, por el mismo motivo
    rem que el de PERF_FATAL: las junctions se montan ANTES de aqui, asi que
    rem quien se entere de que algo fallo va a mirar el final del log.
    echo [Error] Build failed: a check in this build did NOT run.
    echo [Error] The links above could not be verified or created. Which check
    echo [Error] did not run is printed above, where it failed.
    echo [Error] Nothing above this line is a performance result.
    endlocal & exit /b 1
)
if "!PERF_FATAL!"=="1" (
    echo [Error] Build failed: the timing guard could not produce a measurement.
    echo [Error] Nothing above this line is a performance result.
    endlocal & exit /b 1
)
endlocal
