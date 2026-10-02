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
// y no encadenadas en el bucle porque van a crecer: cuando entre una tercera,
// el caso nuevo es una fila mas y no una rama mas. La clave de cada fila es la
// linea EXACTA del build.bat.
const LLAMADAS_NODE = [
  { linea: 'node tools\\test_duraciones_suite.mjs', codigo: 'ntest' },
  { linea: 'node tools\\test_build_bat_perf.mjs', codigo: 'nbanco' },
  { linea: 'node tools\\duraciones-suite.mjs', codigo: 'ncrono' },
];

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
  { ntest: 1, nbanco: 0, ncrono: 0, build: 1, muere: true, motivo: 'el test del cronometro en rojo' },
  { ntest: 0, nbanco: 1, ncrono: 0, build: 1, muere: true, motivo: 'el test del reparto en rojo' },
];

try {
  mkdirSync(BANCO, { recursive: true });
  writeFileSync(SENUELO, '');

  for (const c of CASOS) {
    const cuerpo = [];
    let sustituciones = 0;

    for (const l of [SETLOCAL, PERF_FATAL, 'set "RUN_PERF=1"', ...BLOQUE_PERF, ...COLA]) {
      const s = l.trim();
      const sangria = l.slice(0, l.length - l.trimStart().length);

      // El `if not exist` del binario real mira uno que este banco crea, para
      // que la rama que se prueba sea la de medir y no la de "no hay nada".
      if (s.startsWith('if not exist') && s.includes('ABDAudioLab_Tests.exe')) {
        cuerpo.push(`${sangria}if not exist "${SENUELO}" (`);
        sustituciones += 1;
      } else {
        const llamada = LLAMADAS_NODE.find((x) => x.linea === s);

        if (llamada) {
          cuerpo.push(`${sangria}cmd /c exit ${c[llamada.codigo]}`);
          sustituciones += 1;
        } else {
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
    const r = spawnSync('cmd', ['/c', banco], { cwd: RAIZ, encoding: 'utf8' });
    const salida = (r.stdout || '').split(LF).filter((x) => x.trim());
    const ultima = salida.length ? salida[salida.length - 1].trim() : '';

    comprobar(`${etiqueta}: el banco se ha ejecutado de verdad`,
      !r.error && salida.length > 0);

    comprobar(`${etiqueta}: el banco sustituye el exe y las ${LLAMADAS_NODE.length} llamadas a node (${LLAMADAS_NODE.length + 1})`,
      sustituciones === LLAMADAS_NODE.length + 1);
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

// El numero de ramas que devuelven el fallo, y CUALES son. Cuatro, y solo
// cuatro:
//
//     el test del reparto de codigos en rojo  ->  no se sabe leer el cronometro
//     el test del cronometro en rojo           ->  no se sabe si mide
//     el codigo 2                              ->  no se ha medido
//     un codigo que el tool no usa             ->  no se sabe que paso
//
// El 1 NO esta, y esa es la parte que cuesta trabajo defender ante alguien con
// prisa. Un 1 es una medicion que existe y que dice algo malo; un 2 es que no
// hay medicion. Fallar por lo segundo es correcto y fallar por lo primero es
// como un guard que se pone rojo por ir lento, que es un guard que nadie
// escucha. El dia que ese 1 entre aqui, este numero pasa a 5 y el mensaje de
// este test tiene que decir por que.
const marcasFallo = BLOQUE_PERF.filter((l) => l.trim().toLowerCase() === 'set "perf_fatal=1"').length;

comprobar('build.bat marca el fallo en 4 ramas: reparto rojo, test rojo, codigo 2 y codigo imposible',
  marcasFallo === 4);

comprobar('build.bat devuelve el fallo con exit /b 1 al final de la cola',
  COLA.some((l) => l.trim().toLowerCase() === 'endlocal & exit /b 1'));

console.log('\n' + '='.repeat(64));
console.log(fallos.length === 0
  ? `TODO EN VERDE: ${total} aserciones`
  : `ROJO: ${fallos.length} fallo(s) de ${total} aserciones`);

process.exit(fallos.length === 0 ? 0 : 1);