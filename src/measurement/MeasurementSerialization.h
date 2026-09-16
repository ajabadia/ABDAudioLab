/**
 * @file MeasurementSerialization.h
 * @brief Canonical JSON serializer, parser, and structural validators for measurement contracts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <string>

namespace abdaudiolab::measurement
{

class MeasurementSerialization
{
public:
    static constexpr const char* kExpectedSchemaVersion = "response-measurement-1.0";
    static constexpr const char* kExpectedSchemaUri = "urn:abdaudiolab:response-measurement:1.0";

    /**
     * @brief Validates schema version and URI match response-measurement-1.0.
     */
    static bool validateSchema(const std::string& schemaVersion, 
                               const std::string& schemaUri, 
                               std::string& outError);

    /**
     * @brief Validates structural, numeric and unit integrity of a measurement curve.
     * 
     * Rejects:
     * - Dimension mismatch (x.size() != y.size())
     * - NaN or Infinity in x or y
     * - Missing units when curve has data
     * - Empty curve when status is 'completed'
     * 
     * Allows empty curve when status is 'unreliable', 'skipped', 'invalid' or 'failed'.
     */
    static bool validateCurve(const MeasurementCurve& curve, 
                              MeasurementStatus status, 
                              std::string& outError);

    /**
     * @brief Canonical deterministic JSON serialization for MeasurementSpec.
     */
    static std::string serializeSpec(const MeasurementSpec& spec, int indent = 2);

    /**
     * @brief Parses and validates a MeasurementSpec from a JSON string.
     */
    static bool deserializeSpec(const std::string& jsonStr, 
                                MeasurementSpec& outSpec, 
                                std::string& outError);

    /**
     * @brief Canonical deterministic JSON serialization for MeasurementResult.
     */
    static std::string serializeResult(const MeasurementResult& result, int indent = 2);

    /**
     * @brief Parses and validates a MeasurementResult from a JSON string.
     */
    static bool deserializeResult(const std::string& jsonStr, 
                                  MeasurementResult& outResult, 
                                  std::string& outError);

    /**
     * @brief Serializes a StimulusSpec.
     */
    static std::string serializeStimulus(const StimulusSpec& stimulus, int indent = 2);

    /**
     * @brief Parses a StimulusSpec from a JSON string.
     */
    static bool deserializeStimulus(const std::string& jsonStr, 
                                    StimulusSpec& outStimulus, 
                                    std::string& outError);
};

} // namespace abdaudiolab::measurement
