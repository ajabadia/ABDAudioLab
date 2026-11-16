## Miradas alternativas sobre los modos que ya tienes

La app ya tiene dos modos: uno con lista de tareas y ejecución automática, y otro más guiado que deja menos cosas abiertas a edición. El punto central de esta sección no es inventar un nuevo modo ni decidir cuál ha de ser el principal por decreto. Es revisar si la forma actual de explicarlos está poniendo el foco en el sitio correcto.

### El punto central

La lectura que más me convence es esta:

- el modo actual no se presenta como “el modo no guiado”,
- el modo guiado no se presenta como “el modo normal”,
- la diferencia se lee como niveles de edición, no como uno que oculta y otro que no.

### Qué no están pensados para hacer

No son una explicación técnica del motor, ni una descripción del flujo interno. Solo intentan que, en un vistazo, quede claro que hay un modo con automatización visible y otro modo con distinto nivel de edición, y que ninguno es “el modo que oculta lo que pasa”.


## Primer pase pequeño que merece atención ahora

De todos los temas, el que mejor relación coste/clareza tiene para un primer pase es **la lectura del informe y la identidad de la salida en el modo guiado**.

No es el único importante, pero es el que más se nota cuando el usuario llega al resultado y no encuentra una respuesta rápida a “¿qué significa esto?” y “¿qué me quedo si hago esto?”.

### Por qué este y no otro

- El modo guiado ya tiene un punto de partida claro: un resumen de resultados, un panel de ejecución, un stepper con resumen de sesión, y una capa de exportación.
- Eso significa que no empieza desde cero: ya hay un lugar donde el usuario espera una lectura de resultado y ya hay un botón dominante para exportar.
- El problema actual no es que falte funcionalidad, es que el usuario puede tener que leer métricas, hashes, condiciones y artefactos para entender qué significa el resultado y qué artefacto va a conseguir.
- Un primer pase pequeño en ese lugar mejora la lectura general sin tocar el flujo completo de principio a fin.

### Qué decisión básica necesito como base

Para alinear esto, necesito que asumas o decidas cuatro cosas simples:

1. **El modo guiado es el camino normal para la mayoría de casos.**  
   El modo clásico existe para mayor control, no como un segundo camino igual de recomendado.

2. **El informe debe responder primero en lenguaje claro y poner el detalle técnico después.**  
   La pregunta principal no es “¿qué métricas tengo?” sino “¿qué significó esta medición?”.

3. **La salida principal es el paquete de producción para la mayoría de casos habituales.**  
   El resto son variantes: revisar/compartir, abrir carpeta, ver HTML, previsualizar/A-B, publicar.

4. **El resultado debe leerse como un todo coherente: resumen en pantalla → informe → artefacto exportado.**  
   Que no se sienta como tres cosas distintas sin relación.

Si alguna de esas decisiones ya la tienes definida de otra forma, dímelo y ajusto la propuesta. Si no, las dejo como base para que el resto tenga coherencia.

### Qué sería el pase pequeño, en términos de interfaz y mensaje

#### 1. Un primer plano de lectura en el resumen del modo guiado

Hoy el panel de resumen puede mostrarte métricas de calidad. Eso está bien, pero puede no ser suficiente si lo que el usuario quiere es una lectura rápida de significado.

El pase pequeño consistiría en añadir, encima de eso, una lectura breve en lenguaje claro, algo como:

- “¿Qué hiciste?” → target, excitación, condiciones básicas, alcance.
- “¿Qué salió?” → resultado en lenguaje claro: si la medición está completa, si es aceptable, y con qué confianza.
- “¿Y ahora qué?” → exportar el paquete, revisar el HTML, abrir la carpeta, comparar/reanalizar si hace falta.

Esto no sustituye las métricas. Lo que hace es que el usuario no tenga que desarmar el informe técnico para entender el significado primero.

#### 2. Una separación clara entre evaluación y exportación

Conviene que quede claro qué es:
- la evaluación/resultado principal,
- y qué son los artefactos que lo acompañan.

Un ejemplo útil:
- el informe certifica y describe el resultado,
- el paquete exportado es la materialización de ese resultado para consumo posterior.

Esto evita que el usuario sienta que exportar es “guardar un montón de archivos” sin saber cuál es el resultado real.

#### 3. Un botón principal con identidad y unas variantes con propósito

El botón dominante ya existe y es “EXPORT PRODUCTION PACKAGE (1-CLICK)”. Eso puede mantenerse como acción principal.

Lo que falta es que el usuario entienda, de forma breve y visible, qué significa ese botón y para qué sirve el resto.

Ejemplo de jerarquía útil:
- **acción principal**: exportar el paquete de producción → “genera el resultado principal listo para usar/entregar y lo guarda en la carpeta de exportación”.
- **acciones de revisión**: abrir carpeta, ver HTML.
- **acciones avanzadas**: previsualización DSP, A/B, publicación cloud.

Esto no tiene que ser solo un cambio visual. Tiene que leerse como una jerarquía de propósito.

### Ejemplo de mensajes que ayudarían

Después de pulsar exportar, conviene que el usuario entienda qué se generó y dónde quedó. Algo como:
- “Paquete de producción exportado con éxito: C++ alignas(16), JSON, HTML.”
- “Informe HTML abierto en el navegador.”
- “Carpeta de exportación abierta en el explorador.”
- “Ejecuta primero la exportación si quieres ver el informe HTML.”

 Este tipo de mensajes conecta la acción con el artefacto y con el destino, y quita la sensación de “inicio algo y no sé qué obtuve”.

### Qué no estoy proponiendo aquí

No propongo:
- reescribir el flujo completo,
- cambiar los formatos existentes,
- ni tocar la exportación técnica más allá de la presentación del mensaje.

Lo que propongo es un pase pequeño de interfaz/mensaje: mejorar la lectura del resultado, dejar claro cuál es la salida normal, y explicar el resto como variantes.

### Cómo encaja con lo que ya hay en código

La propuesta se apoya en cosas que ya existen:
- panel de resumen y ejecución del modo guiado,
- stepper con resumen de sesión,
- `SessionReportManager` como gestor de diálogos y formatos,
- `ReportExportService` como servicio de exportación,
- `CertificationReportExporter` para el informe HTML,
- `LutExporter` y `NamDatasetExporter` para los artefactos,
- `ReportExportUiController` para la presentación de la exportación asíncrona,
- y los botones actuales: exportar paquete, abrir carpeta, ver HTML, preview DSP, A/B, publicar.

Así que el pase no empieza desde cero: solo necesita una capa de lectura y una capa de mensaje más clara encima de lo que ya está hecho.

### Limitaciones de este pase

- Mejora la narrativa y la legibilidad, pero no resuelve el nivel técnico del público ni los flujos avanzados del modo clásico.
- Depende de lo que quieras que signifique “resultado normal” en la práctica; si la salida principal por defecto es otra, hay que ajustarlo.
- No es una validación con hardware/plugin real; es una propuesta basada en lo que la app ya expone hoy.

### Siguientes pasos si te parece bien

Si quieres, puedo convertir esto en:
- una propuesta más concreta de texto para el resumen del modo guiado,
- una propuesta de nombres y propósitos por botón,
- o una propuesta de qué mensaje se muestra después de cada acción de exportación.

Si alguna decisión base cambia, avísame y lo reacomodamos antes de concretarlo.

---

## Titulares y tooltips para la barra de modo

Esto es un par de variantes pensadas para que el modo actual y el modo guiado se lean como:

- un flujo con automatización visible que ya existe,
- y un modo con distinto nivel de edición,

sin que ninguno parezca que oculta lo que hace.

### Variante A: modo actual como flujo principal con automatización

#### Títulos para el botón de cambio de modo

- `Modo: Lab · Automático`
- `Modo: Lab · Lista de tareas activa`
- `Modo: Lab · Más automatizado`

#### Tooltips para el botón de cambio de modo

- `Flujo principal con automatización activa. Cambia a modo Guiado si prefieres una ejecución con menos edición.`
- `Modo actual: automatiza la ejecución. El Guiado deja más cosas prefijadas y pide menos edición.`

#### Mensajes breves para la barra o el estado

- `Modo actual: automatiza lo que ya puedes definir tú.`
- `Modo actual: ejecuta con lista de tareas y progreso visible.`
- `Modo guiado: menos edición, más cosas prefijadas.`
- `Modo guiado: ejecución limpia, menos ajustes abiertos.`

### Qué no están pensados para hacer

No son una explicación técnica del motor, ni una descripción del flujo interno. Solo intentan que, en un vistazo, quede claro que hay un modo con automatización visible y otro modo con distinto nivel de edición, y que ninguno es “el modo que oculta lo que pasa”.


## Borrador del resumen del modo guiado

Este es un primer plano posible para el resumen después de ejecutar, dentro del modo guiado. No reemplaza las métricas: lo acompaña.

Este resumen no intenta explicar que el modo guiado es “el modo simple” o “el modo que oculta lo que pasa”. Intenta dejar claro que es un modo con menos edición, y que la salida principal tiene sentido antes de irse a los artefactos.

### Encabezado

`Resumen de la sesión`

### Qué hiciste

- `Target`: <aquí va el apunte breve del target o plugin>
- `Excitación`: <aquí va lo que se usó para generar las respuestas>
- `Condiciones`: <calibración, línea de ganancia o condición relevante>
- `Alcance`: <qué se midió y en qué rango>

### Qué salió

- `Estado`: completado / incompleto / sin ejecutar
- `Resultado en lenguaje claro`: <si el resultado parece aceptable, si hay advertencias, y con qué confianza>
- `Qué dice la medición, en pocas palabras`: <un resumen de una frase de lo que parece haber salido>

### Qué puedes hacer después

- Exportar el paquete de producción
- Abrir la carpeta de exportación
- Ver el informe HTML
- Comparar o reanalizar si hace falta

### Por qué sale primero el paquete de producción

Aquí no se presenta la exportación como una lista de formatos. Se presenta como:

- el resultado principal que queda listo para usar o entregar,
- y el resto como variantes: revisar, compartir, comparar o hacer algo más avanzado.

Así el usuario no tiene que adivinar qué es lo que se supone que debe quedarse.

### Lo técnico después

Debajo de esto se deja el detalle técnico: métricas de calidad, condiciones de calibración, hash y procedencia, y los artefactos generados.

### Qué no pretende ser este resumen

No es un dump de métricas, ni una pantalla de diagnóstico ni un panel de artefactos. Es la lectura rápida que conecta lo que hiciste con lo que obtuviste, antes de que el usuario tenga que leer todo el informe técnico.

