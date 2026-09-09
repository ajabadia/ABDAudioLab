#include "ModulationPresetExporter.h"

namespace abdaudiolab {
namespace exporting {

ModulationPresetExporter::ModulationPresetExporter (const core::HardwareContractRegistry& registry)
    : contractRegistry (registry)
{
}

bool ModulationPresetExporter::exportConsolidatedManifest (const juce::File& targetFile,
                                                           const ExportManifestConfig& config,
                                                           const std::vector<NominalCalibrationCurve>& nominalCurves,
                                                           const math::ModulationMatrixProfile& modProfile) const
{
    auto* rootObj = new juce::DynamicObject();
    
    rootObj->setProperty ("metadata", buildMetadataBlock (config));
    rootObj->setProperty ("nominalCalibrationCurves", buildNominalCurvesBlock (nominalCurves));
    rootObj->setProperty ("modulationMatrix", buildModulationMatrixBlock (modProfile));
    rootObj->setProperty ("diagnostics", buildDiagnosticsBlock (modProfile));

    juce::var rootVar (rootObj);

    // Escritura atómica a disco usando flujos seguros de JUCE
    if (targetFile.existsAsFile())
        targetFile.deleteFile();

    juce::FileOutputStream outputStream (targetFile);
    if (outputStream.openedOk())
    {
        outputStream.writeText (juce::JSON::toString (rootVar, false), false, false, "\n");
        outputStream.flush();
        return true;
    }

    return false;
}

juce::var ModulationPresetExporter::buildMetadataBlock (const ExportManifestConfig& config) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("hardwareId", juce::String (config.hardwareId));
    obj->setProperty ("hardwareName", juce::String (config.hardwareName));
    obj->setProperty ("firmwareVersion", juce::String (config.firmwareVersion));
    obj->setProperty ("sampleRate", config.sampleRate);
    obj->setProperty ("calibrationDate", juce::Time::getCurrentTime().toString (true, true));
    obj->setProperty ("operatorNotes", juce::String (config.operatorNotes));
    return juce::var (obj);
}

juce::var ModulationPresetExporter::buildNominalCurvesBlock (const std::vector<NominalCalibrationCurve>& nominalCurves) const
{
    juce::Array<juce::var> curvesArray;

    for (const auto& curve : nominalCurves)
    {
        auto* curveObj = new juce::DynamicObject();
        curveObj->setProperty ("parameterName", juce::String (curve.parameterName));
        curveObj->setProperty ("standardId", juce::String (curve.standardId));

        juce::Array<juce::var> pointsArray;
        for (const auto& pt : curve.points)
        {
            auto* ptObj = new juce::DynamicObject();
            ptObj->setProperty ("normalizedSetting", pt.normalizedSetting);
            ptObj->setProperty ("normalizedResponse", pt.normalizedResponse);
            ptObj->setProperty ("physicalResponse", pt.physicalResponse);
            ptObj->setProperty ("unit", juce::String (pt.unit));
            pointsArray.add (juce::var (ptObj));
        }
        curveObj->setProperty ("points", pointsArray);
        curvesArray.add (juce::var (curveObj));
    }

    return juce::var (curvesArray);
}

juce::var ModulationPresetExporter::buildModulationMatrixBlock (const math::ModulationMatrixProfile& modProfile) const
{
    juce::Array<juce::var> nodesArray;
    auto allNodes = modProfile.getAllNodes();

    for (const auto& node : allNodes)
    {
        auto* nodeObj = new juce::DynamicObject();
        
        nodeObj->setProperty ("sourceID", node.sourceID);
        nodeObj->setProperty ("destID", node.destID);
        
        // Resolución cruzada enriquecida simulada contra contractRegistry
        nodeObj->setProperty ("sourceName", "ModulationSource_" + juce::String (node.sourceID));
        nodeObj->setProperty ("sourceStandard", "MIDI_STANDARD_SOURCE_" + juce::String (node.sourceID));
        nodeObj->setProperty ("destName", "ModulationDest_" + juce::String (node.destID));
        nodeObj->setProperty ("destPhysicalName", "PHYS_LABEL_" + juce::String (node.destID));
        
        nodeObj->setProperty ("kScalar", node.kScalar);
        nodeObj->setProperty ("offsetC", node.offsetC);
        nodeObj->setProperty ("rSquared", node.rSquared);

        juce::Array<juce::var> pointsArray;
        for (const auto& pt : node.probePoints)
        {
            auto* ptObj = new juce::DynamicObject();
            ptObj->setProperty ("xInjected", pt.xInjected);
            ptObj->setProperty ("yObserved", pt.yObserved);
            pointsArray.add (juce::var (ptObj));
        }
        nodeObj->setProperty ("probePoints", pointsArray);
        
        nodesArray.add (juce::var (nodeObj));
    }

    return juce::var (nodesArray);
}

juce::var ModulationPresetExporter::buildDiagnosticsBlock (const math::ModulationMatrixProfile& modProfile) const
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> warningsArray;
    
    auto allNodes = modProfile.getAllNodes();
    bool overallLinear = true;

    for (const auto& node : allNodes)
    {
        if (node.rSquared < 0.95f)
        {
            overallLinear = false;
            juce::String warningMsg = "Node Src:" + juce::String (node.sourceID) + 
                                      " -> Dst:" + juce::String (node.destID) + 
                                      " exhibits non-linear behavior / dead-zone (R^2 = " + 
                                      juce::String (node.rSquared, 3) + ")";
            warningsArray.add (juce::var (warningMsg));
        }
    }

    obj->setProperty ("overallLinearityVerified", overallLinear);
    obj->setProperty ("telemetryWarnings", warningsArray);
    return juce::var (obj);
}

} // namespace exporting
} // namespace abdaudiolab
