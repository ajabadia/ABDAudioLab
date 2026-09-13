@echo off
setlocal
echo ==============================================================================
echo  ABDAudioLab - Ejecutor Automatico de Tareas Aider: Tests de Tema (Modo Oscuro)
echo ==============================================================================
echo [Info] Ejecutando tarea con Aider en modo desatendido...
aider --file src\tests\test_SoundIdThemeMode.cpp --message-file TASK.txt --yes-always
echo ==============================================================================
echo  Tarea completada
echo ==============================================================================
endlocal
