## Nota técnica: `test_ContractsSnapshotDrift` fracture local vs origen compartido

- **Fecha de observación:** ejecución de la suite completa contra `build/Release/ABDAudioLab_Tests.exe` compilado el 2026-10-11 16:39:27.
- **Caso fallido:** `test_ContractsSnapshotDrift`
- **Aserciones fallidas:** 3
  - `test_ContractsSnapshotDrift.cpp(258)`
  - `test_ContractsSnapshotDrift.cpp(264)`
  - `test_ContractsSnapshotDrift.cpp(270)`
- **Causa registrada en la propia salida de la suite:**
  - el snapshot local `<repo_root>/contracts/hardware`
  - no coincide con el origen compartido `<shared_assets>/contracts`
  - la salida también reporta `falta en la copia: korg_ms2000_parameter_spec.json`

## Qué significa y qué no significa

- **Qué significa:** el directorio local de contratos necesita reposición/copias desde el origen compartido, y luego ajuste del inventario de contratos.
- **Qué no significa:** no es un fallo de lógica interna de la aplicación, ni un fallo introducido por MEJ-02/MEJ-03.

## Acción pendiente separada

- Este tema se mantiene como nota de seguimiento y no como parte del MEJ corriente.
- Si se decide atacar el drift, la acción típica es: copiar los `.json` del origen al snapshot local y ajustar los recuentos del inventario.
