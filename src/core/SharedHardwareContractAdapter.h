/**
 * @file SharedHardwareContractAdapter.h
 * @brief Adapter that loads contracts from ABDSharedCode::HardwareContractRegistry
 *        and exposes them as local abdaudiolab::core::HardwareContract with full
 *        domain data (functions, controls, routingGuide).
 * @details This is the "bridge" pattern from the integration guide:
 *          - Uses shared registry for identity detection (single-source contracts)
 *          - Reads domain-specific sections (functions/controls) from raw JSON
 *          - Exposes local HardwareItem-compatible types
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include <HardwareMidiDetect/HardwareContract.h>
#include "HardwareContractRegistry.h"  // local domain types
#include <nlohmann/json.hpp>

namespace abdaudiolab::core
{

/**
 * @class SharedHardwareContractAdapter
 * @brief Loads contracts via shared registry and rebuilds local domain contracts
 *        with functions/controls/routingGuide from raw JSON.
 */
class SharedHardwareContractAdapter
{
public:
    explicit SharedHardwareContractAdapter(abd::hwid::HardwareContractRegistry& sharedRegistry)
        : sharedRegistry_(sharedRegistry) {}

    /**
     * @brief Load contracts from directory using shared registry, then enrich local cache.
     * @param contractsDir Directory containing contract JSON files.
     * @return True if at least one contract loaded successfully.
     */
    bool loadFromShared(const juce::File& contractsDir)
    {
        if (!sharedRegistry_.loadContractsFromDirectory(contractsDir))
        {
            lastError_ = sharedRegistry_.getLastError();
            return false;
        }
        rebuildLocalCache();
        return true;
    }

    [[nodiscard]] bool hasContracts() const noexcept { return !localCache_.empty(); }
    [[nodiscard]] const std::vector<HardwareContract>& getContracts() const noexcept { return localCache_; }
    [[nodiscard]] const HardwareContract* findContractById(const std::string& id) const noexcept;
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastError_; }

private:
    void rebuildLocalCache();
    void parseFunctionsFromRawJson(const nlohmann::json& j, HardwareContract& out);

    abd::hwid::HardwareContractRegistry& sharedRegistry_;
    std::vector<HardwareContract> localCache_;
    std::string lastError_;
};

} // namespace abdaudiolab::core