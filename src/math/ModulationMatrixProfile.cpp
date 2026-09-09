#include "ModulationMatrixProfile.h"

namespace abdaudiolab {
namespace math {

void ModulationMatrixProfile::setNode (int srcID, int dstID, const ModulationNode& node) {
    sparseMatrix[makeKey (srcID, dstID)] = node;
}

bool ModulationMatrixProfile::hasNode (int srcID, int dstID) const {
    return sparseMatrix.find (makeKey (srcID, dstID)) != sparseMatrix.end();
}

ModulationNode ModulationMatrixProfile::getNode (int srcID, int dstID) const {
    auto it = sparseMatrix.find (makeKey (srcID, dstID));
    return (it != sparseMatrix.end()) ? it->second : ModulationNode{};
}

std::vector<ModulationNode> ModulationMatrixProfile::getAllNodes() const {
    std::vector<ModulationNode> nodes;
    nodes.reserve (sparseMatrix.size());
    for (const auto& pair : sparseMatrix) {
        nodes.push_back (pair.second);
    }
    return nodes;
}

void ModulationMatrixProfile::clear() {
    sparseMatrix.clear();
}

juce::var ModulationMatrixProfile::toDynamicVar() const {
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> nodesArray;

    for (const auto& pair : sparseMatrix) {
        const auto& node = pair.second;
        auto* nodeObj = new juce::DynamicObject();
        
        nodeObj->setProperty ("sourceID", node.sourceID);
        nodeObj->setProperty ("destID", node.destID);
        nodeObj->setProperty ("kScalar", node.kScalar);
        nodeObj->setProperty ("offsetC", node.offsetC);
        nodeObj->setProperty ("rSquared", node.rSquared);

        juce::Array<juce::var> pointsArray;
        for (const auto& pt : node.probePoints) {
            auto* ptObj = new juce::DynamicObject();
            ptObj->setProperty ("xInjected", pt.xInjected);
            ptObj->setProperty ("yObserved", pt.yObserved);
            pointsArray.add (juce::var (ptObj));
        }
        nodeObj->setProperty ("probePoints", pointsArray);
        
        nodesArray.add (juce::var (nodeObj));
    }

    obj->setProperty ("modulationNodes", nodesArray);
    return juce::var (obj);
}

} // namespace math
} // namespace abdaudiolab
