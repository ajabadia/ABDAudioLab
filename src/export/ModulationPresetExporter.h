#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include "../math/ModulationMatrixProfile.h"
#include "../core/HardwareContractRegistry.h"

namespace abdaudiolab {
namespace exporting {

struct NominalCurvePoint {
    float normalizedSetting  { 0.0f };
    float normalizedResponse { 0.0f };
    float physicalResponse   { 0.0f };
    std::string unit;
};

struct NominalCalibrationCurve {
    std::string parameterName;
    std::string standardId;
    std::vector<NominalCurvePoint> points;
};

struct ExportManifestConfig {
    std::string hardwareId;
    std::string hardwareName;
    std::string firmwareVersion;
    double sampleRate { 44100.0 };
    std::string operatorNotes;
};

class ModulationPresetExporter 
{
public:
    ModulationPresetExporter (const core::HardwareContractRegistry& registry);
    ~ModulationPresetExporter() = default;

    /**
     * Fusiona las curvas de calibración nominales y la matriz de modulación K
     * en un manifiesto JSON/LNL consolidado de forma segura y atómica.
     */
    bool exportConsolidatedManifest (const juce::File& targetFile,
                                     const ExportManifestConfig& config,
                                     const std::vector<NominalCalibrationCurve>& nominalCurves,
                                     const math::ModulationMatrixProfile& modProfile) const;

private:
    const core::HardwareContractRegistry& contractRegistry;

    juce::var buildMetadataBlock (const ExportManifestConfig& config) const;
    juce::var buildNominalCurvesBlock (const std::vector<NominalCalibrationCurve>& nominalCurves) const;
    juce::var buildModulationMatrixBlock (const math::ModulationMatrixProfile& modProfile) const;
    juce::var buildDiagnosticsBlock (const math::ModulationMatrixProfile& modProfile) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPresetExporter)
};

} // namespace exporting
} // namespace abdaudiolab
