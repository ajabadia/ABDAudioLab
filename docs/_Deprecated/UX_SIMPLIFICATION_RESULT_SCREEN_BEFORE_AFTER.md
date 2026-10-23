# Antes / después: pantalla de resultados del modo guiado

Esta nota concreta qué cambia en la pantalla que se muestra al acabar un
perfil guiado, y por qué ahora se lee como un flujo en lugar de como un panel
técnico.

## 1. Qué ve el usuario hoy al llegar al paso final

Hoy el modo guiado termina en `SoundIdResultsSummaryView`. Ese componente ya
tiene una estructura clara, pero se lee como un resumen de auditoría técnica:

- título: *“Paso 4: Resultados y Exportación de Modelo”*,
- subtítulo: *“Modelo representativo evaluado contra el conjunto holdout…”*,
- tarjeta de modelo recomendado con:
  - título del modelo,
  - badge de estado técnico,
  - badge de dictamen metrológico,
  - procedencia,
  - SHA-256 truncado + badge de integridad,
  - métricas: ESR, correlación espectral rho, alineación temporal, cumplimiento, dominio validado, coste de CPU,
- tarjeta de audición y verificación A/B con tres botones de reproducción,
- botón verde principal `Export Production Package (1-Click)`,
- botones secundarios `Load Evaluation...`, `Open Certification HTML`, `New Profiling Session`.

El problema no es que falte información. El problema es que la pantalla
empieza directamente con “modelo y auditoría” y no con una lectura de flujo.

## 2. Qué falta para que se lea como flujo

En el estado actual el usuario tiene que ensamblar mentalmente:

- qué se midió,
- qué salió,
- qué puede hacer ahora.

Esa cadena está presente, pero dispersa entre:

- título y subtítulo,
- badge de estado,
- badge de dictamen,
- tarjeta de modelo,
- badge de integridad,
- métricas,
- botones.

Eso funciona si el usuario ya entiende el flujo. No funciona tanto si quiere
entender qué acaba de pasar y qué signfica eso en lenguaje normal.

## 3. Qué cambia en la pantalla

No se trata de añadir una pantalla nueva. Se trata de desplazar el primer plano
visible y dejar los detalles técnicos donde están.

El antes y después se lee así.

### Antes: encabezado directo

Encabezado actual:

- **Título:** `Paso 4: Resultados y Exportación de Modelo`
- **Subtítulo:** `Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles.`
- **Primera tarjeta:** `Modelo Acústico Recomendado y Validación Holdout`
- **Primer bloque dentro de la tarjeta:** `Modelo:...`, `Estado:...`, `Dictamen:...`

Esto salta directamente a la cara técnica del resultado.

### Después: primer plano de lectura

Lo que se añade es un primer plano breve encima o al lado del título actual,
con tres frases cortas:

1. **Qué hiciste**
   - target o plugin medido,
   - excitación o receta usada,
   - condiciones básicas,
   - alcance medido.

2. **Qué salió**
   - completado o no,
   - aceptable, con advertencias o rechazado,
   - algo importante que afecta la confianza.

3. **Qué puedes hacer después**
   - exportar el paquete principal,
   - revisar o compartir una representación,
   - abrir carpeta o informe,
   - comparar o reanalizar si corresponde.

Esto no lleva al usuario a un panel nuevo. Solo cambia el orden de lectura:
primero la historia, luego los datos.

### Ejemplo concreto con texto falso pero realista

Supongamos que el usuario acaba de medir un sintetizador de columna de aire
tipo Juno-106, en modo guiado, con una receta de excitación determinada, y que
el resultado sale aprobado con advertencias.

**Antes:**

- `Paso 4: Resultados y Exportación de Modelo`
- `Modelo representativo evaluado contra el conjunto holdout reservado...`
- `Modelo Acústico Recomendado y Validación Holdout`
  - `Modelo: Juno-106`
  - `Estado: Validación Completada`
  - `Dictamen: PASS WITH LIMITATIONS`
  - `Procedencia: Juno-106 (HardwareMedición)`
  - `SHA-256: abc1234...89xyz`
  - `Integridad: VERIFICADA`
  - `ESR de validación (Holdout): -48.3 dB`
  - `Correlación espectral rho: 0.9712`
  - `Alineación temporal: +2 muestras (alignedTarget[n] = target[n - sampleOffset])`
  - `Estímulos dentro del criterio: 94%`
  - `Dominio validado: Filtro VCF / Envolvente`
  - `Coste relativo de CPU: 1.25x`
- tarjeta de A/B,
- botón verde de exportación,
- botones secundarios.

**Después:**

El mismo sidebar, la misma tarjeta de modelo, las mismas métricas y los mismos
botones, pero con un primer plano visible antes de entrar en la tarjeta:

- **Qué hiciste:**
  - `Medición guiada del sintetizador Juno-106 con excitación de respuesta en frecuencia determinada.`
  - `Condición básica: sesión guiada, hardware conectado y calibración de loopback usada.`
  - `Alcance: caracterización acústica sobre el dominio evaluado.`

- **Qué salió:**
  - `Sesión completada.`
  - `Modelo aceptado con advertencias: válido pero con limitaciones.`
  - `Confianza afectada por: settling prolongado en algunos puntos.`

- **Qué puedes hacer después:**
  - `Exportar el paquete de producción 1-click.`
  - `Revisar o compartir informe HTML.`
  - `Comparar o reanalizar si corresponde.`

Luego viene la tarjeta técnica actual con métricas y auditoría, y los mismos
botones que antes.

## 4. Qué cambia y qué no cambia

### Cambia

- la primera lectura de la pantalla,
- la gramática inicial de resultado: de “resumen técnico” a “qué pasó y qué
  sigue”,
- la brevedad del resumen antes de entrar en la tarjeta de modelo.

### No cambia

- el título y subtítulo actuales,
- la tarjeta de modelo recomendado,
- las métricas objetivas,
- la tarjeta de audición A/B,
- los botones de acción,
- la autoridad de exportación,
- el flujo real del paso 4.

Esto es solo reordenación y añadir una capa de lectura introductoria.

## 5. Qué significa esto para el flujo

Hoy el usuario que llega al paso 4 tiene que inferir el significado del
resultado de piezas técnicas dispersas.

Con el primer plano visible, el usuario puede leer antes:

- qué acaba de medir,
- si salió completo o no,
- qué hacer a continuación.

Eso es lo que falta hoy para que no se sienta tan genérico: no es más
información, es un orden de lectura más humano.

## 6. Dónde se apoya en el código actual

La pantalla de resultado guiada vive en:

- `src/gui/soundid/SoundIdResultsSummaryView.h`
- `src/gui/soundid/SoundIdResultsSummaryView.cpp`

El presente de ese componente ya tiene los bloques necesarios. Lo que hay que
decidir después es solo dónde se pone el primer plano visible y qué texto
lleva en cada caso.

## 7. Preguntas pendientes para materializarlo

- ¿El primer plano va como bloque separado bajo el título, o como columna
  lateral junto a la tarjeta?
- ¿En qué momento se oculta si la pantalla se hace muy estrecha?
- ¿Qué texto se usa cuando no hay medición todavía y el usuario recién llega al
  paso 4?
- ¿Las tres frases se redactan desde el componente mismo o desde una capa de
  presentación externa?
