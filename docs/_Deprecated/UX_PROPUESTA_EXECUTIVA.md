# Propuesta ejecutiva de interfaz

Esta propuesta no añade funcionalidad nueva. Dice qué dejamos intacto, qué aclaramos y qué unificamos primero para que la aplicación se lea mejor desde la interfaz.

---

## 1. Lo que no tocamos

- El flujo por pasos y el stepper visible.
- La navegación gobernada por `WorkflowNavigationController`.
- El motor común de ejecución y exportación.
- La capacidad técnica ya existente: métricas, hash, procedencia, holdout, dictamen, exportación 1-click, audición A/B, informe HTML, carpeta de exportación.

Lo que hago es separar esto en dos frases:
- la aplicación ya tiene la maquinaria,
- pero no está claro cómo el usuario la lee en el momento de decisión.

---

## 2. Lo que aclaro primero

### 2.1 Un único idioma de resultado

Hoy hay dos caras de resultado según modo. La propuesta es:

- que el resultado tenga una lectura primero compartida,
- que esa lectura responda a qué hiciste, qué salió y qué puedes hacer después,
- y que el resto, métricas técnicas, hash, procedencia, auditoría, quede después como detalle.

Esto no pide una nueva pantalla. Pide un primer plano de lectura que esté presente en ambas caras de resultado.

---

### 2.2 Una salida principal identificada

Hoy hay varias acciones visibles y varias salidas posibles. La propuesta es:

- que exista una acción principal clara para la mayoría de casos habituales,
- que su propósito esté explicado en una frase,
- y que el resto se explique como variantes: revisar, compartir, comparar, abrir carpeta, ver HTML, acciones avanzadas.

Esto no pide eliminar formatos. Pide jerarquía visible.

---

### 2.3 Una lectura pública de la sesión recuperada

Hoy las sesiones pueden cargarse y proyectarse. La propuesta es:

- que al cargar una sesión quede claro si se puede continuar,
- si es solo revisión,
- si falta contexto,
- o si está incompleto.

Esto no pide un nuevo modo de guardado. Pide que lo que ya se carga tenga significado visible.

---

### 2.4 Modos explicados como nivel, no como dos programas

La propuesta es:

- que el modo guiado y el modo clásico/lab se expliquen como distinto nivel de edición o detalle,
- no como dos universos aparte,
- y que títulos, mensajes y acciones reflejen esa lectura.

Esto no pide eliminar un modo. Pide que el idioma público de los modos sea coherente.

---

## 3. Lo que propongo como primer paso

### Paso A: lectura mínima del resultado

Añadir o consolidar una lectura breve en el resultado, presente en ambas caras:

- qué hiciste,
- qué salió,
- qué puedes hacer después.

Esto puede vivir encima de las métricas, no en competencia con ellas.

---

### Paso B: identidad de la salida principal

Que quede claro:

- cuál es la acción principal,
- para qué sirve,
- dónde queda lo que genera,
- y qué son las acciones secundarias.

Si hay dos paneles de resultado, ambos deben poder responder eso con el mismo nivel de claridad.

---

### Paso C: significado de la sesión cargada

Que al cargar una sesión el usuario vea:

- si puede continuar,
- si está revisando,
- si falta contexto,
- o si está incompleto.

No tiene que ser un diálogo grande. Tiene que ser visible.

---

## 4. Cómo encaja con lo que ya está en código

La propuesta está pensada para montarse sobre lo que ya existe:

- resultado y exportación guiada:
  - `src/gui/soundid/SoundIdResultsSummaryView.h`
  - `src/gui/soundid/SoundIdResultsSummaryView.cpp`

- resultado y exportación en modo clásico/lab:
  - `src/gui/ExportReportPanel.h`

- control de exportación:
  - `src/gui/controllers/ReportExportUiController.h`
  - `src/gui/controllers/ReportExportUiController.cpp`

- carga y proyección de sesión:
  - `src/gui/controllers/LoadedSessionApplier.cpp`
  - `src/gui/MainContentComponentPersistence.cpp`
  - `src/gui/MainContentComponentReport.cpp`

- navegación y gobierno visible:
  - `src/gui/controllers/WorkflowNavigationController.cpp`
  - `src/gui/MainContentComponentUiGovernance.cpp`

La idea no es inventar un nuevo componente entero, sino añadir un primer plano de lectura y una jerarquía de acciones sobre lo que ya hay.

---

## 5. Lo que no hago en esta propuesta

- No propongo un nuevo motor.
- No propongo eliminar modos.
- No propongo reducir formatos por reducir.
- No propongo tocar la capa técnica de exportación más allá de la presentación de su significado.

Lo que sí propongo es que la aplicación deje de exigir al usuario que adivine:

- qué significa el resultado,
- cuál es la salida normal,
- y qué significa una sesión cargada.

---

## 6. Criterio de aceptación de esta propuesta

Podría considerarse aceptable si, después de aplicarla:

- un usuario nuevo entiende qué hace la aplicación en el paso actual,
- un usuario que llega a un resultado entiende qué significó antes de meterse en métricas,
- un usuario que exporta entiende qué obtuvo y dónde quedó,
- un usuario que carga una sesión sabe si puede continuar o solo revisar,
- y los dos modos se leen como distinto nivel, no como dos productos separados.

---

## 7. Si esto se acepta, el siguiente trabajo sería

- definir la lectura mínima del resultado en una sola forma,
- definir la salida principal y sus variantes en una misma jerarquía,
- y definir los estados visibles de una sesión cargada.

Eso puede hacerse antes de cualquier cambio de componente grande.
