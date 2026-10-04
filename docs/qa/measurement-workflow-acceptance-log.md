# Registro de Aceptación Operativa: Flujo de Medición y Calibración (R5)

**Fecha de Inicio:** 4 de Octubre de 2026  
**Rama:** `main`  
**Objetivo:** Validar el recorrido real del operador desde que abre ABDAudioLab hasta que comienza una sesión de profiling con una calibración válida, reutilizada explícitamente o evitada de forma consciente, interactuando con una interfaz de audio real.

---

## 1. Principios de Validación Operativa de R5

1. **Cero Código Nuevo Inicial**: No añadir nuevas características, paneles ni refactorizaciones durante esta fase. El objetivo es observar la experiencia real y detectar fricciones.
2. **Uso Exclusivo de Interfaz Real**: Las pruebas se realizan sobre hardware de audio físico activo (ASIO / CoreAudio / WASAPI exclusivo), sin abrir MIDI, sin sintetizadores externos y sin módulos auxiliares.
3. **Hermeticidad de Datos de Calibración**: Los archivos `.json` de calibración generados residen exclusivamente en `%APPDATA%/ABDAudioLab/calibration_profiles/` y **nunca** se versionan en Git.
4. **Criterios de Salida de R5**:
   - **PASS**: El flujo es comprensible, intuitivo y robusto; se cierra la línea de calibración.
   - **Fricción de UX/Copy**: Se redacta un microparche de texto/etiquetas sin tocar arquitectura.
   - **Fallo Técnico / Bug**: Se aísla un caso de prueba reproducible con alcance mínimo.

---

## 2. Protocolo de Escenarios Operativos

### Escenario 1: Primera Calibración (Loopback Físico)
- **Procedimiento:**
  1. Abrir `ABDAudioLab.exe`.
  2. Verificar que inicia de forma coherente en **Tarea 1 — Target & Routing**.
  3. Navegar a **Tarea 2 — Calibración de Interfaz de Audio**.
  4. Leer únicamente las instrucciones en pantalla, sin consultar documentación externa.
  5. Conectar físicamente con un cable TRS/jack: `Output 1 ➔ Input 1`.
  6. Pulsar `[Iniciar Calibración Loopback]`.
  7. Evaluar si la información de resultado es clara:
     - Qué se ha medido (latencia de ida y vuelta DAC/ADC, ganancia de bucle).
     - Qué significa éxito (`CALIBRADO`).
     - Magnitud de latencia reportada (ms y muestras) y compensación `Auto-Trim`.
  8. Pulsar `[Guardar Calibración]`.
  9. Desplegar `[Calibraciones Guardadas]` y verificar que el nuevo perfil aparece registrado localmente con fecha, dispositivo y parámetros.
- **Resultado Esperado:** El usuario comprende inequívocamente que calibró su tarjeta y cable, no su sintetizador.

---

### Escenario 2: Reutilización Correcta
- **Procedimiento:**
  1. Cerrar completamente `ABDAudioLab.exe`.
  2. Volver a abrir la aplicación manteniendo la misma interfaz, driver, sample rate, buffer size y conexionado.
  3. Navegar a la Tarea 2.
  4. Verificar que el sistema detecta la coincidencia observable y muestra el botón `[Reutilizar calibración guardada]`.
  5. Verificar que se exhibe la advertencia analógica canónica:
     > *"La interfaz y la configuración actual coinciden con los datos guardados. No se pueden detectar cambios físicos en cables, ganancia analógica o una segunda unidad idéntica."*
  6. Pulsar `[Reutilizar calibración guardada]`.
  7. Comprobar que los valores de latencia y trim se cargan en pantalla y que el estado pasa a éxito sin requerir recalibración obligatoria.
- **Resultado Esperado:** Ninguna calibración se aplica de forma silenciosa o automática; el usuario controla conscientemente la reutilización.

---

### Escenario 3: Incompatibilidad por Cambio de Configuración
- **Procedimiento:**
  1. Con un perfil guardado previo, modificar en la configuración de audio el sample rate (p. ej. 48 kHz ➔ 96 kHz) o el buffer size (p. ej. 256 ➔ 512 muestras).
  2. Navegar a la Tarea 2.
  3. Comprobar que el botón `[Reutilizar calibración guardada]` queda inactivo / no visible.
  4. Verificar que el panel detalla explícitamente las discrepancias encontradas (p. ej. `sampleRate: 48000 -> 96000`).
  5. Comprobar que `inputAutoTrim` queda neutralizado a 1.0f (0.0 dB) y no se conserva ganancia residual.
  6. Verificar que las opciones disponibles son recalibrar ahora o continuar sin calibrar.
- **Resultado Esperado:** Las discrepancias de configuración observable impiden terminantemente reutilizaciones erróneas.

---

### Escenario 4: Bypass Consciente
- **Procedimiento:**
  1. Entrar en la Tarea 2 sin conectar cable loopback.
  2. Sin ejecutar un barrido válido, pulsar `[Continuar sin calibrar (Bypass)]`.
  3. Comprobar que la aplicación avanza limpiamente sin bloquear al operador.
  4. Comprobar que la ganancia activa queda estrictamente neutralizada a 1.0f (0.0 dB) y que la latencia asumida es 0 ms.
  5. Verificar que el estado del sistema no exhibe ninguna calibración fantasma como activa.
- **Resultado Esperado:** El operador puede omitir el paso deliberadamente manteniendo absoluta integridad de nivel.

---

### Escenario 5: Diagnóstico ante Fallo de Retorno o Clipping
- **Procedimiento:**
  1. Provocar deliberadamente una condición anómala:
     - Caso A: Cable desconectado o entrada silenciada.
     - Caso B: Nivel de ganancia de entrada excesivo que cause saturación (clipping).
  2. Pulsar `[Iniciar Calibración Loopback]`.
  3. Verificar el mensaje diagnóstica:
     - Ante retorno nulo: badge `✕ REVISAR RETORNO` e instrucciones claras de verificar cableado.
     - Ante clipping: badge `✕ SEÑAL SATURADA (CLIPPING)` e instrucción de atenuar la ganancia analógica de entrada/salida.
  4. Verificar que `isCalibrated` es estrictamente falso y que no se permite guardar ni reutilizar la medición fallida.
- **Resultado Esperado:** Mensajes en lenguaje comprensible y no técnico, sin ambigüedades y con bloqueo de perfiles inválidos.

---

## 3. Bitácora de Pruebas Operativas (Log de Ejecución)

| # | Fecha | Interfaz / Driver | SR / Buffer | Escenario | Resultado | Observaciones / Fricciones | Acción Requerida |
| :-: | :---: | :---: | :---: | :--- | :---: | :--- | :---: |
| **E1** | 2026-10-04 | PreSonus AudioBox USB | 44.1 kHz / 512 | Primera Calibración (Loopback) | **UX PASS / DSP PENDIENTE** | Interfaz visual, numeración (0..4), layout responsivo, textos en inglés y visibilidad de botones (`Retry Calibration`) validados con éxito (R5-UX1). Retorno físico detectado a -12.5 dBFS y 25.1 ms de latencia; validación espectral pendiente por requerir adaptación del barrido al límite de Nyquist en 44.1 kHz. | Diseñar e implementar microhito técnico R5-AUDIO1 (sweep y evaluación espectral adaptados a fs). |
| **E2** | *Pendiente* | | | Reutilización Correcta | *Pendiente* | Requiere calibración física previa completada. | Ejecutar tras R5-AUDIO1. |
| **E3** | *Pendiente* | | | Cambio de Configuración | *Pendiente* | Requiere calibración física previa completada. | Ejecutar tras R5-AUDIO1. |
| **E4** | *Pendiente* | | | Bypass Consciente | *Pendiente* | | |
| **E5** | *Pendiente* | | | Fallo de Retorno / Clip | *Pendiente* | | |

