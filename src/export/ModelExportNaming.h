#pragma once

#include <juce_core/juce_core.h>
#include <string>

namespace abdaudiolab::exporting
{

/**
 * @struct ModelExportNaming
 * @brief Reglas deterministas y seguras de nomenclatura y resolución de colisiones
 *        para la exportación de artefactos de producción en C++20.
 *
 * Formato canónico:
 *   <Target>_<Model>_<UTC_timestamp>_<HashPrefix>.h
 * Ejemplo:
 *   ReferenceSynth_LUT_SIMD_2D_20260915T095320Z_8c12ce90.h
 */
struct ModelExportNaming
{
    /**
     * @brief Sanitiza una cadena para su uso seguro como componente de ruta en cualquier SO.
     *        Elimina / \ : * ? " < > |, controla caracteres ASCII < 32, previene nombres reservados
     *        de Windows (CON, PRN, AUX, NUL, COM1-9, LPT1-9) y colapsa guiones bajos.
     */
    static std::string sanitizeComponent(const std::string& input,
                                         const std::string& fallbackDefault = "Item",
                                         size_t maxLength = 32);

    /**
     * @brief Genera una marca de tiempo UTC inequívoca en formato ISO-8601 compacto: YYYYMMDDTHHMMSSZ.
     */
    static std::string formatUtcTimestamp(const juce::Time& time);

    /**
     * @brief Extrae los primeros N caracteres (por defecto 8) del hash canónico, forzando minúsculas.
     */
    static std::string extractHashPrefix(const std::string& fullCanonicalHash, size_t prefixLength = 8);

    /**
     * @brief Construye el nombre base canónico del archivo de exportación.
     */
    static std::string buildFileName(const std::string& targetName,
                                     const std::string& modelType,
                                     const juce::Time& utcTime,
                                     const std::string& fullCanonicalHash);

    /**
     * @brief Resuelve un archivo en el directorio destino garantizando que NO sobrescriba
     *        silenciosamente un artefacto existente. Si ya existe, añade sufijo numérico (_1, _2...).
     */
    static juce::File resolveUniqueExportFile(const juce::File& exportDir,
                                             const std::string& baseFileName);
};

} // namespace abdaudiolab::exporting
