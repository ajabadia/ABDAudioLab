# Significado de cargar o recuperar una sesión en interfaz

Este documento define lo que debe poder comunicar la interfaz cuando el usuario carga o recupera una sesión.

---

## 1. No es lo mismo abrir una sesión que reanudarla

Una sesión puede abrirse por varias razones:

- para continuar el trabajo,
- para revisar lo que ya se hizo,
- para completar algo pendiente,
- o porque el usuario no recuerda en qué estado quedó.

La interfaz tiene que poder distinguir eso, o al menos indicarlo, en lugar de cargar silenciosamente y dejar que el usuario infiera.

---

## 2. Qué debe poder decir la interfaz

Cuando se carga una sesión, debe poder responder, según el caso:

- **Puede continuar**
  - hay contexto suficiente,
  - el punto de partida está claro,
  - se puede reanudar el flujo.

- **Es solo revisión**
  - los datos están cargados,
  - pero no tiene sentido reanudar tal cual,
  - o lo que queda es lectura, no acción.

- **Falta contexto**
  - se carga algo que parece completo,
  - pero no hay algo necesario para continuar,
  - o el punto de partida ya no está disponible tal como estaba.

- **Está incompleto**
  - la sesión no alcanzó el estado esperado,
  - o quedó a medio camino,
  - y eso debe ser visible, no implícito.

---

## 3. Lo mínimo que debe quedar claro

Que el usuario sepa:

- si puede seguir trabajando,
- si lo que abre es para revisar,
- si algo importante está incompleto,
- y si hay que hacer algo antes de volver a ejecutar.

No tiene que ser complejo. Tiene que ser visible.

---

## 4. Qué no debe ocurrir

- Que una sesión cargada se comporte como si fuera idéntica a una sesión recién iniciada cuando no lo es.
- Que todo parezca correcto aunque falte contexto.
- Que el usuario tenga que adivinar si puede reanudar o solo revisar.
- Que el auto-guardado dé la impresión de que “todo está recuperado” cuando no lo está.

---

## 5. Relación con lo que ya podría existir

Si el sistema ya tiene persistencia, checkpoints o carga de sesión, esto no cambia ese mecanismo. Cambia la comunicación pública: lo que se guarda puede ser técnicamente completo y todavía no ser legible como “se reanuda”.

---

## 6. Criterio mínimo de aceptación

Al cargar una sesión, el usuario debe poder entender, sin adivinar:

- si puede continuar,
- si está revisando,
- si falta algo,
- y si hay que tomar una decisión antes de volver a ejecutar.

Si eso no queda claro, la sesión tiene datos pero no significado público.
