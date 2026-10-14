# UX / Flujo — Ajuste de documentación y estado en código

Este documento arruga lo que ya había en los documentos de UX y lo recalibre con lo que el código actual dibuja hoy. No Decide nada nuevo. Solo pone el lenguaje al día y señala dónde los docs aún hablan de un plano que hoy está repartido en interfaz.

---

## 1. Lo que los documentos ya decían, dicho con menos ruido

- El flujo tiene pasos visibles:  
  0. Studio Environment  
  1. Target & Routing  
  2. Audio Interface Calibration  
  3. Run Session  
  4. Export & Report

  Eso ya no es una propuesta, es el esqueleto visible del stepper y la navegación.

- La navegación ya no vive “dispersa” en la vista principal. Hay un controlador dedicado que decide qué se muestra por paso y cómo se coordinan los paneles: `WorkflowNavigationController`. Eso ya es arquitectura, no idea.

- El motor de fondo es común a los modos. No estoy diciendo “modos idénticos”, sino que la diferencia está en la presentación y en lo que se expone, no en dos motores separados.

- El resultado/exportación ya tiene un botón 1-click y varias acciones auxiliares. Lo que antes se hablaba como “falta de identidad” ahora tiene operandos reales, pero dos caras distintas según modo.

- El guardado/recuperación ya tiene piezas: persistencia, checkpoints, carga de sesión, proyección sobre stepper/summary/pasos. Lo que falta es un idioma público claro de qué significa eso para el usuario.

---

## 2. Qué cambió respecto a la lectura previa

La lectura anterior decía “parcialmente alineado” y “dos caras de resultados”. La versión ajustada es más firme en esto:

- El stepper, la navegación y la ejecución ya están consolidados. Eso ya es verdad en interfaz, no hipótesis.
- El desfase ya no está en la existencia del flujo, sino en el **idioma del resultado** y en la **identidad visible de la salida**.
- El plan de simplificación antigua mencionaba un componente `SoundIdResultsDashboard`. Hoy eso está presente pero repartido entre `SoundIdResultsSummaryView` y `ExportReportPanel`, según modo. Eso no es un error, es un cambio de forma que los docs antiguos aún no dicen explícitamente.

---

## 3. Qué está resuelto hoy en interfaz

- Paso a paso con estado visible y bloqueos.
- Navegación condicional y resumen de sesión en la barra lateral.
- Vista de medición guiada con progreso, telemetría y comandos de sesión.
- Pista de resultado/exportación con métricas, hash, procedencia, dictamen, audición A/B y exportación 1-click.
- Exportación gobernada por `ExportReadiness` y ejecutada de forma asíncrona con control de progreso y resultados.
- Modo guiado y modo clásico/lab con idioma distinto en la barra de gobierno.

Lo que sí está, pero no está unificado:

- El significado del resultado se lee de forma distinta si estás en modo guiado o en modo clásico.
- La “salida principal” existe, pero no tiene un mismo contrato visible en ambas caras de interfaz.
- La recuperación de sesión puede cargarse y proyectarse, pero no hay un mensaje claro y único de “esto es reanudable” o “esto es solo revisión”.

---

## 4. Qué está por resolver sin reescribir nada grande

- Un idioma de resultado compartido o al menos explicado: qué pone primero, qué es detalle técnico, qué es acción.
- Una explicación visible clara de la salida principal y el resto como variantes, de forma coherente entre las dos caras de interfaz.
- Una lectura pública de la sesión cargada: continuar, revisar, incompleto, con qué contexto.
- Coherencia entre lo que dice el stepper, lo que dice el panel de ejecución y lo que dice el paso 4, porque hoy hay tres voces en vez de una.

---

## 5. Qué archivos sigo marcando como punto de partida

- Navegación y qué panel se muestra por paso:  
  `src/gui/controllers/WorkflowNavigationController.cpp`  
  `src/gui/MainContentComponentUiGovernance.cpp`

- Resultado/exportación en modo guiado:  
  `src/gui/soundid/SoundIdResultsSummaryView.h`  
  `src/gui/soundid/SoundIdResultsSummaryView.cpp`

- Resultado/exportación en modo clásico/lab:  
  `src/gui/ExportReportPanel.h`

- Control de exportación asíncrona:  
  `src/gui/controllers/ReportExportUiController.h`  
  `src/gui/controllers/ReportExportUiController.cpp`

- Carga y proyección de sesión recuperada:  
  `src/gui/controllers/LoadedSessionApplier.cpp`  
  `src/gui/MainContentComponentPersistence.cpp`  
  `src/gui/MainContentComponentReport.cpp`

- Strings de modo y gobierno visible:  
  `src/gui/MainContentComponentUiGovernance.cpp`  
  `src/gui/session/UiStrings.h` si está presente, o los strings cerca de `MODE_GUIDED` / `MODE_LAB`.

---

## 6. Cierre

Lo que sí cuadra hoy:
- flujo, stepper, navegación, motor común, ejecución guiada, y una pista de resultado/exportación real.

Lo que ya no está “pendiente de existir” pero sí “pendiente de unificarse”:
- el lenguaje del resultado,
- la identidad de la salida principal,
- y la lectura pública de lo que significa recuperar una sesión.

En vez de decir “falta esto”, la versión ajustada dice: esto ya tiene base técnica, pero aún no tiene un solo idioma visible que lo haga entender igual en las dos caras de interfaz.
