#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace abdaudiolab {
namespace math {

struct ModulationProbePoint {
    float xInjected   { 0.0f }; // Excitación de origen (ej. 32, 64, 96, 127)
    float yObserved   { 0.0f }; // Delta de la métrica destino leída (Hz, dB, etc.)
};

struct ModulationNode {
    int sourceID      { 0 };
    int destID        { 0 };
    float kScalar     { 0.0f }; // Factor de ganancia K estimado
    float offsetC     { 0.0f }; // Offset / Zona muerta
    float rSquared    { 0.0f }; // Coeficiente de determinación lineal (0.0 a 1.0)
    std::vector<ModulationProbePoint> probePoints;
};

class ModulationMatrixProfile {
public:
    ModulationMatrixProfile() = default;
    ~ModulationMatrixProfile() = default;

    void setNode (int srcID, int dstID, const ModulationNode& node);
    bool hasNode (int srcID, int dstID) const;
    ModulationNode getNode (int srcID, int dstID) const;
    
    std::vector<ModulationNode> getAllNodes() const;
    void clear();

    /**
     * Serializa la matriz dispersa completa a un árbol juce::var (JSON compatible)
     */
    juce::var toDynamicVar() const;

private:
    // Clave de 64 bits: (uint64_t(srcID) << 32) | uint32_t(dstID)
    std::unordered_map<uint64_t, ModulationNode> sparseMatrix;

    static inline uint64_t makeKey (int srcID, int dstID) noexcept {
        return (static_cast<uint64_t> (srcID) << 32) | (static_cast<uint64_t> (static_cast<uint32_t> (dstID)));
    }
};

} // namespace math
} // namespace abdaudiolab
