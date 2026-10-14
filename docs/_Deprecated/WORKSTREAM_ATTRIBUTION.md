# ATRIBUCIÓN POR WORKSTREAM

## Ventana del 1–2 de octubre de 2026: dos hilos sobre el mismo checkout

**Documento:** `docs/WORKSTREAM_ATTRIBUTION.md`
**Fecha de emisión:** 2026-10-02
**Alcance:** 15 commits, `26be85c` … `8606991` (01/10 13:20 → 02/10 00:28)
**Estado:** 🟡 **RECONSTRUIDO A POSTHOC** — la atribución real no quedó registrada en git mientras ocurría
**Addendum:** §6 fija el convenio `Workstream-Origin:` para los commits futuros. Leer §5 antes de usar la tabla de §3 como si fuera un registro nativo: no lo es.

---

## 1. Por qué existe este documento

Entre el 1 y el 2 de octubre de 2026 trabajaron sobre este repositorio **dos hilos al mismo tiempo**, sobre el **mismo checkout**, commiteando ambos en la **misma `main`**. El historial es completamente lineal: no hay un solo merge. Git no registra en ningún sitio qué hilo produjo cada commit.

El síntoma lo dejó escrito el propio commit `f7a8a44`, que es la cita que origina este documento:

> las 318 lineas que anaden todo esto son trabajo sin commitear de otro hilo

Es decir: durante horas, el árbol de trabajo compartido acumuló cambios de los dos hilos sin que ninguno de los dos pudiera saberlo. Este documento reconstruye, a posteriori y con la evidencia que aún sobrevive, qué commit era de quién.

### 1.1 La identidad de git no sirve para distinguir hilos

Lo primero que se ocurre al buscar el autor es `git shortlog -sne`. No sirve:

```
   122   Alejandro Abadía <ajabadia@gmail.com>
    27   ajaba <ajabadia@gmail.com>
     5   ajabadia <ajabadia@users.noreply.github.com>
```

Son **tres grafías y dos correos distintos** para la misma persona. Las tres aparecen mezcladas dentro de la ventana analizada, y ninguna coincide con la frontera entre hilos que sabemos que existió. Cualquier intento de atribuir por autor es inventar.

## 2. Las dos workstreams

| ID | Qué es | Cómo se identifica |
|----|--------|--------------------|
| **WS-1** | Este hilo: el agente que cerró los cinco gaps de la auditoría (`c6010cd`) | Determinable por el registro de la conversación, no por git |
| **WS-2** | Hilo paralelo: hermetización de rutas POST-5D.5, guard de duraciones, desbloqueo de la suite | Determinable por el cuerpo de los commits y por la autoría de los ficheros |

Sobre el nombre del actor de WS-2 solo hay evidencia parcial. El bloque POST-5D.5 viene firmando sus actas como **Antigravity (Lead Architect & Supervisor)**, coherente con el modelo en tándem de `AGENTS.md`; el bloque del guard de duraciones solo se identifica a sí mismo como «otro hilo». No hay dato que permita asignar un rol único a WS-2 en conjunto, y no se le fuerza uno.

## 3. Tabla de atribución

Base de cada atribución:

- **(C) Confirmado** — consta en el registro de la conversación o en una petición explícita del usuario.
- **(E) Evidencia fuerte** — el commit se cita a sí mismo o la autoría del fichero lo delata.
- **(P) Probable** — inferencia por tema, vecindad temporal y firma de las actas.
- **(I) Indeterminado** — hay dos lecturas posibles y la evidencia no decide.

| Commit | Fecha/hora | Asunto | WS | Base |
|--------|-----------|--------|----|------|
| `26be85c` | 01/10 13:20 | refactor(audio): el ruido rosa tiene una sola fuente de coeficientes | ? | **(I)** primer pase de la tarea que cerraría `c6010cd` |
| `46b5361` | 01/10 13:22 | refactor(gui): MeasurementFloatingWindow sale de MainContentComponent.cpp | ? | **(I)** ídem |
| `deafcd2` | 01/10 16:50 | test(gui): la ventana de medicion se monta en la suite y no solo en el enlazado | ? | **(I)** ídem |
| `3206ee4` | 01/10 16:53 | feat(paths): migracion R01-R26 y saneamiento de rutas | WS-2 | **(P)** acta firmada por Antigravity |
| `d7675b6` | 01/10 16:53 | test(paths): anti-regresion de higiene de rutas y deteccion de CWD | WS-2 | **(P)** mismo lote |
| `bb719ae` | 01/10 16:54 | fix(audio): sincronizacion sample-rate y compuertas MIDI | WS-2 | **(P)** mismo lote |
| `c0a523b` | 01/10 16:54 | test(telemetry): runner telemetry con heartbeats | WS-2 | **(P)** mismo lote |
| `6e8484c` | 01/10 16:54 | docs(post-5d.5): cierre de auditoria e inventario de rutas | WS-2 | **(P)** es el acta que firma |
| `f6f5427` | 01/10 18:30 | chore(ci): fijar SHA inmutable de ABDSharedAssets | WS-2 | **(P)** mismo minuto que `b39d3c5`, que sí es WS-2 |
| `b39d3c5` | 01/10 18:30 | tools(diagnostics): lector y alerta de duraciones desde XML de Catch2 | WS-2 | **(E)** `f7a8a44` lo llama «otro hilo» por su nombre |
| `819aaab` | 01/10 18:50 | chore: ignorar las capturas XML de Catch2 | WS-2 | **(E)** los XML que ignora los produce `b39d3c5` |
| `f7a8a44` | 01/10 18:51 | test(tools): la referencia de duraciones | WS-2 | **(E)** su propio cuerpo nombra al otro hilo |
| `c6010cd` | 01/10 21:09 | refactor(auditoria): cerrar los cinco gaps | **WS-1** | **(C)** este hilo |
| `8a19dd9` | 02/10 00:27 | fix(tests): cuelgue de portapapeles y timeouts ST-99/ST-100 | WS-2 | **(C)** petición explícita del usuario |
| `8606991` | 02/10 00:28 | feat(ci): medicion informativa de duraciones y soporte en build.bat perf | WS-2 | **(C)** ídem |

**Los 139 commits anteriores a `26be85c` quedan fuera de alcance y sin atribuir.** No es que se atribuyan a un hilo u otro: es que durante esa época no había dos hilos conviviendo, y no hay ninguna razón para inventar una fila por commit.

## 4. El solapamiento: el hallazgo de esta ventana

`c6010cd` (WS-1) **no es una pieza aislada**. Es el segundo pase de un trabajo que ya había empezado ocho horas antes, y encima de ficheros que otro pase había creado. `git log --follow` lo deja claro: cada uno de estos ficheros tiene exactamente dos commits en su historia.

| Fichero | Lo creó | Lo amplió `c6010cd` | Cómo terminó |
|---------|---------|---------------------|--------------|
| `src/audio/PinkNoise.h` → `src/math/PinkNoise.h` | `26be85c` (13:20, 107 líneas) | gap #3: movido a `src/math/`, namespace `abdaudiolab::math` | 114 líneas |
| `src/gui/MeasurementFloatingWindow.{h,cpp}` | `46b5361` (13:22, 47 + 48 líneas) | gap #1: segundo constructor tipado sobre `MeasurementThemedPanel` | 70 + 57 líneas |
| `src/tests/test_MeasurementFloatingWindow.cpp` | `deafcd2` (16:50, 217 líneas) | prosa reescrita + `PanelDeMentira` para el camino positivo | 267 líneas |

Lo que esto significa, y es lo que el log por sí solo oculta:

1. **El fichero `test_MeasurementFloatingWindow.cpp` es trabajo compartido.** Las 217 líneas iniciales son de un pase anterior; las aserciones que lo suben de 28 a 33 son del segundo. No es atribuible a un hilo en bloque.
2. **La afirmación «cerré los cinco gaps» es correcta pero incompleta.** Se cierra sobre trabajo ya empezado en el mismo árbol, y no fue una decisión consciente: simplemente llegó después.
3. **Revertir `c6010cd` no es una operación segura.** Si alguien intentara deshacerlo para quedarse con el estado anterior, perdería el trabajo del pase previo, porque los dos pases están entrelazados en los mismos ficheros.

## 5. Qué se puede y qué no se puede saber

- **Se puede**: qué ficheros creó cada commit (`--diff-filter=A`), qué commits son de este hilo (el registro de la conversación), y qué commits se declaran entre sí (los cuerpos de mensaje, que en este repo han sido inusualmente explícitos al respecto).
- **No se puede**: qué hilo ejecutó `26be85c`, `46b5361` o `deafcd2`. Los tres usan la grafía `ajaba`, la misma que WS-1, lo que apuntaría a este hilo; pero ocurrireron ocho horas antes de que esta conversación empezara, y son exactamente la clase de trabajo que un hilo anterior habría dejado a medias. Podrían ser una sesión previa de WS-1 o trabajo de WS-2. **La tabla los deja como `?` en lugar de repartirlos.**
- **No se puede** en general: ninguna de las tres grafías de autor correlaciona con la frontera entre hilos (§1.1).

## 6. Convenio para los commits futuros

A partir de ahora, todo commit lleva un trailer. No es opcional y no se aplica a los commits ya publicados: **este documento no reescribe historia**.

**Formato:**

```
Workstream-Origin: WS-<n> — <rol en una línea>
```

**Reglas:**

1. El ID es estable y se reutiliza. Los ya usados en esta ventana son `WS-1` y `WS-2`; un hilo nuevo toma `WS-3`, `WS-4`…
2. **Cuando un commit se apoye en trabajo de otro hilo, hay que decirlo en el trailer**, no solo en el cuerpo:

   ```
   Workstream-Origin: WS-3 sobre WS-2 — amplía el guard de duraciones
   ```

   Es el caso exacto de `c6010cd`, y es el que la tabla de §3 no puede reconstruir.

3. **El trailer va en su propio párrafo, el último del mensaje, después del pie `🤖 Generated with Codebuff` / `Co-Authored-By:`.** No antes. Ver §6.1: colocarlo antes hace que git no lo vea.
4. Cuando dos hilos puedan tocar los mismos ficheros, el trailer es **obligatorio**: es la única señal de que un `revert` o un `bisect` posterior puede romper trabajo ajeno.

### 6.1 Por qué el trailer va después del pie, y no antes

Esto no es una manía de formato: es cómo funciona `git interpret-trailers`. Solo examina el **último párrafo** del mensaje y se detiene en la primera línea que no tenga forma de trailer (`Clave: valor`). La línea `🤖 Generated with Codebuff` no la tiene, así que **termina el bloque** y todo lo que venga después queda fuera.

Medido en este repositorio, sobre los tres formatos posibles:

| Colocación | `git interpret-trailers --parse` |
|-------------|--------------------------------|
| Trailer antes del pie con emoji | **vacío** |
| Trailer en párrafo propio, después del pie | `Workstream-Origin: WS-1 -- rol` |
| Trailer en párrafo propio, sin línea de emoji | `Workstream-Origin: WS-1 -- rol` |

Eso tiene un efecto colateral que conviene conocer: **el `Co-Authored-By:` de este repo tampoco lo parsea git**, por el mismo motivo y desde siempre. Si algún día se quiere leer cualquiera de los dos con `%(trailers)`, el arreglo es el mismo para ambos: que el trailer sea el último párrafo.

Para comprobar el convenio sobre el historial, sin depender del parser:

```bash
git log --format='%h %s' --grep='^Workstream-Origin:'
```

Si devuelve commits que no llevan línea de trailer, significa que hay commits nuevos que no siguen el convenio; no que el convenio esté roto. El commit que introduce este documento lo lleva ya en el formato correcto.

Este documento cita su propio commit por descripcion y no por SHA a proposito: un `--amend` o un rebase bastarian para dejar la referencia obsoleta, y una referencia obsoleta en un documento que sirve para auditar es peor que no tenerla.

## 7. Lo que este documento NO arregla

El convenio de §6 es trazabilidad **después** del hecho. No previene el problema de fondo: **dos hilos en el mismo checkout, en la misma rama, se serializan en silencio**. No hay merge que rechace nada, porque no hay ramas que divergan; el segundo en llegar simplemente commitea encima del primero, y si el primero tenía cambios sin commitear, esos cambios se quedan mezclados con los del segundo sin aviso.

Eso es exactamente lo que pasó: 318 líneas vivieron en el árbol compartido sin que ninguna parte pudiera atribuirlas ni detectarlas, hasta que `f7a8a44` las menciona de paso para explicar por qué commitea una referencia que todavía no puede leerse.

La prevención real no es un trailer, es no compartir el árbol: ramas o worktrees separados por hilo, de modo que git haga de guardián en lugar de dejar que los dos agentes se pisen sin avisar. Eso queda fuera del alcance de este documento y no se ha implementado.