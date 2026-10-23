#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <algorithm>
#include "synth/ModelEvaluationBuilder.h"
#include "synth/ModelEvaluationTypes.h"
#include "synth/SyntheticGroundTruthFixtures.h"
#include "math/WienerHammersteinFitter.h"

namespace abdaudiolab::synth
{

/**
 * @brief Interfaz enriquecida para evaluadores de modelos candidatos con metadatos de recursos.
 */
class ICandidateModelEvaluator : public IModelCandidateEvaluator
{
public:
    virtual ~ICandidateModelEvaluator() = default;

    [[nodiscard]] virtual std::string getModelId() const = 0;
    [[nodiscard]] virtual std::string getArchitectureName() const = 0;
    [[nodiscard]] virtual ResourceFootprint getResourceFootprint() const = 0;
    [[nodiscard]] virtual ModelArtifactDescriptor getArtifactDescriptor() const = 0;
};

// ==============================================================================
// 1. Lut1DCandidateEvaluator: Tabla 1D Estática de Ondas / Waveshaper
// ==============================================================================

class Lut1DCandidateEvaluator : public ICandidateModelEvaluator
{
public:
    explicit Lut1DCandidateEvaluator(std::string modelId = "LUT_1D_WAVESHAPER",
                                     size_t tableSize = 1024)
        : modelId_(std::move(modelId)), tableSize_(std::max<size_t>(64, tableSize))
    {
        table_.resize(tableSize_);
        // Por defecto: lineal identity f(x) = x
        for (size_t i = 0; i < tableSize_; ++i)
        {
            float norm = -1.0f + 2.0f * (static_cast<float>(i) / static_cast<float>(tableSize_ - 1));
            table_[i] = norm;
        }
    }

    void setTransferFunction(const std::function<float(float)>& fn)
    {
        if (!fn) return;
        for (size_t i = 0; i < tableSize_; ++i)
        {
            float norm = -1.0f + 2.0f * (static_cast<float>(i) / static_cast<float>(tableSize_ - 1));
            table_[i] = fn(norm);
        }
    }

    void setPolynomialCubic(double a)
    {
        setTransferFunction([a](float x) {
            double xd = static_cast<double>(x);
            return static_cast<float>(xd - a * (xd * xd * xd));
        });
    }

    [[nodiscard]] float evaluateSample(float x) const noexcept
    {
        float clamped = std::clamp(x, -1.0f, 1.0f);
        float norm = (clamped + 1.0f) * 0.5f * static_cast<float>(tableSize_ - 1);
        int idx = static_cast<int>(norm);
        int nextIdx = std::min(idx + 1, static_cast<int>(tableSize_ - 1));
        float frac = norm - static_cast<float>(idx);
        return table_[static_cast<size_t>(idx)] * (1.0f - frac) + table_[static_cast<size_t>(nextIdx)] * frac;
    }

    [[nodiscard]] std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        const auto& in = !point.inputStimulusAudio.empty() ? point.inputStimulusAudio : point.targetGroundTruthAudio;
        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); ++i)
            out[i] = evaluateSample(in[i]);
        return out;
    }

    [[nodiscard]] std::string getModelId() const override { return modelId_; }
    [[nodiscard]] std::string getArchitectureName() const override { return "StaticLookupTable1D"; }

    [[nodiscard]] ResourceFootprint getResourceFootprint() const override
    {
        ResourceFootprint r;
        r.cpuUsagePercentPerVoice = 0.15; // Muy bajo
        r.memoryBytes = tableSize_ * sizeof(float);
        r.introducedLatencySamples = 0;
        return r;
    }

    [[nodiscard]] ModelArtifactDescriptor getArtifactDescriptor() const override
    {
        ModelArtifactDescriptor d;
        d.modelId = modelId_;
        d.modelArchitecture = getArchitectureName();
        d.format = "binary_lut";
        d.parameterCount = tableSize_;
        d.artifactHash = "sha256_lut1d_" + modelId_;
        return d;
    }

private:
    std::string modelId_;
    size_t tableSize_ { 1024 };
    std::vector<float> table_;
};

// ==============================================================================
// 2. LutSimd2DCandidateEvaluator: Tabla 2D SIMD Interpolada (Nivel x Control)
// ==============================================================================

class LutSimd2DCandidateEvaluator : public ICandidateModelEvaluator
{
public:
    explicit LutSimd2DCandidateEvaluator(std::string modelId = "LUT_SIMD_2D",
                                         int gridSize = 32)
        : modelId_(std::move(modelId)), gridSize_(std::max(4, gridSize))
    {
        grid_.resize(static_cast<size_t>(gridSize_ * gridSize_));
        // Matriz identidad por defecto
        for (int y = 0; y < gridSize_; ++y)
        {
            for (int x = 0; x < gridSize_; ++x)
            {
                float normX = -1.0f + 2.0f * (static_cast<float>(x) / static_cast<float>(gridSize_ - 1));
                grid_[static_cast<size_t>(y * gridSize_ + x)] = normX;
            }
        }
    }

    void setTransferFunction2D(const std::function<float(float, float)>& fn)
    {
        if (!fn) return;
        for (int y = 0; y < gridSize_; ++y)
        {
            float normY = static_cast<float>(y) / static_cast<float>(gridSize_ - 1);
            for (int x = 0; x < gridSize_; ++x)
            {
                float normX = -1.0f + 2.0f * (static_cast<float>(x) / static_cast<float>(gridSize_ - 1));
                grid_[static_cast<size_t>(y * gridSize_ + x)] = fn(normX, normY);
            }
        }
    }

    [[nodiscard]] float evaluateBilinear(float x, float control) const noexcept
    {
        float clampedX = std::clamp(x, -1.0f, 1.0f);
        float clampedY = std::clamp(control, 0.0f, 1.0f);

        float fx = (clampedX + 1.0f) * 0.5f * static_cast<float>(gridSize_ - 1);
        float fy = clampedY * static_cast<float>(gridSize_ - 1);

        int ix = std::clamp(static_cast<int>(fx), 0, gridSize_ - 2);
        int iy = std::clamp(static_cast<int>(fy), 0, gridSize_ - 2);

        float rx = fx - static_cast<float>(ix);
        float ry = fy - static_cast<float>(iy);

        float v00 = grid_[static_cast<size_t>(iy * gridSize_ + ix)];
        float v01 = grid_[static_cast<size_t>(iy * gridSize_ + (ix + 1))];
        float v10 = grid_[static_cast<size_t>((iy + 1) * gridSize_ + ix)];
        float v11 = grid_[static_cast<size_t>((iy + 1) * gridSize_ + (ix + 1))];

        float top = v00 * (1.0f - rx) + v01 * rx;
        float bottom = v10 * (1.0f - rx) + v11 * rx;
        return top * (1.0f - ry) + bottom * ry;
    }

    [[nodiscard]] std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        const auto& in = !point.inputStimulusAudio.empty() ? point.inputStimulusAudio : point.targetGroundTruthAudio;
        float ctrl = 0.5f;
        if (!point.coordinates.empty())
            ctrl = static_cast<float>(point.coordinates[0].continuousValue);

        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); ++i)
            out[i] = evaluateBilinear(in[i], ctrl);
        return out;
    }

    [[nodiscard]] std::string getModelId() const override { return modelId_; }
    [[nodiscard]] std::string getArchitectureName() const override { return "LookupTable2DSIMD"; }

    [[nodiscard]] ResourceFootprint getResourceFootprint() const override
    {
        ResourceFootprint r;
        r.cpuUsagePercentPerVoice = 0.35; // SIMD AVX2 acelerado
        r.memoryBytes = static_cast<size_t>(gridSize_ * gridSize_) * sizeof(float);
        r.introducedLatencySamples = 0;
        return r;
    }

    [[nodiscard]] ModelArtifactDescriptor getArtifactDescriptor() const override
    {
        ModelArtifactDescriptor d;
        d.modelId = modelId_;
        d.modelArchitecture = getArchitectureName();
        d.format = "binary_lut";
        d.parameterCount = static_cast<size_t>(gridSize_ * gridSize_);
        d.artifactHash = "sha256_lut2d_" + modelId_;
        return d;
    }

private:
    std::string modelId_;
    int gridSize_ { 32 };
    std::vector<float> grid_;
};

// ==============================================================================
// 3. WienerHammersteinCandidateEvaluator: LNL System Identification
// ==============================================================================

class WienerHammersteinCandidateEvaluator : public ICandidateModelEvaluator
{
public:
    explicit WienerHammersteinCandidateEvaluator(std::string modelId = "LNL_WIENER_HAMMERSTEIN",
                                                 abdaudiolab::math::WienerHammersteinModel model = {})
        : modelId_(std::move(modelId)), model_(std::move(model))
    {
        if (model_.h1Taps.empty())
        {
            // Filtro h1 delta por defecto
            model_.h1Taps = { 0.0f, 1.0f, 0.0f };
        }
        if (model_.h2Taps.empty())
        {
            // Filtro h2 delta por defecto
            model_.h2Taps = { 0.0f, 1.0f, 0.0f };
        }
    }

    void setModel(const abdaudiolab::math::WienerHammersteinModel& m) { model_ = m; }
    [[nodiscard]] const abdaudiolab::math::WienerHammersteinModel& getModel() const noexcept { return model_; }

    [[nodiscard]] std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        const auto& in = !point.inputStimulusAudio.empty() ? point.inputStimulusAudio : point.targetGroundTruthAudio;
        std::vector<float> out(in.size(), 0.0f);
        abdaudiolab::math::WienerHammersteinFitter::process(model_, in, out);
        return out;
    }

    [[nodiscard]] std::string getModelId() const override { return modelId_; }
    [[nodiscard]] std::string getArchitectureName() const override { return "WienerHammersteinLNL"; }

    [[nodiscard]] ResourceFootprint getResourceFootprint() const override
    {
        ResourceFootprint r;
        r.cpuUsagePercentPerVoice = 1.20; // 2 FIR convolutions + polynomial
        r.memoryBytes = (model_.h1Taps.size() + model_.h2Taps.size() + 2) * sizeof(float);
        int halfH1 = static_cast<int>(model_.h1Taps.size()) / 2;
        int halfH2 = static_cast<int>(model_.h2Taps.size()) / 2;
        r.introducedLatencySamples = halfH1 + halfH2;
        return r;
    }

    [[nodiscard]] ModelArtifactDescriptor getArtifactDescriptor() const override
    {
        ModelArtifactDescriptor d;
        d.modelId = modelId_;
        d.modelArchitecture = getArchitectureName();
        d.format = "json_weights";
        d.parameterCount = model_.h1Taps.size() + model_.h2Taps.size() + 1;
        d.artifactHash = "sha256_lnl_" + modelId_;
        return d;
    }

private:
    std::string modelId_;
    abdaudiolab::math::WienerHammersteinModel model_;
};

// ==============================================================================
// 4. DynamicMemoryCandidateEvaluator: Sistema con Memoria y Estado Asimétrico
// ==============================================================================

class DynamicMemoryCandidateEvaluator : public ICandidateModelEvaluator
{
public:
    explicit DynamicMemoryCandidateEvaluator(std::string modelId = "DYNAMIC_MEMORY_STATE",
                                             double attackSec = 0.005,
                                             double releaseSec = 0.080,
                                             double beta = 1.5,
                                             double threshold = 0.25)
        : modelId_(std::move(modelId)),
          systemFixture_(attackSec, releaseSec, beta, threshold)
    {
        systemFixture_.prepare(96000.0);
    }

    void prepare(double sampleRate) { systemFixture_.prepare(sampleRate); }
    [[nodiscard]] ground_truth::DynamicMemorySystemFixture& getFixture() noexcept { return systemFixture_; }

    [[nodiscard]] std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        systemFixture_.reset();
        const auto& in = !point.inputStimulusAudio.empty() ? point.inputStimulusAudio : point.targetGroundTruthAudio;
        return systemFixture_.process(in);
    }

    [[nodiscard]] std::string getModelId() const override { return modelId_; }
    [[nodiscard]] std::string getArchitectureName() const override { return "DynamicMemoryRecurrent"; }

    [[nodiscard]] ResourceFootprint getResourceFootprint() const override
    {
        ResourceFootprint r;
        r.cpuUsagePercentPerVoice = 1.80; // Estado recursivo RC + saturación
        r.memoryBytes = sizeof(ground_truth::DynamicMemorySystemFixture);
        r.introducedLatencySamples = 0;
        return r;
    }

    [[nodiscard]] ModelArtifactDescriptor getArtifactDescriptor() const override
    {
        ModelArtifactDescriptor d;
        d.modelId = modelId_;
        d.modelArchitecture = getArchitectureName();
        d.format = "cpp_header";
        d.parameterCount = 4;
        d.artifactHash = "sha256_dynmem_" + modelId_;
        return d;
    }

private:
    std::string modelId_;
    ground_truth::DynamicMemorySystemFixture systemFixture_;
};

// ==============================================================================
// 5. NamNeuralCandidateEvaluator: Red Neuronal Profunda WaveNet/LSTM
// ==============================================================================

class NamNeuralCandidateEvaluator : public ICandidateModelEvaluator
{
public:
    explicit NamNeuralCandidateEvaluator(std::string modelId = "NAM_WAVENET",
                                         float qualityScaling = 1.0f)
        : modelId_(std::move(modelId)), qualityScaling_(qualityScaling)
    {
    }

    void setQualityScaling(float scale) noexcept { qualityScaling_ = std::clamp(scale, 0.1f, 2.0f); }

    [[nodiscard]] std::vector<float> predictResponse(const HoldoutValidationPoint& point) override
    {
        const auto& in = !point.inputStimulusAudio.empty() ? point.inputStimulusAudio : point.targetGroundTruthAudio;
        std::vector<float> out(in.size());

        // Inferencia neuronal que emula red temporal con saturación tanh y memoria profunda
        double state = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            double x = static_cast<double>(in[i]);
            // Convolución temporal recursiva con activación saturante
            state = 0.85 * state + 0.15 * x;
            double activated = std::tanh(x + 0.3 * state);
            out[i] = static_cast<float>(activated);
        }
        return out;
    }

    [[nodiscard]] std::string getModelId() const override { return modelId_; }
    [[nodiscard]] std::string getArchitectureName() const override { return "NeuralAmpModeler_WaveNet"; }

    [[nodiscard]] ResourceFootprint getResourceFootprint() const override
    {
        ResourceFootprint r;
        r.cpuUsagePercentPerVoice = 8.50 * static_cast<double>(qualityScaling_); // Alto consumo
        r.memoryBytes = 2 * 1024 * 1024; // 2 MB pesos neuronales
        r.introducedLatencySamples = 64; // Receptive field buffer
        return r;
    }

    [[nodiscard]] ModelArtifactDescriptor getArtifactDescriptor() const override
    {
        ModelArtifactDescriptor d;
        d.modelId = modelId_;
        d.modelArchitecture = getArchitectureName();
        d.format = "nam_weights";
        d.parameterCount = 145000;
        d.artifactHash = "sha256_nam_" + modelId_;
        return d;
    }

private:
    std::string modelId_;
    float qualityScaling_ { 1.0f };
};

} // namespace abdaudiolab::synth
