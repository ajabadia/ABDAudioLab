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
set AIDER_SRC_FILE=src\tests\test_SoundIdThemeMode.cpp

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

    :: Esperar hasta OLLAMA_TIMEOUT_SEC segundos a que responda
    set /a WAIT=0
:WAIT_LOOP
    timeout /t 2 /nobreak >nul
    curl -s --max-time 2 "%OLLAMA_API_BASE%/api/version" >nul 2>&1
    if not errorlevel 1 goto OLLAMA_UP
    set /a WAIT+=2
    if !WAIT! geq %OLLAMA_TIMEOUT_SEC% (
        echo [ERROR] Ollama no arranco en %OLLAMA_TIMEOUT_SEC% segundos.
        exit /b 1
    )
    echo        Esperando... (!WAIT!s / %OLLAMA_TIMEOUT_SEC%s)
    goto WAIT_LOOP
) else (
    echo        OK - servidor Ollama respondiendo.
)

:OLLAMA_UP

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
