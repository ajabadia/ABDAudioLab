# MEJ-03 — nota sobre `test_ResourcePathHygiene`

## Fallo observado en este thread

- Archivo de test: `src/tests/test_ResourcePathHygiene.cpp`
- Línea exacta registrada en la salida de la suite: `test_ResourcePathHygiene.cpp(990): failed`
- Impacto:
  - 1 caso fallido
  - 1 aserción fallida

## Qué dice la salida de la suite

La propia salida registra como causa que un documento en `docs/` enlaza o referencia una ruta personal, y que eso entra en conflicto con el chequeo de higiene de rutas del test.

## Cómo encaja con el resto del thread

- No es fallo del hilo de audio ni del cambio de `memory_order` de MEJ-02.
- No es el mismo fallo que el drift de contratos (`test_ContractsSnapshotDrift`).
- Sí parece estar relacionado con el contenido real de al menos un documento en `docs/`.

## Qué significa para MEJ-03

- MEJ-03 sigue quedando en ejecución/verificación, no en modificación de código de audio.
- Este fallo está documentado aquí para que no se pierda si se decide revisar la higiene de rutas en `docs/` después.
- Si se corrige el documento que referencia la ruta personal, este punto debería revalidarse con la misma suite o con el mismo test en aislamiento.

## Estado en este thread

- Observado: sí.
- Corregido: no.
- Documentado como pendiente aparte: sí.
