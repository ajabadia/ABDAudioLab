# ACTA DE CERTIFICACIÓN DE SPRINT — DESACOPLAMIENTO, CONTRATOS Y LIFETIME DE SCOPE
## ABDAudioLab & ABDSharedCode — Aislamiento Modular, RT-Safety e Higiene de Visualización

**Fecha:** 7 de Octubre de 2026  
**Rama de Trabajo:** `feature/abdscope-sharedcode-scope`  
**Commit Base:** `0c1896b8ce3439a4aeab76464b116d260451675e` (Tag congelado: `v2.1.1-build570-stepper-buttontokens-rc1`)  
**Entorno de Compilación:** MSVC 18.10.3 / C++20 Release x64  
**Documento de Arquitectura:** `docs/audits/INVENTARIO_Y_ARQUITECTURA_ABDSCOPE_SHAREDCODE.md`  
**Estado:** 🟢 **CERTIFICADO (0 REGRESIONES, 0 FUGAS, HIGIENE Y LIFETIME VALIDADOS)**

---

## 1. Definición y Gobernanza Canónica de ABDScope

Se formaliza y ratifica la gobernanza modular del subsistema de visualización y osciloscopio:

1. **Hogar Canónico Oficial:**
   - Todo el desarrollo, mantenimiento y evolución de Scope reside exclusivamente en `ABDSharedCode/Scope/`.
   - Exporta los targets CMake canónicos `ABDShared::ScopeCore` y `ABDShared::ScopeCoreHeaders`.
2. **Deprecación de Repositorio Histórico Standalone:**
   - El directorio y repositorio raíz `ABDScope/` queda declarado como **obsoleto / deprecado**.
   - Se mantiene únicamente con fines de histórico y archivo; **ningún cambio nuevo debe aplicarse en dicha carpeta**.
3. **Consumo en ABDAudioLab:**
   - `ABDAudioLab` enlaza única y directamente `ABDShared::ScopeCore` desde `../ABDSharedCode/Scope`.
   - Cero duplicación de targets CMake y cero dependencias inversas desde `ABDSharedCode` hacia `ABDAudioLab`.

---

## 2. Alcance Técnico Ejecutado (Fases 1 y 2)

### 2.1 Refactorización e Higiene de API en `ABDSharedCode/Scope`
- **Sobrecarga No-Const de `getTap()`:**
  - Añadido `[[nodiscard]] ScopeTap* getTap(size_t index) noexcept` en `ABDSharedCode/Scope/Source/Core/ScopeDataCollector.h`.
  - Permite a los hosts y ventanas modificar el estado atómico de activación de sondas sin recurrir a conversiones forzadas.
- **Búsqueda Insensible a Mayúsculas / Lenient Query:**
  - `ScopeDataCollector::findTapIndex()` ahora normaliza los identificadores wire y slugs mediante `toLowerAscii()`, soportando consultas tanto con el nombre exacto como con identificadores slug en mayúsculas o minúsculas.
- **Higiene C++ en Host GUI:**
  - En `src/gui/ScopeWebFloatingWindow.h`, se erradicó el uso de `const_cast<abd::scope::ScopeTap*>` en el método `onWindowShown()`, consumiendo la nueva sobrecarga tipada de la API.

### 2.2 Suite de Contratos, Rendimiento y Concurrencia
Se incorporó el arnés exhaustivo de pruebas en `src/tests/test_ScopeContractsAndLifetime.cpp` registrado en `CMakeLists.txt`:

1. **SPSC Multi-Block & Overrun Stress:**
   - Comportamiento de buffer circular ante múltiples tamaños de bloque de audio (64, 128, 256, 512, 1024, 2048 y 4096 muestras).
   - Verificación de continuidad de datos y contadores de desbordamiento sin cuelgues.
2. **RT Safety y Cero Asignaciones:**
   - Verificación del método `writeStereo()` sin bloqueos ni asignaciones en heap durante el procesamiento de bloques.
   - Respeto al flag atómico `isActive()`.
3. **Mapeo y Registro de Taps:**
   - Registro de sondas dinámicas (`Hardware In`, `Stimulus`, `Diagnostic`).
   - Selección por índice, slug y consulta tolerante case-insensitive.
4. **Serialización Wire Protocol (JSON):**
   - Validación de esquema canónico para WebView2: tokens `signalType` (`"audio"`, `"control"`), `sampleRate`, buffers de muestras `timeDataL` / `timeDataR`, métricas de pico y afinación estimada.
5. **Robustez de Trigger en Sub-Graves:**
   - Estabilización y detección determinista de frecuencia para tonos puros de sub-bajo (55 Hz A1, 110 Hz A2, 440 Hz A4).
   - Inmunidad a oscilaciones de sub-umbral por histeresis adaptativa.
6. **Concurrencia y Ciclo de Vida (Teardown):**
   - Prueba multi-hilo donde el hilo de audio escribe de forma ininterrumpida mientras el hilo principal activa y desactiva sondas (`deactivateAll()`).
   - Cero carreras críticas, cero memory corruption y cero deadlocks.

---

## 3. Certificación de Pruebas

### 3.1 Suite de Contratos de Scope (`*Scope*`)
```text
Filters: "*Scope*"
Randomness seeded to: 1242620643
===============================================================================
All tests passed (18493 assertions in 6 test cases)
Código de salida: 0
```

### 3.2 Suite de Higiene de Recursos y Rutas (`*hygiene*`)
```text
Filters: "*hygiene*"
Randomness seeded to: 1045324354
===============================================================================
All tests passed (43 assertions in 11 test cases)
Código de salida: 0
```

### 3.3 Suite Autónoma C++ en ABDSharedCode (`ABDScope_CppSmoke`)
```text
============================================
  ABDScope Native C++ Standalone Sanity Test
============================================
[TEST] Testing SpscRingBuffer...
[TEST] Testing ScopeDataCollector & Multi-Tap...
[TEST] Testing TriggerDetector Sub-sample Lock & Pitch...
  Note: A4, Freq: 441 Hz
[TEST] Testing ScopeFrameSerializer JSON Wire-Protocol...
============================================
  [SUCCESS] ALL C++ SMOKE TESTS PASSED!
============================================
Código de salida: 0
```

### 3.4 Suite Frontend Vitest (`WebUI/tests/`)
```text
 Test Files  9 passed (9)
      Tests  62 passed | 1 skipped (63)
   Duration  13.41s
========================================================
  [SUCCESS] ALL BUILDS AND TESTS PASSED (100%)
========================================================
Código de salida: 0
```

---

## 4. Trazabilidad Canónica de Commits

| Repositorio | Rama | Commit | Descripción |
|---|---|---|---|
| `ABDAudioLab` | `feature/abdscope-sharedcode-scope` | `1a70b36` | `feat(scope): contratos de api, higiene de tipos y tests de ciclo de vida en ABDSharedCode/Scope` |
| `ABDSharedCode` | `master` | `0e6ae9d` | `feat(scope): sincronizacion autonoma de ScopeCore, contratos sin const_cast y especificacion de gobernanza` |

---

## 5. Conclusión y Próximos Pasos

El objetivo de la **Fase 2 (Contratos, Ciclo de Vida y Sincronización Bidireccional)** queda formalmente **CERRADO Y CERTIFICADO**:
1. `ABDSharedCode/Scope` es un módulo completamente autónomo, testeable en C++ y WebUI sin dependencias externas de host.
2. `ABDAudioLab` consume limpiamente la API sin `const_cast` ni duplicidad de targets CMake.
3. El repositorio histórico `ABDScope/` queda documentado como obsoleto y archivado.
4. El tag congelado `v2.1.1-build570-stepper-buttontokens-rc1` permanece intacto en `0c1896b`.

**Próxima Etapa:** Fase 3 (Empaquetado de assets web, paridad de temas y verificación del bundle WebView2).
