---
name: agentes-personalizados
description: >-
  Guía completa para crear, configurar e invocar Agentes Personalizados de Antigravity en ABDAudioLab.
  Activa esta skill cuando el usuario pida crear un agente personalizado (custom agent), definir un
  agente especializado con su propio system prompt y herramientas, o cuando necesite entender el
  formato de archivos .agent.md, sus campos de frontmatter, o cómo invocar agentes como subagentes.
---

# Agentes Personalizados de Antigravity

Los **Custom Agents** (Agentes Personalizados) permiten definir roles especializados con su propio
system prompt, modelo y conjunto de herramientas. A diferencia de las **Skills** (que enseñan
procedimientos), los agentes personalizados definen una *identidad completa*: el "quién" en lugar
del "qué".

---

## Diferencia clave: Agente vs. Skill

| Concepto | Archivo | Define |
|---|---|---|
| **Custom Agent** | `<nombre>.agent.md` | Persona, system prompt, modelo, tools |
| **Skill** | `SKILL.md` dentro de un directorio | Procedimientos paso a paso, runbooks |

Un agente personalizado **puede usar Skills** para extender sus capacidades sin inflar su prompt base.

---

## Ubicaciones de Almacenamiento

### Proyecto (compartido en VCS)
```
.agents/
└── agents/
    └── <nombre-del-agente>.agent.md
```

### Global (todas las máquinas del usuario)
```
~/.gemini/config/
└── agents/
    └── <nombre-del-agente>.agent.md
```

> **Regla de ABDAudioLab**: Los agentes de proyecto van en `.agents/agents/` y se incluyen en el
> repositorio Git. Los agentes de uso personal o que contienen datos sensibles van en el directorio
> global.

---

## Formato del Archivo `.agent.md`

Cada agente es un archivo Markdown con frontmatter YAML obligatorio + cuerpo de instrucciones.

### Estructura

```markdown
---
name: nombre-del-agente
description: >-
  Descripción usada para auto-discovery. Define cuándo se activa este agente.
model: gemini-3           # Opcional; usa el modelo por defecto si se omite
tools:                    # Opcional; lista de tools permitidas
  - read
  - grep
  - web-search
disable-model-invocation: false  # true = solo invocación manual
user-invocable: true             # false = solo acceso programático
---

# System Prompt del Agente

Aquí van las instrucciones completas del agente en Markdown libre.
```

### Campos del Frontmatter

| Campo | Tipo | Req. | Descripción |
|---|---|---|---|
| `name` | string | ✅ | Identificador único. Minúsculas con guiones. Debe coincidir con el nombre de archivo sin `.agent.md`. |
| `description` | string | ✅ | Descripción para auto-discovery. Antigravity la lee para decidir si activa el agente automáticamente. |
| `model` | string | ❌ | Modelo LLM específico (ej. `gemini-3`). Si se omite, usa el modelo activo por defecto. |
| `tools` | list | ❌ | Lista de herramientas que el agente puede usar. Si se omite, hereda las del agente coordinador. |
| `disable-model-invocation` | bool | ❌ | `true` = el agente no se activa por auto-discovery, solo por selección manual. Default: `false`. |
| `user-invocable` | bool | ❌ | `false` = el agente solo es accesible de forma programática (no aparece en la UI). Default: `true`. |

---

## Invocación de Agentes

### 1. Selección Manual (UI)
El usuario puede seleccionar un agente como "Main Agent" en la interfaz de Antigravity IDE.
Solo los agentes con `user-invocable: true` (o sin ese campo) aparecen en la lista.

### 2. Auto-discovery
Al iniciar una sesión, Antigravity escanea el campo `description` de todos los agentes disponibles.
Si la tarea encaja con la descripción, puede activar el agente automáticamente.
Desactiva este comportamiento con `disable-model-invocation: true`.

### 3. Delegación como Subagente
Un agente coordinador puede delegar trabajo en un agente personalizado como subagente.
El agente personalizado debe estar definido en `.agents/agents/` o en el config global.

---

## Protocolo de Creación en ABDAudioLab

Cuando el usuario pida crear un agente personalizado en este proyecto, seguir este protocolo:

1. **Determinar el ámbito**: ¿Proyecto (`.agents/agents/`) o global?
2. **Elegir el nombre**: Minúsculas con guiones, sin espacios. El nombre del archivo = `name` + `.agent.md`.
3. **Escribir el frontmatter** con `name`, `description`, y opcionalmente `model` y `tools`.
4. **Redactar el system prompt** en el cuerpo Markdown: rol, capacidades, restricciones.
5. **Verificar** que el archivo está en la ubicación correcta y que `name` coincide con el nombre de archivo.

### Ejemplo concreto para ABDAudioLab

```markdown
---
name: dsp-safety-auditor
description: >-
  Agente especializado en auditoría de seguridad DSP y thread-safety de C++ para ABDAudioLab.
  Actívalo cuando el usuario pida revisar código de audio en tiempo real, comprobar
  asignaciones de heap en el hilo de audio, o validar sincronización de atómicos.
model: gemini-3
tools:
  - read
  - grep
  - search_graph
disable-model-invocation: false
---

# DSP Safety Auditor — ABDAudioLab

Eres un auditor de seguridad especializado en DSP y C++ en tiempo real para ABDAudioLab.

## Rol y Competencias
- Detectar asignaciones de heap en el hilo de audio (violaciones de zero-allocation).
- Validar el uso correcto de `std::atomic` con `acquire`/`release`.
- Revisar sincronización entre hilos (TOCTOU, data races, condiciones de carrera).
- Verificar que la API de JUCE se usa correctamente en contexto RT.

## Reglas Absolutas
- Nunca proponer cambios fuera del alcance de seguridad DSP.
- Siempre referenciar la regla correspondiente en `.agents/rules/dsp_thread_safety.md`.
- Seguir el flujo atómico de `.agents/rules/atomic_mvp_workflow.md`.
```

---

## Referencias

- Documentación oficial: `https://antigravity.google/docs`
- Blog: `https://antigravity.google/blog/introducing-custom-agents`
- Guía de Skills de este proyecto: `.agents/skills/` (si existe)
- Reglas activas del proyecto: `.agents/rules/`
