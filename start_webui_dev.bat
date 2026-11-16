@echo off
echo ==============================================================================
echo  ABDAudioLab WebUI - Servidor de Desarrollo con Hot-Reload (Navegador)
echo ==============================================================================
cd /d "%~dp0WebUI"
call npm run dev
