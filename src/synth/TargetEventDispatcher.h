#pragma once

#include "ISynthTarget.h"
#include "ExperimentPlan.h"
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Despachador desacoplado de eventos hacia el target de síntesis.
 * Implementa la separación entre planificación científica y transporte físico/virtual.
 */
class TargetEventDispatcher
{
public:
    TargetEventDispatcher() = default;

    /**
     * @brief Ejecuta un plan de ensayo sobre el target abstrayendo el protocolo de transporte.
     */
    [[nodiscard]] TargetExecutionTrace dispatchPlan(
        ISynthTarget& target,
        const ExperimentPlan& plan,
        int repetitionIndex = 0);
};

} // namespace abdaudiolab::synth
