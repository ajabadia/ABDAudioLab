/**
 * TESTS DEL CRONOMETRADOR.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUE HAY UN XML HECHO A MANO Y NO UNA SUITE REAL
 *
 * La respuesta obvia seria medir la suite entera y mirar lo que sale. Eso
 * tarda cinco minutos por asercion, asi que el fichero de test seria de cinco
 * minutos: demasiado lento para Untersuche algo que deberia ser instantaneo, y
 * demasiado lento para correr en cada cambio.
 *
 * Asi que el XML se escribe aqui, pequeno y con las formas que importan. Cada
 * caso de este fichero contiene un fallo que semetería en un parser escrito a
 * mano y que aqui se comprueba que NO se cuela.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LAS FORMAS QUE IMPORTAN, Y POR QUE
 *
 *  - Duracion en notacion cientifica (`2.3e-05`). Catch2 la escribe asi cuando
 *    el tiempo es muy pequeno. Un parser que compare cadenas pondria este caso
 *    POR ENCIMA de uno de 9 s, que es justo al reves, y sin avisar.
 *  - El `OverallResult` del CASO, no el de una `Section`. Se confunden porque
 *    se parecen: los dos son `<OverallResult ...>` con duracion, y usar el
 *    equivocado da tiempos que no suman la suite.
 *  - Un `TestCase` sin `OverallResult` (el suite se corto a mitad). Tiene que
 *    ignorarse, no contarse con duracion cero: un cero falso hace que un test
 *    colgado parezca el mas rapido.
 *  - Nombres con `&amp;` y acentos. Vienen escapados de los XML y hay que
 *    poder compararlos con los que da Catch2 en consola.
 */

import { leerDuraciones, resumen, construirBase, compararConBase, resumenBase, leerBase, falloDeSpawn, xmlTruncado, cuadraLaCuenta, paraCatchDe, claveDeFichero, PISO_DE_INTERES, FACTOR, UMBRAL } from './duraciones-suite.mjs';
import { mkdtempSync, writeFileSync, readFileSync, readdirSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const XML = `<?xml version="1.0" encoding="UTF-8"?>
<Catch2TestRun name="ABDAudioLab_Tests.exe">
  <TestCase name="Corto" filename="D:\\a\\uno.cpp" line="1">
    <Section name="s1" filename="D:\\a\\uno.cpp" line="2">
      <OverallResults successes="3" failures="0" durationInSeconds="9.9"/>
    </Section>
    <OverallResult success="true" skips="0" durationInSeconds="0.05"/>
  </TestCase>
  <TestCase name="Medio y conTicks" filename="D:\\a\\dos.cpp" line="1">
    <Section name="s1" filename="D:\\a\\dos.cpp" line="2">
      <OverallResults successes="3" failures="0" durationInSeconds="9.9"/>
    </Section>
    <OverallResult success="true" skips="0" durationInSeconds="2.5e-05"/>
  </TestCase>
  <TestCase name="Lento y con &amp; acentos" filename="D:\\a\\tres.cpp" line="1">
    <OverallResult success="true" skips="0" durationInSeconds="12.5"/>
  </TestCase>
  <TestCase name="Nueve coma nueve" filename="D:\\a\\cinco.cpp" line="1">
    <OverallResult success="true" skips="0" durationInSeconds="9.9"/>
  </TestCase>
  <TestCase name="Diez coma cero cinco" filename="D:\\a\\seis.cpp" line="1">
    <OverallResult success="true" skips="0" durationInSeconds="10.05"/>
  </TestCase>
  <TestCase name="Sin OverallResult porque se corto" filename="D:\\a\\cuatro.cpp" line="1">
    <Section name="s1" filename="D:\\a\\cuatro.cpp" line="2">
      <OverallResults successes="1" failures="0" durationInSeconds="4.4"/>
    </Section>
  </TestCase>
</Catch2TestRun>`;

// Un fichero de referencia con la version de formato equivocada. Se escribe de
// verdad en un temporal, porque una base incompatible tiene que READINGSE de un
// fichero: si se probara con un objeto en memoria no se comprobaria que el
// `JSON.parse` de un fichero roto no revienta el analisis.
const dirTEMP = mkdtempSync(join(tmpdir(), 'dur-base-'));
const rutaIncompatible = join(dirTEMP, 'base.json');
writeFileSync(rutaIncompatible, JSON.stringify({ version: 99, casos_: { A: { s: 1, f: 'a' } } }), 'utf8');

const NO_EXISTE_O_INCOMPATIBLE = rutaIncompatible;

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

console.log('\nlas duraciones se leen del XML de Catch2');

const d = leerDuraciones(XML);

comprobar('se lee un caso por cada uno que tiene OverallResult',
  d.length === 5);

comprobar('un caso sin OverallResult NO se cuenta con duracion cero',
  !d.some((x) => x.nombre.includes('corto a mitad')));

comprobar('la duracion es la del CASO, no la de la Section (9.9)',
  d.find((x) => x.nombre === 'Corto')?.segundos === 0.05);

comprobar('la notacion cientifica se lee como numero',
  d.find((x) => x.nombre.includes('Medio'))?.segundos === 0.000025);

comprobar('2.5e-05 se ordena POR DEBAJO de 0.05, no por encima',
  d.find((x) => x.nombre.includes('Medio')).segundos
    < d.find((x) => x.nombre === 'Corto').segundos);

// ESTE CASO ES EL QUE JUSTIFICA ESTE FICHERO.
//
// Ordenar por CADENA en vez de por numero falla en cuanto hay un numero con
// decimales que cruza el 10: como cadena, "9.9" es MAYOR que "10.05" (el 9 va
// antes que el 1), asi que el test de 9.9 s se declararia mas rapido que el de
// 10.05 s. Sin un par asi en los datos, el resto del fichero pasaria aunque el
// orden estuviera roto, porque en los casos pequenos cadena y numero coinciden.
//
// Se intento antes con notacion cientifica, y no servia: "2.5e-05" frente a
// "0.05" se ordena igual por cadena que por numero, porque en ambas comparaciones
// el 2 va antes que el 0. El fallo existia y el test no lo veia — que es
// exactamente el fallo silencioso que este fichero dice evitar.
console.log('\nun orden por CADENA se detecta, porque no es lo mismo que numerico');

const ordenReal = d.slice().sort((a, b) => b.segundos - a.segundos);
const iNueve = ordenReal.findIndex((x) => x.nombre === 'Nueve coma nueve');
const iDiez = ordenReal.findIndex((x) => x.nombre === 'Diez coma cero cinco');

comprobar('10.05 s va antes que 9.9 s (por numero, no por cadena)',
  iDiez >= 0 && iNueve >= 0 && iDiez < iNueve);

comprobar('y el resumen los lista en ese orden',
  resumen(d, 8).join('\n').indexOf('Diez coma cero cinco')
    < resumen(d, 8).join('\n').indexOf('Nueve coma nueve'));

comprobar('se guarda el nombre aunque venga escapado',
  d.some((x) => x.nombre.includes('&amp;')));

comprobar('se guarda el fichero de origen',
  d.find((x) => x.nombre === 'Corto')?.fichero?.includes('uno.cpp'));

console.log('\nun nombre con > NO se parte por la mitad, que es donde se perdian 11 casos');

// Un nombre puede llevar `>` dentro —«(>= 3 controls)»—, y el reporter lo
// escribe como `&gt;`. Un parser que busca el cierre de la etiqueta con
// `[^>]*` corta por ahi, se queda con los atributos a medias, y el `name="` ya
// no cierra nunca: el caso se descarta o sale como «(sin nombre)». Once casos de
// 927, y no se notaba porque el nombre inventado se repetia once y parecia uno.
const CON_GT = `<?xml version="1.0" encoding="UTF-8"?>
<Catch2TestRun>
  <TestCase name="OperatorCards (>= 3 controls) and Selection" filename="D:\\a\\x.cpp" line="1" tags="[t]">
    <OverallResult success="true" skips="0" durationInSeconds="1.25"/>
  </TestCase>
</Catch2TestRun>`;

const conGt = leerDuraciones(CON_GT);

comprobar('se lee el caso cuyo nombre lleva &gt; escapado',
  conGt.length === 1);

comprobar('el nombre sale COMPLETO, con el >= dentro',
  conGt[0]?.nombre === 'OperatorCards (>= 3 controls) and Selection');

comprobar('y sale su duracion, no un cero',
  conGt[0]?.segundos === 1.25);

console.log('\nel umbral ordena y avisa de lo que lo pasa');

const lineas = resumen(d, 8).join('\n');

comprobar('el resumen cuenta los casos medidos', lineas.includes('5 casos'));
comprobar('el resumen suma el total', lineas.includes('32.5 s'));
comprobar('avisa del caso de 12.5 s', lineas.includes('Lento y con'));
comprobar('avisa de un caso de 10.05 s, que supera el umbral de 8',
  lineas.includes('Diez coma cero cinco'));
// 9.9 s SI supera el umbral de 8, asi que sale en la lista. Lo que se
// comprueba aqui es que salga DESPUES de 10.05, que es el punto del par.
comprobar('un caso de 9.9 s tambien avisa, pero detras del de 10.05',
  lineas.includes('Nueve coma nueve')
    && lineas.indexOf('Diez coma cero cinco') < lineas.indexOf('Nueve coma nueve'));
comprobar('NO avisa de un caso de 0.05 s', !lineas.includes('  0.1 s'));

console.log('\ny un XML vacio o truncado lo dice, en vez de dar un verde vacio');

const vacio = resumen(leerDuraciones(''), 8).join('\n');
comprobar('sin XML dice que no ha medido nada',
  vacio.includes('No se ha medido ningun test'));

const truncado = resumen(leerDuraciones('<Catch2TestRun><TestCase name="x">'),
  8).join('\n');
comprobar('con XML truncado dice que no se puede saber',
  truncado.includes('truncado'));

console.log('\nla referencia se guarda y se compara');

// La base se construye aqui, con tres casos, en vez de medir la suite entera:
// una medicion real tarda cinco minutos por asercion. Lo que se prueba es la
// COMPARACION, que no necesita que los tiempos sean reales sino que esten en la
// relacion correcta entre ellos.
const base = construirBase([
  { nombre: 'A', fichero: 'x.cpp', segundos: 0.3 },
  { nombre: 'B', fichero: 'y.cpp', segundos: 0.4 },
  { nombre: 'C', fichero: 'z.cpp', segundos: 0.5 },
]);

comprobar('la base guarda los casos que se le pasan',
  Object.keys(base.casos_).length === 3);

comprobar('la base lleva su version de formato, que es lo que se recusa si cambia',
  base.version === 1);

comprobar('la base guarda los filtros con los que se midio',
  Array.isArray(base.filtros));

const cmp = compararConBase([
  { nombre: 'A', fichero: 'x.cpp', segundos: 0.9 },
  { nombre: 'B renombrado', fichero: 'y.cpp', segundos: 1.2 },
  { nombre: 'NUEVO', fichero: 'n.cpp', segundos: 2.0 },
], base);

comprobar('un caso que se triplica sale como regresion', cmp.regresiones.length === 2);

comprobar('dice cuanto se ha multiplicado, no solo que ha cambiado',
  cmp.regresiones.every((r) => r.factor === 3));

comprobar('un RENOMBRADO se reconoce por su fichero, no se toma por caso nuevo',
  cmp.regresiones.some((r) => r.renombrado === true));

comprobar('y sale marcado como renombrado, para que no se lea como una regresion nueva',
  resumenBase(cmp).join('\n').includes('(renombrado)'));

comprobar('un caso que no estaba en la base cuenta como nuevo',
  cmp.nuevos.length === 1);

comprobar('un caso que ha desaparecido se cuenta como ausente',
  cmp.ausentes === 1);

console.log('\ny el ruido de los tests de microsegundos no se confunde con una regresion');

// El piso existe por esto: sin el, duplicar el tiempo de un test de 0.1 ms
// avisaria en cada vuelta, y un informe que avisa siempre deja de leerse.
const baseMicro = construirBase([
  { nombre: 'micro', fichero: 'm.cpp', segundos: PISO_DE_INTERES / 10 },
  { nombre: 'normal', fichero: 'n.cpp', segundos: 0.5 },
]);

const cmpMicro = compararConBase([
  { nombre: 'micro', fichero: 'm.cpp', segundos: PISO_DE_INTERES / 5 },
  { nombre: 'normal', fichero: 'n.cpp', segundos: 1.5 },
], baseMicro);

comprobar('el que estaba por debajo del piso NO se avisa, aunque se haya duplicado',
  !cmpMicro.regresiones.some((r) => r.nombre === 'micro'));

comprobar('el que estaba por encima del piso SI se avisa',
  cmpMicro.regresiones.some((r) => r.nombre === 'normal'));

console.log('\nuna referencia que no se puede leer no rompe el analisis');

comprobar('sin fichero de referencia, la comparacion no dice que hay regresiones',
  compararConBase([{ nombre: 'A', fichero: 'x.cpp', segundos: 9 }], null).regresiones.length === 0);

comprobar('sin referencia, el resumen sale vacio en vez de mentir',
  resumenBase(compararConBase([], null)).length === 0);

console.log('\nuna referencia de otra version de formato se recusa, no se compara');

comprobar('una base con version distinta no se usa',
  leerBase(NO_EXISTE_O_INCOMPATIBLE) === null);

console.log('\nel umbral por defecto sale de la medicion, no de un ojo');

comprobar('el umbral por defecto son 8 s', UMBRAL === 8);

comprobar('el factor por defecto es 2', FACTOR === 2);

// ─────────────────────────────────────────────────────────────────────────
// UNA MEDICION QUE NO HA TERMINADO NO ES UNA MEDICION
//
// El fallo que se cerro aqui no era de los lentos: era que un `spawnSync` que no
// acababa devolvia su `stdout` a secas, y ese stdout es un XML cortado por la
// mitad. Cada `<TestCase>` que le cabe esta entero, asi que el parser no tenia
// nada que decir: leia 187 casos de 927, informaba «740 de la referencia no
// estan», y salia con 0. Un pipeline leia un verde de una medicion que no habia
// medido el 80 % de los tests.

console.log('\nel resultado del spawn se mira, no solo su salida');

const LIMITE = Object.assign(new Error('spawnSync ETIMEDOUT'), { code: 'ETIMEDOUT' });
const DESBORDE = Object.assign(new Error('spawnSync ENOBUFS'), { code: 'ENOBUFS' });

comprobar('un cuelgue por agotar el limite de reloj se nombra como cuelgue',
  falloDeSpawn({ status: null, signal: 'SIGTERM', error: LIMITE }).includes('min'));

comprobar('un desborde del buffer se distingue del cuelgue: es escribir de mas',
  falloDeSpawn({ status: null, signal: null, error: DESBORDE }).includes('MB'));

comprobar('una muerte por senal se dice con cual ha sido',
  falloDeSpawn({ status: null, signal: 'SIGSEGV', error: null }).includes('SIGSEGV'));

comprobar('un error de arranque se enseña, no se traga',
  falloDeSpawn({ status: null, signal: null, error: { code: 'ENOENT', message: 'no such file' } }).includes('no such file'));

comprobar('un codigo de Windows que no es de Catch2 se da por muerte inesperada',
  falloDeSpawn({ status: 3221225477, signal: null, error: null }).includes('3221225477'));

// Y el caso que NO es un fallo del cronometro, que es el que mas costararia
// equivocar: Catch2 sale con el numero de casos fallidos, de modo que un 3 es
// una suite con tres tests rojos, que se cronometra igual de bien. Confundirlo
// con una muerte haria que cualquier rojo de la suite se informara como «esta
// medicion no vale», que es un aviso que teaches a no mirar.
comprobar('una suite con tests rojos NO es un fallo del cronometro',
  falloDeSpawn({ status: 3, signal: null, error: null }) === null);

comprobar('una suite terminada bien no da ningun fallo',
  falloDeSpawn({ status: 0, signal: null, error: null }) === null);

comprobar('un spawn que no dice ni status ni motivo se da por no terminado',
  falloDeSpawn({ status: null, signal: null, error: null }) !== null);

console.log('\nun documento sin su cierre esta truncado, y se nota');

comprobar('el XML entero NO esta truncado', xmlTruncado(XML) === false);
comprobar('un XML cortado por la mitad SI esta truncado', xmlTruncado(XML.slice(0, 200)));
comprobar('un XML vacio esta truncado', xmlTruncado(''));
// El cierre que se mira es el de verdad. Pedir `</Catch>` haria que NINGUN
// documento pareciera truncado —el fallo esta en el otro sentido, pero es el
// mismo— y el guard pasaria siempre sin comprobar nada.
comprobar('el cierre que se busca es el que emite Catch2, con la version dentro',
  xmlTruncado('</Catch>') && !xmlTruncado('</Catch2TestRun>'));

console.log('\nla cuenta de casos se comprueba contra la referencia, no se supone');

const medidos = leerDuraciones(XML);
const baseMedida = construirBase(medidos);

comprobar('una medicion que cubre la referencia cuadra',
  cuadraLaCuenta(medidos, baseMedida, 0).ok === true);

// El caso del fallo: la referencia es la vuelta COMPLETA y la medicion se quedo
// a medias. Medir menos de lo que hay no es un dato que comparar, es una
// medicion que no se puede comparar.
const referenciaMasLarga = construirBase([
  ...medidos,
  { nombre: 'Se quedó sin correr 1', fichero: 'z1.cpp', segundos: 9.0 },
  { nombre: 'Se quedó sin correr 2', fichero: 'z2.cpp', segundos: 9.1 },
  { nombre: 'Se quedó sin correr 3', fichero: 'z2.cpp', segundos: 9.2 },
  { nombre: 'Se quedó sin correr 4', fichero: 'z3.cpp', segundos: 9.3 },
]);

const falta = cuadraLaCuenta(medidos, referenciaMasLarga, 4);

comprobar('medir menos casos de los que tiene la referencia NO cuadra', falta.ok === false);
comprobar('y dice cuantos faltan', falta.lineas.join('\n').includes('Faltan 4'));
comprobar('y explica que una medicion incompleta no es una medicion lenta',
  falta.lineas.join('\n').includes('no se ha medido'));

// El caso que mas confunde, porque las dos cuentas cuadran: un caso nuevo tapa
// el hueco de uno que no se ha medido, el total da igual, y sin esto el guard
// diria que todo esta bien sobre una poblacion a la que le falta lo que tardaba.
const compensado = cuadraLaCuenta([
  ...medidos.slice(0, 3),
  { nombre: 'Tapa1', fichero: 't1.cpp', segundos: 0.2 },
  { nombre: 'Tapa2', fichero: 't2.cpp', segundos: 0.3 },
], baseMedida, 2);

comprobar('unos casos de mas NO compensan los que faltan', compensado.ok === false);
comprobar('y lo dice aunque el total cuadre',
  compensado.lineas.join('\n').includes('aunque el total cuadre'));

comprobar('con casos de mas y ninguno ausente, cuadra',
  cuadraLaCuenta([...medidos, { nombre: 'Nuevo', fichero: 'n.cpp', segundos: 0.4 }],
    baseMedida, 0).ok === true);

// Una referencia que no cuadra consigo misma no sirve para contar lo que falta:
// diria que estan todos aqui los que no estan en ninguna parte.
const incoherente = cuadraLaCuenta(medidos, { casos: 99, casos_: baseMedida.casos_ }, 0);

comprobar('una referencia que no cuadra consigo misma no pasa', incoherente.ok === false);
comprobar('y lo dice en vez de contar lo que falta',
  incoherente.lineas.join('\n').includes('99 casos y tiene 5'));

comprobar('sin referencia no hay cuenta que comprobar',
  cuadraLaCuenta(medidos, null, 0).ok === true);

// ─────────────────────────────────────────────────────────────────────────
// Y EL CODIGO DE SALIDA, QUE ES LO QUE LEE UN PIPELINE
//
// Todo lo anterior son funciones puras: se pueden probar sin lanzar nada. Pero
// el fallo que se cerro no estaba en las funciones, estaba en el codigo con el
// que salia el programa, y eso solo se comprueba ejecutando el programa. Un
// informe puede avisar y aun asi salir con 0, que es lo que hacia que un
// cuelgue se viera como un informe.

const rutaScript = join(dirname(fileURLToPath(import.meta.url)), 'duraciones-suite.mjs');

function correr(args) {
  const r = spawnSync(process.execPath, [rutaScript, ...args], { encoding: 'utf8' });

  return { codigo: r.status, salida: `${r.stdout ?? ''}${r.stderr ?? ''}` };
}

const rutaBaseMedida = join(dirTEMP, 'base-medida.json');
const rutaEntero = join(dirTEMP, 'entero.xml');
const rutaCorto = join(dirTEMP, 'corto.xml');
const rutaSinUno = join(dirTEMP, 'sin-uno.xml');
const rutaSinCasos = join(dirTEMP, 'sin-casos.xml');

writeFileSync(rutaBaseMedida, `${JSON.stringify(baseMedida, null, 2)}\n`, 'utf8');
writeFileSync(rutaEntero, XML, 'utf8');
writeFileSync(rutaCorto, XML.slice(0, Math.floor(XML.length / 2)), 'utf8');
writeFileSync(rutaSinUno, XML.replace(/\s*<TestCase name="Nueve coma nueve"[\s\S]*?<\/TestCase>/, ''), 'utf8');
writeFileSync(rutaSinCasos, '<?xml version="1.0"?>\n<Catch2TestRun name="x">\n</Catch2TestRun>\n', 'utf8');

// `--solo-avisar` en todas: sin el, un caso de 12.5 s sale con 1 por lento, y
// entonces el codigo no distingue una cosa de la otra. Lo que se comprueba aqui
// es que ese flag, que silencia los avisos de lentitud, NO silance un XML sin
// cerrar ni una cuenta que no cuadra.
const entero = correr(['--xml', rutaEntero, '--base', rutaBaseMedida, '--solo-avisar']);

comprobar('un XML entero y a la medida sale con 0', entero.codigo === 0);

const corto = correr(['--xml', rutaCorto, '--base', rutaBaseMedida, '--solo-avisar']);

comprobar('un XML TRUNCADO sale con 1 (antes salia con 0)', corto.codigo === 1);
comprobar('y lo dice como truncado, no como «faltan casos»',
  corto.salida.includes('EL XML ESTA TRUNCADO'));

const sinUno = correr(['--xml', rutaSinUno, '--base', rutaBaseMedida, '--solo-avisar']);

comprobar('un XML entero al que le falta un caso de la referencia sale con 1',
  sinUno.codigo === 1);
comprobar('y avisa de que la medicion no cubre la referencia',
  sinUno.salida.includes('no cubre la referencia entera'));

const sinCasos = correr(['--xml', rutaSinCasos, '--base', rutaBaseMedida, '--solo-avisar']);

comprobar('un XML entero con cero casos sale con 1, no con un verde vacio',
  sinCasos.codigo === 1);

// Y que el camino bueno siga siendo el bueno: sin base no hay nada que
// comprobar, y eso no es un fallo. La ruta apunta a un sitio que no existe a
// proposito: sin `--base` se usaria la referencia de verdad, que tiene 927
// casos, y un XML de 5 se saldria con 1 — no por estar mal, sino porque aqui no
// se puede medir la suite entera.
const sinBase = correr([
  '--xml', rutaEntero, '--solo-avisar', '--base', join(dirTEMP, 'no-existe.json')]);

comprobar('sin referencia que comparar sale con 0', sinBase.codigo === 0);

// Y el 2, que es el codigo que mas se confunde con el 1 porque no dice nada
// sobre tiempos: dice que NO SE HA MEDIDO. Fijarlo aqui es lo que impide que
// derive a un 1 sin que nadie lo note, que es justo como paso con el resto de
// este fichero: un codigo de salida que nadie mira se puede mover solo.
const sinFichero = correr(['--xml', join(dirTEMP, 'no-existe.xml')]);

comprobar('un XML que no existe sale con 2, no con 1', sinFichero.codigo === 2);
comprobar('y el mensaje dice que no existe, con la ruta entera',
  sinFichero.salida.includes(join(dirTEMP, 'no-existe.xml')));

console.log('\nel valor de una bandera NO llega a Catch2 como filtro de test');

// Un valor suelto es indistinguible de un filtro de test, y un filtro que no
// nombra a nadie hace que la suite mida CERO casos sin decir por que. Lo que
// pasaba con `--base otra.json` era eso: la ruta se colaba entre los argumentos,
// la suite no encontraba los casos, y el cronometro informaba de que no habia
// cronometrado nada. Un cronometro que no mide nada y uno que no encuentra nada
// se quedan igual de callados.
const conTodo = ['--umbral', '10', '--factor', '3', '--base', 'otra.json',
  '~[integration-01]', '~*PluginHost*'];

comprobar('el valor de --base NO se pasa a Catch2',
  !paraCatchDe(conTodo).includes('otra.json'));

comprobar('el valor de --umbral NO se pasa a Catch2',
  !paraCatchDe(conTodo).includes('10'));

comprobar('el valor de --factor NO se pasa a Catch2',
  !paraCatchDe(conTodo).includes('3'));

comprobar('los filtros de test SI se pasan, que para eso estan',
  paraCatchDe(conTodo).join(' ').includes('~[integration-01]'));

comprobar('ninguna bandera llega a Catch2',
  paraCatchDe(conTodo).every((a) => !a.startsWith('--')));

// Una bandera que no viene no rompe nada, que es la mitad de por que la lista
// puede ser una lista y no una cuenta de posiciones fijas.
comprobar('sin banderas con valor, los filtros pasan tal cual',
  paraCatchDe(['~[integration-01]', 'UnTest']).join(' ')
    === '~[integration-01] UnTest');

comprobar('una bandera sin valor detras no se come el filtro que sigue',
  paraCatchDe(['--solo-avisar', 'UnTest']).includes('UnTest'));

console.log('\nel renombrado se reconoce en otra maquina, y sale UNA vez');

// La referencia guarda rutas absolutas, que llevan el disco, el proyecto y el
// usuario. En otra maquina no coinciden, y con comparar por ruta entera el
// renombrado —que se detecta por el fichero— dejaba de detectarse: el mismo
// test salia como nuevo Y como ausente. Dos avisos para un test, y el segundo
// era mentira.
const RUTA_A = 'D:/trabajo/ABDSynths/ABDAudioLab/src/tests/test_Y.cpp';
const RUTA_B = '/home/b/ABDSynths/ABDAudioLab/src/tests/test_Y.cpp';

const baseRenombrado = construirBase([
  { nombre: 'lento de y', fichero: RUTA_A, segundos: 3 },
  { nombre: 'corto de y', fichero: RUTA_A, segundos: 0.2 },
]);

const medidoRenombrado = compararConBase([
  { nombre: 'lento de y RENOMBRADO', fichero: RUTA_B, segundos: 3.1 },
  { nombre: 'corto de y', fichero: RUTA_B, segundos: 0.2 },
], baseRenombrado);

comprobar('el renombrado no sale como test nuevo', medidoRenombrado.nuevos.length === 0);

comprobar('ni como test desaparecido', medidoRenombrado.ausentes === 0);

// Y el efecto util de emparejarlo: si ademas se ha puesto lento, se ve. Con el
// emparejamiento roto el tiempo anterior era el de otro test o no habia ninguno.
comprobar('un renombrado que se ha puesto lento se ve como regresion',
  compararConBase([
    { nombre: 'lento de y RENOMBRADO', fichero: RUTA_B, segundos: 9.0 },
    { nombre: 'corto de y', fichero: RUTA_B, segundos: 0.2 },
  ], baseRenombrado).regresiones.some((r) => r.nombre === 'lento de y RENOMBRADO'));

comprobar('y sale marcado como renombrado',
  compararConBase([
    { nombre: 'lento de y RENOMBRADO', fichero: RUTA_B, segundos: 9.0 },
    { nombre: 'corto de y', fichero: RUTA_B, segundos: 0.2 },
  ], baseRenombrado).regresiones.every((r) => r.renombrado === true));

// Un test de verdad nuevo en un fichero que ya tiene muchos tests NO se
// empareja con ninguno: los demas ya estan vistos, y no queda nadie libre.
const baseLlena = construirBase([
  { nombre: 'A', fichero: RUTA_A, segundos: 1 },
  { nombre: 'B', fichero: RUTA_A, segundos: 2 },
]);

comprobar('un test de verdad nuevo en un fichero ya visto sigue siendo nuevo',
  compararConBase([
    { nombre: 'A', fichero: RUTA_B, segundos: 1 },
    { nombre: 'B', fichero: RUTA_B, segundos: 2 },
    { nombre: 'C NUEVO', fichero: RUTA_B, segundos: 0.5 },
  ], baseLlena).nuevos.length === 1);

// Y dos renombrados en el MISMO fichero: no hay forma de saber cual es cual, y se
// empareja por el tiempo anterior mas parecido. Un renombrado no cambia cuanto
// tarda el test, y esa es la unica pista que queda.
const baseDos = construirBase([
  { nombre: 'lento', fichero: RUTA_A, segundos: 9 },
  { nombre: 'corto', fichero: RUTA_A, segundos: 0.2 },
]);

const dosRenombrados = compararConBase([
  { nombre: 'corto NUEVO NOMBRE', fichero: RUTA_B, segundos: 0.25 },
  { nombre: 'lento NUEVO NOMBRE', fichero: RUTA_B, segundos: 9.5 },
], baseDos);

comprobar('dos renombrados en el mismo fichero no dejan ninguno ausente',
  dosRenombrados.ausentes === 0);
comprobar('ni se toman por tests nuevos', dosRenombrados.nuevos.length === 0);
comprobar('y cada uno se compara con SU tiempo, no con el del otro',
  dosRenombrados.regresiones.length === 0
    || dosRenombrados.regresiones.every((r) => r.factor < 2));

comprobar('la clave de un fichero no depende de la maquina',
  claveDeFichero(RUTA_A) === claveDeFichero(RUTA_B));
comprobar('y se queda con el nombre, no con la carpeta',
  claveDeFichero('D:/x/y/z/test_Q.cpp') === 'test_Q.cpp');
comprobar('las barras invertidas tambien son separadores',
  claveDeFichero('D:\\x\\test_Q.cpp') === claveDeFichero('D:/x/test_Q.cpp'));
comprobar('un caso sin fichero no rompe la comparacion',
  claveDeFichero(undefined) === '(sin fichero)');

// ─────────────────────────────────────────────────────────────────────────
// LA REFERENCIA SE ESCRIBE ENTERA, O NO SE ESCRIBE
//
// `writeFileSync` trunca el destino antes de escribir. Si el proceso muere a
// mitad —un corte, un antivirus, dos cronometros a la vez— lo que queda es un
// JSON truncado, y `leerBase` no puede leerlo. Peor: no duele, porque `leerBase`
// avisa y sigue como si no hubiera referencia. Lo que se pierde no es la
// referencia nueva, que se regenera, sino la VIEJA, que era la unica.
//
// Se comprueba por fuera porque por dentro no se ve: se guarda una referencia
// encima de otra que ya existe y se mira si la nueva ha llegado entera y si la
// carpeta se ha quedado sin temporales. Que la sustitucion funcione en Windows es
// justo lo que hay que mirar: `renameSync` usa `MOVEFILE_REPLACE_EXISTING`, y si
// no lo hiciera, el renombrado fallaria con el destino ya ahi.

console.log('\nla referencia se escribe sin dejar ni un temporal detras');

const rutaBaseViva = join(dirTEMP, 'viva.json');
writeFileSync(rutaBaseViva, '{ esto no es una referencia', 'utf8');

const guardada = correr([
  '--xml', rutaEntero, '--solo-avisar', '--guardar-referencia', '--base', rutaBaseViva]);

comprobar('guardar la referencia sale con 0', guardada.codigo === 0);

let baseEscrita = null;

try {
  baseEscrita = JSON.parse(readFileSync(rutaBaseViva, 'utf8'));
}
catch (e) {
  // Se deja en `null` y el fallo se ve en la comprobacion de abajo, que es mas
  // util que un error aqui a mitad del fichero de test.
}

comprobar('la referencia escrita se puede leer entera', baseEscrita !== null);
comprobar('y tiene los casos de la medicion', baseEscrita?.casos === 5);

// El renombrado por encima de un fichero que ya existe es el caso que
// distingue un temporal bien puesto de uno que solo funciona en vacio.
comprobar('la escritura SUSTITUYE una referencia previa, no se niega a hacerlo',
  typeof baseEscrita?.medidoEn === 'string');

comprobar('no queda ningun temporal en la carpeta',
  readdirSync(dirTEMP).every((f) => !f.includes('.tmp')));

console.log('\nlos mensajes dicen DONDE se ha mirado, no solo QUE');

// Un `basename` en un mensaje de error es un mensaje inutil: «no esta la suite
// compilada en: ABDAudioLab_Tests.exe» no dice donde se ha buscado, y hay dos
// `build/Release` en juego. Con `--base` pasa lo mismo y con mas motivo, porque
// esa ruta la elige quien llama y puede estar en cualquier parte.
comprobar('el mensaje de referencia guardada lleva la ruta entera',
  guardada.salida.includes(rutaBaseViva));

comprobar('y no solo el nombre del fichero',
  guardada.salida.includes(join(dirTEMP, 'viva.json')));

console.log('\n' + '='.repeat(64));
console.log(fallos.length === 0
  ? `TODO EN VERDE: ${total} aserciones`
  : `ROJO: ${fallos.length} fallo(s) de ${total} aserciones`);

process.exit(fallos.length === 0 ? 0 : 1);