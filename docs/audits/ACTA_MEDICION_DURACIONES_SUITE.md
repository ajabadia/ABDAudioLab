# ACTA DE MEDICIÓN DE DURACIONES DE LA SUITE

## Medición contra la referencia `duraciones-referencia.json` — 2 de octubre de 2026

**Documento:** `docs/audits/ACTA_MEDICION_DURACIONES_SUITE.md`
**Fecha de emisión:** 2026-10-02
**Fase:** POST-5D.5 — verificación del guard de duraciones recién commiteado
**Herramienta:** `tools/duraciones-suite.mjs`
**Estado:** 🟢 MEDICION CORRIDA Y ANALIZADA — 0 regresiones, 9 casos nuevos
**Addendum:** §4 y §5 son lo importante de este acta. El resultado contra la referencia es limpio; lo que no cierra es que el guard, tal y como está configurado, no puede detectar una regresión real. Leer §5 antes de confiar en un verde de este guard.

---

## 1. Qué se midió y en qué condiciones

| Parámetro | Valor |
|-----------|-------|
| Ejecutable | `build/Release/ABDAudioLab_Tests.exe`, **reconstruido** antes de medir |
| Build | `TESTS_BUILD_RC=0`, exe fechado a las 00:52 |
| Filtros | `~[integration-01]` `~*PluginHost*` (los de la herramienta, línea 76) |
| Directorio | raíz del repositorio (obligatorio: desde otro cwd hay skips falsos) |
| Resultado de la vuelta | `SUITE_RC=0`, 153 s de reloj |
| Casos | 936 |
| Aserciones | 210 732, 0 fallos, 49 saltados |
| Referencia | 927 casos, 301,516 s, medida el 2026-10-01T16:42:00.682Z |
| Umbral / piso / factor | 8 s / 0,05 s / 2 |

Comandos, por si hay que reproducirlo:

```bash
node tools/duraciones-suite.mjs --xml build/duraciones-medicion.xml --solo-avisar
```

El XML se capturó con una sola vuelta de la suite y el análisis se hizo sobre ese fichero, en vez de dos vueltas: `duraciones-suite.mjs` sin `--xml` vuelve a correr la suite, y a cinco minutos por vuelta no compensa.

### 1.1 Por qué hubo que reconstruir antes

El ejecutable que había en `build/Release/` estaba fechado a las 00:02, y el commit `8a19dd9` —*resolver cuelgue de portapapeles en test_SmokeStep4UI*— es de las 00:27. Es decir, el binario era **anterior al arreglo del cuelgue**.

Medir con ese binario habría medido precisamente el defecto que el commit iba a arreglar, y en el peor caso se habría colgado: un cuelgue de portapapeles se manifiesta como «el proceso sigue ahí y no termina», que es justo lo que una medición de duraciones confunde con «todavía no ha flushed».

**Regla que sale de aquí:** `build/Release/ABDAudioLab_Tests.exe` hay que reconstruirlo siempre antes de medir. La herramienta no lo hace, y no avisa de que el binario sea viejo. Es un fallo silencioso esperando a ocurrir.

## 2. Resultado contra la referencia

| Métrica | Resultado |
|---------|-----------|
| Regresiones | **0** |
| Casos nuevos sin referencia | **9** |
| Casos de la referencia ausentes | **0** |
| Delta del total | **−49,2 %** (153,06 s frente a 301,516 s) |

Casos por encima del umbral de 8 s: **2**.

| Actual | Referencia | Ratio | Caso |
|--------|------------|-------|------|
| 10,803 s | 33,213 s | 0,33x | La ventana flotante se monta y se re-tematiza sin conocer su contenido |
| 10,226 s | 14,670 s | 0,70x | DigitalSynthMvpProfiler — Deterministic Nominal Benchmark |

Percentiles: **p50 = 0,0007 s**, **p90 = 0,162 s**, **p99 = 6,023 s**.

El veredicto operativo es que **no hay ninguna regresión**: ningún caso ha superado el doble de su tiempo de referencia.

## 3. El total NO es una mejora de rendimiento, y por qué importa decirlo

El −49,2 % del total es el número más llamativo de esta acta y el que más malinterpretable es. **No significa que el código esté el doble de rápido.**

Lo que muestra el reparto es que la caída está concentrada en unos pocos casos, y que esos casos son los que más varían entre entornos:

| Caso | Referencia | Actual | Ratio |
|------|-----------|--------|-------|
| La ventana flotante se monta y se re-tematiza | 33,213 s | 10,803 s | 0,33x |
| ReportExportUiController — Characterization & Lifetime | 22,195 s | 7,377 s | 0,33x |
| DigitalSynthMvpProfiler — Negative Cases & Fault Injection | 6,598 s | 3,385 s | 0,51x |
| FineLatency — Clock Drift Modeling and Topology Discrimination | 8,460 s | 3,999 s | 0,47x |
| ST-99: AutomatedMidi campaign | 6,120 s | 6,076 s | **0,99x** |
| ProfilingSequencer — runUniversalModulationProbe | 6,101 s | 6,099 s | **1,00x** |
| ProfilingArchitectureRefactor — ProfilingSequencer | 6,026 s | 6,023 s | **1,00x** |

Los tres primeros son GUI de JUCE: crean ventanas, pumps de `MessageManager`, cargan fuentes. Pagan un coste de arranque que depende de lo que hubiera cargado el proceso antes y de si la máquina estaba ocupada. La referencia se midió el 1 de octubre con la herramienta en pleno desarrollo, probablemente con compilaciones en paralelo —el propio `--factor` y `--solo-avisar` existen por eso—.

Los últimos son audio con **duración fija**: corren seis segundos de trabajo y tardan seis segundos,, están 0,99x–1,00x porque no pueden ir de otro modo.

**Conclusión:** el total de la suite mide la máquina tanto como el código. Por eso el guard compara ratios por caso y no el total, y por eso esta acta no declara ninguna mejora.

## 4. Reparto del tiempo

| Franja | Casos | Tiempo | % del total |
|--------|-------|--------|-------------|
| 0 – 0,05 s | 778 | 2,6 s | 1,7 % |
| 0,05 – 0,5 s | 107 | 16,2 s | 10,6 % |
| 0,5 – 1 s | 18 | 13,2 s | 8,6 % |
| 1 – 3 s | 16 | 26,1 s | 17,1 % |
| 3 – 6 s | 7 | 24,1 s | 15,7 % |
| 6 – 30 s | 10 | 70,9 s | **46,3 %** |

Dos hechos:

1. **778 de los 936 casos (83 %) suman el 1,7 % del tiempo.** La suite es ancha y plana.
2. **Diez casos se comen el 46 % del tiempo.** Cualquier trabajo de rendimiento que se haga aquí tiene que empezar por esos diez, no por los 778.

## 5. El límite estructural del guard: por qué `--factor 2` no puede disparar

Esta es la sección que justifica el addendum del principio. Se compararon los 927 casos que existen en ambas mediciones contra su tiempo de referencia:

| Ratio actual/referencia | Casos | Tiempo | % del total |
|--------------------------|-------|--------|-------------|
| ≥ 2,0x | 32 | **0,1 s** | 0,1 % |
| 1,2 – 2,0x | 111 | 2,3 s | 1,5 % |
| 0,8 – 1,2x | 232 | 84,9 s | **55,4 %** |
| 0,5 – 0,8x | 210 | 28,5 s | 18,6 % |
| < 0,5x | 342 | 37,4 s | 24,4 % |

Los 32 casos que superan el doble de su referencia **suman 0,1 segundos entre todos**. Los 143 casos por encima de 1,2x suman 2,3 s.

Es decir: **los casos que se mueven mucho son todos sub-milisegundo, y los casos lentos son los que no se mueven.** Las dos poblaciones son disjuntas. Un factor 2 no puede dispararse nunca sobre nada que cueste tiempo de verdad, porque todo lo que cuesta tiempo de verdad está clavado.

La dispersión es **bimodal**, y tiene una explicación concreta:

- **232 casos (55,4 % del tiempo) entre 0,8x y 1,2x.** Son insensibles a la carga: 81 de ellos están clavados en 0,95–1,05x y suman 52,2 s. Siete forman un racimo exacto entre 5,9 y 6,3 s, que suma 42,5 s: `ProfilingSequencer`, `MidiSynthAutomation`, `HITO-02 / ST-11`, `HITO-02 / ST-12`, `ProfilingArchitectureRefactor`, `ST-99` y `ST-100`. Todos son cargas de duración fija.
- **552 casos por debajo de 0,8x**, que suman el 43 % del tiempo. Son los de GUI, y los que absorben la variación del entorno.

**Qué significa para el guard:** un guard que solo avisa cuando un caso pasa de 2x su referencia tiene un doble problema. No puede detectar una regresión real de audio, porque los tests de audio están clavados; y si algún día disparara, sería por un caso de milisegundos cuyo ruido es mayor que su señal. El factor 2 no es permisivo: es ciego.

## 6. Los 9 casos nuevos

Los nueve casos que no están en la referencia son exactamente los nueve casos de prueba de `src/tests/test_PinkNoiseGenerator.cpp`, añadidos por el commit `c6010cd`:

1. El LCG de `nextWhite()` es el que dice ser, muestra a muestra
2. `nextWhite()` cae en [−1, 1) y no se sale nunca
3. La serie rosa está fijada: el hash de tres semillas no se mueve
4. `clearFilterState()` no reinicia el azar: la serie sigue donde estaba
5. `reseed()` devuelve el generador al principio, con la misma serie
6. La semilla por defecto de la clase y la de `reseed()` son la misma
7. Copiar un generador produce dos series independientes, no un handle
8. La pendiente del ruido rosa cae entre la del blanco y la del marrón
9. El ruido rosa no se va de rango, no produce NaN y mantiene el nivel

Ninguno entra en el top 15 por tiempo: los nueve juntos no se notan. La referencia y la medición actual cuadran en 936 = 927 + 9, lo que confirma de forma independiente que la captura y el parser cuentan lo que deben.

## 7. Defectos encontrados

### 7.1 Caracteres chinos en un comentario del tool — `tools/duraciones-suite.mjs:41`

La línea 41 dice, literalmente:

```
 * Un umbral fijo aqui seria un numero sin razon: 5 s es太快 para una suite de
```

«太快» es chino y significa «demasiado rápido». La frase debe decir *«5 s es demasiado rápido para una suite de audio»*. Es un comentario, no afecta a la ejecución, y está en el commit `8606991` de la otra workstream. **No se ha tocado**: es fichero de otro hilo y el arreglo no es parte de esta medición. Se reporta para que quien lo commitee lo decida.

`tools/test_duraciones_suite.mjs` está limpio: la contaminación no se propagó.

### 7.2 La ruta absoluta del campo `f` — no ha mordido esta vez

El commit `f7a8a44` declara que el campo `f` de cada caso guarda la ruta absoluta de esta máquina y que en otra máquina el caso de un test renombrado se degrada a «test nuevo». **Esta medición no ha dado ningún ausente**, así que la comparación por nombre cubrió los 927 casos y el respaldo por ruta absoluta no hizo falta. El defecto sigue ahí, pero hoy es inocuo.

### 7.3 La herramienta no comprueba que el binario sea actual

Ver §1.1. Es el único defecto de la lista que puede volver a dar un resultado falso sin que nadie se entere.

## 8. Recomendaciones

1. **Reconstruir antes de medir, siempre.** Lo más barato es que la herramienta compruebe la fecha del `.exe` contra la del último commit que toca `src/` y avise, o se niegue a correr.
2. **Añadir un índice de carga de máquina al informe.** El racimo de los siete casos clavados en ~6 s (42,5 s, el 27,8 % del total) es un reloj excelente: si esa banda se mueve, la máquina está ocupada y los ratios del resto no significan nada. Ponerlo en el informe evita tener que deducirlo cada vez.
3. **Comparar ratios solo por encima del piso de 0,05 s**, que es lo que ya hace la herramienta, e ignorar además los casos que están en el racimo fijo. Con eso, el `--factor 2` empezaría a tener sentido sobre la población que de verdad cuesta tiempo.
4. **Guardar la ruta relativa en `f`** en la próxima regeneración de la referencia, y no en la actual: cambiar el formato invalidaría las 927 entradas.
5. **Regenerar la referencia cuando sea conveniente**, no ahora. La actual es válida y legible; el problema de la ruta absoluta es latente, no activo.

---

## 9. Cómo se reproduce todo esto

```bash
# 1. Reconstruir (imprescindible, ver 1.1)
MSBuild build/ABDAudioLab_Tests.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. Una sola vuelta de la suite, guardando el XML
./build/Release/ABDAudioLab_Tests.exe -r xml -d yes '~[integration-01]' '~*PluginHost*' \
  > build/duraciones-medicion.xml 2>&1

# 3. Analizar sin volver a correr nada
node tools/duraciones-suite.mjs --xml build/duraciones-medicion.xml --solo-avisar
```

El `--solo-avisar` devuelve 0 aunque haya casos por encima del umbral: es lo que hace falta cuando lo que se quiere es el informe y no el color del CI.