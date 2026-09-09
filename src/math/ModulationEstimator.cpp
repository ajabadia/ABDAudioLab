#include "ModulationEstimator.h"
#include <cmath>

namespace abdaudiolab {
namespace math {

ModulationNode ModulationEstimator::calculateNode (int srcID, int dstID, 
                                                   const std::vector<ModulationProbePoint>& points)
{
    ModulationNode node;
    node.sourceID = srcID;
    node.destID = dstID;
    node.probePoints = points;

    const size_t n = points.size();
    if (n < 2) {
        node.kScalar = 0.0f;
        node.offsetC = 0.0f;
        node.rSquared = 0.0f;
        return node;
    }

    // Acumuladores analíticos locales (puros en el stack, cero heap allocations)
    float sumX  = 0.0f;
    float sumY  = 0.0f;
    float sumXY = 0.0f;
    float sumXX = 0.0f;

    for (const auto& pt : points) {
        sumX  += pt.xInjected;
        sumY  += pt.yObserved;
        sumXY += pt.xInjected * pt.yObserved;
        sumXX += pt.xInjected * pt.xInjected;
    }

    const float floatN = static_cast<float> (n);
    const float denominator = (floatN * sumXX) - (sumX * sumX);

    // Salvaguarda matemática ante estímulos idénticos (denominador cero)
    if (std::abs (denominator) < 1e-6f) {
        node.kScalar = 0.0f;
        node.offsetC = sumY / floatN;
        node.rSquared = 0.0f;
        return node;
    }

    // 1. Regresión por Mínimos Cuadrados
    node.kScalar = ((floatN * sumXY) - (sumX * sumY)) / denominator;
    node.offsetC = (sumY - (node.kScalar * sumX)) / floatN;

    // 2. Coeficiente de Determinación R^2 (Linealidad)
    const float meanY = sumY / floatN;
    float ssTot = 0.0f;
    float ssRes = 0.0f;

    for (const auto& pt : points) {
        const float yPred = (node.kScalar * pt.xInjected) + node.offsetC;
        const float devTot = pt.yObserved - meanY;
        const float devRes = pt.yObserved - yPred;

        ssTot += devTot * devTot;
        ssRes += devRes * devRes;
    }

    // Si ssTot es cercano a cero, significa que la salida fue constante (linealidad perfecta)
    node.rSquared = (ssTot > 1e-6f) ? (1.0f - (ssRes / ssTot)) : 1.0f;
    
    // Asegurar límites estadísticos estrictos ante imprecisiones de coma flotante
    node.rSquared = juce::jlimit (0.0f, 1.0f, node.rSquared);

    return node;
}

} // namespace math
} // namespace abdaudiolab
