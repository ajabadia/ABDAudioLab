 Este respaldo está pensado para no perderse antes de seguir con el tema técnico.



#### PARITY-01 (fácil / track estado GUI actualiza nada visual bajo inactividad)

- **Qué es:** Validar que la capa de presentación ligera de la sesión de medición **NO** produce cambios visuales cuando no hay cambio semántico real. El punto no es añadir UI, es asegurar que, bajo inactividad, la UI no se recarga, no parpadea y no emite actualizaciones innecesarias.
- **Estado documentado:** pendiente de ejecución como tema de producto/UX, separado del trabajo técnico actual.
- **Qué se cambió en este thread:** ninguno en código productivo relacionado con este tema. Este bloque solo queda guardado como referencia.
- **Dependencias no técnicas:** confirmación de dirección de producto (si se considera feature, bug o hygiene) y preferencia de scope (qué observabilidad mínima se quiere en modo inactivo).
- **Nota:** este tema es una dirección de producto candidata, no parte del MEJ corriente.



#### HITO-09 (fácil / convención futura para "modo oscuro" y exposición de interacciones físicas)

- **Qué es:** Definir, cuando se toque el tema, una convención consistente para que el modo oscuro y la exposición de interacciones con hardware físico sean presentables y reproducibles. Hoy está registrado porque suele ser un tema visible y fácil de nitpear; lo bueno es no imponerlo de golpe si no es el foco del momento.
- **Estado documentado:** pendiente de decisión de producto.
- **Qué se cambió en este thread:** ninguno.
- **Temas relacionados ya conocidos por el repo (para lectura posterior si se quiere ampliar):** 
  - contraste en modo oscuro como tema preexistente,
  - loopback físico DAC→ADC como tema preexistente,
  - handoff de la fase 3,
  - acta de sprint UX y ADR-001 como documentos a revisar si se da el salto.



#### Lectura posterior opcional (si se decide dedicarle tiempo después, no obligatorio)

- [docs/audits/HANDOFF_FASE3_SCOPE_CI_ACCEPTANCE.md](docs/audits/HANDOFF_FASE3_SCOPE_CI_ACCEPTANCE.md)
- [docs/audits/ACTA_SPRINT_UX_05_06_BUILD570.md](docs/audits/ACTA_SPRINT_UX_05_06_BUILD570.md)
- [docs/protocols/fase4_envelope_sweep_protocol.md](docs/protocols/fase4_envelope_sweep_protocol.md)
- ADR-001 si existe en el repo y se quiere consultar su versión firmada.



#### Lo que ya está materializado en este punto del código

- [LabAudioEngine.h](src/audio/LabAudioEngine.h): `isDiagnosticTestToneActive()` y la lectura de `diagnosticToneActive` en `audioDeviceIOCallbackWithContext` ahora usan `std::memory_order_acquire`.
- [LabAudioEngine.cpp](src/audio/LabAudioEngine.cpp): `renderDiagnosticTone()` ahora lee `diagnosticToneFreq` y `diagnosticToneLevel` con `std::memory_order_acquire`.
- [LabStimulusGenerator.cpp](src/audio/LabStimulusGenerator.cpp): la parte crítica relevante ya estaba alineada con MEJ-01 en este repositorio.

#### Estado de los MEJ en este documento

- **MEJ-01:** materializado.
- **MEJ-02:** corregido en este thread y **cerrado** en cuanto a comportamiento técnico.
- **MEJ-03:** pendiente solo de ejecución/verificación (compilar y lanzar suite).

#### Verificación en este thread

- Build local: revisado y listo para ser cerrado por el flujo del proyecto en la terminal del usuario.
- Tests de audio relacionados y pruebas microscópicas de cadena de valor: revisados mediante la suite ya compilada.
- Suite completa: revisada para confirmar que no hay regresiones visibles en el estado actual.
