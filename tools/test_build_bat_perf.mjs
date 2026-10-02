/**
 * TESTS DEL REPARTO DE CODIGOS DE build.bat
 *
 * ---------------------------------------------------------------------------
 * QUE ATAN ESTOS TESTS
 *
 * El cronometro sale con tres codigos, y cada uno tiene que acabar en un sitio
 * distinto del build:
 *
 *     0  se midio y no hay nada que decir  ->  el build pasa
 *     1  se midio y hay algo que mirar     ->  el build pasa, avisando
 *     2  NO SE MIDIO                      ->  el build falla
 *     otra cosa                           ->  el build falla
 *
 * Ese reparto estaba escrito en un banco de pruebas de un solo uso, es decir,
 * en la cabeza de quien lo escribio y en el commit que lo explicaba. Un
 * contrato de cuatro filas que solo existe en un commit es un contrato que
 * puede derivar sin que nadie se entere. Estos tests lo dejan en el sitio
 * donde ya se mira solo: la suite de tests.
 *
 * ---------------------------------------------------------------------------
 * POR QUE NO SE REIMPLEMENTA build.bat
 *
 * Una copia de la logica en JavaScript estaria en verde mientras el .bat
 * hiciera otra cosa, que es justo el fallo que se quiere evitar. Asi que esto
 * no reimprime el reparto: EXTRAE del build.bat commiteado el bloque del
 * cronometro y la cola de :end, los pega en el MISMO orden en que se ejecutan
 * y ejecuta eso de verdad. Lo unico que se sustituye son las tres cosas que
 * hacen que un banco no pueda correr solo:
 *
 *     - el `if not exist` del binario de tests, por uno que mira un senuelo,
 *       para que el test no dependa de que haya compilado nada y no pueda dar
 *       un verde falso por cortocircuito;
 *     - la llamada al test del cronometro, por un `cmd /c exit N`;
 *     - la llamada al cronometro, por un `cmd /c exit N`.
 *
 * Todo lo demas --el `if not exist`, el `where node`, los cuatro mensajes, el
 * `set "PERF_FATAL=1"`, el `exit /b 1` de la cola-- es el texto del build.bat.
 *
 * ---------------------------------------------------------------------------
 * POR QUE LA COLA DE :end VA EN EL BANCO
 *
 * Porque ahi esta el `exit /b 1`, que es la parte que no es evidento: el
 * fallo se acumula en PERF_FATAL durante el cronometro y se devuelve al
 * final, despues de que se hayan montado los enlaces de junctions. Si el banco
 * solo pegase el bloque, probaria una cosa que el build no hace.
 *
 * La seccion de junctions NO se pega, y es a proposito: no toca PERF_FATAL y
 * montarla crearia enlaces de verdad en el arbol del repositorio. Lo que ha
 * cambiado es la propagacion del fallo, y eso es lo que se prueba.
 */

import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';

// CERROJO DE INSTANCIA UNICA.
//
// El banco escribe en `build/banco-perf-build`, asi que dos copias de este test
// a la vez se pisan ese directorio: una lee el .bat a medio escribir, ninguna de
// las dos puede terminar, y el sintoma es un cuelgue sin mensaje. El cerrojo no
// es que seais paranoicos, es que el fallo de dos instancias escribiendo en el
// mismo sitio no se parece a nada de lo que un cuelgue deberia ser.
const CERRADO = 'ABD_BUILD_BAT_CERRADO';

if (process.env[CERRADO]) {
  console.error('ROJO  ya hay otra instancia de este test corriendo.');
  console.error(`      se identifica con ${CERRADO}=${process.env[CERRADO]}`);
  console.error('      Dos a la vez se escriben encima en el directorio del banco.');
  process.exit(3);
}

process.env[CERRADO] = join(dirname(fileURLToPath(import.meta.url)), 'build_build_bat_perf.mjs');

import { fileURLToPath } from 'node:url';

const RAIZ = join(dirname(fileURLToPath(import.meta.url)), '..');
// El build.bat que se comprueba. Puede apuntarse a otro con ABD_BUILD_BAT para
// probar una variante, que es lo que hace el banco de mutacion: comprobar que
// una cadena de codigos rota se ve en rojo necesita poder romperla. Apuntar a
// otro fichero a proposito no es un atajo: es lo unico que hace falta para
// mutar el reparto, y mutarlo sin tocar el de verdad es la unica forma de que
// un banco de pruebas no pueda dejar el build en un estado que nadie quiere.
const RUTA_BAT = process.env.ABD_BUILD_BAT
  ? resolve(process.env.ABD_BUILD_BAT)
  : join(RAIZ, 'build.bat');
const BANCO = join(RAIZ, 'build', 'banco-perf-build');

const fallos = [];
let total = 0;

function comprobar(descripcion, condicion) {
  total += 1;

  if (condicion)
    console.log(`  verde  ${descripcion}`);
  else {
    console.log(`  ROJO  ${descripcion}`);
    fallos.push(descripcion);
  }
}

// Un `if not exist` que apunta a un exe de senuelo. Sin el, el banco se
// cortocircuitaba por la rama de "no hay binario que cronometrar" cada vez que
// no hubiera nada compilado, y daba verde en los seis casos sin haber comprobado
// ninguno. Un verde falso en un test que existe para cazar verdes falsos, y el
// peor sitio posible para que uno se cuele: en un `if not exist`, que no
// avisa de nada cuando no se cumple lo que uno cree que se cumple.
const SENUELO = join(BANCO, 'SUITE.exe');

// Las llamadas a node que hay que sustituir por su codigo de salida. En tabla
// y no encadenadas en el bucle porque van a crecer: cuando entre una tercera, el
// caso nuevo es una fila mas y no una rama mas.
//
// POR PREFIJO, Y NO CON IGUALDAD. Una fila dice con que PREFIJO empieza la linea
// del build.bat, no cual es la linea exacta. Con `===` basta con que alguien
// redirija la salida, anyada una bandera o cambie unas comillas para que el
// emparejamiento falle, y entonces el banco no sustituye y ejecuta el cronometro
// DE VERDAD: cinco minutos por caso, seis casos, y un test que se cuelga en vez
// de ponerse rojo. Un banco que ejecuta lo que deberia estar sustituyendo deja
// de ser un banco sin avisar, y eso es exactamente la clase de fallo que este
// test existe para cazar.
//
// El prefijo va con el espacio justo despues de la ruta, de modo que
// `duraciones-suite.mjs` no empareja con un `duraciones-suite.mjs-otro.mjs` que
// se inventara manana.
// La fila guarda la ruta SECA. Lo que viene justo despues se comprueba
// aparte, y puede ser un separador o nada: con el separador dentro del
// prefijo, una linea que se acaba en la ruta no empareja, porque no hay
// nada despues con lo que casarlo. Y el separador no es cosmetico, es lo
// que impide que `duraciones-suite.mjs` case con un
// `duraciones-suite.mjs-otro.mjs` que alguien se invente manana.
const SEPARADORES = [" ", "	", ">"];

const LLAMADAS_NODE = [
  { ruta: 'node tools\\test_duraciones_suite.mjs', codigo: 'ntest' },
  { ruta: 'node tools\\test_build_bat_perf.mjs', codigo: 'nbanco' },
  { ruta: 'node tools\\duraciones-suite.mjs', codigo: 'ncrono' },
];

function esLlamadaDe(s, fila) {
  if (!s.startsWith(fila.ruta))
    return false;

  const resto = s.slice(fila.ruta.length);

  return resto === "" || SEPARADORES.includes(resto[0]);
}

// La frase que cierra el build cuando no hay medicion. Se comprueba que es la
// ULTIMA linea que dice algo, y no solo que este en la salida: que se anuncie
// el fallo al final es la mitad del contrato de la cola de :end.
const CIERRE_FALLO = 'Nothing above this line is a performance result.';

if (process.platform !== 'win32') {
  console.error('ROJO  este test ejecuta cmd.exe y solo puede correr en Windows.');
  console.error(`      platform = ${process.platform}`);
  process.exit(1);
}

// Las comillas van aparte porque los literales de batch las llevan dentro, y
// escribirlas pegadas dentro de un literal de JavaScript es una forma de que
// al copiar una linea se pierda una y el patron deje de casar.
const Q = String.fromCharCode(34);

// El salto de linea se escribe por codigo y no como caracter escapado, porque un
// salto escrito a mano se puede convertir en un salto de verdad al copiar la linea
// y romper el fichero entero por una razon que no tiene nada que ver con el test.
const LF = String.fromCharCode(10);

if (process.env.ABD_BUILD_BAT)
  console.log(`
ATENCION: se esta comprobando ${RUTA_BAT}, no el build.bat del arbol.`);

const LINEAS = readFileSync(RUTA_BAT, 'utf8').split('\n').map((l) => l.replace(/\r$/, ''));

// El bloque que empieza por `primera` y dura hasta que los parentesis cierran.
// Los `rem` y los `echo` se saltan al contar: escriben texto, y un parentesis
// de un mensaje --"no se midio (y no hay nada que leer)"-- no abre un bloque.
function bloqueDesde(primera) {
  const ini = LINEAS.findIndex((l) => l.startsWith(primera));

  if (ini === -1)
    throw new Error('build.bat ya no tiene esta linea: ' + primera);

  let nivel = 0;

  for (let i = ini; i < LINEAS.length; i += 1) {
    if (/^\s*(rem |echo )/i.test(LINEAS[i]))
      continue;

    nivel += (LINEAS[i].match(/\(/g) || []).length;
    nivel -= (LINEAS[i].match(/\)/g) || []).length;

    if (i > ini && nivel === 0)
      return LINEAS.slice(ini, i + 1);
  }

  throw new Error('build.bat tiene un bloque sin cerrar: ' + primera);
}

function colaDesde(etiqueta) {
  const ini = LINEAS.findIndex((l) => l.startsWith(etiqueta));

  if (ini === -1)
    throw new Error('build.bat ya no tiene esta etiqueta: ' + etiqueta);

  return LINEAS.slice(ini);
}

const BLOQUE_PERF = bloqueDesde('if ' + Q + '!RUN_PERF!' + Q + '==' + Q + '1' + Q + ' (');
const COLA = colaDesde(':end');

// El preambulo se extrae tambien, y no se escribe a mano. `PERF_FATAL` nace en
// la linea 113, fuera del bloque del cronometro; si el banco se lo pusiera
// el, el test seguiria en verde con el `set` borrado de build.bat, que es
// justo el cambio que este test existe para ver.
const SETLOCAL = LINEAS.find((l) => l.trim().toLowerCase() === 'setlocal enabledelayedexpansion')
  || 'setlocal EnableDelayedExpansion';
const PERF_FATAL = LINEAS.find((l) => l.trim().toLowerCase() === 'set "perf_fatal=0"')
  || 'set "PERF_FATAL=0"';
const SELFTEST_FATAL = LINEAS.find((l) => l.trim().toLowerCase() === 'set "selftest_fatal=0"')
  || 'set "SELFTEST_FATAL=0"';

console.log('\nlo que el banco ejecuta sale del build.bat, no de una copia');

comprobar('build.bat tiene el setlocal de expansion retardada', SETLOCAL.startsWith('setlocal enabledelayedexpansion'));
comprobar('build.bat inicializa PERF_FATAL a 0 antes del cronometro',
  LINEAS.indexOf(PERF_FATAL) < LINEAS.indexOf(BLOQUE_PERF[0]));
comprobar('el banco ha encontrado el bloque del cronometro y la cola de :end',
  BLOQUE_PERF.length > 40 && COLA.length > 3);

console.log('\nlas cuatro clases del cronometro, cada una a su sitio');

// ntest = codigo del test del cronometro. nbanco = codigo del test de este
// reparto de codigos. ncrono = codigo del cronometro. build = lo que tiene que
// salir del build. muere = si el build tiene que terminar en fallo.
const CASOS = [
  { ntest: 0, nbanco: 0, ncrono: 0, build: 0, muere: false, motivo: 'medido y nada que decir' },
  { ntest: 0, nbanco: 0, ncrono: 1, build: 0, muere: false, motivo: 'medido y algo que mirar' },
  { ntest: 0, nbanco: 0, ncrono: 2, build: 1, muere: true, motivo: 'no medido' },
  { ntest: 0, nbanco: 0, ncrono: 7, build: 1, muere: true, motivo: 'codigo que el tool no usa' },
  { ntest: 1, nbanco: 0, ncrono: 0, build: 3, muere: true, motivo: 'el test del cronometro en rojo' },
  { ntest: 0, nbanco: 1, ncrono: 0, build: 1, muere: true, motivo: 'el test del reparto en rojo' },
];

try {
  mkdirSync(BANCO, { recursive: true });
  writeFileSync(SENUELO, '');

  for (const c of CASOS) {
    const cuerpo = [];
    const sinSustituir = [];
    let sustituciones = 0;

    for (const l of [SETLOCAL, PERF_FATAL, SELFTEST_FATAL, 'set "RUN_PERF=1"', ...BLOQUE_PERF, ...COLA]) {
      const s = l.trim();
      const sangria = l.slice(0, l.length - l.trimStart().length);

      // El `if not exist` del binario real mira uno que este banco crea, para
      // que la rama que se prueba sea la de medir y no la de "no hay nada".
      if (s.startsWith('if not exist') && s.includes('ABDAudioLab_Tests.exe')) {
        cuerpo.push(`${sangria}if not exist "${SENUELO}" (`);
        sustituciones += 1;
      } else {
        const llamada = LLAMADAS_NODE.find((x) => esLlamadaDe(s, x));

        if (llamada) {
          cuerpo.push(`${sangria}cmd /c exit ${c[llamada.codigo]}`);
          sustituciones += 1;
        } else {
          // Una llamada a node que no esta en la tabla no se sustituye, y sin
          // esto se EJECUTA de verdad: el banco deja de ser un banco y el test
          // se cuelga en vez de ponerse rojo. Se apunta y se deja pasar, y la
          // asercion del conteo de abajo dice cual ha sido.
          if (s.startsWith('node '))
            sinSustituir.push(l.trim());

          cuerpo.push(l);
        }
      }
    }

    const etiqueta = `t${c.ntest}-c${c.ncrono}`;
    const banco = join(BANCO, `${etiqueta}.bat`);

    // CRLF a proposito: un .bat con finales LF en Windows se come la primera
    // linea de cada bloque, y el banco probaria otra cosa que no es build.bat.
    writeFileSync(banco, '@echo off\r\n' + cuerpo.join('\r\n') + '\r\n', 'utf8');

    // La ruta va ABSOLUTA y SIN comillas propias. Node entrecomilla un
    // argumento que lleva barras, y cmd.exe busca entonces el fichero con
    // las comillas dentro: el banco no se ejecuta, y status nulo con stdout
    // vacio se lee como un fallo cualquiera. Cuatro casos rojos que eran del
    // arnes y no del reparto, que es la confusion que este test no puede permitirse.
    // El timeout no es una prudencia, es la diferencia entre un rojo y un
    // cuelgue. Lo que el banco hace es ejecutar unas pocas lineas de batch, y
    // eso no tarda 40 s: si tarda, lo que se ha colgado es una llamada a node
    // que el banco no ha sustituido y que ahora esta corriendo de verdad. Sin
    // este limite, eso son cinco minutos por caso y seis casos, sin decir nada
    // en ningun momento. Con el limite, un rojo que dice el caso y el motivo.
    const r = spawnSync('cmd', ['/c', banco], { cwd: RAIZ, encoding: 'utf8', timeout: 40000 });
    const salida = (r.stdout || '').split(LF).filter((x) => x.trim());
    const ultima = salida.length ? salida[salida.length - 1].trim() : '';

    // `signal` es SIGTERM cuando salta el timeout, y entonces el banco no ha
    // terminado: no se ha ejecutado entero. Se comprueba por separado porque un
    // banco a medias puede haber impreso cosas y parecer que ha ido bien.
    comprobar(`${etiqueta}: el banco termina dentro del limite, sin colgarse`,
      !r.signal);

    comprobar(`${etiqueta}: el banco se ha ejecutado de verdad`,
      !r.error && salida.length > 0);

    // El mensaje lleva el nombre de lo que no se sustituyo, y no solo el numero.
    // El caso que de verdad importa no es que falte una de las de la tabla: es
    // que alguien anada una llamada a node NUEVA y no la anada a la tabla. Con
    // un conteo a secas el rojo dice "3" y no dice cual; con el nombre, dice que
    // anadir. Y es un rojo y no un cuelgue: sin esto, esa llamada se ejecutaria
    // de verdad y el test se quedaria cinco minutos sin decir nada.
    comprobar(`${etiqueta}: el banco sustituye el exe y las ${LLAMADAS_NODE.length} llamadas a node`
      + (sinSustituir.length ? `; sin sustituir: ${sinSustituir.join(' | ')}` : ''),
    sustituciones === LLAMADAS_NODE.length + 1);
    comprobar(`${etiqueta}: ninguna llamada a node se queda sin sustituir`,
      sinSustituir.length === 0);

    // Si se ha colgado, lo probable es que una llamada se haya ejecutado de
    // verdad. Se dice, porque el sintoma de un banco mal construido y el de un
    // build lento son el mismo: no termina.
    if (r.signal)
      comprobar(`${etiqueta}: el banco se ha COLGADO, no ha tardado. Si una llamada a`
        + ' node no esta en LLAMADAS_NODE, se ejecuta de verdad y esto no termina nunca',
      false);

    comprobar(`${etiqueta}: ${c.motivo} -> el build sale con ${c.build}`,
      r.status === c.build);
    comprobar(`${etiqueta}: ${c.motivo} -> ${c.muere ? 'anuncia el fallo' : 'no anuncia fallo'}`,
      ultima.includes(CIERRE_FALLO) === c.muere);

    // El fallo tiene que ser lo ULTIMO que se dice. Si aparece antes, el
    // cierre se ha colado dentro del bloque del cronometro y el build se
    // saltaria los junctions, que es justo por lo que el fallo vive en :end.
    comprobar(`${etiqueta}: el fallo se dice al final y no en medio`,
      salida.findIndex((x) => x.includes(CIERRE_FALLO)) === (c.muere ? salida.length - 1 : -1));

    // Ni cronometro ni reparto se autocomprueban, asi que ninguno de los dos
    // puede decir nada de la duracion de la suite. Anunciarse midiendo sin
    // haber medido es el verde falso con forma de cronometro.
    if (c.ntest) {
      comprobar(`${etiqueta}: con el test del cronometro en rojo no se anuncia una medicion`,
        !salida.some((x) => x.includes('Timing the suite')));
    }

    if (c.nbanco) {
      comprobar(`${etiqueta}: con el test del reparto en rojo no se anuncia una medicion`,
        !salida.some((x) => x.includes('Timing the suite')));
    }

    rmSync(banco, { force: true });
  }
} finally {
  rmSync(BANCO, { recursive: true, force: true });
}

console.log('\nla autocomprobacion del cronometro, con codigo propio');

// Tres acumuladores y tres guard. El tercero lleva codigo propio porque es el
// UNICO de los tres que significa que las OTRAS respuestas no son de fiar: si el
// tool no pasa sus propios tests, cualquier medicion que hubiera dado queda sin
// comprobar. Un 1 aqui diria "no he medido", que es cierto y no es lo que hay
// que arreglar.
const initSELF = LINEAS.find((l) => l.trim().toLowerCase() === 'set "selftest_fatal=0"');

comprobar('build.bat tiene un acumulador propio para la autocomprobacion, inicializado a 0',
  initSELF !== undefined);
comprobar('y se inicializa ANTES del bloque del cronometro que lo marca',
  initSELF !== undefined && LINEAS.indexOf(initSELF) < LINEAS.indexOf(BLOQUE_PERF[0]));

// La rama se busca por su MENSAJE y no por el nombre de la variable: lo que se
// comprueba es que ESA rama marque el acumulador nuevo y no el viejo. Con el
// nombre seria tautologia, la asercion compararia una linea consigo misma.
const RAMA_SELF = LINEAS.findIndex((l) => l.includes("timing tool's own tests are red"));

comprobar('la rama que detecta la autocomprobacion en rojo existe',
  RAMA_SELF > -1);
comprobar('y marca SELFTEST_FATAL, no PERF_FATAL',
  RAMA_SELF > -1
  && LINEAS.slice(RAMA_SELF, RAMA_SELF + 12).some((l) => l.trim().toLowerCase() === 'set "selftest_fatal=1"')
  && !LINEAS.slice(RAMA_SELF, RAMA_SELF + 12).some((l) => l.trim().toLowerCase() === 'set "perf_fatal=1"'));

const colaSELF = LINEAS.find((l) => l.trim() === 'if ' + Q + '!SELFTEST_FATAL!' + Q + '==' + Q + '1' + Q + ' (');

comprobar('la cola tiene su propio guard para la autocomprobacion', colaSELF !== undefined);
comprobar('y sale con 3, que no es el 1 de PERF_FATAL ni el de BUILD_FATAL',
  colaSELF !== undefined
  && LINEAS.slice(LINEAS.indexOf(colaSELF), LINEAS.indexOf(colaSELF) + 16)
    .some((l) => l.includes('exit /b 3')));
comprobar('el 3 aparece una sola vez: un codigo repetido en dos sitios son dos reglas',
  LINEAS.filter((l) => l.includes('exit /b 3')).length === 1);

// Y lo que NO se puede tocar, que es la otra mitad del encargo. El 1 del
// cronometro sigue siendo una medicion que EXISTE y que dice algo malo: el build
// avisa y sale con 0. El caso vive en la tabla de arriba, y esta asercion
// existe para que cambiarlo sea una decision explicita y no un efecto colateral
// de haber tocado el 3.
const UNO_SOLO_AVISO = CASOS.find((c) => c.ncrono === 1);

comprobar('el 1 del cronometro sigue siendo SOLO AVISO: el build sale con 0',
  UNO_SOLO_AVISO !== undefined && UNO_SOLO_AVISO.build === 0 && UNO_SOLO_AVISO.muere === false);

console.log('\nlas ramas del reparto, contadas en el build.bat');

// Las ramas se buscan por FORMA y no por texto exacto. En batch el `if`
// cierra la comilla de la variable antes del `==` y la del literal
// despues, asi que una cadena armada a mano depende de donde uno se
// imagine la comilla y deja de casar sin que el build cambie: un rojo que
// no significa nada, y un rojo que no significa nada enseña a ignorar
// los rojos.
//
// Y esto es estructural, y por eso lleva un numero. Si alguien anade una
// quinta clase, o convierte en fatal una rama que antes avisaba, el conteo
// se mueve y el test pide una decision explicita en lugar de dejarla pasar
// en silencio. El mensaje de fallo dice que hacer: actualizar el numero y
// el caso que lo cubre.
function codigoDeRama(linea) {
  const l = linea.trim();

  if (!l.includes('!PERF_EXIT!') || !l.includes('==') || !l.endsWith('('))
    return null;

  const bruto = l.slice(l.indexOf('==') + 2).split('(')[0].split(Q).join('').trim();

  for (const c of bruto)
    if (c < '0' || c > '9')
      return null;

  return bruto === '' ? null : Number(bruto);
}

const codigos = LINEAS.map(codigoDeRama).filter((c) => c !== null);

for (const c of [0, 1, 2]) {
  comprobar(`build.bat tiene su rama para el codigo ${c} del cronometro`,
    codigos.includes(c));
}

comprobar('build.bat no repite codigo en dos ramas',
  new Set(codigos).size === codigos.length);


comprobar('build.bat tiene un else para los codigos que el tool no usa',
  BLOQUE_PERF.some((l) => l.trim() === ') else ('));

// El numero de ramas que devuelven el fallo, y CUALES son. SEIS:
//
//     sin binario de tests          se pidio medir y no habia nada que medir
//     sin node en el PATH           se pidio medir y no se podia medir
//     el test del reparto en rojo    el build no sabe leer un resultado
//     el cronometro ha fallado      el cronometro no pudo terminar
//     el codigo 2                   no se ha medido
//     un codigo que el tool no usa   no se sabe que ha pasado
//
// La septima, "el test del cronometro en rojo", NO esta aqui: tiene su propio
// acumulador y su propio codigo de salida, y se cuenta mas abajo. Que no se
// contara antes no era un descuido de la cuenta: era que las dos cosas
// comparten el mismo `set`, y por eso eran una sola rama.
//     el codigo 2                   no se ha medido
//     un codigo que el tool no usa   no se sabe que ha pasado
//
// El 1 del cronometro NO esta, y esa es la parte que cuesta defender ante alguien
// con prisa: un 1 es una medicion que existe y que dice algo malo, y un 2 es que
// no hay medicion. Fallar por lo segundo es correcto; fallar por lo primero
// convierte el cronometro en un aviso que nadie escucha.
//
// SE CONTAN CON NOMBRE PORQUE EL NUMERO SOLO NO SIRVE. Estas ramas se cuentan
// comparando la linea entera, y una linea `set "PERF_FATAL=1` sin la comilla de
// cierre --que en batch funciona igual, porque cmd es indulgente-- NO CUENTA.
// Cuando se anadieron dos ramas, el test se puso verde con el numero viejo y con
// dos guard invisible: un verde por el motivo equivocado no avisa de nada y
// ademas convence. De ahi la asercion de comillas, mas abajo.
const marcasFallo = BLOQUE_PERF.filter((l) => l.trim().toLowerCase() === 'set "perf_fatal=1"').length;

comprobar('build.bat marca el fallo en 6 ramas, y son las que se han contado',
  marcasFallo === 6);

comprobar('build.bat devuelve el fallo con exit /b 1 al final de la cola',
  COLA.some((l) => l.trim().toLowerCase() === 'endlocal & exit /b 1'));

// ── Los guards de junctions ──
//
// Los mismos que el cronometro, con el mismo patron: se acumulan y se devuelven
// en la cola. Y con la misma excepcion que el `Rastreado=SI`, que NO es fatal y
// por eso necesita su propia comprobacion en positivo: no sustituir un path que
// git rastrea es la respuesta correcta, y hacerlo fatal seria fallar por tener
// la copia buena.

console.log('\nlas comprobaciones que antes solo avisaban, y ahora fallan');

// El acumulador existe y arranca a 0, o el `if` de la cola compararia una
// variable vacia contra "1" y no pasaria nunca.
const initBUILD = LINEAS.find((l) => l.trim().toLowerCase() === 'set "build_fatal=0"');

comprobar('build.bat tiene un acumulador propio para los junctions, inicializado a 0',
  initBUILD !== undefined);
// El orden importa y mira al LUGAR DONDE SE USA, que es el `call`, no la
// definicion de la etiqueta. La asercion anterior miraba la definicion y por
// eso daba verde con el acumulador 40 lineas por DEBAJO de los `call`: un
// `set ...=0` debajo borra el fallo que las subrutinas acaban de marcar, y el
// guard queda verde para siempre. Se comparan las dos ultimas lineas que
// ejecutan estas subrutinas, que son las que mueven el acumulador.
const ULTIMO_CALL = Math.max(
  LINEAS.findIndex((l) => l.trim().toLowerCase().startsWith('call :avisarriesienlace')),
  LINEAS.findIndex((l) => l.trim().toLowerCase().startsWith('call :crearenlacesiprocede')),
);
comprobar('y se inicializa ANTES del ultimo `call` que la marca, no despues',
  initBUILD !== undefined && ULTIMO_CALL > -1 && LINEAS.indexOf(initBUILD) < ULTIMO_CALL);
comprobar('y el cronometro NO lo usa: son dos guard distintos y no uno',
  BLOQUE_PERF.every((l) => !l.toLowerCase().includes('build_fatal')));

// Los cuatro que no se estaban haciendo. Se cuentan por su mensaje, que es lo
// unico que sobrevive a que alguien reescriba el codigo de alrededor.
// Tres mensajes que NO son tres ramas: los dos del mklink explican el mismo
// fallo y salen del mismo `if`. Contarlos como dos seria contar un mensaje, no
// un sitio donde se decide.
const NO_SE_COMPROBABA = [
  'the check that protects this path did not run',
  'could not create the link to',
  'is a junction, and',
];

// En minuscula las dos partes: el texto del build.bat empieza por mayuscula
// porque va detras de un marcador, y comparar en minuscula contra minuscula no
// casa nunca. Un test que no casa con lo que deberia no comprueba nada, y lo
// enseña en rojo sin decir por que.
const enMinusculas = LINEAS.map((l) => l.toLowerCase());

for (const marca of NO_SE_COMPROBABA) {
  comprobar(`build.bat avisa en rojo de "${marca}"`,
    enMinusculas.some((l) => l.includes(marca.toLowerCase())));
}

const marcasBUILD = LINEAS.filter((l) => l.trim().toLowerCase() === 'set "build_fatal=1"').length;

comprobar('build.bat marca BUILD_FATAL en 3 ramas: git mudo, mklink y junction vigilado',
  marcasBUILD === 3);

// El que NO debe ser fatal. Si algun dia lo es, es que alguien ha tratado como
// error tener la copia versionada, que es justo lo contrario de lo que quiere.
comprobar('un path que git rastrea NO es motivo de fallo: se avisa y se sigue',
  LINEAS.some((l) => l.includes('NO se enlaza: git rastrea ficheros'))
  && LINEAS.every((l) => !(l.includes('Rastreado') && l.includes('BUILD_FATAL=1'))));

// La cola, y el orden: junctions se montan ANTES de la cola, asi que el aviso
// tiene que decir que el fallo no es del cronometro.
const colaBUILD = LINEAS.find((l) => l.trim() === 'if ' + Q + '!BUILD_FATAL!' + Q + '==' + Q + '1' + Q + ' (');

comprobar('la cola falla cuando BUILD_FATAL esta a 1', colaBUILD !== undefined);
comprobar('y distingue el fallo del cronometro del fallo de los junctions',
  colaBUILD !== undefined
  && LINEAS.slice(LINEAS.indexOf(colaBUILD), LINEAS.indexOf(colaBUILD) + 12)
    .some((l) => l.includes('did NOT run')));
comprobar('los dos guard se leen los dos, no uno o el otro',
  LINEAS.filter((l) => l.includes('endlocal & exit /b 1')).length === 2);

console.log('\nel flujo principal no puede CAER dentro de una subrutina');

// Batch no distingue llamar de continuar. Si el flujo principal llega a una
// etiqueta sin un `call` delante, %1 es el modo del build y %2 y %3 estan
// vacios: ahi se ejecutaba `mklink /J "" ""` en cada build, que imprimia un
// enlace con la ruta vacia. Y el `goto :eof` de esa subrutina, sin `call` que
// lo contenga, no devuelve: termina el script entero. Con el se iba la cola
// entera de :end, o sea el `exit /b 1` de los dos guards. Medido: con
// PERF_FATAL=1 puesto, el script salia con 0 y sin imprimir nada de la cola.
//
// El banco no lo veia porque no ejecuta el script: se reensambla sus propios
// fragmentos en el orden correcto, que es justo el orden que faltaba aqui.
// Asi que esta asercion es sobre el texto del fichero, no sobre una corrida.
const ETIQUETAS = LINEAS
  .map((l, i) => [l.trim(), i])
  .filter(([l]) => /^:[A-Za-z]/.test(l));

// La ultima linea EJECUTABLE antes de la etiqueta. Comentarios, `rem` y
// blancos no cuentan: batch se los salta, asi que no cortan el flujo.
const ULTIMA_EJECUTABLE = (i) => {
  for (let k = i - 1; k >= 0; k -= 1) {
    const t = LINEAS[k].trim();
    if (!t || t.startsWith('::') || /^rem/i.test(t)) continue;
    return t;
  }
  return '';
};

const POR_CAIDA = ETIQUETAS
  .filter(([, i]) => !/^(goto|exit\s*\/b)/i.test(ULTIMA_EJECUTABLE(i)));

comprobar('build.bat tiene etiquetas de subrutina que comprobar', ETIQUETAS.length >= 3);
comprobar('ninguna se alcanza por caida: la linea de antes siempre salta',
  POR_CAIDA.length === 0);
comprobar('y la de los junctions salta a la cola, no se come el flujo',
  ULTIMA_EJECUTABLE(ETIQUETAS.find(([l]) => l === ':crearEnlaceSiProcede')[1])
    === 'goto :end');

// ── Las comillas ──
//
// La asercion mas tonta del fichero y la que mas ha costado: siete lineas
// `set "X"` se quedaron sin la comilla de cierre. Batch no se rompe por eso --lo
// medido: con y sin comilla, `set` da el mismo valor-- pero el conteo de ramas
// de arriba las cuenta comparando la linea entera, y dos de ellas eran
// precisamente las ramas nuevas. El test se puso verde con el numero viejo y
// dos guard invisible, que es el peor modo de estar verde.
//
// Se comprueba sobre LINEAS, que viene del fichero leido, y no sobre una copia
// filtrada: el fallo es del fichero.

console.log('\nlas comillas de los `set`, que en batch no rompen y aqui si cuentan');

const setsImpares = LINEAS.filter((l) => l.trim().startsWith('set ') && l.split(Q).length % 2 === 0);

comprobar('ninguna linea `set "..."` se queda sin la comilla de cierre',
  setsImpares.length === 0);

if (setsImpares.length > 0)
  for (const l of setsImpares)
    console.log(`          ${l.trim()}`);


console.log('\n' + '='.repeat(64));
console.log(fallos.length === 0
  ? `TODO EN VERDE: ${total} aserciones`
  : `ROJO: ${fallos.length} fallo(s) de ${total} aserciones`);

process.exit(fallos.length === 0 ? 0 : 1);