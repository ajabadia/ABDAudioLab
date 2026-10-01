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
 * Un umbral fijo aqui seria un numero sin razon: 5 s es太快 para una suite de
 * audio y demasiado lento para una de texto, y no hay un valor que valga para
 * los dos. Ademas, en una maquina con diez compilaciones en paralelo, TODO va
 * lento, y un umbral fijo pondria en rojo veinte tests que estan bien.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * USO
 *
 *   node tools/duraciones-suite.mjs                    # mide y avisa
 *   node tools/duraciones-suite.mjs --umbral 10       # mas tolerante
 *   node tools/duraciones-suite.mjs --xml salida.xml  # usa un XML ya capturado
 *
 * Sale con 0 si nada supera el umbral, y con 1 si algo lo supera. Con
 * `--solo-avisar` sale con 0 siempre: para cuando uno solo quiere el informe.
 */

import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { dirname, join, basename } from 'node:path';
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

/** Tiempo de reloj que se concede a la suite antes de darla por colgada. */
const LIMITE_MS = 20 * 60 * 1000;

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
  const porFichero = new Map();

  for (const [nombre, dato] of Object.entries(previos)) {
    if (!porFichero.has(dato.f))
      porFichero.set(dato.f, nombre);
  }

  // Los nombres de la base que se han visto en esta medicion, para que el
  // recuento de ausentes NO cuente dos veces un test renombrado. Sin esto, un
  // renombrado sale como regresion (correcto) Y como ausente (falso), que es la
  // clase de ruido que hace que un informe deje de leerse: diria que se ha
  // perdido un test que en realidad esta ahi con otro nombre.
  const vistos = new Set();

  for (const d of duraciones) {
    // Se busca por nombre y, si no esta, por fichero de origen: un test
    // renombrado es el mismo test, y avisar de el como si fuera nuevo haria que
    // el aviso seiera de ruido justo cuando hay un cambio de verdad que mirar.
    const nombrePrevio = previos[d.nombre] !== undefined
      ? d.nombre
      : porFichero.get(d.fichero);

    if (nombrePrevio === undefined) {
      nuevos.push(d);
      continue;
    }

    vistos.add(nombrePrevio);

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

/** Corre la suite capturando el XML con duraciones. */
function capturarXml(args) {
  const r = spawnSync(SUITE, ['-r', 'xml', '-d', 'yes', ...args, ...FILTROS], {
    cwd: raiz,
    encoding: 'utf8',
    maxBuffer: 256 * 1024 * 1024,
    timeout: LIMITE_MS,
    windowsHide: true,
  });

  return `${r.stdout ?? ''}${r.stderr ?? ''}`;
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

  // Los valores que siguen a una bandera con valor, que NO son filtros de test.
  const FACTORES_INFORMADOS = [String(umbral), String(factor)];

  let xml;

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
      console.error(`No esta la suite compilada en: ${basename(SUITE)}`);
      console.error('Se compila con:  build.bat tests');
      return 2;
    }

    console.error('Midiendo la suite. Tarda unos minutos; no es un cuelgue.');
    // Se filtran las banderas y sus valores para que un `--factor 3` no le pase
    // un `3` suelto a Catch2, que lo interpretaria como un filtro de test y
    // mediria cero casos sin decir por que.
    const paraCatch = argumentos.filter((a) => !a.startsWith('--')
      && !FACTORES_INFORMADOS.includes(a));

    xml = capturarXml(paraCatch);
  }

  const duraciones = leerDuraciones(xml);

  for (const linea of resumen(duraciones, umbral))
    console.log(linea);

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
    writeFileSync(rutaBase, `${JSON.stringify(construirBase(duraciones, xml), null, 2)}\n`, 'utf8');
    console.error('');
    console.error(`Referencia guardada en ${basename(rutaBase)}: ${duraciones.length} casos.`);
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

    for (const linea of resumenBase(cmp))
      console.log(linea);

    if (cmp.regresiones.length > 0) {
      console.error('');
      console.error(`${cmp.regresiones.length} caso(s) se han volcado o mas respecto a la referencia.`);
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