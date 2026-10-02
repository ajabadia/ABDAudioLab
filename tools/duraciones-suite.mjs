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
 *   node tools/duraciones-suite.mjs --guardar-referencia   # mide DOS vueltas
 *
 * `--guardar-referencia` corre la suite DOS VUELTAS, y no por capricho: la
 * primera da los tiempos y la segunda mide cuanto se mueve la maquina con el
 * MISMO codigo. Ese ruido se guarda en la referencia (`ruido`) y sale en cada
 * comparacion al lado del umbral, que es lo que le da una razon a un numero
 * puesto a ojo. Con `--xml` no hay segunda vuelta que correr —un fichero no se
 * puede volver a medir— y la referencia se guarda sin el, avisando.
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
import os from 'node:os';
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

/** Cuantas veces el total de la referencia se concede antes de darla por colgada. */
const VECES_EL_TOTAL = 4;

/**
 * El total de referencia que se supone cuando no hay ninguna.
 *
 * 300 s por cuatro son 1200 s, que es lo que el limite ha sido siempre. Sin
 * referencia no hay nada mejor que ese numero, y cualquier numero inventado seria
 * mejor que el viejo solo si hubiera una razon —que no hay—.
 */
const TOTAL_SIN_REFERENCIA_S = 300;

/**
 * El tiempo de reloj que se concede a la suite antes de darla por colgada.
 *
 * Sale de la referencia en vez de ser un numero fijo, porque un tope fijo solo
 * significa algo mientras la suite mida lo que media cuando se escribio el tope.
 * Con 20 minutos y una suite de 176 s el tope era catorce veces el trabajo
 * esperado: no era un limite, era un tope nominal, y un cuelgue se acababa
 * descubriendo por el reloj del pipeline y no por aqui.
 *
 * CUATRO VECES, y el numero sale de una asimetria. Equivocarse por lo bajo
 * produce un cuelgue FALSO: una maquina tres veces mas lenta que la de la
 * referencia no esta colgada, esta ocupada, y desde 078b2aa eso sale
 * con codigo 1 y con un mensaje de medicion que no ha terminado —una alarma
 * falsa en el sitio donde mas se lee—. Equivocarse por lo alto solo cuesta
 * esperar. Entre las dos, la que avisa antes de tiempo se equivoca por lo alto.
 *
 * QUE PASA SI LA SUITE CRECE. El limite crece con ella, asi que no hay techo:
 * una vuelta legitima mas larga que el tope solo es posible si la referencia
 * esta vieja, y eso es un problema de la referencia, no del limite. Por eso el
 * mensaje de corte dice cuanto decia la referencia y por cuanto se multiplico,
 * para que se pueda ver la cuenta cuando las dos cosas no cuadren.
 *
 * @param {object|null} base la referencia, o `null` si no hay.
 * @returns {number} milisegundos.
 */
export function limiteDeReloj(base) {
  const total = base?.totalSegundos ?? TOTAL_SIN_REFERENCIA_S;

  return Math.round(total * VECES_EL_TOTAL * 1000);
}

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

/**
 * Por debajo de este tiempo, un caso no se considera una regresión, por muy
 * duplicado que se haya quedado.
 *
 * El número sale de medir la suite CONTIGO MISMA dos veces, porque un umbral
 * puesto a ojo sale caro justo aquí: el factor x2 parece una regla y en la
 * práctica es una lotería.
 *
 * Con el mismo ejecutable y el mismo código, dos vueltas seguidas, 7 de 939
 * casos salen como regresión comparados consigo mismos. Ninguno se ha retrasado:
 * se han movido. Y el reparto de ese ruido dice dónde está el límite:
 *
 *   por debajo de 1 s:  mediana x1,11, p90 x1,79, maximo x3,38. El factor x2
 *                      esta DENTRO de esa banda.
 *   por encima de 1 s:  mediana x1,13, p90 x1,28, maximo x1,43. El factor x2
 *                      esta FUERA de esa, con margen.
 *
 * O sea que el factor no es ni demasiado sensible ni demasiado insensible: es
 * hipersensible justo en la franja donde no hay nada que ver. Por eso el umbral
 * no baja de un segundo aunque el factor se pueda subir, y por eso mira SOLO el
 * tiempo de ahora y no el de antes: un caso que pasa de 50 ms a 2 s ha ido de 0 a
 * 2, y eso es una regresión aunque su ratio sea de 40.
 *
 * Lo que quita, medido sobre los datos de hoy: 7 casos entre 0,06 y 0,87 s. Lo
 * que deja pasar: una regresión de verdad llega a segundos, y ahí la desviación
 * entre vueltas es de x1,43 como mucho.
 *
 * Y lo que hace con los casos nuevos: uno por debajo de un segundo no se lista.
 * El RECUENTO sigue saliendo, que es lo que dice cuantos hay; lo que no sale es
 * una lista de nombres de tests que duran menos que un segundo, que en un
 * fichero de duraciones no aporta nada.
 *
 * ── Y DE DONDE SALEN ESAS CIFRAS, QUE YA NO ESTAN PUESTAS A OJO ──
 *
 * Los numeros de arriba salieron de comparar dos vueltas A MANO, y un numero
 * medido a mano y anotado en un comentario no se vuelve a medir nunca: dentro
 * de un ano el comentario seguira diciendo x3,38 y la maquina habra cambiado,
 * sin que nada avise de que el comentario se quedo anticuado.
 *
 * Por eso al escribir la referencia se miden DOS VUELTAS y el ruido va DENTRO
 * de ella, en `ruido`, y `resumenRuido` lo enseña en cada comparacion al lado
 * del factor. Asi el 1 de aqui no es una constante CREIDA: es una constante que
 * se puede auditar contra el numero que salio en la maquina que la midio, y si
 * un dia el ruido medido deja de dar la razon, el aviso lo dice.
 */
const UMBRAL_ABSOLUTO_S = 1;

/** Factor por el que se considera que un test se ha volcado. */
const FACTOR_POR_DEFECTO = 2;

/** El fichero donde vive la referencia. */
export const RUTA_BASE = join(raiz, 'tools', 'duraciones-referencia.json');

/**
 * La version del FORMATO de la referencia, no la del script. Se sube cuando un
 * campo cambia lo que SIGNIFICA, no cuando aparece uno nuevo.
 *
 * La 2 es la de las rutas relativas al repositorio. La 1 guardaba la ruta
 * ABSOLUTA de cada caso, y esa ruta lleva dentro el disco, el proyecto y el
 * usuario, que no son los mismos en otra maquina: dos personas con el mismo
 * codigo generaban dos referencias distintas, y el fichero commiteado no se
 * podia leer de un lado a otro ni comparar entre maquinas.
 *
 * Una base de otra version NO se compara: sus campos pueden querer decir otra
 * cosa, y compararlos daria diferencias que no existen. Se recusa y la vuelta
 * siguiente regenera la referencia.
 */
export const VERSION_FORMATO = 2;

/**
 * La identidad de la maquina que ha medido, para meterla en la referencia.
 *
 * No es adorno. Una referencia de duraciones es una foto del hardware que la
 * hizo: dos vueltas seguidas de esta suite dieron ratios de hasta x3,38 por
 * debajo de un segundo, y una vuelta puede salir con un caso de 0,02 s y la
 * siguiente con 4,02 s sin que la suite haya cambiado. Sin saber de que maquina
 * salio una cifra, ese numero no se puede leer bien, y eso no se arregla
 * mirando la cifra: se tiene que poder mirar de donde vino.
 *
 * Lo que NO va aqui, y es a proposito: el nombre de usuario y la ruta del
 * proyecto. Los dos identifican a una persona y no hacen falta para saber si la
 * maquina es la misma —sistema, nucleos y memoria bastan—, y un fichero
 * commiteado que lleva el nombre de quien lo commiteo es una molestia que se
 * acaba propagando a los logs, a los artefactos y a los mensajes de error.
 *
 * @returns {{sistema: string, nucleos: number, memoriaGb: number, node: string}}
 */
export function identidadDeLaMaquina() {
  return {
    sistema: `${process.platform} ${process.arch}`,
    nucleos: os.cpus().length,
    memoriaGb: Math.round(os.totalmem() / 1024 ** 3),
    node: process.versions.node,
  };
}

/**
 * Un percentil de una lista de numeros, por el metodo del rango mas cercano.
 *
 * Se elige ese metodo y no el de interpolar porque no inventa un valor que
 * nadie midio: el p90 es el noventavo dato de la lista ordenada, y no un punto
 * entre dos. Con 939 casos la diferencia es de centesimas —da igual—, pero con
 * una lista de tres la diferencia es entre «el p90 es el maximo», que es lo
 * unico que se puede decir de tres medidas, y un numero con dos decimales que
 * no esta en ninguna parte.
 *
 * @param {number[]} numeros la lista, da igual en que orden venga.
 * @param {number} p el percentil, de 0 a 100.
 * @returns {number|null} `null` si la lista esta vacia, que es un dato que no
 * existe y no un cero.
 */
export function percentil(numeros, p) {
  if (numeros.length === 0)
    return null;

  const ordenados = [...numeros].sort((a, b) => a - b);
  const i = Math.min(ordenados.length - 1,
    Math.max(0, Math.ceil((p / 100) * ordenados.length) - 1));

  return ordenados[i];
}

/** Un factor a dos decimales, que es toda la precision que se puede leer. */
function factorRedondeado(valor) {
  return valor === null ? null : Number(valor.toFixed(2));
}

/**
 * El ruido de la maquina: cuanto se mueve cada caso entre DOS VUELTAS del
 * MISMO codigo.
 *
 * El factor de un caso es la MAYOR de las dos duraciones partida por la menor,
 * que es lo mismo que decir «cuantas veces mas tardo la vuelta que tardo la
 * otra». Se toma en ese orden, y no con la division al reves, porque el ruido
 * que importa es el que hace un caso parecer PEOR de lo que es, que es el que
 * pone un falso rojo. Con la vuelta buena en el numerador, el numero grande
 * seria el del caso mas rapido que se ha medido nunca, que no dice nada.
 *
 * POR QUE SE MIDE AL GUARDAR Y NO DESPUES. Un ruido que se mide una vez y se
 * anota en un comentario deja de ser verdad sin que nada avise. Guardandolo en
 * la referencia, el numero viaja con los tiempos a los que explica y cada
 * comparacion lo puede volver a enseñar.
 *
 * Y POR QUE EN LA REFERENCIA, Y NO EN UN FICHERO AL LADO. Un numero del ruido
 * sin los tiempos de al lado no se puede comprobar: queda en su propio fichero y
 * la proxima vez que se mire ya no se sabe si era de esta medicion o de la
 * anterior.
 *
 * LAS DOS BANDAS, Y POR QUE ESTAN PARTIDAS POR EL UMBRAL Y NO POR LA MEDIANA.
 * La banda de abajo son los casos que en NINGUNA de las dos vueltas pasaron de
 * un segundo, que son justo los que el umbral absoluto descarta: si el ruido de
 * esa banda esta dentro del factor x2, el umbral esta haciendo falta; y si el
 * ruido de la de arriba llega al factor, el margen se ha perdido y el guard
 * puede ponerse rojo solo. Sin el corte, un unico numero no dice ninguna de las
 * dos cosas.
 *
 * Lo que NO va dentro: el ratio de cada caso. Es lo que mas informacion tiene y
 * lo que mas pesa —casi 25 KB en un fichero que se commitea— y nada lo lee: la
 * comparacion decide con el tiempo de ahora, no con el ruido de un caso
 * concreto. Anadirlo sin un consumidor seria guardar un numero que nadie va a
 * mirar y que un dia nadie sabra reexplicar.
 *
 * @param {{nombre: string, segundos: number}[]} primera la vuelta que se guarda.
 * @param {{nombre: string, segundos: number}[]} segunda la vuelta de contraste.
 * @returns {object} el bloque `ruido` de la referencia.
 */
export function ruidoDe(primera, segunda) {
  const porNombre = new Map();

  for (const d of segunda)
    porNombre.set(d.nombre, d.segundos);

  const pares = [];
  let comunes = 0;

  for (const d of primera) {
    if (!porNombre.has(d.nombre))
      continue;

    comunes += 1;

    const otro = porNombre.get(d.nombre);

    // Un caso que mide cero en alguna de las dos vueltas NO tiene ratio:
    // dividir por cero da un infinito que se lleva por delante el maximo de su
    // banda, y un maximo contaminado acaba justificando un umbral que no se ha
    // medido. Se dejan fuera y se CUENTAN, porque una exclusion que no se ve
    // es una exclusion que no se puede auditar.
    if (!(d.segundos > 0) || !(otro > 0))
      continue;

    pares.push({
      factor: Math.max(d.segundos, otro) / Math.min(d.segundos, otro),
      // Para partir por el umbral se mira el PEOR y el MEJOR de las dos vueltas,
      // no la que se guarda y no su mediana. Lo que hace falta es no meter en
      // la banda de abajo un caso que en una vuelta se fue de un segundo: ese
      // caso va a entrar en la comparacion cuando el tiempo nuevo pase de 1 s,
      // y su ruido no es el de los casos rapidos, es el de uno que ha pegado un
      // salto. Los que cruzan el segundo de una vuelta a la otra se quedan fuera
      // de las dos bandas, y `resumenRuido` dice cuantos son.
      maximo: Math.max(d.segundos, otro),
      minimo: Math.min(d.segundos, otro),
    });
  }

  const banda = (subconjunto) => {
    const factores = subconjunto.map((p) => p.factor);

    return {
      casos: factores.length,
      factorMediano: factorRedondeado(percentil(factores, 50)),
      factorP90: factorRedondeado(percentil(factores, 90)),
      factorMaximo: factorRedondeado(percentil(factores, 100)),
    };
  };

  const todo = banda(pares);

  return {
    vueltas: 2,
    casos: primera.length,
    emparejados: pares.length,
    descartados: comunes - pares.length,
    factorMediano: todo.factorMediano,
    factorP90: todo.factorP90,
    factorMaximo: todo.factorMaximo,
    bajoUmbral: banda(pares.filter((p) => p.maximo < UMBRAL_ABSOLUTO_S)),
    sobreUmbral: banda(pares.filter((p) => p.minimo >= UMBRAL_ABSOLUTO_S)),
  };
}

/**
 * Vuelca las duraciones al formato de referencia.
 *
 * @param {{nombre: string, fichero: string, segundos: number}[]} duraciones
 * @param {string} [xml] el XML del que salio, para dejar constancia de como se
 * midio. Sin esto, una referencia no dice si se midio con la maquina descargada
 * o con diez compilaciones en paralelo, y esa diferencia explica todas las
 * regresiones falsas.
 * @param {object|null} [ruido] lo que devuelve `ruidoDe`: cuanto se movio la
 * maquina con el mismo codigo. Va `null` cuando no se ha medido —con `--xml` no
 * hay segunda vuelta— y en ese caso el campo NO se escribe, porque un `null`
 * se lee como «se midio y dio cero» y no como «no se midio».
 * @returns {object} el objeto que se escribe en disco.
 */
export function construirBase(duraciones, xml = '', ruido = null) {
  return {
    // La version del FORMATO, no la del script. La sube `VERSION_FORMATO`, que
    // lleva escrito por que sube esta vez. `maquina` NO la subio, y el motivo
    // es el de aqui al lado: la version se sube cuando un campo cambia lo que
    // SIGNIFICA, no cuando aparece uno nuevo. Anadir un campo que no lee nadie
    // mas no invalida las bases viejas, que siguen comparandose igual de bien.
    // Subirla obligaria a regenerar cada referencia del mundo para poder seguir
    // usandolas, y ese coste es justo el que solo se paga cuando toca.
    //
    // `ruido` NO la subio tampoco, y es el caso interesante: un campo que SI se
    // lee, porque `resumenRuido` lo ensea al comparar. Lo que decide es si al
    // que le falta se le puede comparar con el: se puede, porque lo unico que
    // `ruido` describe es la maquina, y las duraciones de los casos se leen
    // igual. Una referencia vieja sin `ruido` se compara como siempre y ademas
    // avisa de que no dice cuanto se movia la maquina, que es justo lo que hay
    // que decir en ese caso. El dia que un campo de la referencia cambie lo que
    // un caso significa, la sube; ese dia no es hoy.
    version: VERSION_FORMATO,
    medidoEn: new Date().toISOString(),
    casos: duraciones.length,
    totalSegundos: Number(duraciones.reduce((a, d) => a + d.segundos, 0).toFixed(3)),
    filtros: FILTROS,
    maquina: identidadDeLaMaquina(),
    // El ruido va ENTRE la maquina y los casos, al lado de lo que lo explica.
    // `maquina` dice de donde vino la medicion y `ruido` cuanto se movio; los
    // dos son la foto, y los casos son el dato. Se escribe con un condicional
    // para que el campo NO aparezca cuando no se ha medido: una clave con `null`
    // se lee como «se midio y no se movio nada», que es una afirmacion.
    ...(ruido === null ? {} : { ruido }),
    casos_: Object.fromEntries(
      duraciones.map((d) => [d.nombre, {
        s: Number(d.segundos.toFixed(4)),
        f: rutaDeRepositorio(d.fichero),
      }])
    ),
  };
}

/**
 * La identidad de la maquina en una frase, para no repetir el formato en dos
 * sitios. Dos sitios que cada uno escriben su propia cadena divergen el dia que
 * uno cambia y el otro no, y entonces los dos dicen cosas distintas sin que nada
 * avise.
 *
 * @param {{sistema: string, nucleos: number, memoriaGb: number, node: string}} m
 * @returns {string}
 */
function describeMaquina(m) {
  return `${m.sistema}, ${m.nucleos} nucleos, ${m.memoriaGb} GB, node ${m.node}`;
}

/**
 * De que maquina salio la referencia, y si esta es esa.
 *
 * Es un aviso y no un fallo. Medir en otra maquina es legitimo y hecho a
 * menudo: lo que no es legitimo es hacerlo sin decirlo, porque entonces un
 * numero que depende del hardware se lee como si dependiera del codigo. Un rojo
 * por esto costaria mas de lo que informa —la maquina del otro siempre sera
 * distinta de la tuya— y dejaria de mirarse.
 *
 * @param {object|null} base la referencia, o `null` si no hay.
 * @returns {string[]} lineas para imprimir.
 */
export function resumenMaquina(base) {
  const lineas = [];

  if (base === null)
    return lineas;

  const guardado = base.maquina;

  // Una base sin el campo se generó antes de que existiera. Decirlo es mejor
  // que no decir nada: el silencio se lee como «las maquinas son iguales».
  if (guardado === undefined) {
    lineas.push('La referencia no dice de que maquina salio: se genero antes de que ese campo existiera.');
    return lineas;
  }

  const actual = identidadDeLaMaquina();

  lineas.push(`Referencia medida en ${describeMaquina(guardado)}.`);
  lineas.push(`Esta maquina es ${describeMaquina(actual)}.`);

  if (guardado.sistema !== actual.sistema || guardado.nucleos !== actual.nucleos) {
    lineas.push('NO es la misma maquina. Las duraciones son comparables en orden de magnitud, no al detalle:');
    lineas.push('una regresion aqui puede ser la maquina, y no un cambio en la suite.');
  }

  return lineas;
}

/**
 * El ruido que midio la maquina de la referencia, al lado del umbral que explica.
 *
 * Es un aviso y no un fallo, por la misma razon que el de la maquina: el ruido
 * es de la maquina, no del codigo, y a nadie le sirve que se ponga en rojo por
 * el ruido de otra maquina.
 *
 * Lo que hace es dejar de tratar el segundo como un numero puesto a ojo. Un
 * umbral sin numero al lado se acepta por costumbre y se acaba culpando a tests
 * que solo se mueven, y cuando algo se pone en rojo de verdad ya no hay quien
 * sepa si ese umbral era razonable. Aqui el umbral se mira contra la banda que
 * lo justifica: si el factor x2 esta DENTRO del ruido de los casos de menos de
 * 1 s, el umbral esta haciendo falta; si el ruido de los casos de mas de 1 s
 * llega al factor, el margen se ha perdido y eso hay que decirlo aunque no
 * rompa nada.
 *
 * @param {object|null} base la referencia, o `null` si no hay.
 * @param {number} factor el factor contra el que se compara.
 * @returns {string[]} lineas para imprimir.
 */
export function resumenRuido(base, factor = FACTOR_POR_DEFECTO) {
  const lineas = [];

  if (base === null)
    return lineas;

  const ruido = base.ruido;

  // Una referencia sin el campo se genero con `--xml`, o antes de que el campo
  // existiera. Decirlo es mejor que no decir nada: el silencio se lee como «esta
  // maquina no se mueve», que es justo lo que no se sabe.
  if (ruido === undefined || ruido === null) {
    lineas.push('La referencia no dice cuanto se movio la maquina: se genero sin medir una segunda vuelta.');
    lineas.push(`El umbral de ${UMBRAL_ABSOLUTO_S} s sigue sin un numero que lo respalde.`);
    return lineas;
  }

  const num = (v) => (typeof v === 'number' ? v.toFixed(2) : '?');
  const banda = (etiqueta, b) => (b.casos === 0
    ? `  ${etiqueta}: ningun caso`
    : `  ${etiqueta}: mediana x${num(b.factorMediano)}, p90 x${num(b.factorP90)}, maximo x${num(b.factorMaximo)}   (${b.casos} casos)`);

  lineas.push(`${ruido.vueltas} vueltas del mismo codigo, el mismo dia: ${ruido.emparejados} de ${ruido.casos} casos emparejados.`);

  if (ruido.descartados > 0)
    lineas.push(`${ruido.descartados} caso(s) miden cero en alguna vuelta y quedan fuera: sin los dos tiempos no hay ratio.`);

  lineas.push('');
  lineas.push(banda(`por debajo de ${UMBRAL_ABSOLUTO_S} s`, ruido.bajoUmbral));
  lineas.push(banda(`por encima de ${UMBRAL_ABSOLUTO_S} s`, ruido.sobreUmbral));

  const cruzados = ruido.emparejados - ruido.bajoUmbral.casos - ruido.sobreUmbral.casos;

  if (cruzados > 0)
    lineas.push(`  ${cruzados} caso(s) cruzaron 1 s de una vuelta a la otra, y no cuentan en ninguna de las dos.`);

  lineas.push('');

  const bajo = ruido.bajoUmbral;

  if (bajo.casos > 0 && typeof bajo.factorP90 === 'number') {
    if (bajo.factorP90 >= factor) {
      lineas.push(`El factor x${factor} esta DENTRO del ruido de los casos de menos de ${UMBRAL_ABSOLUTO_S} s (p90 x${num(bajo.factorP90)}):`);
      lineas.push('por eso esos casos no se comparan.');
    }
    else {
      lineas.push(`El ruido de los casos de menos de ${UMBRAL_ABSOLUTO_S} s llega a x${num(bajo.factorP90)} y el factor es x${factor}:`);
      lineas.push(`el umbral de ${UMBRAL_ABSOLUTO_S} s esta MAS ALTO de lo que el ruido de esta maquina pide.`);
    }
  }

  const sobre = ruido.sobreUmbral;

  if (sobre.casos > 0 && typeof sobre.factorMaximo === 'number') {
    if (sobre.factorMaximo < factor)
      lineas.push(`Por encima de ${UMBRAL_ABSOLUTO_S} s el maximo es x${num(sobre.factorMaximo)} y el factor x${factor} esta fuera: hay margen.`);
    else
      lineas.push(`POR ENCIMA de ${UMBRAL_ABSOLUTO_S} s el ruido llega a x${num(sobre.factorMaximo)} y el factor es x${factor}: el margen se ha perdido, y un caso de mas de ${UMBRAL_ABSOLUTO_S} s puede ponerse rojo solo.`);
  }

  return lineas;
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
    //
    // Y se dice POR QUE en vez de devolver null en silencio, porque el silencio
    // aqui se lee como "no hay referencia" y la diferencia entre las dos cosas
    // es una: en un caso se regenera porque no hay nada, y en el otro se
    // regenera porque lo que hay ya no se puede leer. Sin el aviso, quien
    // regenera la referencia sin querer no sabe que ha cambiado el formato.
    if (base?.version !== VERSION_FORMATO) {
      if (Number.isInteger(base?.version))
        console.error(`[referencia] ${ruta} es del formato ${base.version} y este tool `
          + `escribe el ${VERSION_FORMATO}: se recusa en vez de compararse, y la vuelta `
          + 'siguiente la regenera.');

      return null;
    }

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
 * La ruta de un fichero de test tal y como la guarda la referencia: RELATIVA al
 * repositorio, con `/`.
 *
 * Catch2 entrega la ruta ABSOLUTA, y esa lleva dentro el disco, el proyecto y el
 * usuario. En un fichero commiteado eso son tres cosas que no son las mismas en
 * otra maquina y que no dicen nada del test, asi que se quitan.
 *
 * No todo esta dentro del repo, y aqui no se disimula: de los 939 casos de la
 * referencia, 875 son del repo y 64 son del hermano `ABDSharedCode`. Los del
 * hermano salen como `../ABDSharedCode/...`, que sigue diciendo de donde vienen y
 * ya no dice de quien es el disco. Lo que caiga en otro arbol --otro disco, otro
 * usuario-- se deja como venia: no hay ruta relativa que diga la verdad y
 * inventar una seria peor que guardar la ruta entera.
 *
 * Se hace con cadenas y no con `path.relative` a proposito, porque el tool
 * tambien corre en Linux, donde un `relative` entre dos sistemas de ficheros
 * distintos devuelve una ruta absoluta sin avisar.
 *
 * @param {string} fichero la ruta, como la deje el reporter de Catch2.
 * @returns {string} la ruta relativa al repositorio, con `/`.
 */
export function rutaDeRepositorio(fichero) {
  const bruto = String(fichero ?? '');

  if (bruto === '')
    return '(sin fichero)';

  const normal = bruto.replace(/\\/g, '/');
  const raizNormal = raiz.replace(/\\/g, '/').replace(/\/+$/, '');
  const enraizado = normal.toLowerCase();
  const base = raizNormal.toLowerCase();

  if (enraizado.startsWith(base + '/'))
    return normal.slice(raizNormal.length + 1);

  const padre = raizNormal.slice(0, raizNormal.lastIndexOf('/'));

  if (padre !== '' && enraizado.startsWith(padre.toLowerCase() + '/'))
    return '../' + normal.slice(padre.length + 1);

  return normal;
}

/**
 * La segunda vuelta, que es la que mide el ruido de la maquina.
 *
 * El limite de reloj es el de la referencia VIEJA, y no el de lo que acaba de
 * medirse, por un motivo concreto: la primera vuelta acaba de pasar ese limite
 * con esta suite y esta maquina, y la segunda es la misma suite unos minutos
 * despues. Si ese limite sirve para la primera, sirve para la segunda, y medirlo
 * con el total recien medido haria que el corte de la segunda vuelta dependiera
 * de un dato que todavia no esta escrito en ninguna parte.
 *
 * Que la segunda vuelta falle NO es motivo para no guardar la referencia. La
 * primera es buena y perderla porque la segunda se ha colgado seria tirar un
 * dato medido por un dato que no se ha podido medir; lo que se pierde es el
 * ruido, y el ruido se puede medir otro dia.
 *
 * @param {{nombre: string, segundos: number}[]} duraciones la vuelta que se va a
 * guardar, para poder contrastarla caso a caso.
 * @param {string[]} paraCatch los argumentos de Catch2, los mismos.
 * @param {object|null} base la referencia previa, de la que sale el limite.
 * @returns {{ruido: object|null, motivo: string|null}}
 */
function medirSegundaVuelta(duraciones, paraCatch, base) {
  console.error('');
  console.error('SEGUNDA VUELTA, para medir cuanto se mueve la maquina con el');
  console.error('mismo codigo. Tarda lo mismo que la primera: no es un cuelgue.');

  const captura = capturarXml(paraCatch, base);

  if (captura.xml === null)
    return { ruido: null, motivo: `la segunda vuelta ${captura.motivo}` };

  if (xmlTruncado(captura.xml))
    return { ruido: null, motivo: 'el XML de la segunda vuelta esta truncado' };

  const segunda = leerDuraciones(captura.xml);

  if (segunda.length === 0)
    return { ruido: null, motivo: 'la segunda vuelta no ha medido ni un caso' };

  return { ruido: ruidoDe(duraciones, segunda), motivo: null };
}

/**
 * La identidad de un fichero de test, sin la parte que cambia con la maquina.
 *
 * La referencia guarda la ruta RELATIVA de cada caso, porque hace falta saber de
 * donde viene y la relativa basta. Aun asi el emparejamiento no la usa entera, y
 * se queda solo con el nombre del fichero: comparar por ruta entera haria que un
 * test movido de carpeta pareciese un test nuevo y otro ausente a la vez, dos
 * avisos para un solo test, y el segundo falso. Con el nombre, un cambio de
 * carpeta es un renombrado, que es lo que es.
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

    // El umbral absoluto, y solo sobre el tiempo de AHORA. Ponerlo tambien sobre
    // el tiempo de antes taparia justo el caso que mas duele: un test que
    // estaba por debajo del umbral y ha pasado por encima. El tiempo de antes no
    // se filtra porque no dice si ahora el caso es un problema, y si se filtra,
    // el que mas problema tiene es el que mas facil se deja de mirar.
    if (despues < UMBRAL_ABSOLUTO_S)
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

    // Solo los que tardan: un caso nuevo por debajo del umbral absoluto no es un
    // dato que interese en un fichero de duraciones. El recuento de `nuevos`
    // sigue diciendo cuantos hay, que es lo que se necesita para saber si hay.
    const lentos = cmp.nuevos.filter((n) => n.segundos >= UMBRAL_ABSOLUTO_S);

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
export function falloDeSpawn(r, base = null) {
  const codigo = r?.error?.code;

  // El caso principal. El corte por reloj deja la salida puesta, y sin mirar
  // aqui el XML truncado se analiza como si fuera una medicion buena.
  //
  // El mensaje lleva la cuenta —cuanto decia la referencia y por cuanto se
  // multiplico— porque hay dos motivos muy distintos para un corte y se distinguen
  // justo por esa cuenta: o la suite se ha colgado, o la referencia esta vieja y
  // la suite ha crecido. Con los dos numeros a la vista se ve cual de los dos es
  // sin tener que ir a buscar el JSON.
  //
  // Ninguna rama termina en punto: quien imprime pone el suyo, y estas frases son
  // la mitad de una oracion. Una que se cierre aqui y otra que no es como salen
  // dos puntos seguidos al final del mensaje.
  if (codigo === 'ETIMEDOUT') {
    const limite = limiteDeReloj(base);
    const cuenta = base?.totalSegundos === undefined
      ? ''
      : `; la referencia dice que la suite tarda ${base.totalSegundos} s y el limite es ese total x${VECES_EL_TOTAL}`;

    return `no ha terminado en ${(limite / 60000).toFixed(1)} min y se ha matado al agotar el limite${cuenta}`;
  }

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
function capturarXml(args, base) {
  const r = spawnSync(SUITE, ['-r', 'xml', '-d', 'yes', ...args, ...FILTROS], {
    cwd: raiz,
    encoding: 'utf8',
    maxBuffer: MAX_BUFFER_MB * 1024 * 1024,
    timeout: limiteDeReloj(base),
    windowsHide: true,
  });

  const salida = `${r.stdout ?? ''}${r.stderr ?? ''}`;
  const motivo = falloDeSpawn(r, base);

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

// ── La linea de veredicto ──
//
// QUE ES. Una sola linea, con prefijo fijo y el cuerpo en JSON, que dice que
// ha pasado. Va a stdout, no a stderr, porque lo legible por maquina que solo
// aparece cuando algo va a stderr no lo lee nadie: stdout es lo que un
// `| grep` y lo que un pipeline capturan sin pedirlo.
//
// POR QUE HACE FALTA, Y QUE ESTA ROMPIENDO. El cronometro sale hoy con un
// numero, y un numero no dice QUE paso. Hay dos 1 que no se parecen en nada:
// "un test se ha puesto lento" y "el propio cronometro ha petado a mitad de un
// XML". Quien lee un 1 tiene que adivinar, y el que adivina mal se
// desconecta. Peor: un fallo del tool sale con 1 porque es lo que usa Node
// para lo que no captura, de modo que build.bat anunciaba "Suite timings: a
// slow test" cuando el cronometro estaba roto. Un fallo de herramienta
// anunciado como lentitud es la clase de mentira que este cronometro no
// deberia tener, y por eso aqui se arregla.
//
// LOS SEIS ESTADOS, Y POR QUE SON SEIS Y NO TRES.
//
//   ok                  se midio y no hay nada que decir.
//   lento                se midio y hay casos por encima del umbral.
//   regresion            se midio y hay casos que se han volcado o mas que
//                        respecto a la referencia.
//   medicion-incompleta  se empezo a medir y no hay resultado creible: la suite
//                        no ha terminado, el XML esta truncado, no hay casos,
//                        o la medicion no cubre la referencia entera.
//   sin-medir            no se ha medido y no es culpa de la medicion: el
//                        binario o el XML no estaban donde se buscaban.
//   fallo-del-tool       el cronometro ha fallado. No es lentitud, y sale con
//                        2 para que no se pueda leer como tal.
//
// `medicion-incompleta` y `sin-medir` estan separadas a proposito: la primera
// es un problema de la medicion y la segunda del entorno, y quien decide si
// algo tiene que ser puerta necesita saber cual de las dos es.
//
// UN SOLO PUNTO DE EMISION. main() devuelve este descriptor en vez de un
// numero, y el envoltorio emite. Si main emitiera la linea, un camino que la
// emite y despues falla al escribirla dejaria dos lineas, y dos lineas de
// veredicto no son legibles por maquina: son ambiguas. Con un unico punto de
// emision hay cero o una, por construccion, y eso no se puede comprobar con
// un test porque no se puede romper.
function con(estado, codigo, extra) {
  return { estado, codigo, ...(extra || {}) };
}

// El prefijo va con espacio y no con dos puntos: el cuerpo es JSON y un
// `:` delante haria dudar de donde empieza. El JSON se aplana a una linea
// porque una linea legible por maquina tiene que ser una linea de verdad.
const PREFIJO_VEREDICTO = 'ABD-VEREDICTO ';

function emitirVeredicto(descriptor) {
  const cuerpo = JSON.stringify(descriptor).replace(/[\r\n]+/g, ' ');

  console.log(PREFIJO_VEREDICTO + cuerpo);
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

  // Se filtran las banderas y SUS VALORES para que un `--base otra.json` no le
  // pase la `otra.json` a Catch2, que lo interpretaria como un filtro de test
  // sin ningun caso detras y mediria cero sin decir por que.
  //
  // Se calcula aqui y no donde se corre la suite porque el bloque que guarda la
  // referencia necesita los MISMOS argumentos para la segunda vuelta: filtrarlos
  // otra vez en el sitio de usarlos seria un segundo sitio donde se pueden
  // colar por una bandera nueva.
  const paraCatch = paraCatchDe(argumentos);

  // La referencia se lee ANTES de correr nada, y no por ordenar: el limite de
  // reloj de la vuelta se deriva de su total, asi que hace falta tenerla en la
  // mano para lanzar el proceso. Leerla aqui en vez de mas abajo no es un
  // Tattoo de orden, es que despues ya no llega.
  const base = leerBase(rutaBase);

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

      return con('sin-medir', 2, { motivo: 'el XML que se le apunto no existe', ruta });
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

      return con('sin-medir', 2, { motivo: 'la suite no esta compilada', ruta: SUITE });
    }

    console.error('Midiendo la suite. Tarda unos minutos; no es un cuelgue.');
    const captura = capturarXml(paraCatch, base);

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

      return con('medicion-incompleta', 1, { motivo: motivoCorte, casos: 0 });
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

    return con('medicion-incompleta', 1, {
      motivo: 'el XML esta truncado, le falta el cierre del documento',
      casos: 0,
    });
  }

  const duraciones = leerDuraciones(xml);

  for (const linea of resumen(duraciones, umbral))
    console.log(linea);

  // Un XML entero con CERO casos dentro no es una suite rapida: es una medicion
  // que no ha medido nada, y salir con 0 de ahi es un verde que no dice nada.
  if (duraciones.length === 0) {
    console.error('');
    console.error('La medicion no tiene ni un caso. Sin casos no hay nada que comparar.');

    return con('medicion-incompleta', 1, { motivo: 'la medicion no tiene ningun caso', casos: 0 });
  }

  const lentos = duraciones.filter((d) => d.segundos > umbral).length;

  // ── Contra la referencia ──
  //
  // Va DESPUES del informe de lentos y con su propio codigo de salida, porque
  // son dos preguntas distintas: una es «que test es lento ahora» y la otra «que
  // test se ha puesto lento». Se informa de las dos y se sale con 1 si hay
  // cualquier cosa, para que un pipeline no tenga que saber cual de las dos
  // preguntas era la importante.
  //
  // `base` ya esta leido de arriba, antes de correr la suite: el limite de reloj
  // sale de su total, asi que no se puede leer despues de lanzar el proceso.

  if (guardar) {
    // Guardar y comparar a la vez daria un verde de comparacion contra uno
    // mismo, que no compara nada. Por eso la referencia que se acaba de escribir
    // NO es la que se usa para comparar en esta misma vuelta.
    //
    // Y se mide una SEGUNDA vuelta antes de escribir, que es lo que hace esto
    // caro —el doble— y lo que no es opcional: el ruido de la maquina es la
    // razon por la que existe el umbral de un segundo, y un numero medido a
    // mano y anotado en un comentario deja de ser verdad en silencio.
    let ruido = null;
    let motivoSinRuido = null;

    if (indiceXml >= 0) {
      // Con `--xml` no hay segunda vuelta que correr: lo que hay es un fichero, y
      // un fichero no se puede volver a medir. La referencia se guarda igual —
      // perderla por no tener ruido seria tirar un dato medido por otro que no se
      // ha podido medir— y se dice que se ha guardado sin el.
      motivoSinRuido = 'viene de un XML ya capturado, y un XML no se puede volver a medir';
    }
    else {
      const segunda = medirSegundaVuelta(duraciones, paraCatch, base);

      ruido = segunda.ruido;
      motivoSinRuido = segunda.motivo;
    }

    const base2 = construirBase(duraciones, xml, ruido);

    escribirEntero(rutaBase, `${JSON.stringify(base2, null, 2)}\n`);
    console.error('');
    // La ruta entera, por el mismo motivo que la de la suite: `--base` puede
    // apuntar a cualquier parte, y un nombre a secas no dice donde ha quedado la
    // referencia que se acaba de escribir.
    console.error(`Referencia guardada en ${rutaBase}: ${duraciones.length} casos.`);
    console.error(`Medida en ${describeMaquina(base2.maquina)}.`);

    if (ruido === null)
      console.error(`SIN el ruido de la maquina: ${motivoSinRuido}. El umbral de ${UMBRAL_ABSOLUTO_S} s sigue sin un numero que lo respalde.`);
    else {
      console.error('');
      console.error('LO QUE SE MUEVE ESTA MAQUINA');

      for (const linea of resumenRuido(base2, factor))
        console.error(linea);
    }

    console.error('La siguiente vuelta comparara contra esta.');
    if (lentos > 0 && !soloAvisar)
      return con('lento', 1, { casos: duraciones.length, lentos, umbral });

    return con('ok', 0, { casos: duraciones.length, lentos: 0, umbral, referencia: 'guardada' });
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

    const maquina = resumenMaquina(base);

    if (maquina.length > 0) {
      console.log('');
      console.log('LA MAQUINA DE LA REFERENCIA');
      console.log('='.repeat(72));

      for (const linea of maquina)
        console.log(linea);
    }

    // El ruido va al lado de la maquina porque es su continuacion: una dice de
    // donde salio la medicion y el otro cuanto se movio esa medicion. Y va
    // pegado al umbral, porque es el numero que le da la razon.
    const ruido = resumenRuido(base, factor);

    if (ruido.length > 0) {
      console.log('');
      console.log('LO QUE SE MUEVE ESTA MAQUINA');
      console.log('='.repeat(72));

      for (const linea of ruido)
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

      // Los dos fallos posibles se separan, porque son dos preguntas: si se han
      // volcado casos es una regresion de tiempo, y si la medicion no cubre la
      // referencia entera no hay regresion que comparar porque falta la mitad
      // de lo que se iba a mirar.
      return cmp.regresiones.length > 0
        ? con('regresion', 1, {
            casos: duraciones.length,
            regresiones: cmp.regresiones.length,
            ausentes: cuenta.ausentes,
            referencia: base.medidoEn,
          })
        : con('medicion-incompleta', 1, {
            casos: duraciones.length,
            ausentes: cuenta.ausentes,
            motivo: 'la medicion no cubre la referencia entera',
          });
    }
  }

  if (lentos > 0 && !soloAvisar) {
    console.error('');
    console.error(`${lentos} caso(s) por encima de ${umbral} s. Sale con 1 para que un`);
    console.error('pipeline lo vea, aunque el suite haya pasado.');

    return con('lento', 1, { casos: duraciones.length, lentos, umbral });
  }

  return con('ok', 0, { casos: duraciones.length, lentos: 0, umbral });
}

// El envoltorio, y el unico sitio del fichero que emite el veredicto.
//
// El try/catch es lo que convierte una excepcion no controlada en un veredicto
// con nombre. Antes de esto, petar a mitad de un XML salia con 1, que es lo que
// usa Node para lo que no captura, y quien leia ese 1 se enteraba de una
// regresion de tiempo que no existe. El mensaje va entero y sin recortar, que un
// stack trace truncado es una pista que no lleva a ninguna parte.
//
// El 2 y no el 1 es la decision: "no he medido nada" es exactamente lo que ha
// pasado, y es la clase que build.bat ya sabe hacer fatal. Asi que el arreglo no
// necesita tocar el build, y el fallo de una herramienta deja de vestirse de
// lentitud sin cambiar una sola linea de las que lo reparten.
function ejecutar(argumentos) {
  let descriptor;

  try {
    descriptor = main(argumentos);
  } catch (e) {
    console.error('');
    console.error('EL CRONOMETRO HA FALLADO. Esto no es una regresion de tiempo.');
  console.error('No hay medicion que leer, y sale con 2 para que no se confunda');
  console.error('con un caso lento. El error entero va debajo:');
  console.error(e && e.stack ? e.stack : e);
    descriptor = con('fallo-del-tool', 2, {
      motivo: String(e && e.message ? e.message : e).split('\n')[0],
      error: String(e && e.stack ? e.stack : e).split('\n').slice(0, 6).join(' | '),
    });
  }

  emitirVeredicto(descriptor);

  return descriptor.codigo;
}

// Solo cuando se ejecuta como programa. Importado desde un test, no.
if (process.argv[1] && existsSync(process.argv[1])
    && process.argv[1].replace(/\\/g, '/').endsWith('duraciones-suite.mjs'))
  process.exit(ejecutar(process.argv.slice(2)));

// El ejecutable, para el test.
export const RUTA_SUITE = SUITE;
export const FILTROS_POR_DEFECTO = FILTROS;
export const UMBRAL = UMBRAL_POR_DEFECTO;
export const UMBRAL_ABSOLUTO = UMBRAL_ABSOLUTO_S;
export const FACTOR = FACTOR_POR_DEFECTO;
export const BASE = RUTA_BASE;
export { con, PREFIJO_VEREDICTO, emitirVeredicto, ejecutar };