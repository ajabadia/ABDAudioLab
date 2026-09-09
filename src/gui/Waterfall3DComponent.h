#pragma once

#include <visualizers/Waterfall3DComponent.h>
#include "../math/PreScanSpectrumAnalyzer.h"

namespace abdaudiolab {
namespace gui {

/**
 * @class Waterfall3DComponent
 * @brief Forwarder / Adapter for abd::vis::Waterfall3DComponent in ABDSharedCode.
 */
class Waterfall3DComponent : public abd::vis::Waterfall3DComponent
{
public:
    using abd::vis::Waterfall3DComponent::Waterfall3DComponent;

    void updateTrajectoryData(const std::vector<math::PreScanPoint>& newTrajectory)
    {
        std::vector<abd::vis::PreScanPoint> sharedPoints;
        sharedPoints.reserve(newTrajectory.size());
        for (const auto& p : newTrajectory)
        {
            sharedPoints.push_back({ p.controlValue, p.timeSec, p.primaryMetric, p.thdPercent });
        }
        abd::vis::Waterfall3DComponent::updateTrajectoryData(sharedPoints);
    }
};

} // namespace gui
} // namespace abdaudiolab
