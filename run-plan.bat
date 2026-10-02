@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  ABDAudioLab - Ejecutor Automatico de Tareas Aider
echo ==============================================================================

:: -----------------------------------------------------------------------
:: CONFIGURACION
:: -----------------------------------------------------------------------
set OLLAMA_MODEL=qwen2.5-coder:7b-instruct-q5_K_M
set OLLAMA_API_BASE=http://127.0.0.1:11434
set OLLAMA_TIMEOUT_SEC=60
set AIDER_TASK_FILE=TASK.txt
set AIDER_SRC_FILE=src\tests\test_ExportIO.cpp

:: -----------------------------------------------------------------------
:: PASO 1: Comprobar que ollama.exe esta instalado y en el PATH
:: -----------------------------------------------------------------------
echo [1/4] Comprobando instalacion de Ollama...
where ollama >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ollama.exe no encontrado en el PATH.
    echo         Descarga Ollama desde https://ollama.com/download y aniadelo al PATH.
    exit /b 1
)
echo        OK - ollama encontrado.

:: -----------------------------------------------------------------------
:: PASO 2: Comprobar si el servidor Ollama esta respondiendo
:: -----------------------------------------------------------------------
echo [2/4] Comprobando servidor Ollama en %OLLAMA_API_BASE%...

:: Intentar ping al endpoint de version
curl -s --max-time 3 "%OLLAMA_API_BASE%/api/version" >nul 2>&1
if errorlevel 1 (
    echo        Servidor no detectado. Arrancando ollama serve en background...
    start /b "" ollama serve
    call :esperarServidor
    rem `exit /b 1` dentro de una subrutina sale del CALL, no del script. Sin
    rem esta linea, el script imprime su error de plazo y sigue a descargar el
    rem modelo contra un servidor muerto, sin rojo en ninguna parte.
    if errorlevel 1 exit /b 1
) else (
    echo        OK - servidor Ollama respondiendo.
)
goto :finDelPaso2

:: La espera es una SUBRUTINA y no un bucle dentro del `if`, y no por gusto.
::
:: Una etiqueta dentro de un bloque se alcanza con el bloque abierto: el `goto`
:: la busca por el fichero entero y el parser arrastra el parentesis que
:: falta. Medido con una reproduccion de esta misma estructura: con la
:: etiqueta FUERA el bucle se comporta, y con la etiqueta DENTRO no sale de
:: el en 30 s. El sintoma exacto en este script no se ha medido --no se ha
:: ejecutado de verdad porque necesita Ollama--, pero el hecho estructural es
:: el mismo y no depende de quien lo mida.
::
:: Y al salir de la subroutina con `goto :eof`, el `else` vuelve a caer en
:: `:finDelPaso2`, que es el punto donde se juntan las dos ramas. Antes el
:: punto de encuentro era una etiqueta mas, `:OLLAMA_UP`, que solo hacia falta
:: porque el bucle estaba dentro de la rama: al salir el bucle de ahi, ella
:: tambien se va.
:esperarServidor
:: Esperar hasta OLLAMA_TIMEOUT_SEC segundos a que responda
set /a WAIT=0
:WAIT_LOOP
ping -n 3 127.0.0.1 >nul
curl -s --max-time 2 "%OLLAMA_API_BASE%/api/version" >nul 2>&1
if not errorlevel 1 goto :eof
set /a WAIT+=2
if !WAIT! geq %OLLAMA_TIMEOUT_SEC% (
    echo [ERROR] Ollama no arranco en %OLLAMA_TIMEOUT_SEC% segundos.
    exit /b 1
)
echo        Esperando... (!WAIT!s / %OLLAMA_TIMEOUT_SEC%s)
goto WAIT_LOOP

:: El salto de antes lo salta. Sin el, el flujo principal caeria en el cuerpo
:: de la subrutina con `%1` vacio --que es justo el fallo de 6.9 en
:: `build.bat`, con `mklink /J "" ""` en vez de un `curl`.
:finDelPaso2

:: -----------------------------------------------------------------------
:: PASO 3: Comprobar si el modelo esta disponible; descargarlo si no lo esta
:: -----------------------------------------------------------------------
echo [3/4] Comprobando modelo %OLLAMA_MODEL%...

:: Listar modelos locales y buscar el modelo requerido
ollama list 2>nul | findstr /i "%OLLAMA_MODEL%" >nul 2>&1
if errorlevel 1 (
    echo        Modelo no encontrado localmente. Descargando...
    echo        (Esto puede tardar varios minutos segun la conexion)
    ollama pull "%OLLAMA_MODEL%"
    if errorlevel 1 (
        echo [ERROR] No se pudo descargar el modelo %OLLAMA_MODEL%.
        exit /b 1
    )
    echo        Modelo descargado correctamente.
) else (
    echo        OK - modelo %OLLAMA_MODEL% disponible.
)

:: -----------------------------------------------------------------------
:: PASO 4: Verificar que el modelo responde (warm-up rapido)
:: -----------------------------------------------------------------------
echo [4/4] Verificando respuesta del modelo...
ollama run "%OLLAMA_MODEL%" "echo READY" >nul 2>&1
if errorlevel 1 (
    echo [WARNING] El modelo no respondio al warm-up. Aider lo intentara de todas formas.
) else (
    echo        OK - modelo listo.
)

:: -----------------------------------------------------------------------
:: EJECUTAR AIDER
:: -----------------------------------------------------------------------
echo.
echo ==============================================================================
echo  Lanzando Aider con modelo %OLLAMA_MODEL%
echo ==============================================================================
echo.

set OLLAMA_API_BASE=%OLLAMA_API_BASE%
aider ^
    --model "ollama_chat/%OLLAMA_MODEL%" ^
    --file "%AIDER_SRC_FILE%" ^
    --message-file "%AIDER_TASK_FILE%" ^
    --yes-always

set AIDER_EXIT=%errorlevel%

echo.
echo ==============================================================================
if %AIDER_EXIT% equ 0 (
    echo  Tarea completada correctamente.
) else (
    echo  Aider finalizo con codigo de error: %AIDER_EXIT%
)
echo ==============================================================================

endlocal
exit /b %AIDER_EXIT%
