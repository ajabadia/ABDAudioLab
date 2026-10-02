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

## 6.10. `Measurement UI Panels`: de 0,02 s a 4,02 s, y por qué no lleva guard

La pregunta era si ese salto entre vueltas era una regresión del caso. No lo es, y conviene dejar escrito por qué, porque el número `4,02` es el que aparece en §6.1 y en el propio comentario de `identidadDeLaMaquina()` como la razón de meter la máquina en la referencia. Es el mismo hecho mirado desde el otro lado.

**Lo que hace el caso.** `Measurement UI Panels: Safe construction and lifecycle draining` son dos secciones que construyen un panel y lo destruyen. No hay E/S, ni esperas, ni bucles de mensajes, ni dispositivo de audio: `MeasurementAudioPlayerComponent` solo crea un `AudioThumbnail`, un `TransportSource` y un temporizador de 25 Hz. Lo único dinámico es el `juce::ThreadPool(4)` de la sesión, cuyo destructor tiene un tope de `removeAllJobs(true, 3000)`. **Ese tope no se alcanza aquí**, porque en este test no se encola ningún job: `addJob` solo se llama desde la carga de contenedores, y este caso no carga ninguno. No hay, en el código recorrido, nada que pueda costar cuatro segundos.

**Las cinco mediciones que hay en el disco, todas del mismo caso:**

| Medición | Caso | Sección larga |
|---|---|---|
| referencia (`duraciones-referencia.json`) | **0,016 s** | — |
| `%TEMP%/dur.xml`, 10-01 18:27 | 0,020 s | 0,015 s |
| `build/duraciones-medicion.xml`, 10-02 00:55 | 0,014 s | 0,011 s |
| `build/medicion-actual.xml`, 10-02 07:53 | 0,016 s | 0,012 s |
| `build/medicion-actual2.xml`, 10-02 07:58 | **0,033 s** | 0,027 s |
| `build/medicion-final.xml`, 10-02 08:17 | 0,016 s | 0,013 s |

El rango es 0,014–0,033 s. **No hay ningún 4,02 s en ningún sitio**, ni en las mediciones ni en la historia de la referencia, que solo tiene dos commits. En la corrida más lenta que hay (`dur.xml`, 301,5 s de suite frente a los 176,4 s de la referencia, con 26 regresiones) este caso midió 0,020 s: no se movió.

**Qué es el 4,02 entonces.** Una medida de la máquina. La suite entera de esa vuelta iba 1,7 veces más lenta, y un caso de 16 ms con la maquina ocupada es la primera enmagnificarse, porque su ratio es el mayor de todos aunque su coste absoluto sea de los más bajos. Eso es justo lo que mide el campo `maquina`: que `4,02` no dice nada del test.

**Por qué no se ha añadido un guard, y no por pereza.** El guard ya existe y ya dispararía. La regla es: se salta el caso si `despues < 1 s`, y con lo que queda se marca si `ratio >= 2`. Con 4,02 s sobre una referencia de 0,016 s son **x251**, y el informe lo escribiría tal cual. Añadir un umbral propio para este caso sería añadir una segunda regla que dice lo mismo con un número peor.

**Y este caso es, de hecho, el ejemplo de por qué el suelo de 1 s existe.** `medicion-actual2` da 0,033 s contra 0,016: **x2,04**, por encima del factor. No se marca, y está bien que no se marque: está por debajo del segundo, y §6.3 midió que por debajo de 1 s la dispersión entre vueltas llega a x3,38. Un umbral propio para este caso tendría que ser o bien menor que 0,033 s —y entonces marcaría ruido— o bien mayor que 4 s —y entonces no marcaría nada—.

**Un hallazgo real que sale de mirar, y que no se toca.** `~MeasurementComparisonSession` guarda `DrainTimedOut` y lo notifica, y en la línea siguiente guarda `Destroyed` y lo notifica también: el estado final es siempre `Destroyed`, así que `SessionShutdownState::DrainTimedOut` no lo puede observar nadie. No es lo que se preguntó y no se ha cambiado; se anota porque el próximo que lea ese enumerado va a contar con un estado que no existe.

## 6.11. La autocomprobación del cronómetro sale con código propio, y el 1 sigue siendo solo aviso

El reparto de códigos de §6.1 tenía un hueco: las dos autocomprobaciones —el test del reparto y el del cronómetro— marcaban el mismo `PERF_FATAL` que el resto de fallos, de modo que un rojo de cualquiera de ellas salía con **1**, igual que una junction que no se pudo verificar o que un cronómetro que no produjo medición. Tres cosas distintas, un solo código.

**El cambio.** La rama del cronómetro en rojo marca ahora un acumulador propio, `SELFTEST_FATAL`, y la cola tiene su propio guard con **`exit /b 3`**. El guard va **primero**, y no por orden de gravedad: es el único de los tres que significa que *las otras respuestas no son de fiar*. Si el tool no pasa sus propios tests, cualquier medición que hubiera dado queda sin comprobar, y un 1 ahí diría «no he medido», que es cierto y no es lo que hay que arreglar.

**Por qué 3 y no el 1.** Porque el 1 de este build significa «un chequeo no se ha podido hacer», y eso sigue siendo exactamente lo que dice el 1: junctions que no se verificaron, o una medición que no se produjo. El 3 es otra clase: el instrumento está roto, no el build. Son dos razones distintas para mirar cosas distintas, y quien solo pueda mirar el exit debería poder elegir.

**Lo que NO se toca, y es la otra mitad del encargo.** El **1 del cronómetro** —suite lenta, regresión contra la referencia, una vuelta que no terminó— sigue siendo un aviso y el build sale con **0**. No es que se haya decidido aquí: es que es la fila `ncrono: 1, build: 0` de la tabla de casos, y ahora hay una aserción que la lee de ahí y falla si alguien la mueve. Ese 1 es una medición que **existe** y que dice algo malo; fallar por él sería convertir el cronómetro en el aviso que nadie escucha, que es exactamente lo que este build evita desde §6.1.

**El test del reparto se queda en 1, y es una decisión visible, no un olvido.** Es otra clase: el test del cronómetro falla cuando **el cronómetro** no funciona, y el del reparto cuando falla la **comprobación de este build**. Lo primero es un instrumento roto; lo segundo es un chequeo de este build en rojo, que es lo que el 1 ya dice. Pasar el segundo a 3 también sería defendible, y es el siguiente paso natural si algún dia molesta — pero no se ha hecho porque no se ha pedido, y porque el código 3 significa una cosa concreta.

**Las aserciones.** Ocho nuevas, y dos de ellas son las quesostienen el encargo: que la rama marque `SELFTEST_FATAL` y no `PERF_FATAL`, y que el 1 del cronómetro siga saliendo con 0. La primera se busca por el **mensaje** de la rama y no por el nombre de la variable, porque con el nombre sería tautología. Se ha comprobado que muerde con dos mutaciones:

| Mutación | Aserciones en rojo |
|---|---|
| La rama vuelve a marcar `PERF_FATAL` | **3** |
| La cola sale con 1 en vez de con 3 | **4** |

El `exit /b 3` aparece **una sola vez** en el fichero, y hay aserción para eso: un código repetido en dos sitios no es un código, son dos reglas que alguien va a cambiar por separado.

## 6.12. La referencia guarda la ruta relativa al repositorio, y el formato pasa a 2

La referencia guardaba en cada caso la ruta **ABSOLUTA** que le da Catch2, y con ella dentro iban el disco, el proyecto y el usuario: `D:/desarrollos/ABDSynths/ABDAudioLab/src/tests/...`. Tres cosas que no son las mismas en otra máquina y que no dicen nada del test. Y la referencia está **commiteada**, así que el efecto era concreto: dos personas con el mismo código generaban dos ficheros distintos, y el diff de la referencia decia cosas que no eran cambios de tiempo.

**Ahora guarda la ruta relativa al repositorio**, con `/`. De los 939 casos, **875** quedan como `src/tests/...` y **64 como `../ABDSharedCode/...`**, que son los del repo hermano y no son un caso hipotético: son casi siete de cada cien. Los que no caen ni en el repo ni en su hermano **se dejan como venían**, porque ahí no hay ruta relativa que diga la verdad y fabricar una sería peor que guardar la entera. Lo que se fue de la referencia: **39.353 bytes**, todos de disco y de usuario.

**La versión sube a 2, y esta vez sí.** La regla que el propio tool tenía escrita es que la versión sube cuando un campo cambia lo que **significa**, no cuando aparece uno nuevo — por eso `maquina` no la subió y no obligó a regenerar nada—. Aquí `f` cambia de significado: la misma cadena que antes era una ruta absoluta ahora es `../ABDSharedCode/...`. Comparar una base vieja con una nueva daría diferencias que no existen, que es justo para lo que existe el número.

**La transición.** Una base de formato 1 **se recusa**: no se compara, y la vuelta siguiente regenera la referencia. Lo que se ha añadido es que **lo dice**. Antes un `return null` sin más, y el silencio ahí se lee como «no hay referencia», que es una cosa distinta de «la hay y no se puede leer»: se arreglan de forma distinta, y quien regenera sin querer no tenía forma de saber qué había pasado. Ahora el aviso nombra el formato que tenía, el que escribe este tool, y que se recusa. Está comprobado **por fuera**, ejecutando el programa, porque por dentro `leerBase` devuelve `null` y no se ve si ha dicho nada.

**Lo que la migración NO toca, a propósito.** Ni los segundos ni `medidoEn`. La medición se hizo el día que dice que se hizo; cambiar su fecha para que cuadrase con el formato nuevo sería mentir sobre el momento de la medición, que es exactamente el dato que hace falta para leer una cifra de duraciones. Lo único que ha cambiado es cómo se **escribe** la ruta, y eso lo hace el propio tool con su propia función, no un script aparte: una copia de la regla en un script de un solo uso es una segunda regla.

**La comparación no se entera.** `claveDeFichero` sigue quedándose con el nombre del fichero, y el motivo de fondo no cambia: comparar por ruta entera haría que un test movido de carpeta saliese a la vez como nuevo y como ausente, dos avisos para un solo test. Lo que cambia es su premisa, y eso está escrito en su comentario.

Comprobado contra una vuelta real: **939 casos medidos contra 939 de la referencia**, sin recusar nada y con veredicto normal.

## 6.13. Los 301 s y los 176 s: por qué el total se movió tanto, y por qué no fue el portapapeles

La pregunta era si la bajada del total de la suite —de 301,5 s a 176,4 s— venía del commit `8a19dd9`, que quita `SystemClipboard` de dos tests, o de ruido de medición. **Es ruido de medición, y el portapapeles no tiene nada que ver.**

**Lo que aporta el commit del portapapeles, medido.** Son los cuatro datos que lo desmontan:

| Caso | Vuelta lenta | Vuelta rápida | Ahorro |
|---|---|---|---|
| `Smoke Test Paso 4 (UI): Recorrido Automatizado` | 0,26 s | 0,10 s | 0,16 s |
| `Smoke Test Paso 4 (UI): Recorrido Manual Analógico` | 0,42 s | 0,10 s | 0,32 s |
| `ST-99: AutomatedMidi campaign` | 6,07 s | 6,07 s | 0,00 s |
| `ST-100: ManualOperator campaign` | 6,32 s | 6,21 s | 0,11 s |

**0,48 s de 125,5: el 0,4 %.** Y hay un detalle que lo hace más claro: la otra mitad de ese commit **subió** timeouts (`8000` → `15000` ms, y el tope del bucle de ST-100 de 200 a 500 vueltas), y ST-99 y ST-100 no se movieron. Esos timeouts nunca se alcanzan; se tocaron por si acaso, no porque fueran a fallar.

**Y lo decisivo: el frenado no está donde se ha tocado el código.** Agrupando por fichero de origen, y separando lo que `c6010cd` tocó de lo que no tocó nada:

| | Ficheros | Casos | Mediana de sus medianas |
|---|---|---|---|
| `test_MeasurementFloatingWindow.cpp`, **tocado** | 1 | 1 | **×2,04** |
| Todo lo demás, **sin tocar** | 196 | 926 | **×1,34** |

Ciento noventa y seis ficheros que nadie tocó se frenaron por igual. Y hay un dato que cierra la puerta del todo: **hay ficheros intactos que se ACELERARON** —`test_TargetProfileLegacyConsumerParity.cpp` a ×0,47, `test_Parity01GuidedVsClassic.cpp` a ×0,54, `test_AudioABTolerancePolicy5D.cpp` a ×0,55. Un cambio de código no acelera un fichero que no ha tocado, y menos por la mitad. Eso solo lo hace la máquina, o las condiciones en las que se midió.

**La forma del conjunto lo dice también.** La mediana de los 927 casos comunes es ×1,31, el p75 ×2,62 y el p90 ×9,89; el 32 % va a ×2 o más y el 62 % a ×1,1 o más. Los cinco casos con más delta se llevan 37,4 s —el 30 % de los 125— y no tienen relación entre sí: una ventana flotante, un informe HTML, un panel de medición, un benchmark de sintetizador y un modelo de latencia. Un cambio de código hace un delta **localizado**; esto es un factor común con cola, que es la firma de una máquina ocupada. Y esa vuelta fue la que el cronómetro marcó con **26 regresiones** repartidas por tests sin relación entre sí.

**El reparto de los 125,5 s**, sobre los 927 casos comunes —los 12 casos nuevos de la vuelta rápida suman 0,3 s y no mueven nada—: los cinco primeros se llevan 37,4 s, y aplicar el factor de la mediana a todo explica unos 71 s. El resto queda en la cola, en casos sueltos.

**Lo que esto no es.** No es una medición mejor ni peor: la vuelta de 176,4 s es la que está en la referencia, y se ha vuelto a medir tres veces con 150,0, 163,1 y 176,4 s. El 18 % de dispersión **entre tres vueltas de la misma mañana** es el mismo fenómeno que esto, y es la razón por la que §6.3 puso el suelo absoluto de 1 s y por la que la referencia lleva el campo `maquina`. Esta sección no añade ninguna regla: confirma las dos que ya había.

**Lo que no se puede cerrar con los datos que hay.** Solo existe **una** vuelta anterior al commit del portapapeles, así que no se puede decir en qué minuto exacto cae el total. Da igual para la pregunta que se hizo —el commit responde por el 0,4 % y el resto no está donde se tocó código—, pero fechar la caída exigiría una vuelta más con el código viejo.

## 6.14. El ruido de la máquina, medido al regenerar y guardado en la referencia

El suelo absoluto de 1 s de §6.3 se decidió con unas cifras escritas **a mano** en el comentario de la constante: *«por debajo de 1 s: mediana ×1,11, p90 ×1,79, máximo ×3,38»*. El razonamiento era correcto y §6.13 lo confirma con tres vueltas, pero el número tenía la vida de un comentario: dentro de un año seguía diciendo ×3,38 con la máquina cambiada detrás, y nadie podría saber si el umbral seguía teniendo razón.

**Ahora el ruido se mide y se guarda en la referencia.** `--guardar-referencia` corre la suite **DOS VUELTAS**: la primera da los tiempos que se guardan y la segunda contrasta con el mismo ejecutable y el mismo código. La diferencia entre las dos es el ruido de la máquina, y va **dentro** del fichero de referencia, en el bloque `ruido`, entre `maquina` y `casos_`.

**Cómo se mide cada caso.** El factor es **la vuelta peor partida por la mejor**, no al revés: el ruido que importa es el que hace un caso *parecer peor* de lo que es, que es el que pone un falso rojo. Con la vuelta buena en el numerador el número grande sería el del caso más rápido medido nunca, que no dice nada. La simetría se ha comprobado: `ruidoDe(A, B)` y `ruidoDe(B, A)` dan exactamente los mismos factores, porque si dependieran de cuál se guarda, la misma medición daría dos ruidos distintos y solo uno estaría en la referencia.

**Las dos bandas, y por qué están partidas por el umbral y no por la mediana.** Un único número no dice ninguna de las dos cosas que hay que decir:

| Banda | Qué es | Para qué está |
|---|---|---|
| `bajoUmbral` | casos que en **ninguna** de las dos vueltas pasaron de 1 s | si su ruido está **dentro** del factor ×2, el umbral de 1 s está haciendo falta |
| `sobreUmbral` | casos que en **ninguna** de las dos vueltas bajaron de 1 s | si su ruido **llega** al factor ×2, el margen se ha perdido y un caso puede ponerse rojo solo |

Los que **cruzan** 1 s de una vuelta a la otra no cuentan en ninguna de las dos, y se dicen cuántos son. Meterlos en la de abajo sería justo la confusión que el umbral absoluto existe para evitar: un caso que pasa de 0,4 s a 1,2 s no es un caso rápido con ruido, es uno que ha pegado un salto.

**Lo que se guarda, y lo que no.** Ni `medidoEn` ni los segundos se tocan, igual que en §6.12. **El ratio de cada caso NO se guarda**: es lo que más información tiene y lo que más pesa —casi 25 KB en un fichero commiteado— y **nada lo lee**, porque la comparación decide con el tiempo de ahora y no con el ruido de un caso concreto. Un número sin consumidor es un número que nadie sabrá reexplicar dentro de un año.

**El formato sigue en 2, y esta vez es el caso interesante.** La regla escrita en el tool es que la versión sube cuando un campo cambia lo que **significa**, no cuando aparece uno nuevo. `ruido` es un campo que **sí se lee** —`resumenRuido` lo enseña al comparar—, pero lo que decide es si una referencia a la que le falta se puede comparar con las demás: **sí**, porque lo único que `ruido` describe es la máquina y las duraciones de los casos se leen igual. Una referencia vieja se compara exactamente igual y además avisa de que no dice cuánto se movía la máquina, que es lo que hay que decir en ese caso.

**Lo que sale en cada comparación.** El bloque va pegado al de la máquina, y termina con el veredicto sobre el umbral, en las dos direcciones:

- factor **dentro** del ruido de los casos de menos de 1 s → *«el umbral está haciendo falta»*.
- ruido **menor** que el factor en esa banda → *«el umbral de 1 s está más alto de lo que el ruido de esta máquina pide»*.
- ruido por encima de 1 s **llegando** al factor → *«el margen se ha perdido, y un caso puede ponerse rojo solo»*.

Un aviso que sale aunque la comparación esté en verde es justo lo que hace que una constante sea auditable en vez de creída.

**Con `--xml` no hay segunda vuelta, y la referencia se guarda sin el.** Un fichero no se puede volver a medir. No se **recusa** la referencia por eso: perder un dato medido por otro que no se ha podido medir sería tirar el bueno, y lo único que se pierde es el ruido, que se puede medir otro día. Se guarda sin el campo —sin `null`, porque una clave con `null` se lee como «se midió y no se movió nada», que es una afirmación— y **avisa** de que se ha guardado sin él.

**Lo que NO se ha podido hacer todavía: el número.** La regeneración exige dos vueltas enteras, y **la suite que hay en el árbol hoy no termina**. Dos vueltas consecutivas, mismo binario (12:24, sin cambios durante la medición), dos muertes distintas:

| Vuelta | Qué pasó | Qué dijo el cronómetro |
|---|---|---|
| 1.ª | **SIGSEGV** en `test_MeasurementFloatingWindow.cpp:108`, seccion *«el constructor se queda el contenido y la geometria que le pasan»*, cortado a los **585 de 939** casos | `medicion-incompleta`, código 1, *«ha terminado de forma anormal, con codigo 3221226525»* |
| 2.ª | muerte temprana, **XML sin cerrar** antes de los 100 s | `medicion-incompleta`, código 1, *«le falta el cierre del documento»* |

Aislado ese test **pasa** (33 aserciones con el filtro `[floating_window]`), así que la muerte depende del orden — Catch2 siembra el azar distinto en cada vuelta —. Es código **commiteado**: del crash no hay nada en el `working tree` de este hilo, y `test_MeasurementFloatingWindow.cpp` no lo toca ninguno de los ficheros modificados. Queda anotado aquí para el hilo que lo tenga delante.

Las dos muertes demuestran que los guards **del cronómetro** funcionan: se negó a analizar una medición incompleta, dio el motivo exacto y salió con 1.

**Lo que esa frase no decía, y un build de verdad enseñó:** el **build** de esos mismos datos salía **verde**. El cronómetro avisa con un 1, y el 1 del cronómetro es un aviso; el build lo leía como «algo que mirar», imprimía un `[Warn]` y salía con **0**. Sin la cola de `:end`, sin un cartel de fallo y con el de `Build Successful` encima. Eso es §6.15, y es de lo que va esta sección: los guards de dentro sabían que la medición no existía, y los de fuera no lo escuchaban.

Por eso la referencia commiteada **sigue sin bloque `ruido`**, y las cinco comprobaciones que la vigilan están **aparcadas** en `tools/test_duraciones_suite.mjs`, con el cartel que dice qué hay que hacer para volverlas. Todo lo demás —el cálculo, el bloque, el informe y el camino sin segunda vuelta— está en verde y comprobable sin medir la suite.

## 6.15. Un build de verdad, con la suite muerta: la cola no imprimió y el build salió con 0

Esta sección es la que faltaba en todo el acta, y no es un caso más: es la **medición del propio guard**. Todo lo anterior se ha comprobado con bancos de pruebas que reensamblan fragmentos de `build.bat`, y un banco no es un build. Un banco ejecuta las lineas que el test le da, en el orden que el test quiere, con las llamadas sustituidas. Un build de verdad ejecuta el fichero entero, con su parser de cmd y sus reglas de bloque. La diferencia resultó ser justo aquí.

**El encargo:** ejecutar un `build.bat` completo de verdad y comprobar que la cola imprime y devuelve el código correcto. El resultado es que **no hacía ninguna de las dos cosas**, y por el motivo que menos se espera.

| | Lo medido |
|---|---|
| Compilación | `Build Successful` (incremental, binario de las 12:54) |
| Autocomprobación del cronómetro **dentro del build** | 77 + 202 aserciones en verde |
| La suite | **SIGSEGV**, `3221226525`, cortada a los **585 de 939** casos |
| El cronómetro | `{"estado":"medicion-incompleta","codigo":1}` |
| **La cola de `:end`** | **0 líneas** |
| **Código de salida del build** | **0** |

Una suite que se muere en el 62 % del recorrido daba un build **verde**, con su cartel de `Build Successful` y sin una sola línea de fallo. Eso no es un build que ha comprobado que la suite termina: es un build que no ha comprobado nada y dice que sí.

**El defecto A: una comilla que falta, y con ella la rama muerta desde §6.7.**

En `build.bat:293` la línea termina en `set "PERF_ESTADO=%%e` **sin la comilla de cierre**. Esa lónea está dentro de un bloque `if/else`, y dentro de un bloque cmd empareja las comillas cruzando líneas: el `for /f` deja de ejecutar el comando y busca un **fichero** llamado `findstr /b /c:...`. En el log de verdad:

""
El sistema no puede encontrar el archivo "findstr /b /c:"ABD-VEREDICTO " "C:\Users\...\abdl_perf.txt"".
[Warn] State: desconocido.
""

`PERF_ESTADO` se queda en `desconocido` **en silencio**, y con el estado muerto la rama `if "!PERF_ESTADO!"=="fallo-del-tool"` —la que §6.7 construyó para que el build distinguiera un cronómetro roto de una suite lenta— **es código muerto**: no puede dispararse nunca.

Y **aunque la comilla estuviera puesta seguiría saliendo mal**, por dos motivos que se midieron uno a uno:

- `%%e` con `tokens=1,2*` es el **prefijo**, no el estado.
- `%%f` tampoco vale: los `delims` por defecto de `for /f` incluyen la **coma**, así que `%%f` salía truncado en el primer separador del JSON (`{"estado":"medicion-incompleta","codigo`).

La forma que funciona es de una línea y se ha medido en las seis que puede tomar:

""
for /f "tokens=3 delims=:,{} " %%c in ("findstr /b /c:"ABD-VEREDICTO " "!PERF_LOG!"") do set "PERF_ESTADO=%%~c
""

**El defecto B: el código 1 no distingue «lento» de «no he medido».** Y esto **sigue abierto aunque se arregle A**: con el estado leído bien, `medicion-incompleta` cae igualmente en `else if "!PERF_EXIT!=="1"`, que solo avisa. El estado existe justo para cerrar ese hueco —un 1 que es una suite lenta y un 1 que es una medición que no existe— y el build no lo usaba para decidir. Ahora decide por el estado: `medicion-incompleta` es fatal, y `fallo-del-tool` (muerto desde §6.7) vuelve a existir.

**Lo que no lo habría visto: el test.** La regla de comillas del test era `LINEAS.filter(l => l.trim().startsWith('set ') && ...)`, y la 293 empieza por `for /f ... do set "`. Veía **0** líneas sin cerrar donde el fichero tenía una. Ampliarla a «cualquier `set "` en la línea» no habría servido: **trece** líneas de `build.bat` tienen un número impar de comillas sin que pase nada, porque son **comentarios** con prosa entrecomillada —que cmd no ejecuta— y porque el `set` del `for` cierra en el fin de línea, que es lo que ha hecho siempre. Contar comillas no distingue un fallo de un comentario.

**La regla que sí lo vigila es de comportamiento, y por eso es la que se ha añadido.** El banco sustituía la llamada al cronómetro por `cmd /c exit N`, que no imprime nada: la línea de veredicto no llegaba al temporal, el estado se quedaba siempre en `desconocido` y **la rama por estado no se probaba nunca**. No era una laguna teorica, era el mismo defecto A sin que nada se notara. Ahora los casos del banco llevan un `estado` y escriben un veredicto de verdad en el temporal que el build lee, y se comprueba que el build **lo lee**: si aparece `State: desconocido` con un veredicto presente, el rojo sale.

**Comprobado con un segundo build completo, con la misma suite que peta:**

| | Antes | Ahora |
|---|---|---|
| Estado leido del veredicto | `desconocido` | **`medicion-incompleta`** |
| Rama que se toma | `[Warn]`, aviso | `[Error] THE SUITE DID NOT FINISH` |
| Cola de `:end` | 0 líneas | **3 líneas de `[Error]`** |
| Código de salida | **0** | **1** |

El log entero está en `build/registro.txt` y el código en `build/codigo.txt` (ignorado por git). Y las cuatro mutaciones que se han probado sobre el arreglo —quitar otra vez la comilla, cambiar el token, borrar la rama, dejar de mirar el estado— dan **8, 8, 32 y 32 aserciones en rojo**.

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
| §6.10: `Measurement UI Panels` en cinco mediciones en disco | 0,014–0,033 s; **ninguna de 4,02 s** |
| §6.10: el caso en la corrida más lenta (301,5 s de suite, 26 regresiones) | 0,020 s: no se movió |
| §6.10: la regla vigente contra un 4,02 s | `despues` ≥ 1 s y ratio x251 → **ya se marcaría** |
| §6.10: el caso que el suelo de 1 s deja pasar a propósito | 0,033 s = x2,04, por debajo del segundo |
| §6.11: `test_build_bat_perf.mjs` con el código propio | **77 aserciones**, +8 |
| §6.11: autocomprobación del cronómetro en rojo | **exit 3**, antes 1 |
| §6.11: el test del reparto en rojo | **exit 1**, sin cambios |
| §6.11: cronómetro con salida 1 (suite lenta) | **exit 0**, el aviso se queda |
| §6.11: mutación, la rama vuelve a marcar `PERF_FATAL` | **3 en rojo** |
| §6.11: mutación, la cola sale con 1 en vez de 3 | **4 en rojo** |
| §6.12: `test_duraciones_suite.mjs` con el formato 2 y `rutaDeRepositorio` | **172 aserciones**, +12 |
| §6.12: rutas que quedan en la referencia commiteada | **0 absolutas** de 939; 875 del repo, 64 `../ABDSharedCode` |
| §6.12: bytes que se fueron de la referencia | 191.016 → **151.663** |
| §6.12: base de formato 1 | **se recusa y lo dice**; el análisis sale con 0 |
| §6.12: base del formato declarado | **se usa** |
| §6.12: una vuelta real contra la referencia migrada | 939 medidos contra 939, sin recusar |
| §6.12: segundos y `medidoEn` de la referencia | **sin tocar** |
| §6.12: mutación, la base vuelve a escribir la ruta absoluta | **1 en rojo** |
| §6.12: mutación, vuelve una ruta absoluta en la referencia commiteada | **2 en rojo** |
| §6.13: aportación de `8a19dd9` (portapapeles + timeouts) | **0,48 s de 125,5**, el 0,4 % |
| §6.13: ST-99 / ST-100, los timeouts que ese commit subió | 6,07 → 6,07 s y 6,32 → 6,21 s: nunca se alcanzan |
| §6.13: ficheros **tocados** por `c6010cd` | 1 fichero, 1 caso, mediana **×2,04** |
| §6.13: ficheros que **nadie tocó** | 196 ficheros, 926 casos, mediana **×1,34** |
| §6.13: ficheros intactos que se aceleraron | ×0,47, ×0,54, ×0,55 — un cambio de código no puede |
| §6.13: reparto de los 125,5 s | 37,4 s en los 5 primeros; el factor de la mediana explica ~71 s |
| §6.13: tres vueltas posterior al commit | 150,0 / 163,1 / 176,4 s: 18 % de dispersión en la misma mañana |
| §6.14: `test_duraciones_suite.mjs` con el ruido y las dos vueltas | **202 aserciones**, +30 |
| §6.14: `test_build_bat_perf.mjs` sin tocar el `build.bat` | **77 aserciones**, sin cambios |
| §6.14: `VERSION_FORMATO` después de añadir `ruido` | sigue en **2**, y la base sin `ruido` se usa igual |
| §6.14: `ruido` sin haberlo medido | la clave **no aparece**; no hay `null` |
| §6.14: `--xml --guardar-referencia`, de punta a punta | base **sin `ruido`** y aviso de por qué; sale con 0 |
| §6.14: factor invertido (la buena partida por la mala) | **8 en rojo** |
| §6.14: mutaciones del cálculo, del bloque y del veredicto | **7 de 7 en rojo** |
| §6.14: banda de abajo metiendo al caso que cruza 1 s | **5 en rojo** |
| §6.14: p90 interpolado en vez de medido | **1 en rojo** |
| §6.14: suite con SIGSEGV a los 585 casos | `medicion-incompleta`, **1**, motivo con el código 3221226525 |
| §6.14: suite muerta antes de cerrar el XML | `medicion-incompleta`, **1**, *«le falta el cierre»* |
| §6.14: `test_MeasurementFloatingWindow` aislado | **pasa**, 33 aserciones: la muerte depende del orden |
| §6.14: ejecutable durante las dos vueltas | **sin cambios**, las dos del mismo binario |
| §6.14: línea del tool | 1377 → **1729** |
| §6.14: comprobaciones sobre la referencia commiteada | **5 aparcadas**: la suite no termina y no se puede regenerar |
| §6.15: `build.bat perf` de verdad, cola de `:end` | **0 líneas**, y el build saló con **0** |
| §6.15: la misma suite, con el arreglo | cola de **3 líneas de `[Error]`** y build con **1** |
| §6.15: estado que lee el build | antes `desconocido` → ahora `medicion-incompleta` |
| §6.15: `for /f` que extrae el estado, aislado | `["medicion-incompleta"]`; con la comilla quitada, `desconocido` |
| §6.15: `%%f` en vez de `%%~c` | `{"estado":"medicion-incompleta","codigo`: truncado por la coma |
| §6.15: líneas de `build.bat` con comillas impares | **13**, y **ninguna** es ejecutable: son comentarios |
| §6.15: `test_build_bat_perf.mjs` con la columna `estado` | **134 aserciones**, +57 |
| §6.15: casos por estado que antes no se probaban | **6 nuevos**: ok, lento, regresion, sin-medir, fallo-del-tool, medicion-incompleta |
| §6.15: mutaciones del arreglo | **4 de 4 en rojo** (8, 8, 32, 32) |
| §6.15: línea del tool | 570 líneas, **ASCII puro, CRLF puro** |

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
| este commit | §6.11: la autocomprobación del cronómetro en rojo sale con `exit /b 3` en vez de compartir el 1 con las junctions y con «no se ha medido», con acumulador propio (`SELFTEST_FATAL`) y guard propio en la cola, primero de los tres. El 1 del cronómetro sigue siendo solo aviso y el build sale con 0, con aserción que lo lee de la tabla de casos para que moverlo sea una decisión explícita. 77 aserciones |

| este commit | §6.12: la referencia guarda la ruta relativa al repositorio en vez de la absoluta —la del disco, el proyecto y el usuario—, y el formato pasa a 2 porque `f` cambia lo que significa. Una base de formato 1 se recusa y ahora lo dice, que es la diferencia entre «no hay referencia» y «la hay y no se puede leer». De los 939 casos, 875 quedan como `src/tests/...` y 64 como `../ABDSharedCode/...`; lo que no está en este árbol se deja como venía. Se fueron 39.353 bytes, y ni los segundos ni `medidoEn` se tocaron. 172 aserciones |

| este commit | §6.15: la cola de `:end` no imprimia y el build salia con **0** con la suite muerta a mitad de la medicion, comprobado con un `build.bat perf` de verdad. Dos defectos: una comilla de cierre que faltaba en la línea que lee el estado, que hacia que el `for /f` no ejecutara el `findstr` y dejaba `PERF_ESTADO` en `desconocido` en silencio — con lo que la rama de `fallo-del-tool` de §6.7 era código muerto —, y que el código 1 no distinguiera «una suite lenta» de «una medición que no llegó a existir». Arreglados los dos: el estado se lee del veredicto con `tokens=3 delims=:,{} ` y `%%~c`, y `medicion-incompleta` decide por el estado y es fatal. El banco del test ahora escribe un veredicto de verdad y comprueba que el build lo lee, que es lo que faltaba y por lo que nadie lo vio: 134 aserciones |
| este commit | §6.14: `--guardar-referencia` mide **dos vueltas** de la suite y guarda el ruido de la máquina en la propia referencia, en el bloque `ruido`, para que el suelo absoluto de 1 s deje de ser una constante creída y sea una constante auditable: cada comparación enseña las dos bandas —la que el umbral descarta y la que no— y dice si el factor está dentro del ruido de la primera y si el margen de la segunda se ha perdido. El formato **sigue en 2**: `ruido` describe la máquina, y una referencia a la que le falta se compara igual y avisa. El ratio por caso no se guarda porque nada lo lee. La referencia commiteada **sigue sin el bloque** —la suite del árbol no termina, y ver §6.14— y las cinco comprobaciones que lo vigilan quedan aparcadas en el test con el cartel que las devuelve. 202 aserciones |

Al auditar se ha encontrado modificado `contracts/hardware/abdeep_modulation_matrix.json` y `.github/workflows/audio-ab-5d-ci.yml`, que reescriben respectivamente una ruta de `provenance` y algo del workflow. **No son de este trabajo y no se han tocado**: el hilo paralelo está tocando el repositorio a la vez.

Y por el mismo motivo pasó con `78e238f`: el hilo paralelo commiteó los cambios de §6.3 que estaban sin commitear en el árbol de trabajo. El contenido es correcto y el asunto no engaña —*refine absolute suite duration threshold* describe exactamente el umbral absoluto—, pero el commit no lleva cuerpo ni atribución. Queda anotado aquí en lugar de reescribir historia ajena.
