/**
 * CRONOMETRA la suite de Catch2 y avisa de los tests que tardan de mas.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUE ESTE FICHERO EXISTE
 *
 * Una suite que tarda seis minutos y una que se ha colgado se ven IGUALES desde
 * fuera: el proceso sigue ahi, no escribe nada nuevo, y no se termina. Cuando
 * eso paso aqui, la conclusion fue «esta colgado» y se mato el proceso; luego se
 * relanzo con `-s` y termino en tres minutos. El diagnostico equivocado no fue
 * un error de medicion: fue decidir que «no hay salida» queria decir «no hay
 * trabajo», cuando solo queria decir «el que imprime no ha flushed todavia».
 *
 * Asi que este script mide lo que de verdad distingue las dos cosas: si el
 * trabajo AVANZA. Un test que tarda mucho y acaba esta lento; uno que se cuelga
 * no acaba nunca. La diferencia no se ve mirando el reloj desde fuera, se ve
 * mirando cuanto trabajo ha hecho el proceso.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUE NO SE ESCRIBE UN LISTENER DE CATCH2
 *
 * Catch2 3.5.2 trae `--durations` y `-r xml`, que ya dan el tiempo por test.
 * Reescribirlo en C++ seria mas rapido y mas bonito, y seria lo equivocado:
 * obligaria a recompilar la suite para cambiar el umbral, y esto se consulta
 * cuando algo va mal, que es justo cuando mas rato se pierde recompilando.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * DE DONDE SALEN LAS MEDIDAS, Y POR QUE NO HAY QUE FIARSE A BLINDO
 *
 * El `-r xml` de Catch2 da un `<OverallResult durationInSeconds="...">` por cada
 * `<TestCase>`. Se usa ESE, que es el caso de test entero, y no el de cada
 * `<Section>`, que se puede repetir dentro del mismo caso y solo mide su parte.
 *
 * Las duraciones que Catch2 imprime en formato `2.3e-05` se parsean con
 * `Number()`, no comparando cadenas: un orden lexicografico de «2.3e-05» contra
 * «9.9» da el resultado contrario del que se quiere, y ese fallo es silencioso.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * EL UMBRAL, Y POR QUE ESTA EN EL ARGUMENTO Y NO CONSTANTE
 *
 * Un umbral fijo aqui seria un numero sin razon: 5 s es demasiado rapido para una
 * suite de audio, y demasiado lento para una de texto: no hay un valor que
 * valga para los dos. Ademas, en una maquina con diez compilaciones en paralelo,
 * TODO va lento, y un umbral fijo pondria en rojo veinte tests que estan bien.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * USO
 *
 *   node tools/duraciones-suite.mjs                    # mide y avisa
 *   node tools/duraciones-suite.mjs --umbral 10       # mas tolerante
 *   node tools/duraciones-suite.mjs --base otra.json  # compara con otra
 *   node tools/duraciones-suite.mjs --xml salida.xml  # usa un XML ya capturado
 *
 * Sale con 0 si nada supera el umbral, y con 1 si algo lo supera. Con
 * `--solo-avisar` sale con 0 siempre: para cuando uno solo quiere el informe.
 *
 * `--solo-avisar` silencia lo RUIDO, y no lo demas. Una medicion que no ha
 * terminado —un cuelgue, un XML sin cerrar, casos de la referencia que no se han
 * medido— sale con 1 con ese flag puesto: no es ruido, es una medicion que no se
 * puede creer, y un guard que se apaga con una bandera no es un guard.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LOS CODIGOS DE SALIDA, Y POR QUE EL 2 NO ES UN 1
 *
 *   0 = se midio, y no hay nada que decir.
 *   1 = se midio, y hay algo que mirar. Un test lento, una regresion contra la
 *       referencia, o una medicion que no llego a terminar: un cuelgue, un XML
 *       truncado, casos de la referencia que no se han medido, o una medicion sin
 *       un solo caso. Aqui la medicion EXISTE y es la que dice algo.
 *   2 = NO SE MIDIO. Falta el ejecutable de la suite, o el fichero que se le
 *       apunto con `--xml`. Aqui no hay ningun resultado de tiempos que leer.
 *
 * La distincion esta porque quien llama necesita contarlas como dos cosas. Un 2
 * con forma de «tu suite no se ha puesto lenta» es un verde falso con forma de
 * aviso, que es justo la clase de mentira que un cronometro no deberia tener: no
 * se ha medido nada, y no medir nada no es medir rapido. Por eso el 2 sale con un
 * codigo aparte y no como un 1 mas.
 *
 * Y lo que NO se puede distinguir desde fuera, para que no se lea de mas: un
 * error del propio script sale con 1, porque es el codigo que node usa para lo que
 * no captura. Un 1 es por lo tanto «algo va mal», que incluye «algo va lento», no
 * solo «algo va lento». Quien necesite separarlos tiene que mirar el texto que
 * sale por stderr, no el codigo.
 */

import { readFileSync, writeFileSync, renameSync, rmSync, existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const aqui = dirname(fileURLToPath(import.meta.url));
const raiz = join(aqui, '..');

/** El ejecutable de la suite, que es donde vive el tiempo que hay que medir. */
const SUITE = join(raiz, 'build', 'Release', 'ABDAudioLab_Tests.exe');

// ─────────────────────────────────────────────────────────────────────────
// LOS FILTROS, Y POR QUE SON ESTOS Y NO UNOS MAS
//
// La suite entera tarda unos minutos, y `test_PluginHostManager` se cuelga
// esperando `Dexed.vst3`, que no esta en esta maquina. Los dos se dejan fuera a
// proposito, y se dice aqui para que quien lea el numero que sale sepa que es el
// de una suite RECORTADA y no el de la completa. Un tiempo que no se puede
// comparar con el de la vuelta anterior no sirve para nada.
const FILTROS = ['~[integration-01]', '~*PluginHost*'];

// ─────────────────────────────────────────────────────────────────────────
// LAS BANDERAS QUE LLEVAN UN VALOR, Y POR QUE HAY QUE MANTENER LA LISTA
//
// Un valor suelto no es un filtro de test: es el valor de una bandera. Pero para
// Catch2 es indistinguible de un filtro, y un filtro que no nombra a ningun test
// hace que la suite no mida NADA. Lo que pasaba con `--base otra.json` era
// exactamente eso: el valor se colaba entre los argumentos, la suite corria sin
// encontrar los casos, y el cronometro informaba de que no habia cronometrado
// nada. Sin que se viera, porque un cronometro que no mide nada y un cronometro
// que no encuentra nada se quedan igual de callados.
//
// La lista es declarativa y vive junto a las banderas que se leen en `main`, no
// repartida por ahi como estaba antes: anadir una bandera nueva es anadir una
// linea. Un valor se cuela aqui con una consecuencia visible —cero casos
// medidos—, que es justo por lo que no puede quedarse invisible.

/** Las banderas cuyo valor detras NO es un filtro de test. */
const BANDERAS_CON_VALOR = ['--umbral', '--factor', '--base', '--xml'];

/**
 * Los argumentos que se le pasan a Catch2: ni banderas, ni sus valores.
 *
 * Lo que no empieza por `--` se le pasa tal cual, porque un filtro de test
 * legitimo empieza por el nombre o por `~`. Los valores de las banderas se
 * quitan por lista, no por posicion, porque una bandera puede no venir.
 *
 * @param {string[]} argumentos los argumentos de la linea de ordenes.
 * @returns {string[]} lo que Catch2 debe recibir.
 */
export function paraCatchDe(argumentos) {
  const valores = new Set();

  for (const bandera of BANDERAS_CON_VALOR) {
    const i = argumentos.indexOf(bandera);

    if (i >= 0 && argumentos[i + 1] !== undefined)
      valores.add(argumentos[i + 1]);
  }

  return argumentos.filter((a) => !a.startsWith('--') && !valores.has(a));
}

/** Tiempo de reloj que se concede a la suite antes de darla por colgada. */
const LIMITE_MS = 20 * 60 * 1000;

/**
 * Cuanto se deja que escriba la suite antes de cortar su salida.
 *
 * El limite de reloj y este son el mismo fallo por dos caminos distintos, y por
 * eso los dos se comprueban: los dos cortan la salida a medias, y una salida a
 * medias se lee igual que una entera porque `leerDuraciones` no mira el final
 * del documento.
 */
const MAX_BUFFER_MB = 256;

/**
 * Umbral por defecto, en segundos.
 *
 * No es un numero puesto a ojo: sale de medir la suite en esta maquina. De 927
 * casos, 301 s en total, con la mediana en 0.00 s —casi todos son triviales— y
 * una cola larga: 13 casos pasan de 6 s y solo 4 pasan de 8 s. Entre 6 y 8 esta
 * el salto de lo que son tests de audio de verdad (campañas de MIDI, ventanas de
 * latencia, A/B de tres corridas) y lo que es un test que se ha quedado
 * colgado esperando algo.
 *
 * Ponerlo en 5 era demasiado bajo: marcaba veinte casos que solo son lentos. Y
 * subirlo a 20 dejaria pasar los dos casos de mas de 30 s, que son justo los
 * que hay que mirar. Con 8 sale una lista corta y accionable.
 */
const UMBRAL_POR_DEFECTO = 8;

/**
 * Saca un `TestCase` y su duracion del XML de Catch2.
 *
 * Se lee el XML con una expresion regular y no con un parser de verdad porque el
 * que produce Catch2 lo emite en un solo formato estable, y meter una
 * dependencia de parseo para leer once atributos seria mas codigo del que
 * justifica. La expresion esta limitada a lo que el reporter emite: el nombre
 * del test, el fichero de origen, y el `OverallResult` que cierra el caso.
 *
 * El nombre puede llevar `&amp;` y `<` escapados, y NO se desescapan: el informe
 * se lee, no se vuelve a pasar a un parser. Un nombre se ve mejor escapado que
 * roto.
 *
 * @param {string} xml el documento entero.
 * @returns {{nombre: string, fichero: string, segundos: number}[]}
 */
export function leerDuraciones(xml) {
  const salida = [];
  // `[^>]*` para los atributos se traga la mitad de los casos. La razon: un
  // nombre de test puede llevar un `>` dentro, y el reporter lo escribe como
  // `&gt;`. Al buscar el primer `>` CRUDO —que es justo el que cierra la
  // etiqueta— la expresion corta por la `t` de `&gt;`, se queda con los
  // atributos a medias, y el `name="` ya no cierra nunca.
  //
  // En esta suite son 11 casos de 927, y no se notaba porque todos acababan con
  // "(sin nombre)" y ese `(sin nombre)` se repetia once. Un parser que inventa
  // un nombre en vez de fallar es un parser que miente en silencio.
  //
  // El nombre se lee con `[^"]*`, que para en la comilla que lo cierra, y no con
  // `[^>]*`, que para en un `>`. La diferencia es que un `&gt;` escapado
  // dentro del nombre NO es el cierre de la etiqueta, y `[^>]*` lo confunde:
  // corta por la `t`, se queda con los atributos a medias, y el `name="` ya no
  // cierra nunca. Con `[\s\S]*?` hasta `</TestCase>` el caso entero se lee
  // siempre, y el nombre se extrae de dentro con su propia expresion.
const re = /<TestCase\b[\s\S]*?\bname="([^"]*)"[\s\S]*?<\/TestCase>/g;
  let caso;

  while ((caso = re.exec(xml)) !== null) {
    const nombre = caso[1];
    const cuerpo = caso[0];

    // La duracion del CASO esta en el `<OverallResult>` que lo cierra, que es
    // hijo directo del TestCase. Las de cada `<Section>` son de otra cosa: se
    // puede repetir el mismo nombre de seccion y cada una mide solo la suya.
    const overall = cuerpo.match(/<OverallResult\b[^>]*durationInSeconds="([^"]+)"/);

    if (overall === null)
      continue;

    // Un `TestCase` sin `name` no entra en el bucle de arriba, porque la expresion
    // exige `name="`. No se le inventa un nombre: un «(sin nombre)» en el informe
    // seria una fila que no dice que test es, y quien lo lee no puede buscarla
    // en el codigo.
const fichero = (cuerpo.match(/\bfilename="([^"]*)"/) ?? [, '(sin fichero)'])[1];

    const segundos = Number(overall[1]);

    if (Number.isFinite(segundos))
      salida.push({ nombre, fichero, segundos });
  }

  return salida;
}

/**
 * El resumen que se imprime: los mas lentos primero, y la cuenta de los que
 * pasan del umbral.
 *
 * @param {{nombre: string, fichero: string, segundos: number}[]} duraciones
 * @param {number} umbral segundos a partir de los cuales se avisa.
 * @returns {string[]} lineas para imprimir.
 */
export function resumen(duraciones, umbral = UMBRAL_POR_DEFECTO) {
  const lineas = [];
  const total = duraciones.reduce((a, d) => a + d.segundos, 0);

  if (duraciones.length === 0) {
    lineas.push('No se ha medido ningun test. El XML no tiene <TestCase> con duracion.');
    lineas.push('Si la suite se cuelga ANTES de terminar el primer caso, el XML esta');
    lineas.push('truncado y no hay nada que medir: eso es un cuelgue de verdad.');
    return lineas;
  }

  const lentos = duraciones
    .filter((d) => d.segundos > umbral)
    .sort((a, b) => b.segundos - a.segundos);

  lineas.push('');
  lineas.push(`SUITE CRONOMETRADA: ${duraciones.length} casos, ${total.toFixed(1)} s en total`);
  lineas.push('='.repeat(72));

  if (lentos.length === 0) {
    lineas.push(`Ningun caso pasa de ${umbral} s.`);
    return lineas;
  }

  lineas.push(`CASOS POR ENCIMA DE ${umbral} s (${lentos.length}):`);
  lineas.push('-'.repeat(72));

  for (const l of lentos)
    lineas.push(`  ${l.segundos.toFixed(1).padStart(7)} s  ${l.nombre}`);

  lineas.push('');
  lineas.push('Un caso lento no es un caso roto: mide el tiempo que TARDO, no si');
  lineas.push('fallo. Un caso que se cuelga no aparece aqui, porque nunca acaba.');

  return lineas;
}

// ─────────────────────────────────────────────────────────────────────────
// LA LINEA BASE: QUE SE GUARDA, Y POR QUE ES UN JSON Y NO UNA TABLA
//
// Se guarda un JSON, no una tabla de texto, por dos razones que se ven al
// acabarlo. La primera es que los nombres llevan acentos, `&amp;`, `&gt;` y
// parentesis: cualquier separador que se elija acaba colisionando con algun
// nombre, y un fichero de duraciones que se parsea con `split(';')` tiene un
// fallo silencioso esperando. JSON escapa todo solo.
//
// La segunda es que hace falta guardar ALGO MAS QUE EL TIEMPO. Un nombre puede
// cambiar —porque el test se renombre— y entonces comparar por nombre daria
// «test nuevo, 12 s» y «test desaparecido», cuando en realidad es el mismo test
// de siempre con otro nombre. Por eso va tambien el fichero de origen: si el
// nombre no esta pero el fichero si, es un rename y no hay que avisar.
//
// ─────────────────────────────────────────────────────────────────────────
// QUE SIGNIFICA "SE HA DUPLICADO", Y POR QUE UN FACTOR Y NO UNA DIFERENCIA
//
// El factor y no los segundos, porque un test que dura 0.01 s y pasa a 0.02 s ha
// duplicado su tiempo y no se ha enterado nadie; uno que pasa de 9 s a 18 s es el
// mismo doubled y sí se ha enterado todo el mundo. Comparar segundos absolutos
// solo mide un problema a partir de cierto tamaño, que es justo donde no hace
// falta que te lo digan.
//
// Y el umbral de 8 s de antes se SIGUE aplicando: son dos preguntas distintas. Una
// es «este test es lento hoy»; la otra es «este test se ha puesto mas lento que
// la ultima vez». Un test de 12 s puede ser normal, y avisar cada vuelta de el
// cansa tanto que deja de leerse — que es como un rojo permanente se apaga.

/** Por debajo de este tiempo, ningun cambio se considera una regresión. */
const PISO_DE_INTERES_S = 0.05;

/** Factor por el que se considera que un test se ha volcado. */
const FACTOR_POR_DEFECTO = 2;

/** El fichero donde vive la referencia. */
export const RUTA_BASE = join(raiz, 'tools', 'duraciones-referencia.json');

/**
 * Vuelca las duraciones al formato de referencia.
 *
 * @param {{nombre: string, fichero: string, segundos: number}[]} duraciones
 * @param {string} [xml] el XML del que salio, para dejar constancia de como se
 * midio. Sin esto, una referencia no dice si se midio con la maquina descargada
 * o con diez compilaciones en paralelo, y esa diferencia explica todas las
 * regresiones falsas.
 * @returns {object} el objeto que se escribe en disco.
 */
export function construirBase(duraciones, xml = '') {
  return {
    // La version del FORMATO, no la del script. Se sube solo si cambia la forma
    // de los campos, para que una base vieja se pueda recusar en vez de
    // compararse con una nueva y dar diferencias inventadas.
    version: 1,
    medidoEn: new Date().toISOString(),
    casos: duraciones.length,
    totalSegundos: Number(duraciones.reduce((a, d) => a + d.segundos, 0).toFixed(3)),
    filtros: FILTROS,
    casos_: Object.fromEntries(
      duraciones.map((d) => [d.nombre, {
        s: Number(d.segundos.toFixed(4)),
        f: d.fichero,
      }])
    ),
  };
}

/**
 * Lee la referencia de disco.
 *
 * @param {string} [ruta]
 * @returns {object|null} la referencia, o `null` si no hay o no se entiende.
 */
export function leerBase(ruta = RUTA_BASE) {
  if (!existsSync(ruta))
    return null;

  try {
    const base = JSON.parse(readFileSync(ruta, 'utf8'));

    // Una base de otra version del FORMATO no se compara: sus campos pueden
    // querer decir otra cosa, y compararlos daria diferencias que no existen.
    if (base?.version !== 1)
      return null;

    return base;
  }
  catch (e) {
    // Una base corrupta es un fichero de referencia roto, no un fallo del
    // analisis. Se avisa y se sigue como si no hubiera, que es lo que hace que
    // la primera vuelta regenere la referencia en vez de bloquear.
    console.error(`[referencia] no se pudo leer ${ruta}: ${e.message}`);
    return null;
  }
}

/**
 * La identidad de un fichero de test, sin la parte que cambia con la maquina.
 *
 * La referencia guarda la ruta ABSOLUTA de cada caso, porque hace falta saber de
 * donde viene. El problema es que esa ruta lleva el disco, el proyecto y el
 * usuario, y ninguno de los tres es igual en otra maquina. Comparar por ruta
 * entera hace que el renombrado —que se detecta justamente por el fichero— deje
 * de detectarse en cuanto se cambia de equipo, y entonces el mismo test sale a
 * la vez como nuevo y como ausente: dos avisos para un solo test, y el segundo
 * es falso.
 *
 * Aqui solo se queda el nombre del fichero, que en este repo es unico. Si
 * Alguna vez hubiera dos ficheros con el mismo nombre en carpetas distintas, este
 * emparejamiento los confundiria, y se veria como un renombrado que empareja con
 * el test equivocado. Es la unica suposicion de toda la comparacion, asi que
 * conviene que siga siendo cierta.
 *
 * @param {string} fichero la ruta, como la deje el reporter de Catch2.
 * @returns {string} el nombre del fichero, con `/` en vez de `\`.
 */
export function claveDeFichero(fichero) {
  const segmentos = String(fichero ?? '').replace(/\\/g, '/').split('/').filter(Boolean);

  return segmentos.length === 0 ? '(sin fichero)' : segmentos[segmentos.length - 1];
}

/**
 * Compara una medicion contra la referencia.
 *
 * @param {{nombre: string, fichero: string, segundos: number}[]} duraciones
 * @param {object} base la referencia, o `null` si no hay.
 * @param {number} factor el multiplicador a partir del cual se considera que se
 * ha duplicado.
 * @returns {{regresiones: object[], nuevos: object[], ausentes: number, factor: number}}
 */
export function compararConBase(duraciones, base, factor = FACTOR_POR_DEFECTO) {
  const regresiones = [];
  const nuevos = [];

  if (base === null)
    return { regresiones, nuevos, ausentes: 0, factor };

  const previos = base.casos_ ?? {};

  // Una LISTA de nombres por fichero, no uno. Uno valia mientras cada test
  // estuviera en su propio fichero, y dejo de valer en cuanto un fichero tuvo
  // dos: el segundo se emparejaba con el tiempo del primero —el que hubiera
  // aparecido antes en la referencia—, y una regresion se comparaba con el
  // historial de otro test. Con la lista, cada nombre se empareja una vez.
  const porFichero = new Map();

  for (const [nombre, dato] of Object.entries(previos)) {
    const clave = claveDeFichero(dato.f);

    if (!porFichero.has(clave))
      porFichero.set(clave, []);

    porFichero.get(clave).push(nombre);
  }

  // Los nombres de la referencia que ya se han emparejado con algo de esta
  // medicion. Sirve para dos cosas: para que un renombrado no cuente dos veces,
  // y para que un nombre no se gasta en dos casos medidos.
  const vistos = new Set();
  const emparejados = [];
  const sinEmparejar = [];

  // ── Primera pasada: por nombre, que es el caso normal ──
  for (const d of duraciones) {
    if (previos[d.nombre] === undefined) {
      sinEmparejar.push(d);
      continue;
    }

    vistos.add(d.nombre);
    emparejados.push({ d, nombrePrevio: d.nombre });
  }

  // ── Segunda pasada: renombrados, decididos por el fichero de origen ──
  //
  // Un caso cuyo nombre no esta en la referencia puede ser un test nuevo o un
  // test renombrado, y el fichero es lo que lo dice: si en ese fichero queda
  // algun nombre de la referencia sin emparejar, ese es su nombre viejo.
  //
  // Marcarlo como emparejado es JUSTO lo que evita el aviso doble: antes, un
  // caso sin pareja se empujaba a `nuevos` y se pasaba de largo, sin dejar rastro
  // de que era el mismo test de antes. Salia «test nuevo» y «test desaparecido»
  // para un solo test, y de los dos el segundo era mentira.
  for (const d of sinEmparejar) {
    const candidatos = (porFichero.get(claveDeFichero(d.fichero)) ?? [])
      .filter((nombre) => !vistos.has(nombre));

    if (candidatos.length === 0) {
      nuevos.push(d);
      continue;
    }

    // Con un solo candidato no hay duda. Con varios, se empareja con el tiempo
    // anterior mas parecido: un renombrado no cambia cuanto tarda el test, y
    // esa es la unica pista que queda cuando en un mismo fichero se han
    // renombrado o borrado varios a la vez. Es una suposicion y el informe la
    // senala: el renombrado sale marcado, y si el emparejamiento fuera erroneo se
    // veria en el tiempo que aparece al lado.
    const nombrePrevio = candidatos.length === 1
      ? candidatos[0]
      : candidatos.reduce((mejor, nombre) => (
        Math.abs(previos[nombre].s - d.segundos)
          < Math.abs(previos[mejor].s - d.segundos) ? nombre : mejor));

    vistos.add(nombrePrevio);
    emparejados.push({ d, nombrePrevio });
  }

  for (const { d, nombrePrevio } of emparejados) {
    const antes = previos[nombrePrevio].s;
    const despues = d.segundos;

    // El piso evita el ruido de los tests que duran microsegundos: pasar de
    // 0.0001 s a 0.0002 s es ruido, no una regresion, y si se avisa de eso
    // el informe deja de tener señal.
    if (antes < PISO_DE_INTERES_S || despues < PISO_DE_INTERES_S)
      continue;

    const ratio = despues / antes;

    if (ratio >= factor) {
      regresiones.push({
        nombre: d.nombre,
        antes,
        despues,
        factor: Number(ratio.toFixed(1)),
        renombrado: nombrePrevio !== d.nombre,
      });
    }
  }

  let ausentes = 0;

  for (const nombre of Object.keys(previos)) {
    if (!vistos.has(nombre))
      ausentes += 1;
  }

  return { regresiones, nuevos, ausentes, factor };
}

/** El informe de la comparacion, en lineas. */
export function resumenBase(cmp) {
  const lineas = [];

  if (cmp.regresiones.length === 0 && cmp.nuevos.length === 0 && cmp.ausentes === 0)
    return lineas;

  lineas.push('');
  lineas.push('CONTRA LA REFERENCIA');
  lineas.push('='.repeat(72));

  if (cmp.regresiones.length > 0) {
    lineas.push(`Se han volcado o mas (x${cmp.factor} o mas):`);
    lineas.push('-'.repeat(72));

    for (const r of cmp.regresiones.sort((a, b) => b.factor - a.factor)) {
      lineas.push(`  x${String(r.factor).padStart(5)}  ${r.antes.toFixed(2)} s -> ${r.despues.toFixed(2)} s`);
      lineas.push(`          ${r.nombre}${r.renombrado ? '   (renombrado)' : ''}`);
    }

    lineas.push('');
  }

  if (cmp.nuevos.length > 0) {
    lineas.push(`Casos nuevos, sin referencia (${cmp.nuevos.length}).`);

    // Solo los que tardan: un caso nuevo de 0.01 s no es un dato que interese.
    const lentos = cmp.nuevos.filter((n) => n.segundos >= PISO_DE_INTERES_S);

    for (const n of lentos.slice(0, 15))
      lineas.push(`  ${n.segundos.toFixed(2).padStart(7)} s  ${n.nombre}`);

    if (lentos.length > 15)
      lineas.push(`  ... y ${lentos.length - 15} mas`);
  }

  // Avisar de los ausentes y seguir como si nada es lo que hacia que este
  // fallo pasara inadvertido. Ahora la misma linea sale con codigo 1, en `main`.
  if (cmp.ausentes > 0)
    lineas.push(`${cmp.ausentes} caso(s) de la referencia no estan en esta medicion.`);

  return lineas;
}

// `--xml` es para mirar una vuelta anterior SIN volver a correr la suite. Para
// medir de verdad, no hay bandera: se corre y ya. Este comentario esta aqui
// porque `build.bat` hizo justo el error contrario — pasar el ejecutable a
// `--xml` — y por eso conviene dejar escrito cual es cual.
const SIN_XML = 'SIN_XML_POR_DEFECTO';

export const SIN_XML_MARCA = SIN_XML;

// ─────────────────────────────────────────────────────────────────────────
// QUE UNA MEDICION TRUNCADA NO SE PAREZCA A UNA MEDICION ENTERA
//
// Todo este bloque se apoya en una idea: un XML cortado por la mitad NO se
// distingue de uno entero mirando los `<TestCase>`, porque los que le caben
// estan completos. `leerDuraciones` los lee sin quejarse, el recuento sale mas
// corto que el de la referencia, y el informe parece un informe — con 740 casos
// de menos y sin decir por que. Asi se produjo el fallo que esto tapa: un
// cuelgue de la suite se informaba como «la referencia tiene mas casos que esta
// medicion» y se salia con 0, que es la peor forma de fallar, porque un verde.
//
// Por eso se mira el resultado del spawn y no solo su salida, y por eso se
// comprueba que el documento este cerrado: el mismo defecto tiene que verse
// tambien en el XML que alguien guardo hace dias, que es por donde se cuela si
// la comprobacion se deja solo en el camino de correr la suite.

/**
 * El cierre del documento que emite Catch2.
 *
 * Es `</Catch2TestRun>` y no `</Catch>`, porque el nombre de la etiqueta raiz
 * lleva la version del reporter dentro. Buscar el cierre equivocado haria que
 * NINGUN documento pareciera truncado, que es justo el fallo que esto tapa.
 */
const CIERRE_XML = '</Catch2TestRun>';

/**
 * Si un documento se ha cortado antes de cerrarse.
 *
 * Se mira el final del documento y no si el numero de casos cuadra con el de la
 * referencia, porque son dos preguntas distintas y las dos se hacen: esta dice
 * «el fichero esta entero», y `cuadraLaCuenta` dice «estos son todos los que
 * había». Un XML truncado a tres cuartos tiene la mitad de los casos y el
 * documento sin cerrar; uno entero al que un filtro mal escrito deja fuera la
 * mitad tambien sale con la mitad, pero se puede comparar con lo que se tenga.
 *
 * @param {string} xml el documento entero.
 * @returns {boolean} `true` si le falta el cierre.
 */
export function xmlTruncado(xml) {
  return !xml.includes(CIERRE_XML);
}

/**
 * El fallo de un `spawnSync` que no se ve mirando su salida.
 *
 * Devuelve `null` cuando el proceso ha terminado por su cuenta, y el motivo
 * cuando no. Va separado del `spawnSync` para poder probarlo sin lanzar nada:
 * un fallo que solo se reproduce colgando la suite es un fallo que no se
 * comprueba nunca.
 *
 * Lo unico delicado es el codigo de salida: Catch2 sale con el numero de casos
 * fallidos, de modo que 1 a 255 es una suite con tests rojos, que se cronometra
 * igual de bien. Lo que no es Catch2 es un 3221225477 —un Access Violation de
 * Windows—, ni un `status: null` con senal, ni un error de Node.
 *
 * @param {object} r lo que devuelve `spawnSync`.
 * @returns {string|null} por que no ha terminado, o `null` si ha terminado.
 */
export function falloDeSpawn(r) {
  const codigo = r?.error?.code;

  // El caso principal. El corte por reloj deja la salida puesta, y sin mirar
  // aqui el XML truncado se analiza como si fuera una medicion buena.
  if (codigo === 'ETIMEDOUT')
    return `no ha terminado en ${LIMITE_MS / 60000} min y se ha matado al agotar el limite`;

  // El mismo fallo por el otro lado: escribir de mas tambien corta la salida, y
  // tambien deja un XML que parece entero hasta el final.
  if (codigo === 'ENOBUFS')
    return `ha escrito mas de los ${MAX_BUFFER_MB} MB del buffer y se ha cortado la salida`;

  if (r?.signal)
    return `ha terminado por la senal ${r.signal}`;

  if (r?.error)
    return `no se ha podido lanzar: ${r.error.message}`;

  // Por encima de 255 no es un numero de tests fallidos: es una muerte
  // inesperada, y el XML que deja detras esta a medias tambien.
  if (typeof r?.status === 'number' && (r.status < 0 || r.status > 255))
    return `ha terminado de forma anormal, con codigo ${r.status}`;

  // `status: null` sin senal ni error no deberia pasar. Si pasa, es que el
  // proceso no ha terminado y nadie sabe por que, y ante eso la postura que
  // sale cara es decir que no ha terminado: es el fallo que se cuela por alto.
  if (r?.status === null || r?.status === undefined)
    return 'no ha informado de como ha terminado';

  return null;
}

/**
 * Escribe un fichero sin dejar nunca a medias el que ya habia.
 *
 * `writeFileSync` abre el destino en modo truncado, de modo que si el proceso
 * muere a mitad —un corte, un antivirus que se lleva el fichero por delante, dos
 * cronometros corriendo a la vez— lo que queda es un JSON truncado. Y una
 * referencia ilegible no duele: `leerBase` avisa y sigue como si no hubiera, de
 * modo que lo que se pierde no es la referencia nueva, que se regenera, sino la
 * VIEJA, que ya no esta en ninguna parte y era la unica que se tenia.
 *
 * Se escribe a un temporal y se renombra encima. En Windows el renombrado
 * sustituye el destino porque Node usa `MOVEFILE_REPLACE_EXISTING`, y con eso el
 * destino viejo esta entero o no esta: no hay un punto en el que se vea a medias.
 *
 * @param {string} ruta donde queda el fichero bueno.
 * @param {string} contenido lo que lleva dentro.
 */
function escribirEntero(ruta, contenido) {
  // El `pid` en el nombre evita que dos escrituras simultaneas se pisen el
  // temporal una a otra. No es decorativo: el caso de dos cronometros a la vez
  // es justo el que produce este fichero corrupto.
  const temporal = `${ruta}.${process.pid}.tmp`;

  try {
    writeFileSync(temporal, contenido, 'utf8');
    renameSync(temporal, ruta);
  }
  catch (e) {
    // El temporal no se deja tirado: se acumula en la carpeta de tools con cada
    // intento, y un JSON a medias con nombre de temporal confunde mas de lo que
    // ayuda a encontrar.
    try {
      rmSync(temporal, { force: true });
    }
    catch {
      // Si el temporal tampoco se puede borrar, el error que importa es el de
      // antes: el de la escritura que no se ha podido completar.
    }

    throw e;
  }
}

/**
 * Corre la suite capturando el XML con duraciones, o diciendo por que no hay.
 *
 * @param {string[]} args los argumentos que se pasan a Catch2.
 * @returns {{xml: string|null, motivo: string|null, salida: string}} el XML, o
 * `null` con el motivo; `salida` siempre, porque es lo que hay que enseñar para
 * entender por que.
 */
function capturarXml(args) {
  const r = spawnSync(SUITE, ['-r', 'xml', '-d', 'yes', ...args, ...FILTROS], {
    cwd: raiz,
    encoding: 'utf8',
    maxBuffer: MAX_BUFFER_MB * 1024 * 1024,
    timeout: LIMITE_MS,
    windowsHide: true,
  });

  const salida = `${r.stdout ?? ''}${r.stderr ?? ''}`;
  const motivo = falloDeSpawn(r);

  return { xml: motivo === null ? salida : null, motivo, salida };
}

/**
 * Comprueba que la medicion cubra la referencia entera.
 *
 * Que el numero de casos no cuadre no es por si mismo un fallo: la referencia es
 * de un dia y desde entonces la suite ha tenido tests nuevos. Lo que no puede
 * pasar es que falten, porque entonces la comparacion de tiempos se hace sobre
 * una poblacion mas pequena, y los que faltan no se eligen al azar: son
 * precisamente los que no llegaron a correr, que son los que tardaban.
 *
 * @param {{nombre: string}[]} duraciones lo medido en esta vuelta.
 * @param {object|null} base la referencia, o `null` si no hay.
 * @param {number} ausentes los que `compararConBase` no ha visto.
 * @returns {{ok: boolean, medidos: number, esperados: number, lineas: string[]}}
 */
export function cuadraLaCuenta(duraciones, base, ausentes) {
  const medidos = duraciones.length;
  const esperados = base?.casos ?? 0;
  const lineas = [];

  if (base === null)
    return { ok: true, medidos, esperados: 0, lineas };

  // Una referencia que no cuadra consigo misma no sirve para contar lo que falta:
  // diria que estan todos aqui los que no estan en ninguna parte.
  const guardados = Object.keys(base.casos_ ?? {}).length;

  if (guardados !== esperados)
    return {
      ok: false,
      medidos,
      esperados,
      lineas: [
        `La referencia dice ${esperados} casos y tiene ${guardados}.`,
        'No se puede saber cual falta. Se regenera con --guardar-referencia.',
      ],
    };

  lineas.push(`Medidos ${medidos}, contra ${esperados} de la referencia.`);

  if (medidos < esperados) {
    lineas.push(`Faltan ${esperados - medidos} caso(s) para poder comparar.`);
    lineas.push('Una medicion incompleta no dice que un test se haya puesto lento:');
    lineas.push('dice que no se ha medido.');
  }
  else if (ausentes > 0) {
    // Aqui el total puede cuadrar y aun asi faltar, que es el caso que mas
    // confunde: se ha medido un caso nuevo para tapar el hueco de uno que no se
    // ha medido. Un caso nuevo no tapa uno que falta — son mediciones distintas—,
    // y por eso se sale con 1 aunque las cuentas den igual.
    lineas.push(`${ausentes} caso(s) de la referencia no estan, aunque el total cuadre.`);
    lineas.push('Un caso nuevo no tapa uno que falta: son mediciones distintas.');
  }
  else if (medidos > esperados)
    lineas.push(`Sobran ${medidos - esperados}, y salen como casos nuevos.`);

  return { ok: !(medidos < esperados || ausentes > 0), medidos, esperados, lineas };
}

function main(argumentos) {
  const indice = argumentos.indexOf('--umbral');
  const umbral = indice >= 0 ? Number(argumentos[indice + 1]) : UMBRAL_POR_DEFECTO;
  const soloAvisar = argumentos.includes('--solo-avisar');
  const indiceXml = argumentos.indexOf('--xml');
  const guardar = argumentos.includes('--guardar-referencia');
  const indiceFactor = argumentos.indexOf('--factor');
  const factor = indiceFactor >= 0 ? Number(argumentos[indiceFactor + 1]) : FACTOR_POR_DEFECTO;
  const indiceBase = argumentos.indexOf('--base');
  const rutaBase = indiceBase >= 0 ? argumentos[indiceBase + 1] : RUTA_BASE;

  let xml;
  // Por que no hay un XML entero, si es que no lo hay. Es `null` cuando si lo
  // hay: un fallo y una captura sin casos se distinguen a proposito, porque solo
  // el primero es un fallo.
  let motivoCorte = null;

  if (indiceXml >= 0) {
    // Un XML ya capturado se analiza sin volver a correr la suite, que es lo
    // que permite mirar el resultado de una vuelta anterior sin esperar otra.
    const ruta = argumentos[indiceXml + 1];

    if (!existsSync(ruta)) {
      console.error(`No existe el fichero XML: ${ruta}`);
      return 2;
    }

    xml = readFileSync(ruta, 'utf8');
  }
  else {
    if (!existsSync(SUITE)) {
      // La ruta ENTERA, no el nombre del fichero. Si el binario no esta, lo que
      // hace falta es saber DONDE se ha mirado, y `ABDAudioLab_Tests.exe` a
      // secas no lo dice. Hay dos `build/Release` en juego —el que genera
      // build.bat y el de una compilacion manual— y distinguirlos cuesta media
      // hora de buscar el exe en el arbol equivocado.
      console.error(`No esta la suite compilada en: ${SUITE}`);
      console.error('Se compila con:  build.bat tests');
      return 2;
    }

    console.error('Midiendo la suite. Tarda unos minutos; no es un cuelgue.');
    // Se filtran las banderas y SUS VALORES para que un `--base otra.json` no le
    // pase el `otra.json` a Catch2, que lo interpretaria como un filtro de test
    // sin ningun caso detras y mediria cero sin decir por que.
    const paraCatch = paraCatchDe(argumentos);

    const captura = capturarXml(paraCatch);

    if (captura.xml === null)
      motivoCorte = captura.motivo;
    else
      xml = captura.xml;

    if (motivoCorte !== null) {
      // Se enseña el final de la salida porque es donde esta el ultimo caso que
      // llego a correr, y por lo tanto donde se ve en que test se quedo parado.
      const cola = captura.salida.trimEnd().split('\n').slice(-15);

      console.error('');
      console.error(`LA SUITE NO HA TERMINADO: ${motivoCorte}.`);
      console.error('Lo que salio de ella es una medicion incompleta, y no se analiza.');
      console.error('');
      console.error('Ultimas lineas de la salida:');
      console.error(cola.join('\n'));
      return 1;
    }
  }

  // El documento sin cerrar esta truncado, y da igual por que se truncara: por
  // un cuelgue, por una muerte inesperada, o porque el fichero que alguien
  // guardo con `--xml` estaba a medias. La comprobacion va DESPUES de leer y no
  // dentro de `capturarXml`, porque `--xml` es una puerta de entrada igual que
  // correr la suite, y si solo se comprueba en la segunda, por la primera se
  // cuela un XML truncado tan tranquilo.
  //
  // `--solo-avisar` no lo excusa, y no es una excepcion: ese flag silencia los
  // avisos sobre lo RUIDO, y un XML sin cerrar no es ruido, es una medicion que
  // no se puede creer. Un guard que se puede apagar con una bandera no es un
  // guard.
  if (xmlTruncado(xml)) {
    console.error('');
    console.error('EL XML ESTA TRUNCADO: le falta el cierre del documento.');
    console.error('Un cuelgue de la suite se ve exactamente asi: los casos que le');
    console.error('caben estan enteros, de modo que el XML parece bueno mientras no');
    console.error('se mire el final. Se relanza la medicion entera.');
    console.error(`Venia de: ${indiceXml >= 0 ? argumentos[indiceXml + 1] : 'la captura de esta vuelta'}.`);
    return 1;
  }

  const duraciones = leerDuraciones(xml);

  for (const linea of resumen(duraciones, umbral))
    console.log(linea);

  // Un XML entero con CERO casos dentro no es una suite rapida: es una medicion
  // que no ha medido nada, y salir con 0 de ahi es un verde que no dice nada.
  if (duraciones.length === 0) {
    console.error('');
    console.error('La medicion no tiene ni un caso. Sin casos no hay nada que comparar.');
    return 1;
  }

  const lentos = duraciones.filter((d) => d.segundos > umbral).length;

  // ── Contra la referencia ──
  //
  // Va DESPUES del informe de lentos y con su propio codigo de salida, porque
  // son dos preguntas distintas: una es «que test es lento ahora» y la otra «que
  // test se ha puesto lento». Se informa de las dos y se sale con 1 si hay
  // cualquier cosa, para que un pipeline no tenga que saber cual de las dos
  // preguntas era la importante.
  const base = leerBase(rutaBase);

  if (guardar) {
    // Guardar y comparar a la vez daria un verde de comparacion contra uno
    // mismo, que no compara nada. Por eso la referencia que se acaba de escribir
    // NO es la que se usa para comparar en esta misma vuelta.
    escribirEntero(rutaBase, `${JSON.stringify(construirBase(duraciones, xml), null, 2)}\n`);
    console.error('');
    // La ruta entera, por el mismo motivo que la de la suite: `--base` puede
    // apuntar a cualquier parte, y un nombre a secas no dice donde ha quedado la
    // referencia que se acaba de escribir.
    console.error(`Referencia guardada en ${rutaBase}: ${duraciones.length} casos.`);
    console.error('La siguiente vuelta comparara contra esta.');
    return lentos > 0 && !soloAvisar ? 1 : 0;
  }

  if (base === null) {
    console.error('');
    console.error('No hay referencia con la que comparar. Se guarda una con:');
    console.error(`  node tools/duraciones-suite.mjs --guardar-referencia`);
  }
  else {
    const cmp = compararConBase(duraciones, base, factor);
    const cuenta = cuadraLaCuenta(duraciones, base, cmp.ausentes);

    for (const linea of resumenBase(cmp))
      console.log(linea);

    if (cuenta.lineas.length > 0) {
      console.log('');
      console.log('LA CUENTA DE CASOS');
      console.log('='.repeat(72));

      for (const linea of cuenta.lineas)
        console.log(linea);
    }

    // Los dos fallos se juntan ANTES de salir, para que una vuelta que tenga los
    // dos no pueda tapar uno con el otro: se informa de los dos y se sale con 1.
    const fallos = [];

    if (cmp.regresiones.length > 0)
      fallos.push(`${cmp.regresiones.length} caso(s) se han volcado o mas respecto a la referencia.`);

    // Los ausentes salen con 1 tambien. Avisar de 740 casos que no se han medido
    // y seguir con 0 es exactamente lo que hacia que un cuelgue se informara
    // como un informe: el aviso estaba ahi, pero su codigo de salida decia que
    // todo iba bien, y es el codigo de salida lo que lee un pipeline.
    if (!cuenta.ok)
      fallos.push('La medicion no cubre la referencia entera.');

    if (fallos.length > 0) {
      console.error('');
      console.error(fallos.join('\n'));
      console.error(`Referencia del ${base.medidoEn}, ${base.casos} casos.`);
      return 1;
    }
  }

  if (lentos > 0 && !soloAvisar) {
    console.error('');
    console.error(`${lentos} caso(s) por encima de ${umbral} s. Sale con 1 para que un`);
    console.error('pipeline lo vea, aunque el suite haya pasado.');
    return 1;
  }

  return 0;
}

// Solo cuando se ejecuta como programa. Importado desde un test, no.
if (process.argv[1] && existsSync(process.argv[1])
    && process.argv[1].replace(/\\/g, '/').endsWith('duraciones-suite.mjs'))
  process.exit(main(process.argv.slice(2)));

// El ejecutable, para el test.
export const RUTA_SUITE = SUITE;
export const FILTROS_POR_DEFECTO = FILTROS;
export const UMBRAL = UMBRAL_POR_DEFECTO;
export const PISO_DE_INTERES = PISO_DE_INTERES_S;
export const FACTOR = FACTOR_POR_DEFECTO;
export const BASE = RUTA_BASE;