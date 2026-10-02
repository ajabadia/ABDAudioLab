/**
 * AUDITOR DE LOS .bat DEL REPOSITORIO: LA FAMILIA DE LA CAIDA DENTRO DE
 * SUBRUTINAS
 *
 * ---------------------------------------------------------------------------
 * QUE BUSCA, Y POR QUE NO ES LO MISMO QUE LA REGLA DE 6.17
 *
 * La regla de 6.17 vive en `test_build_bat_perf.mjs` y mira UN fichero: el
 * build.bat. Aqui lo que se busca es la misma clase de defecto en CUALQUIER
 * .bat del arbol, y con dos reglas que el banco del build no necesita porque en
 * el build no se dan.
 *
 * Batch no distingue llamar de continuar. Consecuencias, y cada una es un
 * fallo de layout --no de logica--, es decir un fallo que sale de donde esta la
 * linea y no de lo que dice:
 *
 *   CAIDA        el flujo llega a una etiqueta sin un salto delante y ejecuta el
 *                cuerpo de la subrutina con los parametros del que la llamo.
 *                En el build.bat eso imprimia `mklink /J "" ""` y se llevaba la
 *                cola de :end entera; es el fallo de 6.9.
 *   HUECO        detras del salto que cierra un flujo no puede quedar ninguna
 *                linea ejecutable. La regla de 6.17.
 *   ETIQUETA     una etiqueta DENTRO de un bloque `if (...)`. Un `goto` a una
 *   EN BLOQUE    etiqueta de ese tipo se ejecuta con el bloque abierto, y el
 *                parser arrastra el parentesis que falta. En run-plan.bat las dos
 *                etiquetas estan dentro del `if errorlevel 1 (` del paso 2.
 *   :EOF         un `goto :eof` en el FLUJO PRINCIPAL no devuelve de nada:
 *   SUELTO       termina el script entero. En una subrutina es lo correcto; en
 *                el cuerpo del build es una forma de perder la cola de :end sin
 *                que se note.
 *   CALL         un `call :x` sin `:x` no avisa: cmd se lo come y sigue, y el
 *   INEXISTENTE  `call` devuelve 0 como si la subrutina hubiera hecho bien su
 *                trabajo.
 *
 * ---------------------------------------------------------------------------
 * POR QUE UN FICHERO Y NO UNO POR CADA .bat
 *
 * Los .bat del arbol se reparten el trabajo de un fichero que hay que volver a
 * leer: el analisis de la profundidad de parentesis y de los saltos tiene que
 * ser el mismo para todos, o dos medidas que no hablan el mismo idioma dan
 * rojos que no significan nada. Este script recorre lo que haya, asi que
 * anadir un .bat nuevo lo audita sin tocar nada.
 */

import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const NL = String.fromCharCode(10);

// Los .bat que hay en el arbol versionado. Se pregunta a git y no al disco: el
// arbol de trabajo tiene cuatrocientos ficheros de CMake en build/ y ningun
// .bat, y un `readdir` recursivo que se come build/ tarda mas que el analisis.
//
// Con una ruta como argumento se audita ESE fichero en vez de todos. No es un
// atajo para saltarse el arbol: es lo unico que hace falta para comprobar que
// las reglas muerden, y mutar sin tocar el fichero de verdad es la unica forma
// de que un auditor no pueda dejar el arbol en un estado que nadie quiere.
const RUTAS = process.argv.slice(2).length
  ? process.argv.slice(2)
  : execFileSync('git', ['ls-files', '*.bat', '*.cmd'], {
    encoding: 'utf8',
    maxBuffer: 8 * 1024 * 1024,
  }).split(NL).map((l) => l.trim()).filter(Boolean);

if (RUTAS.length === 0) {
  console.log('No hay .bat ni .cmd versionados.');
  process.exit(0);
}

const hallazgos = [];

function anotar(ruta, regla, donde, que) {
  hallazgos.push({ ruta, regla, donde, que });
}

// ── La depth de parentesis, con la convencion de `bloqueDesde` ──
//
// `rem` y `echo` no cuentan: cmd no los ejecuta, asi que un parentesi de un
// texto no abre un bloque. Es la misma convencion que usa el banco del build,
// y tiene que ser la misma: con otra, las dos medidas no comparan.
function analizar(lineas) {
  const profundidad = [];

  let nivel = 0;

  for (const l of lineas) {
    if (/^\s*(rem\s|echo )/i.test(l)) {
      profundidad.push(nivel);
      continue;
    }

    nivel += (l.match(/\(/g) || []).length;
    nivel -= (l.match(/\)/g) || []).length;
    profundidad.push(nivel);
  }

  return profundidad;
}

const EJECUTABLE = (l) => {
  const t = l.trim();
  return t !== '' && !t.startsWith('::') && !/^rem\b/i.test(t);
};

const ES_ETIQUETA = (l) => /^:[A-Za-z]/.test(l.trim());
const ES_SALTO = (l) => /^(goto|exit\s*\/b)/i.test(l.trim());

for (const ruta of RUTAS) {
  const bruto = readFileSync(resolve(dirname(fileURLToPath(import.meta.url)), '..', ruta), 'utf8');
  const lineas = bruto.replace(/\r\n/g, NL).split(NL).map((l) => l.replace(/\r$/, ''));
  const prof = analizar(lineas);

  const etiquetas = [];

  lineas.forEach((l, i) => {
    if (ES_ETIQUETA(l))
      etiquetas.push(i);
  });

  // QUE ETIQUETAS SON SUBRUTINAS, Y CUALES NO. Y ESTA DISTINCION CORRIGE UN
  // FALSO POSITIVO QUE ESTA MISMA REGLA DARIA SIN ELLA.
  //
  // El hueco --la regla de 6.17-- solo tiene sentido para una etiqueta que se
  // llama con `call`, porque solo para esa hay algo de donde caer dentro. Una
  // etiqueta a la que solo se llega con `goto` es un punto de encuentro, y ahi
  // CAER ESTA BIEN: es lo que hace un `if (...) else (...)` cuando despues sigue
  // el codigo comun de las dos ramas.
  //
  // Medido en run-plan.bat: `:OLLAMA_UP` va justo despues del `)` que cierra el
  // `if errorlevel 1 (...) else (...)` del paso 2, y llegar ahi cayendo es
  // exactamente lo que tiene que pasar en las DOS ramas. Con la regla sin esta
  // distincion salen dos rojos que no significan nada, y un rojo que no
  // significa nada enseña a ignorar los rojos.
  const llamadas = new Set();

  lineas.forEach((l) => {
    const m = l.trim().match(/^call\s+:([A-Za-z]\w*)/i);

    if (m)
      llamadas.add(':' + m[1].toLowerCase());
  });

  // El dos puntos va en LOS DOS lados. Medido: sin el, la comparacion no casa
  // nunca, `esSubrutina` devuelve false para todo el fichero, la regla del hueco
  // no se aplica a NINGUNA etiqueta y el auditor sale en verde con una
  // subrutina que ha perdido su `goto :eof`. Un auditor que se queda mudo no
  // avisa de que se ha quedado mudo.
  const esSubrutina = (i) => llamadas.has(':' + lineas[i].trim().slice(1).toLowerCase());

  // 1. CAIDA y 2. HUECO. Solo para las subrutinas: ahi el hueco tiene que ser
  //    el salto y nada mas. Cero ejecutables es caida, y uno que no sea el salto
  //    tambien.
  for (const i of etiquetas) {
    if (!esSubrutina(i))
      continue;

    const hueco = [];

    for (let k = i - 1; k >= 0; k -= 1) {
      if (ES_ETIQUETA(lineas[k]))
        break;

      if (!EJECUTABLE(lineas[k]))
        continue;

      hueco.push(k);

      if (prof[k] === 0 && ES_SALTO(lineas[k]))
        break;
    }

    const nombre = lineas[i].trim();

    if (hueco.length === 0) {
      anotar(ruta, 'CAIDA', i + 1, nombre + ': nada salta antes, el flujo se cae dentro');
      continue;
    }

    if (hueco.length !== 1 || prof[hueco[0]] !== 0 || !ES_SALTO(lineas[hueco[0]])) {
      // El hueco se recorre hacia atras, asi que el primero de la lista es el
      // mas CERCA de la etiqueta, no el ultimo. Decir "el ultimo" cuando es el
      // mas cercano hace que el rojo senale una linea que no es la que hay que
      // mirar, y ahi es donde un rojo deja de serve.
      anotar(ruta, 'HUECO', i + 1, nombre + ': el hueco tiene ' + hueco.length
        + ' ejecutable(s) y el mas cercano a la etiqueta es '
        + JSON.stringify(lineas[hueco[0]].trim())
        + '; el salto se perdio o no protege');
    }
  }

  // 3. ETIQUETA EN BLOQUE. Una etiqueta dentro de un `if (...)` se alcanza con el
  //    bloque abierto, y el `goto` que salta a ella arrastra el parentesis que
  //    falta. Da igual que se llame con `call` o con `goto`: el problema es el
  //    sitio donde esta, no como se llega.
  for (const i of etiquetas) {
    if (prof[i] > 0)
      anotar(ruta, 'ETIQUETA-EN-BLOQUE', i + 1,
        lineas[i].trim() + ': esta a ' + prof[i] + ' parentesis de profundidad');
  }

  // 4. :EOF SUELTO. `goto :eof` en el flujo principal termina el script: no hay
  //    nada a lo que volver. Solo es correcto dentro de una subrutina, y solo si
  //    la subrutina se llama con `call`.
  for (let i = 0; i < lineas.length; i += 1) {
    if (!/^goto\s+:eof\s*$/i.test(lineas[i].trim()))
      continue;

    const dentroDeSubrutina = etiquetas.some((e) => e < i && prof[e] === 0
      && !lineas.slice(e + 1, i).some((l, k) => ES_ETIQUETA(l) && prof[e + 1 + k] === 0));

    if (!dentroDeSubrutina)
      anotar(ruta, 'EOF-SUELTO', i + 1, 'goto :eof en el flujo principal: termina el script');
  }

  // 5. CALL INEXISTENTE. Ni cmd ni el script avisan: el `call` se come el fallo
  //    y devuelve 0.
  lineas.forEach((l, i) => {
    const m = l.trim().match(/^call\s+:([A-Za-z]\w*)/i);

    if (m && !lineas.some((x) => x.trim() === ':' + m[1]))
      anotar(ruta, 'CALL-INEXISTENTE', i + 1, 'call :' + m[1] + ' y no hay etiqueta con ese nombre');
  });
}

console.log('auditados ' + RUTAS.length + ' fichero(s): ' + RUTAS.join(', '));
console.log('');

if (hallazgos.length === 0) {
  console.log('TODO EN VERDE: ningun .bat tiene la familia de la caida dentro de subrutinas.');
  process.exit(0);
}

for (const h of hallazgos)
  console.log('  ROJO  ' + h.ruta + ':' + h.donde + '  [' + h.regla + '] ' + h.que);

console.log('');
console.log('ROJO: ' + hallazgos.length + ' hallazgo(s)');
process.exit(1);