// ==============================================================================
// ABDAudioLab - GENERADO. NO EDITAR ESTE FICHERO A MANO.
// ==============================================================================
//
// La mitad de C++ de la regla de cuarentena, escrita desde el enum del esquema
// `ABDSharedAssets/contracts/hardware_profile.schema.json`. Los tres nombres de
// abajo no estan tecleados en ningun sitio de este repositorio: salen de ahi.
//
// ESTE FICHERO NO SE ESCRIBE A MANO, Y ESO ES JUSTO LO QUE LO HACE FIABLE. Con
// los literales en la cabecera, cambiar el enum del esquema dejaba a C++ mirando
// un nombre viejo: `evaluar` devolvia "no retenido" para siempre, el registro
// cargaba el contrato dudoso, y no habia ningun rojo en ningun lado. Los dos
// tests que ataban las dos mitades hacian SKIP sin el repositorio hermano, que
// es el clon limpio. Aqui no hay mitad que comparar: hay un fichero generado, y
// lo unico que puede quedar viejo es el fichero, que se comprueba sin ejecutarlo.
//
//   Se escribe con:  node scripts/generar-cuarentena-cpp.mjs        (ABDSharedAssets)
//   Se comprueba con: node scripts/generar-cuarentena-cpp.mjs --check
//
// El preflight corre el `--check`, y eso es lo que lo ata: no necesita el hermano
// para decidir, solo necesita el esquema.
// ==============================================================================

#pragma once

namespace abdaudiolab::core::quarantine::generado
{

/** El campo del contrato que lleva la marca. Del esquema: `status`. */
inline constexpr const char* campoEstado = "status";

/**
 * El unico valor de `campoEstado` que retiene.
 *
 * Del enum del esquema, y el enum tiene un solo valor. Si algum dia declara mas,
 * el generador se para y esa decision se escribe en los dos lados a proposito.
 */
inline constexpr const char* valorEstado = "quarantined";

/** El campo del motivo. Derivado del anterior: `statusReason`. */
inline constexpr const char* campoMotivo = "statusReason";

} // namespace abdaudiolab::core::quarantine::generado
