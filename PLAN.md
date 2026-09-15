# Plan de Trabajo: ABDAudioLab — SoundID Guided Workflow

## Estado Actual: Fase 20.7.1 Completada y Validada (14/09/2026)
- **Suite de Pruebas**: 216 / 216 casos de prueba ejecutados y superados (158.123 aserciones, 0 fallos).
- **Single Source of Truth**: `ModelEvaluation` consolidado como Fuente Única de Verdad en `SoundIdResultsSummaryView`.
- **Integridad Criptográfica RFC 8785**: Pipeline determinista de 8 pasos con serialización canónica, exclusión de hash declared y recálculo SHA-256 bit a bit.
- **5 Fixtures Canónicos**: Generados estrictamente con `ModelEvaluationBuilder` en `fixtures/evaluations/` (Approved, Dexed, Tampered, Inconclusive, Rejected).
- **Invarianza Estricta y Nuevo Estado**: `EvaluationLoadedForReview` distingue evaluaciones importadas sin alterar targetId, sessionId ni controllerGeneration.
- **Protección de Resultados Previos**: Al reanudar o reiniciar perfilado, la evaluación anterior se preserva de forma transaccional ante fallos o cancelaciones.
- **Exportación Atómica Verificada**: `exportModel` escribe a archivo temporal, verifica el hash canónico en el archivo y realiza commit atómico. Diálogo modal con revelado seguro en Explorador (`destFile.existsAsFile()`).
- **Carga Asíncrona Segura en GUI**: Menú desplegable `Cargar Evaluación... ▼` con 5 fixtures predefinidos y explorador de archivos con `SafePointer`.
- **Informe Técnico de Auditoría y Metrología**: Visor modal interactivo con desglose de métricas físicas (ESR dB, rho), dominios, dinámica, hash canónico y hash de archivo fuente, con botón 1-clic para copiar al portapapeles.
- **Stepper de Etapas**: Estados expuestos en botones (`1. Target [OK]`, `2. Medir [Listo]`, `3. Resultados [Disp.]`) con vista explicativa en Paso 3 cuando no hay evaluación activa.

---

## Estado Actual: Fase 20.8.3 Completada y Validada (15/09/2026)
- **Modo Guiado MVP**: Validado de extremo a extremo en compilación Release con `ReferenceSynth.vst3` y proceso worker aislado `ABDAudioLab_PluginWorker.exe`.
- **Aislamiento IPC por Named Pipes**: Adaptador `OutOfProcessVst3LifecycleAdapter` con protocolo binario estructurado y resiliencia demostrada ante fallos del plugin sin comprometer el host principal.
- **Nomenclatura Canónica de Exportación**: Formato `<Target>_<Model>_<UTC_timestamp>_<HashPrefix>.h` implementado en `ModelExportNaming`, con formateo UTC estricto, sanitización de nombres reservados de Windows (CON, AUX, NUL...) y resolución automática de colisiones con sufijo secuencial.
- **Validación Operacional**: Checklist de Release completado al 100% (9/9 escenarios PASS) documentado en `docs/architecture/MVP_RELEASE_VALIDATION.md`.
- **Suite de Pruebas**: Tests de nomenclatura y ciclo de vida de worker 100% pasando.

---

## Siguiente Hito: Fase 20.9 — Integración de Hosting VST3 Real (Dexed.vst3) y Hardware Analógico en vivo

### Objetivos Principales:
1. **Hosting de Plugins Externos Complejos**:
   - Validación y escaneo dinámico de `Dexed.vst3` en el worker aislado out-of-process.
   - Sincronización de parámetros MIDI SysEx y mapeo automatizado del espacio tímbrico FM.
2. **Soporte de Hardware Analógico**:
   - Rutas de excitación analógica con loopback y calibración automática de niveles/latencia.
   - Protección contra sobrecarga acústica y clipping analógico.


