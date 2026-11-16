# ABDAudioLab - Directrices de Desarrollo y Agentes

Este proyecto opera bajo un modelo de trabajo en tándem:
- **Antigravity**: Lead Architect & Planner (analiza roadmap, diseña arquitectura, redacta `PLAN.md` y `TASK.txt`, valida tests y mantiene la integridad de la base de código).
- **Aider**: Local Execution Worker (ejecuta ediciones de código mediante `.\run-plan.bat`, compila con `build.bat` y realiza commits).

## Reglas y Directivas Activas
1. **Flujo de Trabajo en Tándem**: [.agents/rules/aider_tandem.md](.agents/rules/aider_tandem.md)
   - Roles de Antigravity (Lead) y Aider (Local Worker).
   - Matriz de pre-evaluación (ejecución directa vs delegación en Aider).
   - Protocolo de señalización explícita al usuario.
2. **Seguridad DSP, Concurrencia y Calidad C++**: [.agents/rules/dsp_thread_safety.md](.agents/rules/dsp_thread_safety.md)
   - Sincronización obligatoria entre hilos (atómicos con `acquire`/`release`).
   - Cero asignaciones de memoria (`zero heap allocation`) en el hilo de audio en tiempo real.
   - Prevención de TOCTOU y seguridad con punteros.
3. **Lecciones Aprendidas del Proyecto**: [GUIDE_ISSUES_TO_AVOID.md](docs/GUIDE_ISSUES_TO_AVOID.md)
   - Casos reales detectados en auditorías previas y soluciones normativas.
4. **Flujo Atómico y Foco Exclusivo en MVP**: [.agents/rules/atomic_mvp_workflow.md](.agents/rules/atomic_mvp_workflow.md)
   - Secuencia de cambio mínimo: hipótesis -> inspección -> cambio -> test específico -> test suite -> decisión.
   - Prohibición estricta de scope creep. Foco absoluto en la cadena de valor real.

## Regla Fundamental: El Usuario Compila y Lanza Tests
⛔ **Antigravity NUNCA debe ejecutar compilaciones (`cmake --build`, `build.bat`, MSBuild) ni lanzar ejecutables de tests (`ABDAudioLab_Tests.exe`) en segundo plano o terminal.**
- **Rol de Antigravity:** Analizar, diseñar, editar código y redactar especificaciones.
- **Rol del Usuario:** El usuario compila y ejecuta las suites de tests en su propia terminal.
- **Protocolo de Espera:** Tras aplicar cambios de código, Antigravity debe indicar qué compilar y qué comando de test ejecutar, y esperar pacientemente a que el usuario le facilite el resultado en el chat.

<!-- antislop:start -->
## antislop
For UI, copy, people, mobile layout, or code comments work, read these installed skill files directly (use these paths even if a same-named global skill exists):
- Core filter, always on: `antislop`: `.agents/skills/antislop/SKILL.md`
Before starting, follow the core's "Two Usage Modes" section in strict order: explicit session instruction first, then global preference, then ask. A session instruction always wins. For a resolved mode, say `antislop active: <mode> (session override).` or `antislop active: <mode> (global preference).` once before presenting findings or making edits, using the actual mode and source. Acknowledging the user's request without naming the source does not replace this notice.
Only an explicit choice of antislop during or after selects a session mode. A request to review, audit, or avoid file edits does not select a mode; read the global preference in that case. Another skill's mode does not select antislop's mode.
If the mode is unresolved, ask during/after and end the response; wait for the answer before any UI review, planning, or concept. For read-only tasks, put the active-mode notice only at the start of the final answer, never in progress messages. For editing tasks, announce before the first edit and omit it from the final answer.
To update antislop later: `npx antislop-ai --update`, or run `npx antislop-ai` and pick Overwrite them.
<!-- antislop:end -->
