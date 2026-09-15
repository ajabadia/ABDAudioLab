#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "ISynthTarget.h"
#include "ModelEvaluationTypes.h"
#include "Sha256.h"
#include "gui/session/ProfilingSessionContracts.h"

namespace abdaudiolab::synth
{

/**
 * @brief Huella metrológica e inmutable de procedencia e integridad física del target.
 *
 * El SHA-256 demuestra integridad física y trazabilidad empírica, no autenticidad
 * ni firma de autoría. Para auditoría completa se registran todos los parámetros
 * del entorno de ejecución del binario.
 */
struct TargetFingerprint
{
    std::string pluginPath;
    std::string pluginFormatVersion; // Ej. "VST 3.7.x" o "Synthetic-v1"
    std::string vendor;
    std::string pluginUid;
    uint64_t fileSizeBytes { 0 };
    std::string binarySha256;
    std::string buildConfiguration;  // Ej. "Release-x64", "Debug-x64"
    double hostSampleRate { 96000.0 };
    int hostBlockSize { 256 };
    std::string osArchitecture;      // Ej. "x86_64-windows"
    std::string normalizedFingerprint;

    /**
     * @brief Calcula la huella normalizada determinista.
     * Normaliza la ruta (minúsculas, separadores estándar) de modo que dos
     * rutas distintas que apunten al mismo archivo generen idéntica huella.
     */
    [[nodiscard]] std::string computeNormalizedFingerprint() const
    {
        // Normalización canónica de la ruta
        std::string normPath = pluginPath;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');
        std::transform(normPath.begin(), normPath.end(), normPath.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        std::ostringstream ss;
        ss << normPath << "|"
           << pluginFormatVersion << "|"
           << vendor << "|"
           << pluginUid << "|"
           << fileSizeBytes << "|"
           << binarySha256 << "|"
           << buildConfiguration << "|"
           << std::fixed << std::setprecision(1) << hostSampleRate << "|"
           << hostBlockSize << "|"
           << osArchitecture;

        return Sha256::computeHex(ss.str());
    }
};

/**
 * @brief Interfaz de ciclo de vida del target de síntesis (Lifecycle Adapter).
 *
 * Desacopla la creación, preparación, reset, liberación y procedencia
 * de la ejecución del procesamiento DSP (ISynthTarget).
 *
 * Permite que el orquestador trabaje indistintamente con:
 * - SyntheticTargetLifecycleAdapter (Fixture determinista in-process)
 * - InProcessVst3LifecycleAdapter (Target VST3 cargado en el proceso host)
 * - Futuro VST3ProcessIsolatedTarget (Target VST3 aislado en proceso externo vía IPC/shm)
 */
class ISynthTargetLifecycleAdapter
{
public:
    virtual ~ISynthTargetLifecycleAdapter() = default;

    /**
     * @brief Inicializa e instancia el target físico o simulado.
     * PRECONDICIÓN: Invocado fuera del hilo de audio en tiempo real.
     */
    virtual bool initializeTarget(const gui::session::TargetSelectionState& targetState,
                                  double sampleRate,
                                  int blockSize,
                                  std::string& outErrorMessage) = 0;

    /**
     * @brief Obtiene el target de procesamiento DSP activo.
     * @return Puntero a ISynthTarget si está listo; nullptr si no está inicializado o ha sido liberado.
     */
    [[nodiscard]] virtual ISynthTarget* getTarget() noexcept = 0;
    [[nodiscard]] virtual const ISynthTarget* getTarget() const noexcept = 0;

    /**
     * @brief Resetea el target antes de iniciar un nuevo ensayo de medición.
     */
    virtual void resetForTrial() = 0;

    /**
     * @brief Libera los recursos del target de forma segura y permanente.
     * Tras invocar releaseTarget(), getTarget() devuelve nullptr y el adaptador no puede procesar.
     */
    virtual void releaseTarget() = 0;

    /**
     * @brief Indica si el target está preparado y listo para procesar bloques.
     */
    [[nodiscard]] virtual bool isReady() const noexcept = 0;

    /**
     * @brief Indica el origen formal de la evaluación generada por este target.
     */
    [[nodiscard]] virtual EvaluationOrigin getEvaluationOrigin() const noexcept = 0;

    /**
     * @brief Retorna la etiqueta formal del modo de ejecución.
     * Ej: "DemoMode", "InProcessVST3", "OutOfProcessVST3".
     */
    [[nodiscard]] virtual std::string getExecutionMode() const = 0;

    /**
     * @brief Retorna la huella metrológica del target.
     */
    [[nodiscard]] virtual TargetFingerprint getFingerprint() const = 0;

    /**
     * @brief Informa si el formato del target requiere el Message Thread durante su creación.
     */
    [[nodiscard]] virtual bool requiresUnblockedMessageThread() const noexcept { return false; }
};

} // namespace abdaudiolab::synth
