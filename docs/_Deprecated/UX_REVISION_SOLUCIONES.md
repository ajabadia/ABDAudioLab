# Revisión de UX — Alternativas documentadas

Este documento recoge, por cada dolor identificado, alternativas plausibles y qué implicaría cada una. No es un plan de implementación; es una hoja de decisiones para después elegir dirección.

## Estado de la revisión

- Documento de problemas: [docs/UX_REVISION_NOTAS.md](docs/UX_REVISION_NOTAS.md)
- Este documento: alternativas y buenas prácticas asociadas a esos problemas.
- Alcance: flujo doble, nuevo hardware sin recompilar, configuración de pruebas, guardado/recuperación, informe y salidas.

---

## 1. Flujo doble: guiado vs clásico

### Problema en pocas palabras
Los dos modos parecen caminos distintos cuando el usuario busca un proceso normal, y no queda claro cuál es el principio.

### Alternativas

#### 1A. Un proceso con dos vistas
- Lo que cambia es la presentación, no el flujo subyacente.
- El modo guiado sería la vista simplificada; el modo clásico, la vista técnica completa.
- Buena opción si el modo clásico es “más detalle”, no “otra secuencia de pasos”.

#### 1B. Modo principal y modo avanzado explícito
- Se declara cuál es el flujo normal.
- El otro se posiciona como avanzado, para usuarios que necesitan más control, no como un alternativo equivalente.
- Reduce la ambigüedad de “¿cuál debo usar?”.

#### 1C. Reglas claras de cambio entre modos
- Se explica cuándo conviene cambiar y qué se conserva al hacerlo.
- El riesgo menor es que el cambio se sienta como reinicio en vez de progresión.

#### 1D. Un solo flujo con capas de complejidad
- Menos modos en el interfaz, más progresión: empezar simple, mostrar detalles cuando el usuario entra en configuración avanzada.
- Útil si el objetivo es que un principiante no se sienta perdido en opciones técnicas.

### Buenas prácticas habituales
- Si hay dos modos, debe quedar claro cuál es el camino recomendado y cuál es opcional.
- Es mejor que la diferencia se explique en términos de “qué necesitas” que en términos de “qué modo estás usando”.
- Evitar que el modo alternativo parezca legacy o paralelo sin justificación.

### Implicaciones si se elige esto
- Hay que decidir primero cuál es el flujo pretendido.
- Después, los mensajes y vistas deben estar alineados con esa decisión, no con ambos modos por igual.

---

## 2. Nuevo hardware sin recompilar

### Problema en pocas palabras
No está claro cómo agregar un dispositivo nuevo principalmente mediante datos y cuándo inevitablemente se requiere código.

### Alternativas

#### 2A. Contrato JSON dentro del formato ya soportado
- Se agrega el dispositivo como contrato con los campos esperados.
- Útil si el dispositivo encaja en un tipo de control/disponibilidad ya soportado.

#### 2B. Plantilla o asistente de contrato
- Se proporciona una forma repetible de crear el contrato válido.
- Ayuda a separar “el archivo está mal formado” de “la app no soporta ese caso”.

#### 2C. Validación temprana del contrato
- Se revisa el contrato antes o durante la carga, con mensajes claros de qué falta o qué no soporta.
- Reduce el ciclo de “crear archivo, abrir app, ver que no aparece o no funciona”.

#### 2D. Extensión de la app cuando el dispositivo exige algo nuevo
- Se requiere código si el nuevo dispositivo necesita un modo de control, un protocolo o un bloque de medición que la app no expondrá.
- Esta alternativa no es “sin recompilar”; es “hay límite y ahí se extiende”.

### Buenas prácticas habituales
- Debe existir un contrato que sea autoexplicativo y fácil de validar.
- Debe quedar claro qué capacidades necesita para participar en el flujo normal.
- Debe haber un mensaje claro cuando el dispositivo no puede ejecutarse con lo que ya existe.

### Implicaciones si se elige esto
- Si el objetivo es que la mayoría de dispositivos nuevos sean “solo datos”, conviene que el contrato esté muy cerca de lo que el flujo ya ejecuta.
- Si hay muchos dispositivos fuera de ese patrón, la excepción se vuelve la regla y hay que replantear el modelo.

---

## 3. Preparar pruebas a ejecutar

### Problema en pocas palabras
No queda claro, desde la interfaz, cómo definir qué se va a medir antes de ejecutar, o la configuración de la prueba no se lee como un plan.

### Alternativas

#### 3A. Vista de resumen de prueba antes de ejecutar
- Una pantalla o panel que diga, en lenguaje claro, qué se va a medir, con qué estímulo, alcance, pasos, repeticiones y condiciones.
- Útil para reducir ejecuciones “solo por ejecutar”.

#### 3B. Nivel básico y nivel avanzado en la prueba
- Lo esencial primero: qué medir y los parámetros principales.
- Lo técnico desplegable o en vista separada.
- Buena opción si hay muchos parámetros y el usuario común no los necesita todos a la vez.

#### 3C. Plantillas de prueba por función u objetivo
- Empezar desde una configuración razonable en vez de desde cero.
- Luego permitir ajustar.
- Ayuda si hay muchas combinaciones posibles y se quiere evitar empezar siempre vacío.

#### 3D. Bloqueo preventivo de pruebas incoherentes
- Validar que la prueba armada tiene sentido para el target elegido y para el modo actual.
- Mejor que dejar que el usuario configure algo imposible o sin sentido.

#### 3E. Planes de prueba como objetos reutilizables
- Guardar una configuración preparada como algo revisable y reejecutable.
- Útil si el trabajo implica repetir mediciones similares o comparar planes.

### Buenas prácticas habituales
- Lo primero que debe leerse es “qué se va a medir”, no “cómo se configura”.
- Si la prueba tiene muchos parámetros, no los pongas todos a la misma altura.
- Una prueba preparada debe leerse como una historia corta: qué función, qué rango, qué condición, qué se espera validar.

### Implicaciones si se elige esto
- Habría que definir qué parte de la configuración es “principal” y qué parte es “técnica”.
- Después, el interfaz puede mostrar primero lo principal y dejar lo técnico explícitamente secundario.

---

## 4. Guardar y recuperar

### Problema en pocas palabras
Existe persistencia y auto-guardado, pero no queda claro qué significa cargar algo después, ni qué estados son recuperables con qué intención.

### Alternativas

#### 4A. Definir niveles de guardado
- Por ejemplo: sesión en marcha, punto de recuperación, archivo/cierre, resultado definitivo.
- Ayuda si ahora todo parece un blob igual de “guardado” sin matices.

#### 4B. Sesión recuperable con contexto explícito
- Al cargar, indicar claramente si se puede reanudar, si es solo lectura, o si requiere acción.
- Reduce la duda de “¿puedo continuar desde aquí?”.

#### 4C. Resumen de estado recuperado
- Mostrar qué está completo, qué falta, qué está en modo revisión y qué requiere acción.
- Importante si el usuario abre algo que parece completo pero no lo es del todo.

#### 4D. Historia de guardado predecible
- Dejar claros cuándo se guarda automáticamente, cuándo el usuario debe guardar, y qué pasa si se cierra a medias.
- Útil para quitar la incertidumbre de “¿se perdió lo que hacía?”.

### Buenas prácticas habituales
- El riesgo no es tener auto-guardado, es que el usuario no sepa si lo que recupera es “continuar” o “revisar”.
- Lo recuperado debe tener un estado legible, no solo cargarse en silencio.
- Si hay artefactos distintos, cada uno debe tener su propósito claro.

### Implicaciones si se elige esto
- Habría que nombrar y distinguir los distintos “tipos de guardado” que el sistema ya hace o podría hacer.
- Después, el interfaz debería comunicarlos en lugar de mezclarlos.

---

## 5. Informe

### Problema en pocas palabras
El informe tiene contenido técnico rico, pero puede mezclar capas distintas y no queda claro qué leer primero.

### Alternativas

#### 5A. Formato en capas
- Primero una lectura rápida de qué se hizo, qué salió y con qué confianza.
- Después, los detalles técnicos.
- Útil si el informe quiere ser útil sin ser denso de entrada.

#### 5B. Resumen humano antes que diagnóstico técnico
- El primer plano responde la pregunta que el usuario hace antes de meterse en métricas.
- Importante si el objetivo es comunicar, no solo documentar.

#### 5C. Línea clara entre evaluación y paquete/export
- Que no sea evidente que un es “el resultado” y otro solo “el archivo que lo acompaña”.
- Ayuda si hay varios artefactos y el usuario no sabe cuál es el significativo.

#### 5D. Información contextual bien situada
- Condiciones de medición, estado de calibración, alcance validado, advertencias relevantes, pero colocadas donde pertenecen y no mezcladas con la conclusión principal.
- Buena práctica si el informe es técnico pero intenta ser leído por humanos.

### Buenas prácticas habituales
- Un informe técnico funciona mejor cuando tiene un primer nivel legible y el resto explotable sin forzar al lector.
- Si el informe quiere ser convincente, no por ser más denso sino por ser más claro en la historia.
- La narrativa de “qué hiciste, qué salió, qué significa, qué puedes hacer después” suele ser más útil que el dump de campos.

### Implicaciones si se elige esto
- Habría que separar qué es “resumen de lectura rápida” y qué es “detalle técnico”.
- Después, el informe puede reorganizarse en torno a esa jerarquía.

---

## 6. La salida

### Problema en pocas palabras
Hay varios formatos y varias acciones, pero no queda claro cuál es el resultado principal ni para qué sirve cada uno.

### Alternativas

#### 6A. Una salida por defecto y el resto como variantes
- Que el usuario entienda cuál es la salida recomendada en casos normales.
- Importante si el usuario no quiere navegar una “nevera de formatos”.

#### 6B. Asignar un propósito a cada formato
- Por ejemplo: uno para consumo posterior, uno para revisar/compartir, uno para usar en flujo de trabajo, uno para validación o archivado.
- Ayuda a que cada opción tenga razón de existir.

#### 6C. Nombres y contexto claros
- Al pulsar, que quede claro qué se genera y dónde queda.
- Reduce la sensación de “inicio algo y no sé qué obtuve”.

#### 6D. Jerarquía de resultados
- Decir cuál es el resultado “normal” y cuáles son “extra” o “para casos concretos”.
- Buena opción si la app tiene varias salidas pero pocos usuarios expertos.

### Buenas prácticas habituales
- Si la app puede exportar muchas cosas, conviene que al menos una tenga identidad clara.
- No basta con ofrecer formatos; hay que explicar qué resuelve cada uno.
- Lo que el usuario busca normalmente es una respuesta a “qué me quedo si hago esto”.

### Implicaciones si se elige esto
- Habría que definir el resultado principal antes de presentar la gama.
- Después, el resto puede explicarse como variantes con propósito.

---

## Cruce entre temas

Estos problemas no son independientes:

- Si el flujo es claro, el guardado y las pruebas se leen mejor.
- Si las pruebas tienen una vista previa clara, el informe y la salida también ganan legibilidad.
- Si el informe cuenta una historia clara, la salida y el guardado se entienden mejor como artefactos coherentes.

Por eso la mejor ordenación probablemente sea:
1. Definir el flujo pretendido.
2. Definir qué se guarda y qué significa recuperarlo.
3. Definir cómo se prepara una prueba antes de ejecutar.
4. Conseguir que el informe tenga un primer plano legible.
5. Dar identidad clara a la salida principal.

---

## Preguntas de decisión que quedarían

1. ¿El modo guiado debe ser el flujo normal o un modo más entre varios?
2. ¿Qué tipos de dispositivo nuevos se quieren soportar sin recompilar?
3. ¿Qué parte de la configuración de pruebas debe ser obligatoria y visible antes de ejecutar?
4. ¿Qué significa exactamente “recuperar una sesión” en la práctica?
5. ¿Qué es lo que el informe debe comunicar en los primeros segundos?
6. ¿Cuál es la salida normal y para qué sirve cada otra salida?

---

## Nota metodológica

Estas alternativas se basan en lectura del código y de los contratos de sesión/exportación, no en una evaluación tras usar la app con hardware/plugin real. Algunas opciones pueden ser más adecuadas que otras según el público real y el flujo real de trabajo.
