/**
 * @file HardwareContractQuarantine.cpp
 * @brief The quarantine rule, applied to a parsed contract or to one on disk.
 * @author ABDSynths
 * @date 2026
 */

#include "HardwareContractQuarantine.h"

#include <fstream>

namespace abdaudiolab::core::quarantine
{

Resultado evaluar(const nlohmann::json& contrato)
{
    Resultado r;

    if (!contrato.is_object())
        return r;

    // Solo el literal. Un `status` que no sea este NO retiene, y eso parece
    // contrario a la prudencia pero es lo contrario: retener es lo que hace
    // dano, porque esconde hardware. Un valor mal escrito tiene que verse, no
    // desaparecer. El aviso de "estado desconocido" lo pone el que carga, mas
    // abajo en su cadena, donde ya hay donde avisar.
    if (contrato.value(campoEstado, std::string()) != valorEstado)
        return r;

    r.retenido = true;
    r.motivo = juce::String(contrato.value(campoMotivo, std::string()));

    // Retenido SIN motivo es un retenido sin explicacion, y eso es lo que hace
    // que alguien pregunte dentro de tres semanas por que falta un Aparato. Se
    // dice igual, pero se dice que no lo dice.
    if (r.motivo.isEmpty())
        r.motivo = motivoPorDefecto;

    return r;
}

Resultado evaluar(const juce::File& contrato)
{
    // Se lee SOLO lo que hace falta. A esta puerta se la llama con cada JSON
    // del catalogo, y un buen numero de ellos son recetas, matrices y patches
    // que no son contratos: leerlos enteros aqui para mirar dos claves seria
    // tirar el doble de trabajo en el arranque, que es cuando se nota.
    //
    // Y si el fichero no se puede leer, NO se retiene. Retener un contrato
    // ilegible seria esconder un problema de permisos o de JSON truncado detras
    // de una decision editorial, que es la peor forma de perder un error.
    std::ifstream in(contrato.getFullPathName().toStdString(), std::ios::binary);

    if (!in.good())
        return {};

    nlohmann::json j;

    try
    {
        in >> j;
    }
    catch (const std::exception&)
    {
        // No es JSON. Lo dira quien lo carga, que es quien tiene el contexto
        // para explicar un parseo fallido.
        return {};
    }

    return evaluar(j);
}


//------------------------------------------------------------------------------
// LAS TRES FRASES.
//
// estan al final y no junto a `evaluar` porque no son la regla: son como se
// cuenta. Y estan en este fichero, y no en quien las usa, porque hay tres
// consumidores --el registro, el adapter y el panel de avisos del arranque-- y
// tres redacciones distintas del mismo hecho son tres maneras de que una de
// ellas se quede sin actualizar.
//------------------------------------------------------------------------------

juce::String descripcionDeRetencion(const juce::String& nombre, const juce::String& motivo)
{
    // El "decision editorial, no fallo" va el primero y sin mayuscula,
    // porque lo primero que se lee de una linea de aviso decide como se
    // interpretan las siguientes cuarenta. Y el motivo lleva delante el nombre
    // del campo del que sale: sin eso, "de 31 bloques solo 7 casan" parece un fallo de carga.
    return juce::String::fromUTF8 (u8"decision editorial, no fallo: el contrato ")
           + nombre + juce::String::fromUTF8 (u8" esta retenido por cuarentena.")
           + juce::String::fromUTF8 (u8" Motivo (statusReason del contrato): ")
           + (motivo.isEmpty() ? juce::String(motivoPorDefecto) : motivo);
}

juce::String descripcionDeRetenido(const Retenido& retenido)
{
    const auto base = descripcionDeRetencion(retenido.nombre, retenido.motivo);

    // El fichero solo cuando se sabe cual es. Un retenido sin ruta no lleva
    // ninguno: un texto de "edita el fichero" sin decir cual es peor que no decirlo,
    // porque suena a que si lo dice.
    if (retenido.fichero.getFullPathName().isEmpty())
        return base;

    return base + juce::String::fromUTF8 (u8" Se levanta quitando la marca \"status\": \"quarantined\" de ")
           + retenido.fichero.getFullPathName() + ".";
}

juce::String lineaDeLogDeRetencion(const Retenido& retenido)
{
    return juce::String(prefijoLog) + " " + descripcionDeRetenido(retenido);
}

} // namespace abdaudiolab::core::quarantine
