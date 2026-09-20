# PLAN HITO-04B: Tarea A + B — Harness Temporal y Test ST-86 (Approved Export)

## Objetivo
Crear en `src/tests/test_ExportIO.cpp` el harness de filesystem temporal RAII y el primer test de exportación real `ST-86` (Approved ProductionPackage).

## Alcance Permitido
- `src/tests/test_ExportIO.cpp` exclusivamente.
- Prohibido modificar archivos de producción, contratos, DSP o CMakeLists.txt.

## Componentes a Implementar en `src/tests/test_ExportIO.cpp`
1. **Harness RAII `TestTempDirectory`**:
   - Crea un subdirectorio único dentro de `std::filesystem::temp_directory_path() / "abdaudiolab_export_tests"`.
   - Limpia en constructor cualquier residuo previo de esa carpeta.
   - En destructor, elimina recursivamente el directorio temporal (`std::filesystem::remove_all`).
   - Expone `std::filesystem::path path`.

2. **Test ST-86 (`[export][io][ST-86]`)**:
   - Instancia `TestTempDirectory tempDir;`.
   - Prepara una solicitud `ReportExportRequest` con:
     - `manifest` con datos válidos (`hardwareDisplayName`, `activeFunctionName`).
     - Al menos 3 `MeasuredPoint` válidos (SNR > 60 dB, THD < 0.1%).
     - `options.includeProductionPackage = true`.
     - `destinationDirectory = tempDir.path`.
     - `baseFileName = "ApprovedModel_Prod"`.
   - Ejecuta `ReportExportResult res = ReportExportService::exportReport(req);`.
   - Aserciones obligatorias:
     - `REQUIRE(res.succeeded());`
     - `REQUIRE(res.status == ReportExportStatus::Success);`
     - Los artefactos existen en disco (`dest / "ApprovedModel_Prod_lut.h"`, `dest / "ApprovedModel_Prod_manifest.json"`, etc.).
     - Todos los `res.artifacts` existen, tienen `byteSize > 0` y `sha256.length() == 64`.
     - No existen carpetas residuales con prefijo `.staging_` o `.backup_`.
