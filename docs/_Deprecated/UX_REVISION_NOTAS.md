# Revisión de UX — ABDAudioLab

Lectura desde la capa GUI actual y el contrato de sesión/exportación. Enfocada en comprensión, guardado/recuperación, informe y salidas, no en rasgos visuales puntuales.

## Visión general

La aplicación tiene una estructura GUI bastante formalizada:
- sidebar tipo stepper con pasos numerados y lectura de estado,
- selector de hardware/catálogo con metadatos dinámicos,
- dos modos operativos distintos (`Classic` / `Guided`),
- y varias capas de persistencia/exportación (`.abdlabtest`, informes HTML, paquetes, LUTs, JSON).

Eso es coherente para un banco de medición técnico. El problema que veo no es falta de cosas, es que varias decisiones clave no están suficientemente explícitas para el usuario.

---

## 1. El doble flujo

### Lo que veo
- Un flujo guiado con pasos claros.
- Un modo clásico/lab con cola de pruebas, plotter, y paneles técnicos.
- Un código que distingue `UiWorkflowMode::Classic` y `UiWorkflowMode::Guided`.

### Lo que no me convence
- Parece que hay dos formas de trabajar sin que quede del todo claro cuál es el camino normal.
- No se lee inmediatamente la relación entre ambos: ¿uno es versión simplificada del otro? ¿son casos distintos? ¿convergen o tienen rutas separadas?
- Para un usuario nuevo, el riesgo es que la app sienta “tienes dos modos y hay que saber cuál usar” antes de sentir “tengo un proceso claro”.

### Lo que me gustaría entender mejor
- Si el modo guiado ya cubre la mayoría de casos, ¿cuál es el rol exacto del modo clásico?
- Cuándo conviene cambiar de uno a otro y con qué garantía de que no se pierde el contexto.
- Si la diferencia real es más sobre presentación/ritmo que sobre capacidad.

---

## 2. Guardar y recuperar

### Lo que veo
- Persistencia de sesión en `.abdlabtest`.
- Auto-guardado/checkpoints de recuperación.
- Carga de sesión y aplicación de manifiesto + puntos medidos.
- Exportación separada a otros formatos.

### Lo que no me convence
- No queda del todo claro qué se guarda en cada nivel y con qué intención:
  - ¿guarda el plan/intención de medición?
  - ¿guarda las medidas ya tomadas?
  - ¿guarda evaluación/resultados?
  - ¿guarda enough para reanudar el flujo tal cual?
- Hay guardado manual, guardado asíncrono y checkpoints, pero la diferencia operativa puede no ser obvia.
- Al cargar, no me queda claro si recupero una “sesión” completa o una “lectura de datos” que luego hay que reinterpretar.

### Lo que me gustaría entender mejor
- Qué estados son recuperables y cuáles son “solo registro”.
- Cuándo se considera que una sesión está “aberta”, “en pausa”, “listo para reanudar”, “archivado”.
- Si hay riesgo de recuperar algo que parece completo pero en realidad necesita contexto faltante.

---

## 3. El informe

### Lo que veo
- Exportación rica con métricas físicas, hash, procedencia, dominios validados, y varias representaciones (HTML, JSON, paquetes, LUTs).
- Vista de resumen de resultados con métricas y verificación criptográfica.

### Lo que no me convence
- Parece una mezcla de capas: metadatos de sesión, información del target, calibración, resultados del análisis, y artefactos exportados.
- Si todo está a la misma altura visual, el lector puede tardar en encontrar la lectura rápida: ¿qué fue lo que hice, qué resultó, y con qué confianza?
- Tiene el riesgo de ser técnicamente correcto y comunicativamente denso.
- Si el resumen es muy técnico desde el inicio, deja de funcionar como “historia de la medición”.

### Lo que me gustaría que tuviera
- Un primer plano que responda:
  - de qué se trató la medición,
  - qué salió,
  - si está aceptable o no,
  - qué se puede hacer con ese resultado.
- Detalles técnicos después, desplegados o en secciones separadas.
- Claridad entre “evaluación/modelo” y “paquete de artículo”.

---

## 4. La salida

### Lo que veo
- Varios formatos de salida: C++ LUT, JSON, HTML, package y posiblemente publicación.
- Varias acciones relacionadas: exportar reporte, exportar paquete, abrir HTML, etc.

### Lo que no me convence
- No queda claro cuál es la salida principal y cuál es auxiliar.
- No me resulta evidente qué formato sirve para qué propósito sin meterme en detalles técnicos.
- Puede parecer que la aplicación “exporta muchas cosas” sin dejar claro “el resultado que yo quiero”.
- Si el usuario no tiene una respuesta rápida a “qué obtengo si hago clic aquí”, la acción pierde fuerza.

### Lo que me gustaría entender
- Qué es el resultado más relevante para un caso típico.
- Qué formato es para qué:
  - consumo posterior,
  - compartir/revisar,
  - uso en flujo de trabajo,
  - o artefacto de validación.
- Por qué existen varias salidas y cuál debería ser la opción por defecto en cada caso.

---

## Temas transversales

### Lectura y lenguaje
- El sistema usa conceptos muy precisos internamente (estados, modo de excitación, control, validación, procedencia, hash).
- Eso es útil para el motor, pero puede alejar al usuario si los mensajes y resúmenes heredan esa precisión sin traducción.
- Mejoraría si el nivel de lenguaje fuera progresivo: lo esencial primero, lo técnico después.

### States visibles vs states recuperables
- Hay mucha información técnica pasando por el snapshot y la UI.
- Parece que el reto es separar “lo que la app necesita para funcionar” de “lo que el usuario necesita para entender”.
- Especialmente en guardado/recuperación, donde un estado serializable no siempre es un estado legible.

### Principiante vs experto
- El producto parece orientado a un experto en medición acústica.
- Eso está bien, pero el UX todavía tiene que explicar el proceso sin forzar al usuario a inferir la regla interna.
- Si hay dos modos, el principio debe ser: el usuario no tiene que adivinar cuál es el “correcto” para su caso.

---

## Configuración de pruebas a ejecutar

### Lo que veo
- La cola de pruebas se compone desde contrato y desde la función elegida.
- Hay opciones para añadir pruebas estándar y pruebas personalizadas.
- En el flujo guiado el usuario suele pasar por configuración antes de iniciar.

### Lo que no me convence
- No me queda claro, desde la interfaz, cómo preparar un plan de pruebas completo sin tener que andar apuntando fuera de la app lo que voy a medir.
- Si el contrato define funciones, parece que el usuario debe saber qué función elegir y qué implica en la medición real.
- Puede que falte una forma más legible de:
  - ver qué va a medirse antes de ejecutar,
  - ajustar alcance, pasos, repeticiones o condiciones de captura desde una vista clara,
  - validar que lo que se va a ejecutar tiene sentido para el target elegido.
- El punto de decisión parece estar más en “qué contrato/funcion” que en “qué y cómo mediré”.

### Lo que me gustaría entender mejor
- Dónde termina la configuración guiada y dónde empieza la edición avanzada.
- Qué parámetros de la prueba son los que más importan para el usuario común y cuáles son técnicos.
- Si falta un modo de “ver el plan antes de correrlo” más explícito.

## Preguntas que me quedarían para afinar

1. **Flujo**: ¿el modo guiado es la versión simplificada del modo clásico, o representan objetivos distintos?
2. **Save/recover**: ¿qué quieres que recuerde exactamente una sesión guardada y recuperable?
3. **Informe**: ¿cuál es el mensaje principal que debe leer el usuario en 10 segundos?
4. **Salida**: ¿qué formato/resultado es el “producto” por defecto para la mayoría de usuarios?
5. **Reanudación**: ¿se espera que una sesión cargada vuelva a un punto de acción, o solo permite revisión?
6. **Nivel técnico**: ¿el público habitual ya sabe qué es un LUT, un paquete, un hash canónico, una verificación de procedencia, o eso debe explicarse?

---

## Posibles direcciones (sin decidir nada aún)

- Unificar la narrativa de “flujo normal” incluso si existen dos modos.
- Hacer que el guardado/recuperación tenga niveles claros: intención, medidas, evaluación, artefactos.
- Dar al informe un primer plano de lectura rápida más agnóstico y técnico en capas.
- Definir una salida “principal” y explicar el resto como variantes por caso de uso.

---

## Nota metodológica

Estas notas son una lectura desde código y contratos, no una evaluación después de usarlo con hardware/plugin real. Algunos puntos pueden ser más o menos fuertes según el flujo real del usuario.
