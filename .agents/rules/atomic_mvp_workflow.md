# Regla de Trabajo: Flujo Atómico y Foco Exclusivo en MVP

Esta regla es obligatoria y vinculante para Antigravity y cualquier agente en este repositorio.

## 1. El Objetivo Intocable: MVP End-to-End
Toda acción debe conducir directa e inequívocamente a completar la siguiente cadena de valor real:
```text
ReferenceSynth.vst3 ──> ABDAudioLab_PluginWorker.exe ──> Render / Medición ──> GUI ──> Exportación LNL
```
- **Quedan congelados**: CLAP, AU, Standalone, hardware MIDI externo, análisis de identificabilidad y nuevas pantallas abstractas.
- **Prohibido el scope creep**: No se añaden capas intermedias ni refactorizaciones especulativas por adelantado.

## 2. Protocolo de Ciclo de Desarrollo Atómico
Cada cambio debe ejecutarse bajo la siguiente disciplina estricta:
```text
1. Definir un único objetivo pequeño.
2. Inspeccionar antes de editar:
   - Namespaces reales y cómo están calificados en el código existente.
   - Declaraciones e includes requeridos.
   - Firmas de métodos existentes.
   - Fuentes registradas en CMakeLists.txt (verificar si es header-only o requiere .cpp).
3. Aplicar el cambio mínimo necesario.
4. Revisar estáticamente el diff antes de proponer compilar.
5. El usuario ejecuta la compilación del target mínimo (`build.bat`).
6. Ejecutar el test específico del cambio.
7. Si el test específico pasa, ejecutar la suite completa de tests para descartar regresiones.
8. Registrar el resultado y decidir el siguiente paso.
```

## 3. Criterios de Aceptación y Manejo de Errores
- **Fallo de compilación**: Corregir exclusivamente el error de compilación reportado. No tocar código circundante ni ampliar el alcance.
- **Fallo de test**: Aislar la aserción exacta. Si el test reflejaba una expectativa errónea del entorno (e.g. metadata por defecto de CMake vs string hardcodeado), corregir con precisión quirúrgica.
- **Regresión en la suite completa**: Detener inmediatamente el avance hasta que todos los tests previos vuelvan a estar en verde.
- **Nunca asumir que un test pasó**: Solo avanzar cuando se observe la salida exacta con 0 fallos.
