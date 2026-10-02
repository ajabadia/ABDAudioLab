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
 * ---------------------------------------------------------------------------
 * POR QUE SE EJECUTA EL FICHERO ENTERO, Y NO UNA COPIA DE SU LOGICA
 *
 * Una copia de la logica en JavaScript estaria en verde mientras el .bat hiciera
 * otra cosa, que es justo el fallo que se quiere evitar. Y pegar el bloque del
 * cronometro con su cola de :end es mucho mas que eso, pero aun asi se deja
 * cosas por el camino: LOS FALLOS DE LAYOUT.
 *
 * Un fallo de layout no depende de lo que dice una linea sino de lo que hay a su
 * alrededor. El medido: `set "PERF_ESTADO=%%e` sin la comilla de cierre, DENTRO
 * de un bloque if/else. cmd empareja las comillas cruzando lineas, el `for /f`
 * deja de ejecutar el `findstr` y busca un fichero, el estado se queda en
 * `desconocido` y la rama que separa un cronometro roto de una suite lenta no
 * puede dispararse nunca. El banco de fragmentos daba todo eso por bueno,
 * porque al pegar el bloque no hay bloque donde el parser pueda equivocarse.
 *
 * Pegar mas lineas tampoco lo arregla: al pegar se pegan lineas sueltas y se
 * pierde el sitio donde estaban. Asi que aqui se ejecuta el FICHERO, con su
 * estructura de bloques y con las lineas que tiene alrededor, y no se toca ni
 * un byte de el.
 *
 * ---------------------------------------------------------------------------
 * LO QUE SE SUSTITUYE, Y POR QUE NO HACE FALTA TOCAR NADA
 *
 * Un build de verdad mata procesos, compila, cronometra la suite y ejecuta este
 * mismo test. Las cuatro cosas se sustituyen por un shim en el PATH, que es
 * donde cmd busca los programas antes que en ningun otro sitio. Y `node` NO
 * lleva shim, y esa es la parte buena: el build lo llama como `node tools\algo.mjs`
 * RELATIVO al directorio de trabajo, de modo que el esqueleto lleva sus propios
 * `tools/` con tres stubs que contestan lo que el caso pide. El node de verdad
 * los ejecuta y el codigo de salida es el que el stub devuelve.
 *
 * Que el esqueleto traiga `tools/` tiene una consecuencia buena: el stub de
 * `test_build_bat_perf.mjs` NO es este fichero, de modo que el build no puede
 * meterse en un bucle de bancos que se llamen entre si.
 *
 * El esqueleto es lo que hace que el build no dependa de nada de lo que hay
 * alrededor: ni de que este compilado, ni de que el hermano de assets exista.
 * Un `CMakeCache.txt` y un `ABDAudioLab_Tests.exe` de mentira evitan el
 * `if not exist` del binario, y un `ABDSharedAssets` de mentira al lado es lo
 * que ve `SHARED_ASSETS`, para que la seccion de junctions se ejecute DE VERDAD
 * y no se salte. Con esa seccion dentro, cada caso pasa por el principio entero
 * del fichero, que es justo lo que el banco de fragmentos se saltaba entero.
 *
 * Y el `%~dp0` del git se sigue resolviendo al repositorio de verdad, porque el
 * .bat que se ejecuta vive ahi: es lo que permite que `git ls-files` diga la
 * verdad sobre `contracts/hardware` en vez de responder que no hay git. Por eso
 * la copia que se hace para mutar va en la RAIZ y no donde quede la mutacion.
 */

import { spawnSync } from 'node:child_process';
import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
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

// La frase que cierra el build cuando no hay medicion. Se comprueba que es la
// ULTIMA linea que dice algo, y no solo que este en la salida: que se anuncie
// el fallo al final es la mitad del contrato de la cola de :end.
// La frase que cierra el build cuando no hay medicion. Se comprueba que es la
// ULTIMA linea que dice algo, y no solo que este en la salida: que se anuncie
// el fallo al final es la mitad del contrato de la cola de :end.
// La frase que cierra los cuatro guards. La cuarta clase la dice partida en dos
// lineas --'Nothing above this line is a performance result: the thing' / 'that reads the result is the thing that is broken.'--, asi que se
// busca el TROZO que las cuatro comparten, no la frase entera.
const CIERRE_FALLO = 'Nothing above this line is a performance result';

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

// El preambulo NO se pega en ningun sitio. Antes se copiaba aqui, y con el
// banco entero no hace falta: el build de verdad ejecuta su propio
// `setlocal` y sus propios `set`, de modo que comprobarlos es comprobar que
// estan donde tienen que estar, no que un pegado los conserve.

comprobar('build.bat tiene el setlocal de expansion retardada',
  LINEAS.some((l) => l.trim().toLowerCase() === 'setlocal enabledelayedexpansion'));
comprobar('build.bat inicializa PERF_FATAL a 0 antes del cronometro',
  LINEAS.some((l) => l.trim().toLowerCase() === 'set "perf_fatal=0"')
  && LINEAS.findIndex((l) => l.trim().toLowerCase().startsWith('set "perf_fatal=0'))
  < LINEAS.indexOf(BLOQUE_PERF[0]));
comprobar('el banco ha encontrado el bloque del cronometro y la cola de :end',
  BLOQUE_PERF.length > 40 && COLA.length > 3);
console.log('\nlas cuatro clases del cronometro, cada una a su sitio');

// ────────────────────────────────────────────────────────────────────────,// ────────────────────────────────────────────────────────────────────────// LOS SHIMS, Y POR QUE NO SON LO QUE UN ESCRIBE PRIMERO
//
// Un shim tiene que ser un `.exe`, y eso no es un capricho. Medido: un `.cmd`
// que el build invoca por su NOMBRE, encontrado en el PATH y sin `call`
// delante, TERMINA el script que lo llama. No devuelve el control, y su codigo
// de salida pasa a ser el del build entero. Con un `.cmd` de shim, el build se
// paraba tras el cartel de cabecera, con codigo 0, y no hacia ni una linea mas.
//
// Y no puede ser un `.exe` de mentira, porque `cmake` tiene que SALIR CON 0: si
// sale con otra cosa, el build dice [Error] Build failed y se va antes de
// cronometrar. Asi que hay tres formas y cada una tapa un caso:
//
//   cmake.exe      una copia de attrib.exe. Se busca un ejecutable que acepte
//                  cualquier argumento, salga con 0 y no toque nada; de los que se
//                  han probado, ese es el unico. Imprime una linea de error
//                  inutil, y esa linea se comprueba en vez de esconderse.
//   taskkill.exe   un fichero que no es un ejecutable. Al build no le importa su
//                  codigo y redirige su salida a nul, de modo que solo hace
//                  falta que devuelva el control: sin esto el test mataria la
//                  aplicacion que alguien tenga abierta.
//   ping.exe       lo mismo. El build espera un segundo al empezar --con un
//                  `ping -n 2 127.0.0.1` y no con un `timeout /t`, que con la
//                  entrada redirigida no espera-- y aqui eso no es lo que se
//                  prueba.
//
// Y `node` NO lleva shim, y esa es la parte buena. El build lo llama como
// `node tools\algo.mjs`, RELATIVO al directorio de trabajo, de modo que el
// esqueleto lleva sus propios `tools/` con tres stubs que contestan lo que el
// caso pide. El node de verdad ejecuta los stubs, el codigo de salida es el que
// el stub devuelve, y el veredicto lo imprime el stub por stdout, que es donde el
// build lo recoge con `type` y lo lee despues con `findstr`.
//
// Que el esqueleto traiga `tools/` tiene una consecuencia buena: el stub de
// `test_build_bat_perf.mjs` NO es este fichero, de modo que el build no puede
// meterse en un bucle de bancos que se llamen entre si.
const SHIMS = [
  {
    nombre: 'cmake.exe',
    de: 'attrib.exe',
    porque: 'tiene que salir con 0 o el build no llega al cronometro',
  },
  { nombre: 'taskkill.exe', porque: 'sin esto el test mata la app de quien lo corre' },
  { nombre: 'ping.exe', porque: 'sin esto cada caso espera un segundo de mas' },
  // El mas importante de los cuatro y el mas facil de no pensar. El build
  // arranca con `where cl.exe`: si lo encuentra se lo salta todo y se va
  // directo a junctions. Aqui eso no es solo una prueba de menos: medido, un
  // caso pasaba de 1,2 s a 17 s por buscar Visual Studio, y con el limite de
  // 40 s del banco habia casos que SE COLGABAN sin haber hecho nada malo. El
  // banco tiene que depender de este .bat y de node, no de lo que tenga
  // instalado el equipo donde se corre.
  { nombre: 'cl.exe', porque: 'sin esto se busca Visual Studio, y el banco depende de el' },
];

// Los stubs. Tabla y no ifs, por lo mismo que los shims: anadir uno nuevo es
// anadir una fila. Cada uno deja su nombre en un rastro, que es lo que permite
// comprobar que los tres se han llamado y que no se ha llamado ninguno mas.
const STUBS = [
  { nombre: 'auditar-bats.mjs', salida: 'ABD_LAYOUT' },
  { nombre: 'test_duraciones_suite.mjs', salida: 'ABD_TEST_TOOL' },
  { nombre: 'test_build_bat_perf.mjs', salida: 'ABD_TEST_BANCO' },
  { nombre: 'duraciones-suite.mjs', cronometro: true },
];

// El esqueleto. Es lo que hace que el build no dependa de nada de lo que hay
// alrededor: ni de que este compilado, ni de que el hermano de assets exista, ni
// de que haya un node de verdad al que poder llamar.
function montarBanco(etiqueta) {
  const raiz = join(BANCO, etiqueta);
  const cwd = join(raiz, 'caso');
  const shims = join(raiz, 'shims');
  const tools = join(cwd, 'tools');
  const rastro = join(raiz, 'rastro.txt');

  // SHARED_ASSETS es '..\ABDSharedAssets' relativo al directorio de trabajo, asi
  // que el hermano va al lado del caso. Sin el, el build se salta la seccion de
  // junctions entera y no se prueba nada de ella.
  const hermano = join(raiz, 'ABDSharedAssets');

  mkdirSync(join(cwd, 'build', 'Release'), { recursive: true });
  mkdirSync(shims, { recursive: true });
  mkdirSync(tools, { recursive: true });
  // El hermano tiene que EXISTIR, porque el build se salta la seccion entera
  // si no. Lo que hay dentro da igual: no se llega a enlazar nada.
  mkdirSync(hermano, { recursive: true });

  // Y estos dos se crean de verdad para que el build NO llegue a `mklink`: su
  // comprobacion `if exist` los deja como estan. Es lo que evita que el banco
  // monte junctions de verdad, que un borrado recursivo despues no sabe quitar
  // sin llevarse por delante el contenido de a donde apuntan. El unico path que
  // se deja sin crear es `contracts\hardware`, que es el vigilado: ese es el que
  // tiene que llegar a git y decir que esta versionado.
  mkdirSync(join(cwd, 'assets', 'models'), { recursive: true });
  mkdirSync(join(cwd, 'assets', 'brands'), { recursive: true });

  // El CMakeCache evita la configuracion y el exe de mentira evita la rama de
  // "no hay binario que cronometrar", que cortocircuitearia el banco entero.
  writeFileSync(join(cwd, 'build', 'CMakeCache.txt'), 'banco');
  writeFileSync(join(cwd, 'build', 'Release', 'ABDAudioLab_Tests.exe'), '');
  writeFileSync(rastro, '');

  for (const shim of SHIMS) {
    if (shim.de) {
      // Una COPIA y no un enlace: un enlace a System32 seria un shim que depende
      // de lo que la limpieza hiciese con build/, que es justo la carpeta donde
      // vive.
      copyFileSync(join(process.env.SystemRoot || 'C:/Windows', 'System32', shim.de),
        join(shims, shim.nombre));
    }
    else
      writeFileSync(join(shims, shim.nombre), 'esto no es un ejecutable');
  }

  for (const stub of STUBS)
    writeFileSync(join(tools, stub.nombre), stub.cronometro ? stubDelCronometro() : stubDe(stub), 'utf8');

  return { raiz, cwd, shims, rastro };
}

// El stub de los dos tests: contesta con el codigo del caso y anota que se ha
// llamado.
function stubDe(stub) {
  return [
    `import { appendFileSync } from 'node:fs';`,
    ``,
    `appendFileSync(process.env.ABD_RASTRO, ${Q}${stub.nombre}${Q} + String.fromCharCode(10));`,
    `process.exit(Number(process.env.${stub.salida}));`,
    ``,
  ].join(LF);
}

// Y una cuarta forma, que es la que mas ha costado: la comilla simple. El
// veredicto es JSON, y el JSON lleva comillas DOBLES dentro, asi que un
// literal de JavaScript con comillas dobles no se puede escribir con comillas
// dobles. Con la doble a pelo el stub salia con el literal cortado en la
// primera de dentro, el node de verdad se caia antes de imprimir nada, y el
// banco se daba verde con el cronometro sin ejecutar.
const S = String.fromCharCode(39);

// El stub del cronometro. Imprime el veredicto por stdout, que es donde el build
// redirige con `>"!PERF_LOG!"` antes de buscarlo con `findstr`. Y sale con el
// codigo que el caso pide, que es lo que decide el reparto.
function stubDelCronometro() {
  return [
    `import { appendFileSync } from 'node:fs';`,
    ``,
    `appendFileSync(process.env.ABD_RASTRO, ${S}duraciones-suite.mjs${S} + String.fromCharCode(10));`,
    ``,
    `if (process.env.ABD_SIN_VEREDICTO !== ${S}1${S})`,
    `  console.log(${S}ABD-VEREDICTO {"estado":"${S} + process.env.ABD_CRONO_ESTADO`,
    `    + ${S}","codigo":${S} + process.env.ABD_CRONO_SALIDA + ${S}}${S});`,
    ``,
    `process.exit(Number(process.env.ABD_CRONO_SALIDA));`,
    ``,
  ].join(LF);
}

// El .bat que se ejecuta. Sin ABD_BUILD_BAT es el del arbol, sin copiar. Con el,
// una COPIA EN LA RAIZ, y no donde quede la mutacion: %~dp0 es donde el build
// pregunta a git por contracts/hardware, y esa pregunta solo tiene respuesta
// buena dentro del repositorio. Ejecutar la copia en otro sitio no daria un
// fallo: daria OTRO resultado, que es peor.
function batDe(etiqueta) {
  if (!process.env.ABD_BUILD_BAT)
    return { ruta: RUTA_BAT, copia: null };

  const copia = join(RAIZ, 'build.bat.banco-' + etiqueta + '.bat');

  copyFileSync(RUTA_BAT, copia);

  return { ruta: copia, copia };
}

// ntest = codigo del test del cronometro. nbanco = codigo del test de este
// reparto. ncrono = codigo del cronometro. build = lo que tiene que salir.
// muere = si el build tiene que terminar en fallo. estado = lo que dice la linea
// de veredicto, si el caso la trae. sinVeredicto = el tool se rompe antes de
// imprimir nada, y el build no puede inventarse un estado.
//
// Las dos degradadas van en la misma tabla y no aparte porque son clases del
// mismo reparto y del mismo mecanismo: un 1 sin veredicto avisa y un 2 sin
// veredicto falla, porque el codigo es lo unico que queda.
const CASOS = [
  { ntest: 0, nbanco: 0, ncrono: 0, estado: 'ok', build: 0, muere: false, motivo: 'medido y nada que decir' },
  { ntest: 0, nbanco: 0, ncrono: 1, estado: 'lento', build: 0, muere: false, motivo: 'medido y algo que mirar' },
  { ntest: 0, nbanco: 0, ncrono: 1, estado: 'regresion', build: 0, muere: false, motivo: 'el veredicto dice regresion' },
  { ntest: 0, nbanco: 0, ncrono: 2, estado: 'sin-medir', build: 1, muere: true, motivo: 'no medido' },
  { ntest: 0, nbanco: 0, ncrono: 7, estado: 'ok', build: 1, muere: true, motivo: 'codigo que el tool no usa' },
  { ntest: 1, nbanco: 0, ncrono: 0, estado: 'ok', build: 3, muere: true, motivo: 'el test del cronometro en rojo' },
  { ntest: 0, nbanco: 1, ncrono: 0, estado: 'ok', build: 1, muere: true, motivo: 'el test del reparto en rojo' },
  { ntest: 0, nbanco: 0, ncrono: 2, estado: 'fallo-del-tool', build: 1, muere: true, motivo: 'el cronometro se rompio' },
  // La que hace que esto valga: el 1 de una medicion que NO llego a terminar es
  // la misma clase que el 1 de una regresion, y no pueden acabar igual.
  { ntest: 0, nbanco: 0, ncrono: 1, estado: 'medicion-incompleta', build: 1, muere: true, motivo: 'la medicion no llego a terminar' },
  { ntest: 0, nbanco: 0, ncrono: 1, sinVeredicto: true, build: 0, muere: false, motivo: 'sin veredicto y el codigo es 1' },
  { ntest: 0, nbanco: 0, ncrono: 2, sinVeredicto: true, build: 1, muere: true, motivo: 'sin veredicto y el codigo es 2' },
  // La cuarta clase, y la que hace que sea una clase: el .bat esta roto por
  // dentro, asi que lo que el cronometro mida no lo lee un script fiable. Sale
  // con 4 y no con el 1 de PERF_FATAL, que seria "no he medido": aqui el
  // problema es de quien lee, no de lo que se mide.
  { nlayout: 1, ntest: 0, nbanco: 0, ncrono: 0, estado: 'ok', build: 4, muere: true, motivo: 'el build script esta roto por dentro' },
];

// El orden REAL en que el build llama a node. Vive al lado de la tabla y no
// dentro de ella porque describe al .bat, no a los stubs: si el build cambia
// el orden, esto es lo que se pone rojo.
const ORDEN = ['auditar-bats.mjs', 'test_build_bat_perf.mjs', 'test_duraciones_suite.mjs', 'duraciones-suite.mjs'];

const COPIAS = [];

try {
  mkdirSync(BANCO, { recursive: true });

  for (const c of CASOS) {
    // El estado va en la etiqueta porque hay dos casos con el mismo codigo de
    // cronometro y distinto estado, y con la etiqueta de antes se pisarian.
    const etiqueta = 'l' + (c.nlayout || 0) + '-t' + c.ntest + '-b' + c.nbanco + '-c' + c.ncrono
      + (c.estado ? '-' + c.estado : '')
      + (c.sinVeredicto ? '-sinveredicto' : '');
    const { raiz, cwd, shims, rastro } = montarBanco(etiqueta);
    const { ruta: bat, copia } = batDe(etiqueta);

    if (copia)
      COPIAS.push(copia);

    // `perf` y no nada: sin el, `RUN_PERF` se queda a 0 y el bloque entero del
    // cronometro -- con sus ramas y su lectura del veredicto -- no se ejecuta
    // nunca. El banco pasaria en verde sin haber llegado a la mitad que
    // comprueba, y lo que hace es precisamente ejecutar esa mitad.
    const r = spawnSync('cmd', ['/c', bat, 'perf'], {
      cwd,
      encoding: 'utf8',
      // El limite no es una prudencia, es la diferencia entre un rojo y un
      // cuelgue. Lo que se cuelga aqui no es un build lento: es una llamada a
      // node que el esqueleto no ha sustituido y que esta midiendo la
      // suite de verdad, que son minutos. Y tiene que ser amplio: medido, un
      // caso va de 1 s a 20 s con la maquina ocupada, asi que 40 s --lo que
      // bastaba para el banco de fragmentos-- caia de vez en cuando y ponia
      // en rojo un banco entero sin que el .bat hubiera cambiado.
      timeout: 150000,
      env: {
        ...process.env,
        PATH: shims + ';' + process.env.PATH,
        ABD_RASTRO: rastro,
        ABD_LAYOUT: String(c.nlayout || 0),
        ABD_TEST_TOOL: String(c.ntest),
        ABD_TEST_BANCO: String(c.nbanco),
        ABD_CRONO_SALIDA: String(c.ncrono),
        ABD_CRONO_ESTADO: c.estado || 'ok',
        ABD_SIN_VEREDICTO: c.sinVeredicto ? '1' : '0',
      },
    });

    const salida = (r.stdout || '').split(LF).filter((x) => x.trim());
    // La salida JUNTA. El ancho de la consola parte los mensajes largos en dos
    // lineas, y las cuatro colas de :end dicen el cierre partido en algún punto:
    // la de la cuarta clase lo tiene justo en el colon. Buscar linea a linea
    // hace que las dos comparaciones del final fallen solo para esa clase,
    // cuando las cuatro dicen exactamente lo mismo. Quien lee el log lo lee
    // junto, asi que aqui tambien.
    const junto = salida.join('');
    // La salida JUNTA. El ancho de la consola parte los mensajes largos en dos
    const llamado = readFileSync(rastro, 'utf8').split(LF).filter((x) => x.trim());

    // signal es SIGTERM cuando salta el timeout, y entonces el build no ha
    // terminado. Se comprueba aparte porque un build a medias puede haber
    // impreso cosas y parecer que ha ido bien.
    comprobar(etiqueta + ': el build termina dentro del limite, sin colgarse', !r.signal);

    if (r.signal)
      comprobar(etiqueta + ': el build se ha COLGADO. Sin stubs, el cronometro de'
        + ' verdad se estaria midiendo la suite entera: cinco minutos por caso',
      false);

    // ESTA ES LA ASERCION QUE JUSTIFICA EL BANCO ENTERO, y las tres partes son
    // puntos distintos del fichero: los junctions del principio, la compilacion
    // en medio y el cronometro al final. El fallo de 6.15 estaba en la tercera, y
    // no por lo que decia la linea sino por el bloque de if/else que la rodeaba,
    // que es exactamente lo que no se conserva al pegar lineas sueltas.
    comprobar(etiqueta + ': el FICHERO ENTERO se ejecuta, no un reensamblado',
      !r.error
      && salida.some((x) => x.includes('NO se enlaza'))
      && salida.some((x) => x.includes('Build Successful')));

    comprobar(etiqueta + ': y llega al cronometro cuando el .bat esta bien y los dos tests pasan',
      c.nlayout || c.ntest || c.nbanco || salida.some((x) => x.includes('Timing the suite')));

    // El orden de las llamadas lo pone el build.bat y no esta tabla, que esta
    // en otro orden: este test se llama PRIMERO, antes que la autocomprobacion
    // del cronometro, porque el reparto de codigos es la promesa que sostiene
    // a la otra. Con los dos en rojo no se llega al cronometro; con el segundo
    // en rojo se llega a los dos primeros y no al tercero.
    // OJO: `c.nlayout !== 0` seria TRUE en todos los casos sin la dimension,
    // porque undefined !== 0. Y ahi esta el fallo entero del banco entero: los
    // doce casos se paraban en el auditor y ninguno llegaba a medir. La forma
    // correcta es preguntar por la verdad de la dimension, no por comparar con
    // un numero que en la maioria de los casos no existe.
    const esperado = c.nlayout
      ? ORDEN.slice(0, 1)
      : c.nbanco !== 0
        ? ORDEN.slice(0, 2)
        : c.ntest !== 0
          ? ORDEN.slice(0, 3)
          : ORDEN;

    // Que los stubs se hayan llamado es lo que prueba que el cronometro se
    // ha ejecutado de verdad, y que no se ha llamado ninguno mas es lo que
    // atrapa una llamada nueva a node: sin esto pasaria desapercibida, porque el
    // build no avisa de lo que no entiende.
    comprobar(etiqueta + ':' + (esperado.length === ORDEN.length
      ? ' el cronometro y sus dos tests se han ejecutado de verdad'
      : ' se ha parado en el primer node en rojo, ANTES de medir')
      + ' [' + esperado.join(' | ') + ']',
      llamado.join(' | ') === esperado.join(' | '));

    // El ruido del shim de cmake se comprueba en vez de esconderse: si manana
    // shimea otra cosa y deja tres lineas, el rojo lo dice aqui y no en el
    // informe de un caso que fallara por otra cosa.
    comprobar(etiqueta + ': el shim de cmake deja el ruido que se espera, ni uno mas',
      salida.filter((x) => x.includes('Formato de par')).length === 1);

    comprobar(etiqueta + ': ' + c.motivo + ' -> el build sale con ' + c.build,
      r.status === c.build);
    comprobar(etiqueta + ': ' + c.motivo + ' -> ' + (c.muere ? 'anuncia el fallo' : 'no anuncia fallo'),
      junto.includes(CIERRE_FALLO) === c.muere);

    // El fallo tiene que ser lo ULTIMO que se dice. Si aparece antes, el cierre
    // se ha colado dentro del bloque del cronometro y el build se saltaria los
    // junctions, que es justo por lo que el fallo vive en :end.
    comprobar(etiqueta + ': el fallo se dice al final y no en medio',
      salida.findIndex((x) => x.includes(CIERRE_FALLO)) === (c.muere
        ? salida.map((x) => x.includes(CIERRE_FALLO)).lastIndexOf(true)
        : -1));

    // Ni cronometro ni reparto se autocomprueban, asi que ninguno de los dos
    // puede decir nada de la duracion de la suite. Anunciarse midiendo sin haber
    // medido es el verde falso con forma de cronometro.
    if (c.ntest || c.nbanco)
      comprobar(etiqueta + ': con un test en rojo no se anuncia una medicion',
        !salida.some((x) => x.includes('Timing the suite')));

    if (c.estado && !c.sinVeredicto) {
      // El stub imprime un veredicto de verdad y el build tiene que LEERLO. Si la
      // extraccion se rompe, el estado se queda en desconocido y el build no se
      // entera: por eso lo que se comprueba es que no aparezca desconocido, y no
      // que salga el estado bueno.
      comprobar(etiqueta + ': el build lee el estado del veredicto y no se queda en desconocido',
        !salida.some((x) => x.includes('State: desconocido')));

      comprobar(etiqueta + ': y el estado que lee es el que dice el veredicto',
        !salida.some((x) => x.includes('State: ') && !x.includes('State: ' + c.estado)));
    }

    if (c.sinVeredicto)
      comprobar(etiqueta + ': sin veredicto el estado se queda en desconocido y se dice',
        salida.some((x) => x.includes('State: desconocido'))
        || !salida.some((x) => x.includes('State: ')));

    rmSync(raiz, { recursive: true, force: true });
  }
} finally {
  for (const copia of COPIAS)
    rmSync(copia, { force: true });

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
// El 1 del cronometro esta SOLO para una de sus dos clases. Un 1 es una medicion
// que existe y que dice algo malo --un test lento, una regresion--, y eso avisa y
// sale con 0, que es lo que hay que hacer: fallar por un test lento convierte el
// cronometro en un aviso que nadie escucha. El otro 1, el de una medicion que no
// llego a existir, NO es un aviso y por eso tiene su rama propia: se decide por el
// estado, que es lo unico que separa las dos clases.
//
// SE CONTAN CON NOMBRE PORQUE EL NUMERO SOLO NO SIRVE. Estas ramas se cuentan
// comparando la linea entera, y una linea `set "PERF_FATAL=1` sin la comilla de
// cierre --que en batch funciona igual, porque cmd es indulgente-- NO CUENTA.
// Cuando se anadieron dos ramas, el test se puso verde con el numero viejo y con
// dos guard invisible: un verde por el motivo equivocado no avisa de nada y
// ademas convence. De ahi la asercion de comillas, mas abajo.
//
// Y POR QUE EL NUMERO DE COMILLAS NO PUEDE SER EL INVARIANTE. Trece lineas de
// build.bat tienen un numero impar de comillas sin que pase nada: son comentarios
// con prosa entrecomillada, que cmd no ejecuta, y el `set` del `for`, que cierra
// en el fin de linea como ha cerrado siempre. Contar comillas no distingue un fallo
// de un comentario. La regla que vigila esto es de COMPORTAMIENTO: el banco pone
// un veredicto de verdad en el temporal y se comprueba que el build lo lee. Un
// `desconocido` con el veredicto presente es el fallo entero, y es lo que se vio
// en un build de verdad: la comilla que faltaba hacia que el `for` no ejecutara
// el `findstr`, el estado se quedaba en desconocido y la rama de `fallo-del-tool`
// no podia dispararse nunca.
const marcasFallo = BLOQUE_PERF.filter((l) => l.trim().toLowerCase() === 'set "perf_fatal=1"').length;

comprobar('build.bat marca el fallo en 7 ramas, y son las que se han contado',
  marcasFallo === 7);

// Y la rama nueva, buscada por su MENSAJE y no por el contador: el numero dice
// que hay siete, y no cuales siete. Con una rama mas y el numero viejo, el test se
// pondria verde sin que el fallo nuevo existiera nunca.
const RAMA_INCOMPLETA = LINEAS.findIndex((l) => l.includes('THE SUITE DID NOT FINISH'));

comprobar('la rama que detecta una medicion que no llego a terminar existe',
  RAMA_INCOMPLETA > -1);

comprobar('y decide por el ESTADO, no solo por el codigo',
  RAMA_INCOMPLETA > -1
  && LINEAS.slice(Math.max(0, RAMA_INCOMPLETA - 8), RAMA_INCOMPLETA)
    .some((l) => l.includes('PERF_ESTADO') && l.includes('medicion-incompleta')));

comprobar(`y marca PERF_FATAL, que es lo que hace que el build falle al final`,
  RAMA_INCOMPLETA > -1
  && LINEAS.slice(RAMA_INCOMPLETA, RAMA_INCOMPLETA + 16)
    .some((l) => l.trim().toLowerCase() === 'set "perf_fatal=1"'));

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
// El banco no lo veia porque no ejecutaba el script: se reensamblaba sus
// propios fragmentos en el orden correcto, que es justo el orden que faltaba
// aqui. Con el fichero entero esta asercion sigue siendo sobre el TEXTO, y no
// por precaucion sino porque un `goto` que falta no se ve ejecutando el build:
// lo que se veria es el sintoma de mas adelante, la cola que no imprime.
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

// ── EL HUECO ENTERO, Y NO SOLO LA LINEA DE ANTES ──
//
// Lo de arriba mira UNA linea: la de justo antes de la etiqueta. Es una regla
// fuerte, y aun asi deja el resto del hueco sin vigilar. Entre ese salto y la
// etiqueta puede haber mas lineas ejecutables, y esas lineas se ejecutan.
//
// LA FORMA DE LA REGLA, Y POR QUE NO ES "MAS DE UNA". El hueco entre el fin de
// un flujo y la etiqueta siguiente tiene cuatro estados posibles, y solo uno es
// un build:
//
//   0 ejecutables   se CAE dentro de la etiqueta. Es el fallo de 6.9: por ahi
//                   entran %1, %2 y %3 --el modo del build y dos vacios--, y el
//                   `goto :eof` de la subrutina sin `call` delante termina el
//                   script entero.
//   1, y no es el    El salto falta y lo que hay es otra cosa. El mismo fallo,
//   salto           con la baja de la senal.
//   1 que es el     Lo unico que se acepta: el salto, y nada detras.
//   salto
//   mas de 1        Codigo despues de un salto que ya no hace falta. Eso es
//                   CODIGO MUERTO y no lo ve ni este test: un `echo` entre dos
//                   `goto :end` no se ejecuta nunca. Se deja escrito porque es
//                   la unica casilla que la regla no cubre, y un invariante
//                   con un agujero sin nombrar es peor que uno sin.
//
// Asi que "mas de una linea ejecutable" deja en verde los dos primeros, que son
// justo el fallo. Lo que se comprueba es que el hueco SEA el salto y nada mas.
const PROFUNDIDAD = [];

{
  let nivel = 0;

  // `rem` y `echo` no se cuentan, con la misma convencion que `bloqueDesde`:
  // cmd no los ejecuta, asi que un parentesis de un texto no abre un bloque.
  // Con otra convencion las dos medidas no hablarian el mismo idioma.
  for (const l of LINEAS) {
    if (/^\s*(rem\s|echo )/i.test(l)) {
      PROFUNDIDAD.push(nivel);
      continue;
    }

    nivel += (l.match(/\(/g) || []).length;
    nivel -= (l.match(/\)/g) || []).length;
    PROFUNDIDAD.push(nivel);
  }
}

// El hueco: las ejecutables que hay entre el salto mas cercano y la etiqueta.
// Se camina hacia atras y se para en el primer salto de la RAIZ del flujo. Un
// `goto` dentro de un `if (...)` NO para la cuenta: detras suyo el flujo sigue
// igual, y el `)` que cierra el bloque es justo lo que hay que contar.
const HUECO = (i) => {
  const dentro = [];

  for (let k = i - 1; k >= 0; k -= 1) {
    if (/^:[A-Za-z]/.test(LINEAS[k].trim()))
      break;

    const t = LINEAS[k].trim();

    if (!t || t.startsWith('::') || /^rem\b/i.test(t))
      continue;

    dentro.push([k + 1, t]);

    if (PROFUNDIDAD[k] === 0 && /^(goto|exit\s*\/b)/i.test(t))
      break;
  }

  return dentro.reverse();
};

const HUECOS = ETIQUETAS.map(([nombre, i]) => [nombre, HUECO(i)]);

// El hueco limpio es una sola linea y es el salto. Ni cero --que es caerse
// dentro-- ni mas de una, que es codigo de mas.
const HUECOS_SUCIOS = HUECOS.filter(([, h]) =>
  h.length !== 1 || !/^(goto|exit\s*\/b)/i.test(h[0][1]));

comprobar('entre el fin de cada flujo y la etiqueta siguiente no hay ni una linea ejecutable de mas',
  HUECOS_SUCIOS.length === 0);

// Y el del final del flujo principal por su nombre, que es el que se rompio en
// 6.9 y el que mas se toca al anadir secciones al final del script. Se cuenta
// en el mensaje para que el rojo diga cuantos sobran y no solo que sobran.
const [NOMBRE_PRIMERA, HUECO_PRIMERO] = HUECOS[0];

// El recuento va ACOTADO. Un hueco roto puede ser enorme --si el `goto` de en
// medio desaparece, la cuenta se va hasta el principio del fichero-- y un rojo
// que escupe doscientas lineas no es un rojo: es ruido que esconde al que hay
// que leer. Se enseñan las primeras y el numero, que es lo que dice la verdad.
const CUANTAS_SE_VEN = 5;

function resumenDe(h) {
  if (h.length === 0)
    return 'no hay ninguna';

  const primeras = h.slice(0, CUANTAS_SE_VEN).map(([n, t]) => n + ': ' + t).join(' | ');
  const resto = h.length - CUANTAS_SE_VEN;

  return (resto > 0 ? primeras + ` | ... y ${resto} linea(s) mas` : primeras)
    + `  (${h.length} en total)`;
}

comprobar(`el hueco antes de ${NOMBRE_PRIMERA} es solo el salto: ${resumenDe(HUECO_PRIMERO)}`,
  HUECO_PRIMERO.length === 1 && HUECO_PRIMERO[0][1] === 'goto :end');

for (const [nombre, h] of HUECOS)
  if (h.length !== 1 || !/^(goto|exit\s*\/b)/i.test(h[0][1]))
    console.log(`          ${nombre}: ${resumenDe(h)}`);

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
console.log(LF + 'el build comprueba que el propio .bat esta bien, y es clase aparte');

// La cuarta clase. No es un PERF_FATAL mas y la asercion de por que es
// distinta de las otras esta en el motivo, aqui esta la forma.

// El acumulador existe y arranca a 0, y ANTES del bloque que lo marca: un
// `set ...=0` debajo BORRARIA el fallo, y el guard se quedaria verde para
// siempre. Es el fallo de BUILD_FATAL, que nacio en la linea 119.
const initLAYOUT = LINEAS.findIndex((l) => l.trim().toLowerCase() === 'set ' + Q + 'layout_fatal=0' + Q);

comprobar('build.bat tiene un acumulador propio para el layout, inicializado a 0',
  initLAYOUT > -1);
comprobar('y se inicializa ANTES del bloque del cronometro que lo marca',
  initLAYOUT > -1 && initLAYOUT < LINEAS.indexOf(BLOQUE_PERF[0]));

// La rama, buscada por su MENSAJE y no por el nombre de la variable: lo que
// se comprueba es que ESA rama marque el acumulador nuevo y no el viejo. Con el
// nombre seria tautologia.
const RAMA_LAYOUT = LINEAS.findIndex((l) => l.includes('This build script is broken inside'));

comprobar('la rama que detecta el .bat roto por dentro existe', RAMA_LAYOUT > -1);
comprobar('y marca LAYOUT_FATAL, no PERF_FATAL ni SELFTEST_FATAL',
  RAMA_LAYOUT > -1
  && LINEAS.slice(RAMA_LAYOUT, RAMA_LAYOUT + 14).some((l) => l.trim().toLowerCase() === 'set ' + Q + 'layout_fatal=1' + Q)
  && !LINEAS.slice(RAMA_LAYOUT, RAMA_LAYOUT + 14).some((l) => l.trim().toLowerCase().includes('perf_fatal')));

// El guard, y el codigo propio.
const colaLAYOUT = LINEAS.find((l) => l.trim() === 'if ' + Q + '!LAYOUT_FATAL!' + Q + '==' + Q + '1' + Q + ' (');

comprobar('la cola tiene su propio guard para el layout', colaLAYOUT !== undefined);
comprobar('y sale con 4, que no es el 1 de PERF_FATAL ni el 3 de SELFTEST_FATAL',
  colaLAYOUT !== undefined
  && LINEAS.slice(LINEAS.indexOf(colaLAYOUT), LINEAS.indexOf(colaLAYOUT) + 16)
    .some((l) => l.includes('exit /b 4')));
comprobar('el 4 aparece una sola vez: un codigo repetido en dos sitios son dos reglas',
  LINEAS.filter((l) => l.includes('exit /b 4')).length === 1);

// Y VA PRIMERO. El orden no es estetico: si el script esta roto, los dos tests
// que vienen despues estan contando sobre un script que no es el que se
// cree, asi que un guard que se ejecutara despues de ellos no protegeria
// nada. Se comprueba por posicion, no por comentario.
const nodoAuditor = LINEAS.findIndex((l) => l.includes('auditar-bats.mjs') && l.trim().startsWith('node'));
const nodoReparto = LINEAS.findIndex((l) => l.includes('test_build_bat_perf.mjs') && l.trim().startsWith('node'));

comprobar('el auditor se llama, y se llama ANTES que los dos tests del cronometro',
  nodoAuditor > -1 && nodoReparto > -1 && nodoAuditor < nodoReparto);

// Y el caso que hace que todo esto signifique algo: con el stub en rojo el
// build sale con 4, no con el 1. Si saliera con el 1, la cuarta clase seria un
// PERF_FATAL renombrado y este bloque entero no diria nada.
const CASO_LAYOUT = CASOS.find((c) => c.nlayout);

comprobar('el caso del .bat roto esta en la tabla y sale con 4, no con el 1 de PERF_FATAL',
  CASO_LAYOUT !== undefined && CASO_LAYOUT.build === 4);
comprobar('y no llega a medir: si el script que lee el resultado esta roto, no hay',
  CASO_LAYOUT !== undefined && CASO_LAYOUT.muere === true);


