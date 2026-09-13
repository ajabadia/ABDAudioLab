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
3. **Lecciones Aprendidas del Proyecto**: [GUIDE_ISSUES_TO_AVOID.md](GUIDE_ISSUES_TO_AVOID.md)
   - Casos reales detectados en auditorías previas y soluciones normativas.
