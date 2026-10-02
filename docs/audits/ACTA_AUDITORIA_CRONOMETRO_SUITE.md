# ACTA DE AUDITORÍA DEL CRONÓMETRO DE LA SUITE

## `tools/duraciones-suite.mjs` — 2 de octubre de 2026

**Documento:** `docs/audits/ACTA_AUDITORIA_CRONOMETRO_SUITE.md`
**Fecha de emisión:** 2026-10-02
**Objeto auditado:** `tools/duraciones-suite.mjs`, 554 líneas en `8606991` (964 en la versión corregida)
**Alcance:** la herramienta y su fichero de test `tools/test_duraciones_suite.mjs`
**Estado:** 🟡 4 hallazgos — 2 ALTO, 1 MEDIO, 1 BAJO. Los 4 corregidos.
**Addendum:** §3 contiene los tres fallos con demostración ejecutable. §4 contiene el cuarto, que solo se demuestra por inspección, y se dice explícitamente. §6.1 es lo que hay que leer antes de confiar en que el guard vigila algo: ahora dice la verdad, y no hay nadie escuchando.

---

## 1. Qué se ha auditado y por qué

`duraciones-suite.mjs` corre la suite de Catch2, mide el tiempo de cada caso y los compara con una referencia guardada. Es el guard de tiempos del proyecto: su veredicto es lo que se usa para decidir si un test se ha puesto lento.

Un guard que miente en silencio no es un guard. Y esta herramienta tiene una forma particular de callar: **no falla, informa**. Informa de que la referencia tiene más casos de los medidos, informa de que no ha medido ningún test, informa de que no hay nada por encima del umbral — y todo eso con el mismo aspecto que un informe bueno. El código de salida era la única señal que desentonaba, y era la única que nadie miraba (§6.1).

Quién lo invoca:

| Quién | Dónde | Qué hace con el código de salida |
|-------|-------|----------------------------------|
| `build.bat` | línea 192, con `RUN_PERF=1` | Lo guarda en `PERF_EXIT` y elige qué `[Warn]` imprimir. No lo propaga. |
| CI de Windows | `.github/workflows/audio-ab-5d-ci.yml` línea 169 | `continue-on-error: true` y `exit 0` al final. Lo ignora, y está escrito que es a propósito. |
| A mano | `node tools/duraciones-suite.mjs` | La persona lo lee. |

## 2. Método, y qué cuenta como demostración

Secuencia: hipótesis → inspección del código → ejecución que la sostiene. Un hallazgo **sin demostración ejecutable no se cuenta como hallazgo, se cuenta como sospecha**, y se dice cuál de las dos cosas es.

Las tres primeras carencias del §3 se demostraron ejecutando el código de `8606991` con datos y argumentos construidos al efecto. La cuarta (§4) no: escribir un fichero de forma no atómica solo se demuestra cortando la corriente en el momento exacto, así que se demuestra por lectura y se marca como tal.

| Hallazgo | Severidad | Cómo se demuestra |
|----------|-----------|-------------------|
| El valor de `--base` llega a Catch2 | ALTO | Ejecución: se imprime lo que recibiría Catch2 |
| El renombrado sale dos veces en otra máquina | ALTO | Ejecución: se imprimen los contadores de la comparación |
| El mensaje oculta dónde se ha buscado | BAJO | Ejecución: se imprime el mensaje y la ruta real |
| La referencia se escribe encima de la buena | MEDIO | Inspección: no hay demostración posible sin cortar la corriente |

### 2.1 El fallo que ya estaba abierto cuando empezó esta auditoría

Antes de estos cuatro había uno más, ya demostrado y cerrado en el commit `078b2aa`: `capturarXml` concatenaba `stdout` y `stderr` sin mirar `r.error`, `r.signal` ni `r.status`, de modo que un cuelgue dejaba un XML truncado que se analizaba como una medición buena — 187 casos de 927, el aviso de «740 ausentes», y salida con **0**. Se cerraron tres cosas juntas: mirar el resultado del spawn, comprobar que el documento esté cerrado (`</Catch2TestRun>`) y que la cuenta de casos cuadre con la referencia. Se deja aquí anotado para que esta acta se lea completa, no porque esté abierto.

## 3. Los tres fallos demostrados

### 3.1 ALTO — El valor de `--base` llega a Catch2 como filtro de test

**Dónde:** `8606991` líneas 455 y 481-483.

El filtro que decide qué se le pasa a Catch2 separaba a mano los valores de las banderas:

```js
const FACTORES_INFORMADOS = [String(umbral), String(factor)];
const paraCatch = argumentos.filter((a) => !a.startsWith('--')
  && !FACTORES_INFORMADOS.includes(a));
```

La lista tiene dos entradas y las banderas con valor son cuatro. `--base` y `--xml` no están, así que su valor se cuela entre los argumentos.

**Demostración**, con el filtro tal y como estaba escrito:

```
[A] paraCatch = ["otra.json","~[integration-01]"]
```

Para Catch2, `otra.json` es un filtro de test que no nombra a ningún caso. La suite corre entera y no mide nada.

**Por qué es ALTO.** El resultado de un filtro que no casa con nada es una medición vacía, y una medición vacía en esta herramienta salía con **0**: `lentos` vale cero, no hay regresiones y no había forma de que el código de salida dijera otra cosa. O sea, la vía de obtener un verde falso aquí es no medir, que es la peor de las tres. Y el síntoma es indistinguible del correcto: `resumen()` imprime «No se ha medido ningun test» tanto si el XML vino vacío de verdad como si la medición se annuló en el filtro de argumentos.

**Alcance, y hay que decirlo.** Hoy nadie del repositorio pasa `--base`: `build.bat` y el workflow llaman al tool sin banderas con valor. Es una trampa latente, no un fallo activo en los pipelines — pero la introducen el valor y la bandera, y `--base` es justo la bandera que alguien usaría para comparar dos máquinas, que es el escenario donde un verde falso cuesta más caro.

**Arreglo:** la lista pasa a ser declarativa y a vivir junto a las banderas que `main` lee (`BANDERAS_CON_VALOR`, línea 101), y el filtrado pasa a ser una función pura y comprobable sin lanzar la suite (`paraCatchDe`, línea 113).

**Verificación:** `node tools/test_duraciones_suite.mjs` comprueba que `otra.json` no aparece entre lo que recibe Catch2, que los filtros de test sí llegan, que ninguna bandera llega, y que una bandera sin valor detrás no se come el filtro que la sigue.

### 3.2 ALTO — El renombrado sale dos veces en otra máquina, y de paso esconde su regresión

**Dónde:** `8606991` líneas 321-323 y 338-346.

El emparejamiento era: por nombre exacto, y si no, por fichero de origen. Dos cosas estaban mal y una era la consecuencia de la otra.

La causa era que `porFichero` guardaba **un solo nombre por fichero**:

```js
if (!porFichero.has(dato.f))
  porFichero.set(dato.f, nombre);
```

Y la clave era la ruta **absoluta** (`dato.f`), que lleva el disco, el proyecto y el usuario. En otra máquina no coincide con nada, de modo que el renombrado —que se detecta justamente por el fichero de origen— dejaba de detectarse. Un caso sin pareja se empujaba a `nuevos` y se pasaba de largo con `continue` antes de `vistos.add()`, así que tampoco dejaba rastro de que fuera el mismo test de antes.

**Demostración**, con la referencia tomada en la máquina A y la medición hecha en la B, con un test renombrado:

```
[B] nuevos = 1  ausentes = 1
[B] renombrado x4 -> regresiones = 0
```

Los dos primeros números son un solo test: sale como nuevo y como desaparecido, y de los dos avisos el segundo es mentira. El tercero es lo grave. Un renombrado que además se multiplica por cuatro no se ve **como ninguna cosa**, porque sin pareja no hay tiempo anterior con el que compararlo.

**Por qué es ALTO.** Ruido duplicado es annoyance. Un guard que deja de detectar una regresión cuando el test ha cambiado de nombre es otra cosa: en el repositorio hay un único `duraciones-referencia.json`, comiteado, con las rutas absolutas de la máquina que lo generó. Cualquier otra persona que lo ejecute tiene el emparejamiento por fichero muerto desde el primer día, sin ningún cambio de código de por medio. Un guard que se apaga en cuanto alguien renombra un test es un guard que no vigila.

**Arreglo:** la identidad de un fichero pasa a ser su nombre sin la parte dependiente de la máquina (`claveDeFichero`, línea 380), `porFichero` pasa a guardar una **lista** de nombres por fichero, y el emparejado se hace en dos pasadas: primero por nombre, y para lo que quede, por fichero de origen. Si en ese fichero queda algún nombre de la referencia sin gastar, ese es el nombre viejo; si quedan varios, se empareja por el tiempo anterior más parecido, porque un renombrado no cambia cuánto tarda el test. Cada nombre se gasta una sola vez, que es lo que evita el aviso doble.

**Verificación:** además del doble aviso, hay un caso con dos renombrados en el mismo fichero —sin forma de saber cuál es cuál— que comprueba que cada uno se compara con **su** tiempo y no con el del otro.

### 3.3 BAJO — El mensaje de error oculta dónde se ha buscado

**Dónde:** `8606991` líneas 473 y 510.

```js
console.error(`No esta la suite compilada en: ${basename(SUITE)}`);
console.error(`Referencia guardada en ${basename(rutaBase)}: ${duraciones.length} casos.`);
```

**Demostración:**

```
[C] mensaje = No esta la suite compilada en: ABDAudioLab_Tests.exe
[C] ruta    = D:\...\ABDAudioLab\build\Release\ABDAudioLab_Tests.exe
```

La ruta va recortada porque este documento lo escanea `test_ResourcePathHygiene.cpp`, que prohibe citar la ruta de la maquina de nadie. Es el mismo criterio que aplica al resto de la suite: un documento que lleva la ruta del autor no es mas útil para el siguiente, y ese acta lo aprendio la vez que la escribio entera.

**Por qué es BAJO y no más.** No produce ningún resultado falso: solo cuesta tiempo de diagnóstico. Un `basename` en un mensaje dice *qué* falta y no *dónde* se ha mirado, y aquí hay dos `build/Release` en juego —el que genera `build.bat` y el de una compilación a mano—, así que el nombre a secas invita a buscar el ejecutable en el árbol equivocado. Con `--base` el mismo defecto pesa más, porque esa ruta la elige quien llama y puede estar en cualquier parte.

**Arreglo:** los dos mensajes llevan la ruta entera y `basename` se queda sin uso, así que sale del `import` de `node:path`.

## 4. El cuarto, demostrado solo por inspección

### 4.1 MEDIO — La referencia se escribe encima de la buena

**Dónde:** `8606991` línea 508.

```js
writeFileSync(rutaBase, `${JSON.stringify(construirBase(duraciones, xml), null, 2)}\n`, 'utf8');
```

**Por qué es un fallo.** `writeFileSync` abre el destino en modo truncado. Si el proceso muere a mitad —un corte, un antivirus que se lleva el fichero por delante, dos cronometros a la vez— lo que queda es un JSON truncado.

**Por qué es MEDIO y no ALTO.** Porque la pérdida **no duele en el momento**, y esa es la parte interesante. `leerBase` no falla ante una referencia ilegible: avisa con `[referencia] no se pudo leer …` y sigue como si no hubiera referencia. Así que lo que se pierde no es la referencia nueva, que la siguiente vuelta regenera sin problema, sino **la vieja**, que era la única copia y desaparece sin que nadie se entere de que estaba. Un fallo que borra el histórico y lo hace callado está por debajo de un fallo que inventa un resultado falso, y por encima de un mensaje molesto.

**Por qué no hay demostración ejecutable, y por qué no se ha buscado una.** La forma no atómica es una ventana de tiempo entre el `open` con truncado y el último `write`. Fabricarla de forma determinista exigiría cortar la corriente en ese instante. Se ha preferido decir que no hay demostración en vez de escribir un test que simule un corte de luz y dé una falsa sensación de cobertura.

**Arreglo:** `escribirEntero` (línea 666) escribe a un temporal con el `pid` en el nombre y lo renombra encima. En Windows el renombrado sustituye el destino porque Node usa `MOVEFILE_REPLACE_EXISTING`, y con eso el destino viejo está entero o no está. El temporal se borra si algo falla, porque un JSON a medias con nombre de temporal confunde más de lo que ayuda.

**Verificación:** se comprueba por fuera, que es donde se ve. Se guarda una referencia encima de otra que ya existe, se lee el fichero, se comprueba que tiene los casos de la medición y que su `medidoEn` es una cadena — o sea, que la sustitución funcionó sobre un destino existente y no solo en vacío — y se mira que en la carpeta no quede ningún `.tmp`.

## 5. Resumen

| # | Hallazgo | Severidad | Alcance | Arreglado en |
|---|----------|-----------|---------|--------------|
| 3.1 | El valor de `--base` llega a Catch2 | **ALTO** | Latente: nadie pasa `--base` hoy | `cf17e42` |
| 3.2 | Renombrado duplicado, y su regresión oculta | **ALTO** | Activo en cualquier otra máquina | `cf17e42` |
| 3.3 | El mensaje oculta dónde se ha buscado | **BAJO** | Activo siempre | `cf17e42` |
| 4.1 | La referencia se escribe encima de la buena | **MEDIO** | Requiere que el proceso muera a mitad | `cf17e42` |

Criterio de severidad usado: **ALTO** puede producir un resultado falso o perder algo de forma silenciosa; **MEDIO** produce pérdida o comportamiento raro pero con un aviso; **BAJO** no cambia ningún resultado y cuesta tiempo de diagnóstico.

## 6. Lo que queda abierto

### 6.1 El código de salida es honesto, y `build.bat` ya lo hace suyo

Se escribió abierto y se ha cerrado **a medias**, que es la parte honesta del titular. Los arreglos de `078b2aa` y `cf17e42` hacen que el programa salga con **1** cuando la medición no sirve, y `93c47cd` le añadió el **2** de *no medido*. Sus tres consumidores ya no hacen los tres lo mismo:

- La CI marca el paso `continue-on-error: true` y termina con `exit 0`. Está escrito que es a propósito, porque un rojo permanente por culpa del tiempo de una máquina es peor que no mirar. **Correcto para el 1. No para el 2**, que es un problema de entorno y no de lentitud, pero la CI sigue tragándose los dos.
- `build.bat` guarda `PERF_EXIT`, distingue sus cuatro clases (`[Info]` para 0, `[Warn]` para 1, `[Error]` para 2 y para cualquier código que el tool no use), y **sí propaga el fallo a su propio código de salida**. También falla cuando el test propio del cronómetro sale en rojo: un tool que no se autocomprueba ya no está vigilando nada, y dejarlo pasar es cómo una puerta deja de ser puerta sin que nadie lo diga.
- Solo quien lo lanza a mano lo lee.

La decisión que hace que el fallo llegue entero es **dónde** se devuelve. `PERF_FATAL` se acumula durante el bloque del cronómetro y el `exit /b 1` está en la cola de `:end`, no donde se detecta, porque los enlaces de junctions se montan después: salir ahí los saltaría y dejaría el árbol peor que uno que no llegó a empezar.

Lo que **queda abierto** es la CI, y no es un arreglo de código: un 2 —el binario de tests o el XML que se le apunto no estaba donde se buscaba— deja hoy la pipeline verde igual que un 1. Qué significa eso en un pipeline donde el resto de los pasos sí son puerta es una decisión de proyecto, y aquí no se ha tomado. Lo que sí es puerta ya, en todas partes, es la autocomprobación: `Self-check the Timing Tool` corre sin `continue-on-error` y `build.bat` ejecuta `test_duraciones_suite.mjs` antes de medir.

### 6.2 El límite de reloj era un tope nominal — CORREGIDO, ahora sale de la referencia

Se escribió abierto y se ha cerrado. El texto original se conserva al final.

**Lo que decía antes.** `LIMITE_MS = 20 * 60 * 1000`, 1200 s fijos. La última vuelta real medianía 153,1 s de suite y la referencia sumaba 301,516 s. Un límite casi ocho veces por encima del trabajo esperado no es un límite: es un tope nominal, y un cuelgue real se descubre por el reloj del pipeline, no por la herramienta.

**Lo que se ha hecho.** El límite de reloj se **deriva del total de la referencia**: cuatro veces. Con la referencia de ahora son 705 s, 11,8 min.

**Por qué cuatro.** Por una asimetría, que es el argumento entero:

- Equivocarse por lo **bajo** produce un cuelgue **falso**. Una máquina tres veces más lenta que la de la referencia no está colgada, está ocupada, y desde `078b2aa` eso sale con código 1 y con un mensaje de «la medición no ha terminado»: una alarma falsa en el sitio donde más se lee.
- Equivocarse por lo **alto** solo cuesta esperar.

Entre las dos, la que avisa antes de tiempo se equivoca por lo alto. Por eso cuatro y no dos.

**Lo que se ha arreglado de paso.** El límite **crece con la suite**, así que no hay techo. Con un tope fijo, una vuelta legítima más larga que el tope solo se explicaba porque el tope era enorme; ahora se explica porque la referencia está vieja, y el mensaje del corte lo dice con los dos números a la vista:

```
LA SUITE NO HA TERMINADO: no ha terminado en 0.3 min y se ha matado al agotar
del limite; la referencia dice que la suite tarda 5 s y el limite es ese total x4.
```

Eso convierte un corte opaco en una cuenta que se puede comprobar.

**Sin referencia** el límite son 1200 s, que es exactamente lo que ha sido siempre. No hay nada mejor que ese número, y cualquier otro sería inventado sin razón.

**Lo que sigue en pie.** El paso de medir de la CI **no tiene `timeout-minutes` propio**: hereda el del job, que en este workflow es el valor por defecto de Actions. Es decir que el límite del cronómetro es la única red contra un paso colgado, y ahora es más corta: 11,8 min en vez de 20. Si algún día se le pone un `timeout-minutes` a ese paso, tiene que ser **mayor** que el límite del cronómetro y no al revés, o el pipeline matará el proceso antes de que la herramienta pueda explicar por qué lo mató.

*Lo que decía antes, literal:* «`LIMITE_MS = 20 * 60 * 1000` (línea 127) son 1200 s. La última vuelta real midió 153,1 s de suite y la referencia suma 301,516 s. Un límite que está casi ocho veces por encima del trabajo esperado no es un límite: es un tope nominal. Derivar el límite de la referencia lo dejaría en un número con razón.»
### 6.3 El factor ×2 señalaba ruido por debajo de un segundo — CORREGIDO EN `78e238f`

Este punto se escribió abierto y se ha cerrado. Se conserva lo que se pensaba antes para que se vea la diferencia con lo que se midió después.

**Lo que decía antes.** `PISO_DE_INTERES_S = 0.05` era el suelo de interés, y la vuelta real de las 07:0x daba **7 regresiones**, todas entre 0,06 y 0,87 s: de 0,12 a 0,52, de 0,19 a 0,44, de 0,20 a 0,87. Duplicar el tiempo de un caso de 120 ms no es una regresión, y el acta de medición ya lo medía desde el otro lado: los casos que superan el factor 2 suman 0,1 s entre todos. Es decir: el guard era hipersensible justo en la franja donde no hay nada que ver.

**Lo que se ha medido para decidir el umbral, en vez de justificarlo.** La pregunta útil no era «¿son pequeños?» sino «¿se mueven solos?». Con el mismo ejecutable y el mismo código, dos vueltas seguidas, **7 de 939 casos salen como regresión comparados consigo mismos**. Ninguno se ha retrasado: se han movido. Y el reparto de ese ruido dice dónde está el límite:

| Franja | Ratio vuelta a vuelta | Cuántos duplican (×2) |
|--------|----------------------|------------------------|
| Por debajo de 1 s | mediana ×1,11 · p90 ×1,79 · **máx ×3,38** | **7 de 118** |
| Por encima de 1 s | mediana ×1,13 · p90 ×1,28 · **máx ×1,43** | **0 de 35** |

El factor ×2 no era ni demasiado sensible ni demasiado insensible: estaba **dentro** de la banda de ruido por debajo de un segundo y **fuera** de la de arriba.

**El arreglo.** Un umbral absoluto de 1 s además del factor: un caso solo es regresión si se ha duplicado **y** ahora tarda al menos un segundo. Sustituye a `PISO_DE_INTERES_S`, que queda sin uso porque el nuevo lo subsume — un caso de 0,2 ms está por debajo de un segundo igual que cualquier otro—. Con el umbral puesto, los 7 falsos positivos de la tabla desaparecen y **ninguno de los 35 casos de un segundo o más**.

**Que no se lleve la señal por delante.** El umbral mira **solo el tiempo de ahora**, no el de antes. Si mirase los dos, taparía el caso más grave: uno que pasa de 50 ms a 2 s ha ido de 0 a 2, y eso es una regresión aunque su ratio sea de 40.

Comprobado sobre datos reales, no sobre un ejemplo inventado. Se tomó un caso estable de la suite que hoy tarda 6,15 s y se fingió que antes tardaba 2,46 s —×2,5—: sale como regresión. El mismo truco sobre un caso estable de 0,22 s **no** sale, porque está por debajo del segundo. Y con la referencia intacta, 0 regresiones.
### 6.4 La referencia guarda rutas absolutas — REGENERADA, y con la máquina dentro

Este punto también se escribió abierto y se ha cerrado a medias. Lo que se dijo antes se conserva al final, porque la parte que sigue en pie es la que importa.

**Lo que se ha hecho.** La referencia se ha regenerado con la herramienta ya arreglada, y ahora lleva **la identidad de la máquina que la midió**: sistema, núcleo, memoria y versión de Node. Sin eso, una cifra de duraciones no se puede leer bien: dos vueltas seguidas de esta suite dieron ratios de hasta ×3,38 por debajo de un segundo, y un caso puede pasar de 0,02 s a 4,02 s entre vueltas sin que la suite haya cambiado.

Lo que **no** va dentro, a propósito: el nombre de usuario y la ruta del proyecto. Identifican a una persona, no hacen falta para saber si la máquina es la misma —sistema, núcleos y memoria bastan— y en un fichero commiteado acaban propagándose a los logs, a los artefactos y a los mensajes.

La comparación avisa cuando la máquina no es la misma, y es un **aviso y no un fallo**: medir en otra máquina es legítimo y se hace a menudo, y un rojo por eso costaría más de lo que informa.

**El `version: 1` NO sube.** Añadir un campo no cambia lo que significan los campos viejos, y la versión existe para recusar una base cuyos campos significan otra cosa. Subirla obligaría a regenerar cada referencia del mundo para poder seguir usándolas.

**Lo que sigue en pie, y es la parte que no se ha arreglado.** La referencia sigue guardando la ruta **absoluta** de cada caso. `claveDeFichero` lo mitiga para el emparejado —por eso §3.2 no se repite en otra máquina—, pero el fichero sigue sin ser portable: una comparación entre dos referencias de dos máquinas distintas seguiría sin poder emparejar. Arreglarlo de verdad es guardar `f` **relativo al repositorio**, que ningún lector actual necesita que sea absoluto. Es un cambio pequeño de código y grande de política, y por eso queda anotado y no hecho.

*Lo que decía antes:* 927 entradas con la ruta de la máquina que las midió, lo que la ata a un disco y a un usuario. `claveDeFichero` lo mitiga para el emparejado. Lo que **no** se arregla así es el resto: `construirBase` seguía grabando `f` tal cual. Regenerarlo exigiría un `f` relativo al repositorio, que es un cambio de formato — y el formato tiene `version: 1` a propósito para que un cambio así se pueda **recusar en vez de compararse**.
### 6.5 El ejecutable puede estar viejo y la herramienta no lo dice

Heredado del acta de medición y sin resolver: `build/Release/ABDAudioLab_Tests.exe` hay que reconstruirlo antes de medir, y la herramienta no comprueba que el binario sea posterior al código. Medir con un binario viejo mide el defecto que se iba a arreglar.

### 6.6 El reparto de códigos estaba en un banco de un solo uso — CORREGIDO

Se cerró en `78e238f`… no: se escribió abierto aquí, porque es lo que faltaba después de cerrar §6.1. Cerrar §6.1 dejó el build leyendo bien el código del cronómetro, pero el reparto —qué código significa qué cosa— vivía en un banco de pruebas de un solo uso, es decir en la cabeza de quien lo escribió y en el commit que lo explicaba. **Un contrato de cuatro filas que solo existe en un commit es un contrato que puede derivar sin que nadie se entere.**

Ahora está en [tools/test_build_bat_perf.mjs](../tools/test_build_bat_perf.mjs), y `build.bat` lo ejecuta antes de cronometrar. **42 aserciones.**

**Lo que hace y por qué no reimplementa nada.** Una copia de la lógica del reparto en JavaScript estaría en verde mientras el `.bat` hiciera otra cosa, que es justo el fallo que se quiere evitar. El test **extrae del `build.bat` commiteado** el bloque del cronómetro y la cola de `:end`, los pega en el mismo orden en que se ejecutan y ejecuta eso de verdad. Sustituye solo tres cosas, las que impiden que un banco corra solo:

| Lo que sustituye | Por qué |
|---|---|
| el `if not exist` del binario de tests | por uno que mira un señuelo, para que el test no dependa de que haya compilado nada |
| `node tools\test_build_bat_perf.mjs` | por `cmd /c exit N` |
| `node tools\test_duraciones_suite.mjs` | por `cmd /c exit N` |
| `node tools\duraciones-suite.mjs` | por `cmd /c exit N` |

La sección de junctions no se pega, y es a propósito: no toca `PERF_FATAL` y montarla crearía enlaces de verdad en el árbol.

**Lo que no es evidente.** El preámbulo del banco —`setlocal` y `set "PERF_FATAL=0"`— también se extrae del fichero real, no se escribe en el test. Si el banco se los pusiera él, seguiría en verde con el `set` borrado de `build.bat`, que es exactamente el cambio que este test existe para ver.

**Y el fallo se devuelve en la cola, que es la mitad del contrato.** El `exit /b 1` está en `:end`, después de los junctions. El banco pega la cola por eso, y comprueba que el mensaje de fallo sea la **última** línea que dice algo, no solo que esté en la salida.

**Lo que se ha demostrado que detecta.** Un test que solo dice que el reparto es el de hoy no demuestra nada, porque puede estar comprobando una copia de sí mismo. Un banco de mutación rompe el reparto de siete maneras distintas y exige que el test se ponga rojo en las siete, diciendo la aserción correcta y no una cualquiera:

| Mutación | Lo que se rompe |
|---|---|
| el 2 deja de fallar el build | un entorno roto vuelve a ser un build verde, en silencio |
| el 1 pasa a fallar el build | "tu máquina va lenta" se lee como "tu build está roto" |
| el `exit /b 1` sale de la cola | el build falla pero se salta los junctions |
| la rama del 2 no existe | un 2 cae al `else` y pierde el mensaje |
| el test del cronómetro en rojo no tumba el build | la puerta se queda muda |
| el test del reparto en rojo no tumba el build | el reparto puede derivar sin que nadie se entere |
| el `if not exist` del binario desaparece | el reparto se lee entero siempre, sin dispensa posible |

Las siete se ven. Y una mutación que **no** se veía, que es la razón de que el banco este en `build/`: apuntar el `if not exist` a un exe inexistente **no** cortocircuita nada, porque el banco sustituye ese `if` por el suyo. Lo que lo cortocircuita es que el `if` no esté. Corregir eso es lo que convirtió la séptima mutación de decorativa en real.

**Dos cosas que se han encontrado de paso.** El banco no detectaba nada porque Node entrecomilla un argumento que lleva barras y `cmd.exe` buscaba el fichero *con* las comillas dentro: `status` nulo y `stdout` vacío, que se leían como un fallo cualquiera. Y las ramas del `build.bat` se buscaban por una cadena armada a mano que no casaba, porque en batch el `if` cierra la comilla de la variable *antes* del `==`. Las dos se arreglan: con una comprobación de que el banco se ha ejecutado de verdad, y buscando las ramas por forma.

**Lo que sigue abierto.** El `build.bat` no se puede mutar en un banco sin tocar el fichero real, así que el test acepta `ABD_BUILD_BAT` para apuntarse a una variante. El gancho está en el propio test, visible, y avisa en voz alta cuando se usa. El banco de mutación está en `build/`, que está ignorado: es andamiaje para demostrar que el test funciona, no algo que se commitee.

### 6.7 El código de salida no decía QUÉ pasó — CORREGIDO, ahora hay una línea de veredicto

Es la laguna que quedaba abierta dentro de §6.1 y que solo se ve al escribir el reparto: **un número no dice qué pasó**. Hay dos `1` que no se parecen en nada —un test que se ha puesto lento, y el cronómetro que ha petado a mitad de un XML— y quien lee un `1` tiene que adivinar.

Lo peor no era teórico. **Un fallo no controlado en Node sale con `1`**, porque es el código que usa para lo que no captura, de modo que `build.bat` anunciaba literalmente `Suite timings: a slow test` con el cronómetro roto encima. Un fallo de herramienta anunciado como lentitud es la clase de mentira que un cronómetro no debería tener.

Ahora el cronómetro imprime **una línea de veredicto**, una sola, con prefijo fijo y el cuerpo en JSON:

```
ABD-VEREDICTO {"estado":"fallo-del-tool","codigo":2,"motivo":"EISDIR: illegal operation on a directory, read","error":"..."}
```

Va a **stdout** y no a stderr, porque lo legible por máquina que solo aparece cuando algo va a stderr no lo lee nadie. Y es la **última** línea que imprime el programa, para que un `tail -1` la encuentre.

**Los seis estados, y por qué son seis y no tres:**

| `estado` | Qué es | `codigo` |
|---|---|---|
| `ok` | se midió y no hay nada que decir | 0 |
| `lento` | se midió y hay casos por encima del umbral | 1 |
| `regresion` | se midió y hay casos volcados o ausentes frente a la referencia | 1 |
| `medicion-incompleta` | se empezó a medir y no hay resultado creíble | 1 |
| `sin-medir` | no se midió y no es culpa de la medición: el entorno | 2 |
| `fallo-del-tool` | el cronómetro falló | 2 |

`medicion-incompleta` y `sin-medir` están separadas a propósito: la primera es un problema de la medición y la segunda del entorno, y quien decide si algo tiene que ser puerta necesita saber cuál de las dos es.

**El arreglo que no ha hecho falta tocar el build.** Un fallo del tool sale con **2**, no con 1. `build.bat` ya hacía fatal el `2` porque `2` significa «no he medido nada», y eso es exactamente lo que ha pasado: no hay medición. El `try/catch` nuevo convierte la excepción en un veredicto con nombre, y el reparto por código —que ya estaba comprovado por tests— lo trata como fatal sin que haya cambiado una línea. Lo que **sí** ha cambiado en `build.bat` es que ahora **lee el estado** y lo dice: el `estado` no decide el exit, solo lo explica, de modo que el log dice cuál de las tres cosas que caben en un `1` es la que ha pasado.

**Por qué un solo punto de emisión.** `main()` devuelve un descriptor en vez de un número, y el envoltorio emite. Si `main` emitiera la línea, un camino que la emite y después falla al escribirla dejaría **dos** líneas, y dos líneas de veredicto no son ambiguas: son *ambiguas*, porque un `tail -1` se lleva la última y quien esté leyendo la otra no se entera. Con un único punto de emisión hay cero o una, por construcción, y eso no se puede comprobar con un test porque no se puede romper.

**Lo que ha costado encontrar: dos fallos del banco, no del cronómetro.** Al redirigir la salida del cronómetro a un temporal —para que `build.bat` pudiera leer el estado— su línea dejó de coincidir **exactamente** con la que el banco del reparto tenía en su tabla. El banco no la sustituyó, y por lo tanto **ejecutó el cronómetro de verdad**: cinco minutos por caso, seis casos. El test no se puso rojo: se colgó, y el motivo del cuelgue no era el reparto sino que el banco había dejado de ser un banco.

Las dos lecciones están en el banco, porque las dos costaron un rato:

| Lo que pasó | Lo que se hizo |
|---|---|
| la línea del cronómetro tenía redirección y `===` dejó de casar | emparejamiento por **prefijo** más separador, no por igualdad exacta |
| una llamada sin sustituir **se ejecuta** en vez de fallar | las que no se sustituyen **se nombran** en el mensaje del rojo |
| el banco tardaba 5 min y no decía nada | **timeout de 40 s por caso**, para que un cuelgue sea un rojo |
| dos tests a la vez se pisan `build/banco-perf-build` | cerrojo de instancia única por variable de entorno |

La última aserción de la tabla es la que más importa: **una llamada a node que no esté en `LLAMADAS_NODE` produce un rojo que la nombra**, no un cuelgue. Un test que se cuelga en vez de ponerse rojo es un test del que nadie se fía, porque no se sabe si falló o si solo va lento.

## 6.8. Las comprobaciones que solo avisaban, y el criterio de cuándo avisar es un fallo

El encargo era extender el tratamiento de códigos de salida —el de §6.1 y §6.7— al resto de comprobaciones de `build.bat`, que hasta aquí avisaban y nunca fallaban. Seis avisaban y ninguna fallaba. Cinco están ahora en la misma clase que el `2` del cronómetro, y la sexta se queda avisando a propósito.

**El criterio, que es lo que hace falta para no pasarse de lejos: no poder hacer la comprobación es fatal; que la comprobación se haga y su respuesta sea «no» se avisa y se sigue.** La primera significa que el build no puede afirmar nada sobre un path que no ha mirado, y un build que afirma sin mirar es el mismo fallo que un guard que miente en verde. La segunda significa que el build ha preguntado y la respuesta es negativa, y esa respuesta es un estado legítimo del repositorio: el guion tiene algo que decir y lo dice.

| Comprobación | Antes | Ahora | Por qué |
|---|---|---|---|
| `perf` sin binario de tests | `[Warn]` y sigue | `PERF_FATAL=1` | Se pidió medir y no hay nada medido. El fallo de compilación ya se había impreso justo encima |
| `perf` sin node en el PATH | `[Warn] se salta el cronómetro` | `PERF_FATAL=1` | En `perf` el cronómetro no es *una* comprobación más: es la razón del build. Un build normal no llega a ese bloque, así que «sin node se salta» sigue siendo cierto para quien no ha pedido medir |
| `git` no responde al preguntar si un path está versionado | `[Aviso]` y sigue | `BUILD_FATAL=1` | La pregunta era la que protege el path versionado, y no se ha podido hacer. El enlace tampoco se crea, que es lo correcto: lo que no se sostiene es salir con verde |
| `mklink` falla | `[Aviso] se sigue con una copia vacía` | `BUILD_FATAL=1` | El mensaje anterior mentía: si no hay enlace no hay copia, ni vacía ni de otro tipo. Con los assets sin enlazar, todo lo que los mide compara contra nada |
| El path vigilado ya es una junction | `[Aviso]` y sigue | `BUILD_FATAL=1` | El junction existe y el aviso es cierto; lo que no puede ser es el verde. Todo lo que vigila esa ruta se compara consigo mismo |
| **`Rastreado=SI`: git dice que hay ficheros dentro | `[Aviso]` y sigue | **`[Aviso]` y sigue** | **Esta se deja como estaba, y es la que prueba que el criterio no es «todo pasa a fatal»** |

Sobre la última fila, que es la única decisión de criterio y no un descuido: cuando git responde «sí, hay 40 ficheros versionados aquí», ha contestado **completo** a la pregunta que se le hizo. El guion existe para no sustituir con una junction un path que git vigila, y lo hace —no enlaza y dice por qué—. Fallar ahí convertiría en error de build un checkout normal en el que esas copias existen a propósito. La respuesta negativa no es un fallo de la comprobación: es la comprobación funcionando.

**El fallo que esto destapó, y que es de la misma familia que todos los de esta acta: el acumulador nacía debajo de sus propios `call`.** `set "BUILD_FATAL=0"` se había puesto junto a `PERF_FATAL`, en la línea 119. Los `call :avisarSiEsEnlace` y `call :crearEnlaceSiProcede` que mueven ese acumulador estaban en las líneas 77-80, **cuarenta líneas antes**. Todo `BUILD_FATAL=1` se borraba en la 119 y el guard, recién escrito, era decorativo: exactamente el «guard que miente en verde» que esta sección describe en los demás. El acumulador se ha subido a la línea 80, justo antes del `if` que contiene los `call`, que han pasado a ser las líneas 84-87.

El comentario que justificaba la posición antigua afirmaba que «los enlaces de la seccion de junctions se montan DESPUES del cronómetro», y es justo al revés: se montan antes. Esa frase es lo que inducía a colocar el acumulador donde no tocaba, así que también se ha corregido.

**Y la aserción que tenía quearlo daba verde.** Comparaba la posición del `set` con la **definición** de la etiqueta (ahora la 367), que siempre cae después, en vez de con el **`call`** que la usa. Con el acumulador en su sitio malo daba verde. Ahora compara con la última línea que ejecuta esas subrutinas, y se ha comprobado que muerde: devuelto el acumulador a la línea 119, la aserción se pone roja.

**Los dos guards, probados con el texto real y no con una paráfrasis.** Un banco ejecuta la subrutina tal cual está en `build.bat`, extraída del fichero, y exige el código de salida:

| Entrada | Antes | Ahora |
|---|---|---|
| `PATH` sin git, `contracts/hardware` sin enlazar | verde | **exit 1** |
| El path vigilado ya es una junction | verde | **exit 1** |

Dos cosas de batch que el banco tuvo que aprender, y que están escritas porque las dos son trampas que se cobran un `exit=0` que parece verde: un `goto :eof` **fuera de un `call`** termina el script entero y se come la cola, así que las subrutinas tienen que ir **al final** del `.bat` generado; y un comentario `::` no es una etiqueta, aunque empiece por `:`. El primer banco fallaba con `exit=0` y ningún veredicto, y era el banco el que estaba mal.

## 6.9. El enlace con la ruta vacía, y lo que había detrás de él

El aviso era `[Error] Could not create the link to .` —con un punto y nada detrás—, y salía en **cada** ejecución, con cualquier modo. En `perf` se veía más porque el bloque del cronómetro es lo último que se ejecuta y es lo que se está mirando en ese momento.

**La causa no era una llamada mal formada. Era que no había ninguna llamada.** El flujo principal terminaba en la línea 356, el `)` que cierra el bloque del cronómetro, y detrás no había ni un `goto` ni un `exit /b`: solo comentarios y, cuatro líneas más abajo, la etiqueta `:crearEnlaceSiProcede`. Batch no distingue *llamar* de *continuar*, así que el flujo principal **cayó dentro de la subrutina**. Dentro ya no hay un `call` delante, de modo que `%1` es el modo del build y `%2` y `%3` están vacíos: en `perf`, `%1` vale `perf` y los otros dos no existen.

A partir de ahí la subrutina se ejecuta con argumentos que nadie le pasó, y hace exactamente lo que se le pedía hacer con ellos:

| Línea | Qué pasa con `%~2` vacío |
|---|---|
| `if exist "%~2" goto :eof` | `if exist ""` es **falso**, así que la salida temprana **no** dispara |
| `git ls-files -- "%~1"` | con `%~1` = `perf` no lista nada, `Rastreado=NO` |
| `mklink /J "%~2" "%~3"` | `mklink /J "" ""` falla, y el mensaje es `Could not create the link to .` |

El detalle que lo hace tan difícil de leer es que la salida temprana de la línea 399 es justamente la que protege de este camino: con un destino real se sale en la primera línea, y con la ruta vacía no. Un `if exist` que protege el resto del script pero no su propia entrada.

**Lo que había detrás era peor que el ruido.** El `goto :eof` del final de esa subrutina, sin un `call` que lo contenga, no devuelve a quien llama: **termina el script entero**. Con él se iban la cola de `:end` y los dos guards, que es donde vive el `exit /b 1`. Medido antes del arreglo, con `PERF_FATAL=1` puesto a mano y el flujo principal llegando al final: el script imprimía los dos `[Error]` del enlace, se callaba y salía con **0**, sin imprimir nada de la cola.

Es decir que todo lo de §6.1 y §6.8 —los códigos de salida, el 2 del cronómetro, los guards de junctions— estaba escrito y comprobándose sobre una cola que el script real no ejecutaba nunca. Los tests lo decían sin saberlo.

**Por qué el banco no lo veía, que es la parte que conviene quedarse.** `tools/test_build_bat_perf.mjs` no ejecuta `build.bat`: extrae el preámbulo, el bloque del cronómetro y la cola, y los reensambla en un `.bat` propio. Ese montaje es exactamente el layout correcto —flujo, salto, subrutinas, cola— que es lo que faltaba en el original. El banco era una especificación de cómo debería ser el fichero, y una especificación no puede detectar que el fichero no la cumple.

**El arreglo es un `goto :end`** al final del flujo principal, con el motivo escrito al lado para que nadie lo borre por parecer muerto: no lo está. Va antes de la etiqueta y no un `exit /b` al final del fichero, porque `:end` tiene que seguir siendo alcanzable —es la cola que devuelve el fallo—.

**La aserción que lo fija** no ejecuta nada, porque no hay nada que ejecutar: mira el texto. Toda etiqueta de subrutina tiene que tener delante una línea ejecutable que salte (`goto` o `exit /b`); comentarios, `rem` y blancos no cuentan, porque batch se los salta. Se ha comprobado que muerde: quitando el `goto :end` de una copia, se ponen rojas dos.

## 7. Verificación

| Comprobación | Resultado |
|---|---|
| Test del tool antes de esta auditoría | 65 aserciones |
| Test del tool con §6.3 | **99 aserciones, todas en verde** |
| Test del tool después | **91 aserciones, todas en verde** (+26) |
| `--base otra.json` entre lo que recibe Catch2 | antes sí → **después no** |
| Renombrado entre máquinas: `nuevos` / `ausentes` | antes `1 / 1` → **después `0 / 0`** |
| Renombrado que se multiplica por cuatro | antes 0 regresiones → **después 1, con su tiempo anterior** |
| Sustitución de una referencia existente | la nueva se lee entera, sin temporales |
| Medición real, sin cambios de comportamiento | 936 casos, 9 nuevos, salida con 0 |
| §6.3: falsos positivos del factor ×2 en dos vueltas del mismo código | 7 de 939 → **0** con el umbral de 1 s |
| §6.3: señal intacta, caso real de 6,15 s fingido a ×2,5 | **sale como regresión** |
| §6.3: ruido intacto, caso real de 0,22 s fingido a ×2,5 | **no sale** |
| Suite completa tras §6.3 | **939 casos, 890 pasados, 0 fallos, 49 saltados**, 2 m 56 s |
| §6.1: `build.bat` con el cronómetro en 0 / 1 / 2 / 7 | **exit 0 / 0 / 1 / 1** |
| §6.6: `test_build_bat_perf.mjs` | **42 aserciones, todas en verde** |
| §6.1: `build.bat` con el test del cronómetro en rojo | **exit 1**, y sin anunciar una medición que no pidió |
| §6.6: el mismo test con el test del reparto en rojo | **exit 1**, y sin anunciar una medición |
| Fichero de la herramienta | 554 líneas → 964 |
| §6.6: mutaciones del reparto que el test NO ve | **7 mutaciones → 7 en rojo** |
| §6.7: `test_duraciones_suite.mjs` con el contrato del veredicto | **160 aserciones**, +41 |
| §6.7: `test_build_bat_perf.mjs` tras el arreglo del banco | **54 aserciones**, y 43 s con una llamada sin sustituir |
| §6.7: un fallo del cronómetro (`EISDIR` a media lectura) | antes `1` leido como lentitud → **ahora `2`, `fallo-del-tool`** |
| §6.6: `build.bat` intacto tras el banco de mutación | **sí** |
| §6.8: `test_build_bat_perf.mjs` con los guards de junctions | **66 aserciones**, +12 |
| §6.8: comprobaciones que solo avisaban y ahora fallan | **5 de 5**, y 1 que sigue avisando a propósito |
| §6.8: junctions con `git` mudo, sobre el texto real de `build.bat` | antes **verde** → ahora **exit 1** |
| §6.8: el path vigilado ya es una junction | antes **verde** → ahora **exit 1** |
| §6.8: el acumulador debajo de sus `call` (fallo encontrado al revisar) | aserción nueva **roja**; con el acumulador en su sitio, verde |
| §6.8: mutaciones del reparto tras mover el acumulador | **7 mutaciones → 7 en rojo** |
| §6.8: `test_duraciones_suite.mjs`, sin cambios en el tool | **160 aserciones**, en verde |
| §6.9: el flujo principal caía dentro de `:crearEnlaceSiProcede` | `[Error] Could not create the link to .` en cada build |
| §6.9: con `PERF_FATAL=1`, a mano y llegando al final | antes **exit 0** y sin cola → ahora **exit 1** con los dos `[Error]` de la cola |
| §6.9: `test_build_bat_perf.mjs` con la regla de alcance | **69 aserciones**, +3 |
| §6.9: esa aserción quitando el `goto :end` de una copia | **2 en rojo** |

## 8. Commits

| Commit | Qué cierra |
|--------|-----------|
| `078b2aa` | El fallo abierto: resultado del spawn, cierre del documento, cuenta de casos, ausentes con código 1 |
| `cf17e42` | Los cuatro hallazgos de esta acta |
| `78e238f` | §6.3: el umbral absoluto de 1 s. **Commit del hilo paralelo**: recogió los cambios de este acta que estaban sin commitear, así que el asunto es suyo y no lleva cuerpo ni trailer `Workstream-Origin`. Los ficheros son los de WS-1 y el cambio es el descrito en §6.3. |
| `93c47cd` | `build.bat` distingue el código 2 de entorno del 1 de rendimiento, y fija el contrato en la cabecera del tool |
| `4b9199c` | §6.1: `build.bat` falla el build con un 2 y con un código que el tool no usa, y solo avisa con un 1. **Commit del hilo paralelo**: recogió los cambios de este acta que estaban sin commitear en el árbol de trabajo junto a su propio arreglo de `MockAudioEngine`, así que el asunto es suyo y no lleva cuerpo ni trailer `Workstream-Origin`. El fichero es el de WS-1 y el cambio es el descrito en §6.1. |
| este commit | §6.6: el reparto de códigos pasa de un banco de un solo uso a `tools/test_build_bat_perf.mjs`, y `build.bat` lo ejecuta antes de cronometrar (42 aserciones). Con `*.bat text eol=crlf` en `.gitattributes`, que evita que el diff del build.bat sea el fichero entero cada vez que se toca |
| este commit | §6.7: el cronómetro imprime una línea de veredicto legible por máquina (seis estados, un solo punto de emisión), un fallo del tool sale con 2 en vez de 1, y `build.bat` lee el estado. El banco del reparto deja de colgarse: emparejamiento por prefijo, las llamadas sin sustituir se nombran, timeout de 40 s por caso y cerrojo de instancia única |
| este commit | §6.8: cinco comprobaciones de `build.bat` que solo avisaban pasan a fallar el build, con un acumulador propio para las junctions y otro para el cronómetro. El acumulador nuevo ha nacido 40 líneas por debajo de los `call` que lo mueven — lo ha detectado la revisión de este commit, no el test, cuya aserción miraba la definición de la etiqueta en vez del `call` — y esa aserción está corregida. Se deja sin hacer fatal `Rastreado=SI`, que es la comprobación contestando de verdad |
| este commit | §6.9: el flujo principal se caía dentro de `:crearEnlaceSiProcede` sin un `call` delante, de modo que `%2` y `%3` llegaban vacíos y `mklink /J "" ""` imprimía un enlace con la ruta vacía en cada build. Detrás venía el `goto :eof` de esa subrutina, que sin `call` termina el script y se llevaba la cola de `:end`, o sea el `exit /b 1` de los guards de §6.1 y §6.8. Añadido el `goto :end` que faltaba, y una aserción que prohíbe que una etiqueta de subrutina se alcance por caída |

Al auditar se ha encontrado modificado `contracts/hardware/abdeep_modulation_matrix.json` y `.github/workflows/audio-ab-5d-ci.yml`, que reescriben respectivamente una ruta de `provenance` y algo del workflow. **No son de este trabajo y no se han tocado**: el hilo paralelo está tocando el repositorio a la vez.

Y por el mismo motivo pasó con `78e238f`: el hilo paralelo commiteó los cambios de §6.3 que estaban sin commitear en el árbol de trabajo. El contenido es correcto y el asunto no engaña —*refine absolute suite duration threshold* describe exactamente el umbral absoluto—, pero el commit no lleva cuerpo ni atribución. Queda anotado aquí en lugar de reescribir historia ajena.
