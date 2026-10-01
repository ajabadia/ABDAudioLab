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

import { leerDuraciones, resumen, construirBase, compararConBase, resumenBase, leerBase, PISO_DE_INTERES, FACTOR, UMBRAL } from './duraciones-suite.mjs';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

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

console.log('\n' + '='.repeat(64));
console.log(fallos.length === 0
  ? `TODO EN VERDE: ${total} aserciones`
  : `ROJO: ${fallos.length} fallo(s) de ${total} aserciones`);

process.exit(fallos.length === 0 ? 0 : 1);