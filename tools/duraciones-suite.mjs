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

import { readFileSync, existsSync } from 'node:fs';
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
  const re = /<TestCase\b([^>]*)>([\s\S]*?)<\/TestCase>/g;
  let caso;

  while ((caso = re.exec(xml)) !== null) {
    const atributos = caso[1];
    const cuerpo = caso[2];

    // La duracion del CASO esta en el `<OverallResult>` que lo cierra, que es
    // hijo directo del TestCase. Las de cada `<Section>` son de otra cosa: se
    // puede repetir el mismo nombre de seccion y cada una mide solo la suya.
    const overall = cuerpo.match(/<OverallResult\b[^>]*durationInSeconds="([^"]+)"/);

    if (overall === null)
      continue;

    const nombre = (atributos.match(/\bname="([^"]*)"/) ?? [, '(sin nombre)'])[1];
    const fichero = (atributos.match(/\bfilename="([^"]*)"/) ?? [, '(sin fichero)'])[1];

    // `Number()` y no un parseo a mano: convierte `2.3e-05`, que Catch2 escribe
    // en notacion cientifica cuando el tiempo es pequeño, sin inventarse un
    // numero. Un comparador de cadenas las ordenaria al reves.
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
    xml = capturarXml(argumentos.filter((a) => !a.startsWith('--') && a !== String(umbral)));
  }

  const duraciones = leerDuraciones(xml);

  for (const linea of resumen(duraciones, umbral))
    console.log(linea);

  const lentos = duraciones.filter((d) => d.segundos > umbral).length;

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