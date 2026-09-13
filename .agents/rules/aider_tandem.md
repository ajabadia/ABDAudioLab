# Protocolo de Trabajo en Tándem: Antigravity (Arquitecto) + Aider (Ejecutor Local)

## 1. Filosofía y Roles
En este repositorio (`ABDAudioLab`), el desarrollo sigue un modelo de trabajo en tándem:
- **Antigravity (Lead Architect & Supervisor)**:
  - Analiza el Roadmap, el diseño de audio/DSP/JUCE y el grafo de código (usando codebase-memory-mcp).
  - Diseña la solución técnica y descompone las fases en tareas atómicas y precisas.
  - Redacta `PLAN.md` (especificación técnica exacta) y `TASK.txt` (prompt directo para Aider).
  - Configura `run-plan.bat` con los archivos a editar.
  - Verifica la calidad del código generado, ejecuta los tests unitarios (`ABDAudioLab_Tests.exe`) y actualiza `docs/ROADMAP.md`.
- **Aider (Worker / Coder Local con GPU)**:
  - Ejecuta las tareas de edición de código de forma desatendida en la máquina local (vía Ollama).
  - **Especialista en Creación de Tests Unitarios (Catch2)**: Escribe baterías de tests para nuevos módulos aprovechando el bucle `auto-test`.
  - Ejecuta automáticamente la compilación (`build.bat`) con `auto-test: true`.
  - Realiza los commits en git automáticamente (`auto-commits: true`).

## 2. Matriz de Pre-Evaluación: ¿Cuándo ejecuta Antigravity directamente y cuándo se delega en Aider?
Antes de decidir delegar una tarea a Aider o ejecutarla directamente, Antigravity realiza una evaluación coste/beneficio y de seguridad térmica del hardware:

| Criterio | Ejecución Directa (Antigravity) 🟢 | Delegación en Aider Local 🤖 |
| :--- | :--- | :--- |
| **Complejidad algorítmica** | Alta (DSP crítico, FFT, de-convolución, concurrencia, multithreading). | Moderada o baja (estructuras, adapters, modelos de datos, UI glue code). |
| **Redundancia de especificación** | Si escribir el `PLAN.md` ya requiere redactar el 80%-100% del código en C++, se aplica directamente. | Si una orden concisa en lenguaje natural basta para que el modelo local lo deduzca y edite. |
| **Tamaño de archivos objetivo** | Archivos > 250 líneas (para evitar saturar la VRAM/GPU local a 94°C o provocar thermal throttling). | Archivos pequeños (< 200 líneas) o creación de archivos completamente nuevos. |
| **Tipo de tarea** | Hotfixes, correcciones de errores de compilación complejos, parches matemáticos puntuales. | **Creación y ampliación de Tests Unitarios (Catch2)**, tareas mecánicas, repetitivas, boilerplate, serializadores, stubs. |

---

## 3. Protocolo de Señalización Explícita al Usuario
El usuario **NUNCA** debe tener que adivinar si le toca o no ejecutar Aider. Al final de cada turno o respuesta, Antigravity incluirá de forma obligatoria uno de estos dos carteles visuales destacados:

### Opción A: Cuando Antigravity ha resuelto la tarea directamente:
```markdown
🟢 **[EJECUCIÓN DIRECTA REALIZADA]**
> He aplicado los cambios directamente en el código.
> 👉 **NO necesitas ejecutar Aider.** Puedes compilar en tu terminal (`build.bat`) y darme el resultado, o probar la aplicación.
```

### Opción B: Cuando se ha preparado una tarea para Aider:
```markdown
🤖 **[ACCIÓN REQUERIDA: EJECUTAR AIDER LOCAL]**
> He preparado la especificación en `PLAN.md` y `TASK.txt`.
> 👉 **Abre tu terminal y ejecuta:** `.\run-plan.bat`
> *(Aider aplicará el diff, compilará con `build.bat` y hará el commit automáticamente).*
```

---

## 4. Política de Compilación: Control Preferente del Usuario
- **Por norma general, el usuario compila**: Salvo que el usuario indique explícitamente *"compila tú"* o *"hazlo tú"*, Antigravity no lanzará builds en segundo plano; aplicará los cambios de código y dejará que el usuario compile en su terminal con `build.bat` o CMake y proporcione el resultado.
- **Aider**: En las tareas delegadas a Aider (`.\run-plan.bat`), Aider sí compila automáticamente vía su `auto-test: true` para su ciclo cerrado de edición.

---

## 5. Reglas de Optimización y Seguridad de Hardware para Aider (Ollama 7B)
- **Formato Diff obligatorio**: Usar `edit-format: diff` en `.aider.conf.yml` para evitar regenerar archivos completos y reducir la carga térmica de la GPU.
- **Contexto quirúrgico**: Nunca inyectar archivos de especificaciones masivos (como `ESPECIFICACIONES_LABORATORIO.md` de 30k tokens). Solo los archivos a modificar y el `PLAN.md` conciso.
- **Automatización**: Mantener `yes-always: true` y `auto-test: true` con `test-cmd: "cmd /c build.bat"` para que el ciclo sea completamente autónomo.

---

## 6. Especialidad Destacada: Creación Autónoma de Tests Unitarios por Aider
La redacción de baterías de pruebas unitarias en Catch2 es el **caso de uso ideal (sweet spot)** para Aider y el modelo local:
- **Aislamiento y tamaño**: Cada test es un archivo nuevo o independiente (`src/tests/test_*.cpp`) de menos de 150 líneas. No hay riesgo de desbordar la VRAM ni calentar la GPU.
- **Bucle cerrado auto-reparador**: Aider redacta los `TEST_CASE` y `SECTION`. Si comete un fallo sintáctico o tipográfico en una aserción (`REQUIRE`), `build.bat` le entrega el error de MSVC, Aider lo corrige por sí mismo en la siguiente iteración y comitea cuando todo esté en verde.
- **Protocolo de delegación**:
  1. Antigravity diseña la cabecera del módulo (`MiModulo.h`) y registra el archivo de test en `CMakeLists.txt`.
  2. Antigravity especifica en `TASK.txt` los casos a verificar (nominal, límites, tolerancias).
  3. El usuario lanza `.\run-plan.bat` y Aider implementa, compila, prueba y comitea el test.

