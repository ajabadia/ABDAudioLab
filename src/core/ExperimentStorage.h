/**
 * @file ExperimentStorage.h
 * @brief Transactional writer and verifying reader for immutable ExperimentRecord containers.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ExperimentRecord.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <utility>

namespace abdaudiolab::core
{

/**
 * @interface IExperimentReader
 * @brief Interfaz común para lectores de experimentos (carpeta estructurada y legados).
 */
class IExperimentReader
{
public:
    virtual ~IExperimentReader() = default;
    [[nodiscard]] virtual bool canRead(const juce::File& location) const = 0;
    [[nodiscard]] virtual std::optional<ExperimentRecord> read(const juce::File& location, juce::String& outError) = 0;
};

/**
 * @class ExperimentFolderReader
 * @brief Lector estricto de carpetas de experimento con verificación SHA-256 bit a bit.
 */
class ExperimentFolderReader : public IExperimentReader
{
public:
    ExperimentFolderReader() = default;
    ~ExperimentFolderReader() override = default;

    [[nodiscard]] bool canRead(const juce::File& location) const override;
    [[nodiscard]] std::optional<ExperimentRecord> read(const juce::File& location, juce::String& outError) override;
};

/**
 * @brief Carga útil opcional para embeber el modelo representativo en el experimento.
 */
struct EmbeddedModelPayload
{
    std::string relativePathInsideExperiment { "models/ModelPackage.h" };
    std::string modelSourceCode;
    juce::File convenienceExportFile; /**< Archivo de destino en exports/ para copia de distribución. */
};

/**
 * @class ExperimentStorage
 * @brief Gestor transaccional de persistencia inmutable de experimentos.
 */
class ExperimentStorage
{
public:
    /**
     * @brief Retorna el directorio base predeterminado para almacenar experimentos (resuelto via LabDataDirectories).
     */
    [[nodiscard]] static juce::File getDefaultExperimentsDirectory();

    using StagingHook = std::function<bool(const juce::File& stagingDir, juce::String& stageError)>;

    /**
     * @brief Guarda un experimento de forma estrictamente transaccional.
     *        Crea carpeta temporal en experiments/, escribe JSONs, copia audio, genera y comprueba
     *        modelo embebido y copia a exports/, calcula manifest con SHA-256,
     *        verifica y renombra atómicamente al destino final.
     * @param baseDir Directorio raíz donde reside la carpeta de experimentos.
     * @param record Metadatos y definición del experimento.
     * @param audioFilesToInclude Lista de pares (rutaRelativa, archivoOrigenWav).
     * @param outError Mensaje de diagnóstico si la operación falla.
     * @param embeddedModel Payload opcional del modelo representativo C++20.
     * @param stagingHook Gancho opcional ejecutado estrictamente sobre el stagingDir antes de manifest.
     * @return true si el experimento se escribió y verificó con éxito.
     */
    static bool saveExperiment(const juce::File& baseDir,
                               const ExperimentRecord& record,
                               const std::vector<std::pair<std::string, juce::File>>& audioFilesToInclude,
                               juce::String& outError,
                               const std::optional<EmbeddedModelPayload>& embeddedModel = std::nullopt,
                               StagingHook stagingHook = nullptr);


    /**
     * @brief Carga y valida criptográficamente un experimento desde una carpeta.
     *        Si algún archivo fue alterado o falta, retorna el registro con estado Corrupt.
     */
    [[nodiscard]] static std::optional<ExperimentRecord> loadExperiment(const juce::File& experimentDir,
                                                                        juce::String& outError);

    /**
     * @brief Calcula el hash SHA-256 en formato hexadecimal de un archivo en disco.
     */
    [[nodiscard]] static std::string computeFileSha256(const juce::File& file);
};

} // namespace abdaudiolab::core
