# 🎨 Guía de Assets Compartidos (ABDShared)

> **Nota de Consolidación:** Los recursos compartidos anteriormente distribuidos entre `ABDSharedAssets` y `ABDSharedCode` han sido consolidados de forma definitiva en el monorepo transversal:
> 👉 **`ABDShared/Assets`** (Iconos, modelos 3D, marcas, fuentes, temas y estilos CSS)
> 👉 **`ABDShared/Contracts`** (Esquemas JSON, perfiles de hardware y matrices de modulación)
> 👉 **`ABDShared/Docs/`** (Documentación técnica y guías de integración)

---

## 1. Resolución de Rutas en ABDAudioLab

ABDAudioLab resuelve los recursos estáticos y contratos compartidos mediante la interfaz única en C++ [LabResourcePaths.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/core/LabResourcePaths.h):

- `abdaudiolab::core::sharedAssetsDir()`: Resuelve prioritariamente `ABDShared/Assets` (con fallback de retrocompatibilidad a `ABDSharedAssets`).
- `abdaudiolab::core::sharedContractsDir()`: Resuelve prioritariamente `ABDShared/Contracts` (con fallback a `ABDSharedAssets/contracts`).

## 2. Enlaces en Modo Desarrollo

El script de compilación local [build.bat](file:///d:/desarrollos/ABDSynths/ABDAudioLab/build.bat) crea automáticamente enlaces NTFS (junctions) hacia:
- `..\ABDShared\Assets\models` ➔ `assets\models`
- `..\ABDShared\Assets\brands` ➔ `assets\brands`

Para consultar la guía oficial de integración de toda la suite, diríjase a `ABDShared/Docs/integration/INTEGRATION_GUIDE.md`.
