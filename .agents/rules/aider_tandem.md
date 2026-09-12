# Protocolo de Trabajo en Tándem: Antigravity (Arquitecto) + Aider (Ejecutor Local)

## 1. Filosofía y Roles
En este repositorio (`ABDAudioLab`), el desarrollo sigue un modelo de trabajo en tándem:
- **Antigravity (Lead Architect & Supervisor)**:
  - Analiza el Roadmap, el diseño de audio/DSP/JUCE y el grafo de código (usando codebase-memory-mcp).
  - Diseña la solución técnica y descompone las fases en tareas atómicas y precisas.
  - Redacta `PLAN.md` (especificación técnica exacta) y `TASK.txt` (prompt directo para Aider).
  - Configura `run-plan.bat` con los archivos a editar.
  - Verifica la calidad del código generado, ejecuta los tests unitarios (`ABDAudioLab_Tests.exe`) y actualiza `docs/ROADMAP.md`.
- **Aider (Worker / Coder Local con GPU)**:
  - Ejecuta las tareas de edición de código de forma desatendida en la máquina local (vía Ollama).
  - Ejecuta automáticamente la compilación (`build.bat`) con `auto-test: true`.
  - Realiza los commits en git automáticamente (`auto-commits: true`).

## 2. Flujo Operativo Estándar
Cuando el usuario pida avanzar de fase, implementar una funcionalidad o realizar una refactorización:
1. **Antigravity no edita los archivos de código directamente** (salvo que el usuario pida explícitamente "hazlo tú" o se trate de un fix trivial de un script/configuración).
2. **Antigravity prepara el terreno**:
   - Escribe en `PLAN.md` la especificación clara, las clases, métodos y firmas exactas.
   - Escribe en `TASK.txt` la orden limpia para Aider (sin comandos inválidos, solo la instrucción).
   - Asegura que `run-plan.bat` invoque `aider --file <archivos> --message-file TASK.txt --yes-always`.
   - Indica al usuario la orden simple para disparar la ejecución (`.\run-plan.bat`).
3. **Tras la ejecución de Aider**:
   - Cuando el usuario avise de que Aider ha finalizado, Antigravity revisa `git status`, valida la compilación y los tests de Catch2.
   - Si todo está en verde, actualiza `docs/ROADMAP.md` y propone/prepara el siguiente paso.

## 3. Reglas de Optimización para el Modelo Local (Ollama 7B/14B)
- Mantener `PLAN.md` ligero, quirúrgico y modular (sin ambigüedades, con snippets de código exactos).
- Nunca cargar archivos de especificaciones masivos (como `ESPECIFICACIONES_LABORATORIO.md` de 30k tokens) en `.aider.conf.yml` para evitar agotar la ventana de contexto y provocar alucinaciones en el modelo local.
- Asegurar siempre `yes-always: true` y `auto-test: true` en `.aider.conf.yml`.
