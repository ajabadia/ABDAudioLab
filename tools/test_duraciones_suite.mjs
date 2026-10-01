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

import { leerDuraciones, resumen, UMBRAL } from './duraciones-suite.mjs';

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

console.log('\nel umbral por defecto sale de la medicion, no de un ojo');

comprobar('el umbral por defecto son 8 s', UMBRAL === 8);

console.log('\n' + '='.repeat(64));
console.log(fallos.length === 0
  ? `TODO EN VERDE: ${total} aserciones`
  : `ROJO: ${fallos.length} fallo(s) de ${total} aserciones`);

process.exit(fallos.length === 0 ? 0 : 1);