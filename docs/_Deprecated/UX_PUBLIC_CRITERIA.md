# Criterios públicos mínimos de interfaz

Este documento define lo que el usuario debe poder leer y esperar de la interfaz, sin entrar en arquitectura ni en decisiones de código.

---

## 1. Qué se ve por defecto

La aplicación debe presentar un camino principal claro:

- un flujo con pasos visibles,
- un modo de trabajo explícito,
- un panel de ejecución o resultado que responda a “qué estoy haciendo ahora”.

No debe sentirse como dos programas empaquetados en uno. Si hay dos modos, deben leerse como distinto nivel de detalle o edición, no como dos flujos paralelos sin relación.

---

## 2. Qué debe responder el resultado

Cuando el usuario llega a un resultado medido o cargado, la interfaz debe poder responder en primer plano a:

- **Qué hiciste**
  - target o plugin elegido,
  - excitación o receta usada,
  - condiciones básicas relevantes,
  - alcance de lo medido.

- **Qué salió**
  - si la medición está completa o no,
  - si el resultado es aceptable, con advertencias o rechazado,
  - con qué confianza o limitaciones importantes.

- **Qué puedes hacer después**
  - exportar el resultado principal,
  - revisar o compartir una representación,
  - abrir la carpeta o informe,
  - comparar o reanidar si corresponde.

Esto no sustituye las métricas técnicas. Lo acompaña. El detalle técnico puede estar después, no delante.

---

## 3. Qué es la salida principal

Debe haber una salida normal clara para la mayoría de casos habituales. El resto puede existir, pero debe leerse como variantes con propósito, no como una nevera de opciones equivalentes.

Lo mínimo que debe quedar claro es:

- cuál es la acción principal,
- para qué sirve,
- dónde queda lo que genera,
- y qué son las acciones secundarias.

Si el usuario pulsa una salida, debe entender qué obtuvo, no solo que “algo se exportó”.

---

## 4. Guardado y recuperación

Una sesión guardada o recuperada debe tener un significado visible. No basta con cargar algo en silencio.

La interfaz debe poder indicar, según el caso:

- si se puede reanudar,
- si es solo revisión,
- si falta contexto,
- si está completa, en pausa o incompleta.

El riesgo no es tener auto-guardado. El riesgo es que el usuario no sepa si lo que recupera es “continuar” o “solo leer”.

---

## 5. Idioma de los modos

Si la aplicación tiene un modo guiado y un modo más técnico o libre, deben explicarse en términos de qué necesita el usuario, no en términos de “qué modo estás usando”.

Lo mínimo es que quede claro:

- cuál es el camino recomendado para la mayoría de casos,
- cuál es el otro y por qué existe,
- cuándo puede tener sentido cambiarlos,
- y qué se conserva o no al hacerlo.

---

## 6. Lo que no debe ocurrir

- Que el usuario tenga que adivinar cuál es el flujo normal.
- Que el resultado principal se lea como un dump técnico sin lectura rápida.
- Que exportar sea “abrir un menú de formatos” sin saber cuál es el resultado normal.
- Que una sesión cargada se comporte como si fuera idéntica a una sesión en marcha cuando no lo es.
- Que los modos parezcan dos productos distintos sin relación.

---

## 7. Cómo saber si esto está cumplido

No se mide solo por funcionalidad. Se mide por legibilidad:

- Un usuario nuevo puede entender qué hace la aplicación en el paso actual.
- Un usuario que llega a un resultado entiende qué significa antes de meterse en métricas.
- Un usuario que exporta entiende qué obtuvo y dónde quedó.
- Un usuario que carga una sesión sabe si puede reanudarla o solo revisarla.
- Dos modos se explican como distinto nivel, no como dos universos separados.

---

## 8. Nota de uso

Esto no dice cómo se implementa. Dice qué debe poder leerse en la interfaz. Si la aplicación ya tiene la maquinaria, el trabajo siguiente es hacer que ese significado sea visible y coherente en los puntos donde el usuario toma decisiones.
