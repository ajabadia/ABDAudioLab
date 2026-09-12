@echo off
setlocal
echo ==============================================================================
echo  ABDAudioLab - Ejecutor Automatico de Tareas Aider
echo ==============================================================================
echo [Info] Ejecutando tarea con Aider en modo desatendido...
aider --file src\gui\SoundIdSuiteList.cpp --message-file TASK.txt --yes-always
echo ==============================================================================
echo  Tarea completada
echo ==============================================================================
endlocal
