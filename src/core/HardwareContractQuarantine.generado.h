// ==============================================================================
// ABDAudioLab - GENERADO. NO EDITAR ESTE FICHERO A MANO.
// ==============================================================================
//
// La mitad de C++ de la regla de cuarentena. Las reglas las DECLARA el
// esquema, en x-cuarentena.reglas, y los valores salen de los enums de esos
// campos. Ninguno de estos nombres esta tecleado en ningun sitio de este
// repositorio: salen de ahi.
//
//   Se escribe con:  node scripts/generar-cuarentena-cpp.mjs        (ABDSharedAssets)
//   Se comprueba con: node scripts/generar-cuarentena-cpp.mjs --check
//
// El preflight corre el --check, y eso es lo que lo ata: no necesita el hermano
// para decidir, solo necesita el esquema.
// ==============================================================================

#pragma once

#include <array>
#include <cstddef>

namespace abdaudiolab::core::quarantine::generado
{

/** Una regla: el campo que lleva la marca, el valor que retiene y su motivo. */
struct Regla
{
    const char* campoEstado;
    const char* valorEstado;
    const char* campoMotivo;
};

/** Cuantas reglas declara el esquema. */
inline constexpr std::size_t numeroReglas = 1;

/** Todas las reglas, en el orden que las declara el esquema. */
inline constexpr std::array<Regla, numeroReglas> reglas = {{
    {"status", "quarantined", "statusReason"},
}};

// --- La primera, con los nombres de siempre ---
//
// El consumidor de C++ usa estos tres. Son la PRIMERA regla del mapa, y el
// static_assert de HardwareContractQuarantine.h dice cuantas reglas sabe
// atender: cuando aparezca una segunda, el laboratorio deja de compilar en
// vez de mirar la primera y olvidar la otra.
inline constexpr const char* campoEstado = reglas[0].campoEstado;
inline constexpr const char* valorEstado = reglas[0].valorEstado;
inline constexpr const char* campoMotivo = reglas[0].campoMotivo;

} // namespace abdaudiolab::core::quarantine::generado
