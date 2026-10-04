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
