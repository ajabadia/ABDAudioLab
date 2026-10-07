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

---

## 4. Estado de Archivos Modificados

| Repositorio | Archivo | Modificación |
|---|---|---|
| `ABDAudioLab` | `CMakeLists.txt` | Registro del nuevo arnés `src/tests/test_ScopeContractsAndLifetime.cpp`. |
| `ABDAudioLab` | `src/gui/ScopeWebFloatingWindow.h` | Eliminación de `const_cast` y adopción de la API canónica. |
| `ABDAudioLab` | `src/tests/test_ScopeContractsAndLifetime.cpp` | Arnés de pruebas unitarias y de integración de ciclo de vida. |
| `ABDAudioLab` | `docs/audits/INVENTARIO_Y_ARQUITECTURA_ABDSCOPE_SHAREDCODE.md` | Documento rector actualizado y certificado sin rutas absolutas. |
| `ABDSharedCode` | `Scope/Source/Core/ScopeDataCollector.h` | Sobrecarga no-const de `getTap()` y búsqueda insensible a mayúsculas. |

---

## 5. Conclusión y Próximos Pasos

El objetivo de las **Fases 1 y 2** queda formalmente **CERRADO Y CERTIFICADO**.
La arquitectura de `ABDSharedCode/Scope` garantiza un desacoplamiento limpio, seguridad en el hilo de audio en tiempo real y plena compatibilidad con el host `ABDAudioLab`.

Para la siguiente etapa:
1. Proceder a consolidar el commit atómico en la rama `feature/abdscope-sharedcode-scope`.
2. Mantener la congelación intacta del tag `v2.1.1-build570-stepper-buttontokens-rc1` hasta la promoción definitiva.
