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
[C] ruta    = D:\desarrollos\ABDSynths\ABDAudioLab\build\Release\ABDAudioLab_Tests.exe
```

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

### 6.1 El código de salida ya es honesto, y no hay nadie escuchando

Es lo más importante que sale de esta auditoría y no es un fallo de la herramienta. Los arreglos de `078b2aa` y `cf17e42` hacen que el programa salga con **1** cuando la medición no sirve. Sus tres consumidores hacen los tres lo mismo:

- La CI marca el paso `continue-on-error: true` y termina con `exit 0`. Está escrito que es a propósito, porque un rojo permanente por culpa del tiempo de una máquina es peor que no mirar. **Correcto, y sigue siendo cierto.**
- `build.bat` guarda `PERF_EXIT`, imprime un `[Warn]` distinto según valga 0 o 1, y **no lo propaga a su propio código de salida**: el script termina en `:end / endlocal`.
- Solo quien lo lanza a mano lo lee.

Consecuencia honesta: el guard ahora es fiable **cuando alguien lo mira**, y no hay ninguna puerta automática que se ponga roja por un cuelgue de la suite. Cerrar eso es una decisión de proyecto, no un arreglo: implica elegir qué significa «una medición incompleta» en un pipeline donde el resto de los pasos sí son puerta.

### 6.2 El límite de reloj es 7,8 veces la medición real

`LIMITE_MS = 20 * 60 * 1000` (línea 127) son 1200 s. La última vuelta real midió 153,1 s de suite y la referencia suma 301,516 s. Un límite que está casi ocho veces por encima del trabajo esperado no es un límite: es un tope nominal, y un cuelgue real se descubre por el reloj del pipeline, no por esta herramienta. Lo que sí se ha hecho en `078b2aa` es que, cuando el límite se agota, el resultado no se confunda con una medición buena. Derivar el límite de la referencia lo dejaría en un número con razón.

### 6.3 El factor ×2 señala ruido por debajo de un segundo

`PISO_DE_INTERES_S = 0.05` (línea 294) es el suelo de interés. La vuelta real de hoy, con el guard ya arreglado, da **7 regresiones** y todas están entre 0,06 y 0,52 s: de 0,12 a 0,52, de 0,19 a 0,44, de 0,20 a 0,87. Duplicar el tiempo de un caso de 120 ms no es una regresión de la que haya que preocuparse, y el acta de medición ya lo medía desde el otro lado: los casos que superan el factor 2 suman 0,1 s entre todos.

Es decir: **el guard es insensible a lo que de verdad importa y sensível a lo que no importa**. Lo que importa —un caso que pasa de 9 a 18 s— lo ve; lo que no —ruido de scheduling— le pone un rojo. Lo que no se ha tocado es el umbral, porque subirlo es cambiar la política del guard y eso es una decisión, no un bug.

### 6.4 La referencia guarda rutas absolutas, y eso no se va a regenerar

927 entradas con la ruta de la máquina que las midió, lo que la ata a un disco y a un usuario. `claveDeFichero` lo mitiga para el emparejado, y por eso §3.2 no se repite en otra máquina. Lo que **no** se arregla así es el resto: `construirBase` sigue grabando `f` tal cual, de modo que el fichero sigue sin ser portable y una comparación entre dos referencias de dos máquinas seguiría sin poder emparejar. Regenerarlo exigiría un `f` relativo al repositorio, que es un cambio de formato — y el formato tiene `version: 1` a propósito para que un cambio así se pueda **recusar en vez de compararse**.

### 6.5 El ejecutable puede estar viejo y la herramienta no lo dice

Heredado del acta de medición y sin resolver: `build/Release/ABDAudioLab_Tests.exe` hay que reconstruirlo antes de medir, y la herramienta no comprueba que el binario sea posterior al código. Medir con un binario viejo mide el defecto que se iba a arreglar.

## 7. Verificación

| Comprobación | Resultado |
|---|---|
| Test del tool antes de esta auditoría | 65 aserciones |
| Test del tool después | **91 aserciones, todas en verde** (+26) |
| `--base otra.json` entre lo que recibe Catch2 | antes sí → **después no** |
| Renombrado entre máquinas: `nuevos` / `ausentes` | antes `1 / 1` → **después `0 / 0`** |
| Renombrado que se multiplica por cuatro | antes 0 regresiones → **después 1, con su tiempo anterior** |
| Sustitución de una referencia existente | la nueva se lee entera, sin temporales |
| Medición real, sin cambios de comportamiento | 936 casos, 9 nuevos, salida con 0 |
| Fichero de la herramienta | 554 líneas → 964 |

## 8. Commits

| Commit | Qué cierra |
|--------|-----------|
| `078b2aa` | El fallo abierto: resultado del spawn, cierre del documento, cuenta de casos, ausentes con código 1 |
| `cf17e42` | Los cuatro hallazgos de esta acta |

Al auditar se ha encontrado modificado `contracts/hardware/abdeep_modulation_matrix.json` y `.github/workflows/audio-ab-5d-ci.yml`, que reescriben respectivamente una ruta de `provenance` y algo del workflow. **No son de este trabajo y no se han tocado**: el hilo paralelo está tocando el repositorio a la vez.
