## Plan teórico — clientes de ABDAudioLab

### Migración al repositorio consolidado

La hipótesis deseable —que no está implementada en este thread— sería:

- **Migración al repositorio consolidado:** se muda el contenido de `ABDSharedAssets` y `ABDSharedCode` al nuevo repositorio `ABDShared`, de modo que ABDAudioLab una vez adaptado, usa directamente ese repositorio consolidado sin duplicación y sin depender de copias hermanas.

Esto implicaría, en la práctica:

- una política de migración clara (qué se mantiene, qué se queda obsoleto y cómo se valida byte a byte),
- cambiar los efectos de audio para usar zero allocation y evitar fugas de memoria y problemas de concurrencia,
- que ABDAudioLab pierda el apoyo de la copia hermanas de `ABDSharedAssets` y `ABDSharedCode`.

El trabajo actual se mantiene dentro del consumo existente de `ABDSharedAssets` y `ABDSharedCode` en ABDAudioLab. Del mismo modo, se harán mejoras de UX, metrología y generación de informes sin alterar esa dependencia externa.

> Ver [Comparison](docs/COMPARISON_MEJ.txt) para el resumen comparativo de mejoras.
