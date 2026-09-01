# 📦 ABDSharedAssets — Recursos y Contratos Compartidos

Repositorio centralizado de **recursos gráficos (modelos y marcas)** y **contratos de hardware JSON** para todo el ecosistema de software y plugins de **ABDSynths** (`ABDAudioLab`, `ABDBankManager`, `ABDMS2000`, `ABDMonoLead`, sintetizadores VST/AU, etc.).

---

## 🎯 Filosofía de Diseño: Fuente Única de la Verdad (*Zero-Copy*)

Para evitar duplicación de archivos, desincronizaciones accidentales o sobreescritura de versiones (*forks*), los proyectos satélite **nunca copian** los archivos. En su lugar, se vinculan mediante **Directory Junctions NTFS (`mklink /J`)**.

- Cualquier cambio o incorporación realizada en este directorio maestro se refleja **de forma instantánea** en todos los proyectos dependientes.
- Los *Junctions* de Windows **no requieren privilegios de administrador** y son totalmente transparentes para los compiladores (MSVC, Clang, GCC), motores Web (Tauri, WebUI, WebView2) y DAWs.

---

## 📂 Estructura de Directorios

```text
D:\desarrollos\ABDSynths\ABDSharedAssets\
├── brands/       <- Logotipos vectoriales SVG de fabricantes (Roland, Korg, Behringer, Moog, Yamaha, etc.)
├── models/       <- Renders e imágenes (WebP / PNG / SVG) de sintetizadores, módulos y bancos
└── contracts/    <- Contratos JSON de especificación de hardware y automatización
```

---

## 🔗 Mapa de Enlaces por Proyecto

| Proyecto | Carpeta Local Vinculada | Destino en `ABDSharedAssets` | Tipo de Enlace |
| :--- | :--- | :--- | :--- |
| **ABDAudioLab** | `ABDAudioLab\contracts\hardware` | `..\ABDSharedAssets\contracts` | Junction NTFS (`mklink /J`) |
| **ABDAudioLab** | `ABDAudioLab\assets\models` | `..\ABDSharedAssets\models` | Junction NTFS (`mklink /J`) |
| **ABDAudioLab** | `ABDAudioLab\assets\brands` | `..\ABDSharedAssets\brands` | Junction NTFS (`mklink /J`) |
| **ABDBankManager** | `ABDBankManager\WebUI\vendor\images\models\thumbs` | `..\..\..\..\ABDSharedAssets\models` | Junction NTFS (`mklink /J`) |
| **ABDBankManager** | `ABDBankManager\WebUI\vendor\images\models\logos` | `..\..\..\..\ABDSharedAssets\brands` | Junction NTFS (`mklink /J`) |
| **ABDMS2000 / Otros** | *(Según estructura del proyecto)* | `..\ABDSharedAssets\...` | Junction NTFS (`mklink /J`) |

---

## 🛠️ Cómo Integrar un Nuevo Proyecto (ej. `ABDMS2000`)

Para conectar un nuevo proyecto al ecosistema compartido:

### Opción A: Añadir al `build.bat` del proyecto (Recomendado)
Añade el siguiente bloque antes de compilar o empaquetar en tu `build.bat`:

```bat
:: ============================================================================
:: Enlace a Assets Compartidos via NTFS Junctions (Cero Copias)
:: ============================================================================
set "SHARED_ASSETS=..\ABDSharedAssets"
if exist "%SHARED_ASSETS%" (
    if not exist "assets\models" (
        if not exist "assets" mkdir "assets"
        mklink /J "assets\models" "%SHARED_ASSETS%\models" >nul 2>nul
    )
    if not exist "assets\brands" (
        if not exist "assets" mkdir "assets"
        mklink /J "assets\brands" "%SHARED_ASSETS%\brands" >nul 2>nul
    )
    if not exist "contracts" (
        mklink /J "contracts" "%SHARED_ASSETS%\contracts" >nul 2>nul
    )
)
```

### Opción B: Comando Manual vía PowerShell / CMD
Desde la raíz del proyecto destino:
```cmd
mklink /J "ruta\local\models" "..\ABDSharedAssets\models"
mklink /J "ruta\local\brands" "..\ABDSharedAssets\brands"
```

---

## 📄 Esquema Estándar de Contrato JSON (`contracts/*.json`)

Todos los contratos de hardware deben incluir los metadatos de visualización para que la UI pueda cargar el logotipo y el render correspondiente:

```json
{
  "id": "roland_aira_bitrazer",
  "displayName": "Roland AIRA Bitrazer Modular (SysEx)",
  "description": "USB MIDI SysEx & Loopback Audio Return (24-bit 96 kHz)",
  "category": "AUTOMATED_SYSEX",
  "brand": "Roland",
  "brandLogo": "brands/roland-logo.svg",
  "modelImage": "models/roland-bitrazer.png",
  "midiIdentification": {
    "manufacturer": "Roland",
    "model": "Bitrazer",
    "modelIdHex": "00 00 00 24",
    "autoDetectSysEx": "F0 7E 10 06 01 F7"
  },
  "parameters": [
    { "index": 1, "name": "SampleRate", "type": "Normalized", "default": 0.5 },
    { "index": 2, "name": "BitDepth", "type": "Normalized", "default": 0.5 },
    { "index": 3, "name": "FilterCutoff", "type": "Normalized", "default": 0.5 },
    { "index": 4, "name": "Resonance", "type": "Normalized", "default": 0.0 }
  ]
}
```

---

## 🎨 Convenciones de Nomenclatura de Archivos

- **Marcas / Logos (`brands/`)**: `[fabricante]-logo.svg` en minúsculas y kebab-case (ej. `roland-logo.svg`, `korg-logo.svg`).
- **Modelos / Renders (`models/`)**: `[fabricante]-[modelo].[webp|png]` (ej. `korg-ms2000.webp`, `roland-juno-106.webp`).
- **Contratos (`contracts/`)**: `[fabricante]_[familia]_[modelo].json` en snake_case (ej. `roland_aira_bitrazer.json`, `generic_midi_synth.json`).
