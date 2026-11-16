# Decisión de diseño: primer plano en pantalla de resultado guiada

Esta nota es la decisión concreta que pediste: dónde va el primer plano en el
modo guiado, por qué se elige ese sitio, y qué limitaciones tiene. No es un
cambio de código todavía, es la lectura del espacio disponible y la opción que
tiene más sentido con la arquitectura actual.

## 1. Lo que hay en la pantalla hoy

Hoy el modo guiado llega al paso final en
`SoundIdResultsSummaryView`, y su encabezado actual es este:

- `headerTitle_` → “Paso 4: Resultados y Exportación de Modelo”
- `headerSubtitle_` → “Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles.”
- después entra directamente `modelCard_` con modelo, estado, dictamen, procedencia, hash, métricas, etc.

Esto está en `src/gui/soundid/SoundIdResultsSummaryView.h` y `.cpp`.

También hay que tener en cuenta que el contenedor visible no es el panel en
sí: el paso 4 guiado se coloca desde
`src/gui/MainContentComponentUiGovernance.cpp` con esto:

- `resultsSummaryView->setVisible(true);`
- `resultsSummaryView->setBounds(bounds);`

Donde `bounds` ya ha sido troceado por la barra superior, sidebar izquierdo,
medidor derecho, avisos, etc. Por tanto, el espacio que le llega no es el
tamaño total de la ventana, es lo que queda después de esas piezas.

## 2. Las dos opciones que se pueden decidir

### Opción A: bloque bajo título

El primer plano va como bloque de tres tarjetas o tres bloques de texto
inmediatamente debajo del título actual y antes de `modelCard_`.

Estructura visual:

1. título
2. subtítulo
3. primer plano: qué hiciste / qué salió / qué puedes hacer después
4. tarjeta de modelo y métricas
5. tarjeta de A/B
6. botones de acción

### Opción B: columna lateral

El primer plano va como columna al lado de `modelCard_`, usando la misma
ventana pero separando información narrativa de información técnica.

Estructura visual:

1. título
2. subtítulo
3. columna izquierda: primer plano breve
4. columna derecha: tarjeta de modelo, métricas, hash, etc.
5. tarjeta de A/B y botones abajo

## 3. Por qué gana la opción A en este proyecto

Gana la opción A, no por estética, sino por lo que el código actual permite
sin reescribir el layout.

Razones prácticas:

- El panel ya usa un flujo vertical fuerte: `area.removeFromTop(28)`, luego
  `removeFromTop(22)`, luego espacio, luego tarjeta de 270 px, luego A/B, luego
  botones. Es un encadenamiento vertical puro.
- No hay columna de lado preparada en este componente. Añadir una columna
  significaría rehacer la distribución actual de métricas, hash y tarjetas.
- El contenedor real ya le quita espacio a los lados: sidebar, medidor y
  márgenes. Una columna lateral competiría por el ancho mismo que ya es tenso.
- El primer plano es texto breve y lineal. No necesita una columna de ancho
  personalizado. Su lectura natural es de arriba abajo.

Por eso la opción A recae menos sobre la geometría actual y más sobre el
orden de lectura.

## 4. Lo que cambia con el bloque bajo título

Con la opción A, la pantalla de resultado deja de empezar así:

- “Paso 4: Resultados y Exportación de Modelo”
- “Modelo representativo evaluado contra el conjunto holdout…”
- tarjeta de modelo recomendado…

Y empieza así:

- “Paso 4: Resultados y Exportación de Modelo”
- “Modelo representativo evaluado contra el conjunto holdout…”
- bloque de lectura rápida: qué hiciste, qué salió, qué puedes hacer después
- tarjeta de modelo recomendado…

El resto se queda donde está: métricas, hash, A/B, botones.

## 5. Contenido propuesto del primer plano

El primer plano no es un párrafo largo. Es tres bloques cortos.

### 5.1 Qué hiciste

Texto típico de ejemplo:

- “Has medido de forma guiada el sintetizador Juno-106 con la excitación de
  respuesta en frecuencia seleccionada.”
- “Condición básica: sesión guiada, hardware conectado, medición en modo señal
  controlada.”
- “Alcance de la medición: caracterización acústica del dominio evaluado.”

Esto se puede reducir a una frase si la pantalla es estrecha.

### 5.2 Qué salió

Texto típico de ejemplo:

- “La sesión está completada.”
- “El modelo sale aceptado con advertencias: válido, pero con limitaciones.”
- “El factor que más afecta la confianza es el tiempo de asentamiento en
  algunos puntos.”

O si fuera rechazado:

- “La sesión está completada.”
- “El resultado salió rechazado porque no cumple el criterio de fidelidad
  requerido.”
- “No es exportable tal cual.”

O si no hay medición todavía:

- “No hay medición todavía.”
- “Carga una evaluación o inicia una sesión para ver resultados.”
- “Puedes revisar archivos previos o empezar uno nuevo.”

### 5.3 Qué puedes hacer después

Texto típico de ejemplo:

- “Exportar el paquete de producción con un clic.”
- “Abrir o compartir el informe HTML de auditoría.”
- “Comparar o reanalizar si necesitas confirmar algo.”

Esto puede estar también implícito en los botones, pero aquí se lee antes.

## 6. Reglas de contenido que debería seguir

Para que no se vuelva genérico, el primer plano tiene que obedecer estas
reglas:

- No repetir el título.
- No entrar en métricas técnicas.
- No ser un párrafo largo.
- Depender del estado real: sin medición, completado, advertencias, rechazo,
  fallo de integridad.
- Ocultarse o contraerse si el espacio es muy estrecho.

Esto puede vivir como tres labels alineados verticalmente con tipografía sutil,
o como tres mini tarjetas con encabezado de línea corta.

## 7. Vista de ejemplo: Juno-106 aprobado con advertencias

Antes, encabezado visible:

- Paso 4: Resultados y Exportación de Modelo
- Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles.
- tarjeta de modelo: modelo, estado, dictamen, procedencia, hash, métricas
- tarjeta de A/B
- botones

Después, encabezado visible:

- Paso 4: Resultados y Exportación de Modelo
- Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles.

Primer plano:

Qué hiciste:
- Medición guiada del sintetizador Juno-106 con excitación de respuesta en frecuencia.
- Sesión guiada con hardware conectado.
- Caracterización del dominio evaluado.

Qué salió:
- Sesión completada.
- Modelo aceptado con advertencias: válido pero con limitaciones.
- Confianza afectada por: asentamiento prolongado en algunos puntos.

Qué puedes hacer después:
- Exportar el paquete de producción 1-click.
- Abrir informe HTML.
- Comparar o reanalizar si corresponde.

Luego viene lo que ya existe: tarjeta de modelo, métricas, hash, A/B, botones.

Esto no es una pantalla nueva. Es un primer plano de lectura antes de entrar
en los detalles técnicos.

## 8. Implementación mínima de la decisión

La decisión no requiere cambiar el modo guiado completo. Requiere lo mínimo
siguiente en
`src/gui/soundid/SoundIdResultsSummaryView`:

- añadir tres labels o mini bloques de lectura para:
  - qué hiciste
  - qué salió
  - qué puedes hacer después
- rellenarlos desde `updateFromSnapshot` según el estado real
- en `resized()`, colocarlos entre el subtítulo actual y `modelCard_`
- ocultarlos o contraerlos si no cabe

El resto del componente se mantiene igual.

## 9. Qué no decide esta nota

Esta nota decide la forma del primer plano, pero no el texto final por cada
estado. Para eso faltan preguntas pendientes:

- Qué dice el primer plano cuando el usuario recién llega al paso 4 sin
  medición.
- Qué dice cuando sale aprobado limpio, con advertencias, rechazado y con fallo
  de integridad.
- Si se redacta dentro del componente o desde una capa externa.
- Si hay que ocultar alguna de las tres frases en pantallas estrechas.

## 10. Conclusión

La opción con más sentido hoy es el bloque bajo título.

Es la que menos rompe el layout actual, la que encaja mejor con un panel que ya
está pensado verticalmente, y la que mejor lee como primer plano de flujo sin
reescribir el componente ni añadir una columna de lado.

Si quieres, el siguiente paso es redactar el texto concreto de ese primer
plano para los estados reales del modo guiado: sin medición, aprobado limpio,
aprobado con advertencias, rechazado y con fallo de integridad.
