#pragma once

#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <memory>
#include <cstddef>
#include <numeric>

namespace abdaudiolab::synth::ground_truth
{

/**
 * @brief Métricas analíticas estándar para comparación de sistemas sintéticos.
 */
struct GroundTruthMetrics
{
    static double computeEsrDb(const std::vector<float>& target, const std::vector<float>& candidate)
    {
        const size_t n = std::min(target.size(), candidate.size());
        if (n == 0) return 0.0;

        double sumErrSq = 0.0;
        double sumTargetSq = 0.0;

        for (size_t i = 0; i < n; ++i)
        {
            double diff = static_cast<double>(target[i]) - static_cast<double>(candidate[i]);
            sumErrSq += diff * diff;
            sumTargetSq += static_cast<double>(target[i]) * static_cast<double>(target[i]);
        }

        if (sumErrSq < 1e-15) return -180.0; // Prácticamente nulo
        if (sumTargetSq < 1e-15) return 0.0;

        return 10.0 * std::log10(sumErrSq / sumTargetSq);
    }

    static double computeRmse(const std::vector<float>& target, const std::vector<float>& candidate)
    {
        const size_t n = std::min(target.size(), candidate.size());
        if (n == 0) return 0.0;

        double sumErrSq = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            double diff = static_cast<double>(target[i]) - static_cast<double>(candidate[i]);
            sumErrSq += diff * diff;
        }

        return std::sqrt(sumErrSq / static_cast<double>(n));
    }

    static double computeRSquared(const std::vector<float>& target, const std::vector<float>& candidate)
    {
        const size_t n = std::min(target.size(), candidate.size());
        if (n < 2) return 1.0;

        double meanTarget = 0.0;
        for (size_t i = 0; i < n; ++i)
            meanTarget += target[i];
        meanTarget /= static_cast<double>(n);

        double ssTot = 0.0;
        double ssRes = 0.0;

        for (size_t i = 0; i < n; ++i)
        {
            double dTarget = static_cast<double>(target[i]) - meanTarget;
            double diff = static_cast<double>(target[i]) - static_cast<double>(candidate[i]);
            ssTot += dTarget * dTarget;
            ssRes += diff * diff;
        }

        if (ssTot < 1e-15) return 1.0;
        return 1.0 - (ssRes / ssTot);
    }
};

/**
 * @class PureLinearBypassFixture
 * @brief Módulo 1: Bypass Lineal Puro y canal de retardo sin dispersión.
 *
 * Modelo analítico: y[n] = g * x[n - tau]
 * Comprueba error residual nulo (ESR -> -inf dB) frente a modelos lineales ideales.
 */
class PureLinearBypassFixture
{
public:
    explicit PureLinearBypassFixture(double gain = 1.0, int delaySamples = 0)
        : gain_(gain), delaySamples_(std::max(0, delaySamples))
    {
        reset();
    }

    void setGain(double gain) noexcept { gain_ = gain; }
    [[nodiscard]] double getGain() const noexcept { return gain_; }

    void setDelaySamples(int delay) noexcept
    {
        delaySamples_ = std::max(0, delay);
        delayLine_.assign(static_cast<size_t>(delaySamples_ + 1), 0.0f);
        writeIndex_ = 0;
    }
    [[nodiscard]] int getDelaySamples() const noexcept { return delaySamples_; }

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 96000.0;
        reset();
    }

    void reset()
    {
        delayLine_.assign(static_cast<size_t>(delaySamples_ + 1), 0.0f);
        writeIndex_ = 0;
    }

    void process(const float* in, float* out, size_t numSamples)
    {
        if (delaySamples_ == 0)
        {
            float g = static_cast<float>(gain_);
            for (size_t i = 0; i < numSamples; ++i)
                out[i] = in[i] * g;
            return;
        }

        const size_t lineSize = delayLine_.size();
        float g = static_cast<float>(gain_);

        for (size_t i = 0; i < numSamples; ++i)
        {
            delayLine_[writeIndex_] = in[i];
            size_t readIndex = (writeIndex_ + lineSize - static_cast<size_t>(delaySamples_)) % lineSize;
            out[i] = delayLine_[readIndex] * g;
            writeIndex_ = (writeIndex_ + 1) % lineSize;
        }
    }

    std::vector<float> process(const std::vector<float>& input)
    {
        std::vector<float> output(input.size());
        process(input.data(), output.data(), input.size());
        return output;
    }

private:
    double gain_ { 1.0 };
    int delaySamples_ { 0 };
    double sampleRate_ { 96000.0 };
    std::vector<float> delayLine_ { 0.0f };
    size_t writeIndex_ { 0 };
};

/**
 * @enum TptFilterType
 * @brief Tipología de filtro SVF TPT/ZDF.
 */
enum class TptFilterType
{
    Lowpass,
    Highpass,
    Bandpass
};

/**
 * @enum TptFilterSlope
 * @brief Orden y pendiente del filtro SVF TPT/ZDF.
 */
enum class TptFilterSlope
{
    Order2_12dB,
    Order4_24dB
};

/**
 * @class TptZdfFilterFixture
 * @brief Módulo 2: Filtro IIR de 2º y 4º orden Topology-Preserving Transform (ZDF).
 *
 * Implementa un State Variable Filter (SVF) bilineal con realimentación instantánea (Zero-Delay Feedback).
 * Comportamiento analítico LTI exacto, sin armónicos espurios y con magnitud y fase deterministas.
 */
class TptZdfFilterFixture
{
public:
    TptZdfFilterFixture(double cutoffHz = 1000.0,
                        double resonanceQ = 0.70710678,
                        TptFilterType type = TptFilterType::Lowpass,
                        TptFilterSlope slope = TptFilterSlope::Order2_12dB)
        : cutoffHz_(cutoffHz), resonanceQ_(resonanceQ), type_(type), slope_(slope)
    {
        updateCoefficients();
    }

    void setCutoff(double cutoffHz) noexcept
    {
        cutoffHz_ = cutoffHz;
        updateCoefficients();
    }
    [[nodiscard]] double getCutoff() const noexcept { return cutoffHz_; }

    void setResonance(double q) noexcept
    {
        resonanceQ_ = std::max(0.1, q);
        updateCoefficients();
    }
    [[nodiscard]] double getResonance() const noexcept { return resonanceQ_; }

    void setType(TptFilterType type) noexcept { type_ = type; }
    [[nodiscard]] TptFilterType getType() const noexcept { return type_; }

    void setSlope(TptFilterSlope slope) noexcept { slope_ = slope; }
    [[nodiscard]] TptFilterSlope getSlope() const noexcept { return slope_; }

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 96000.0;
        updateCoefficients();
        reset();
    }

    void reset() noexcept
    {
        s1_stage1_ = s2_stage1_ = 0.0;
        s1_stage2_ = s2_stage2_ = 0.0;
    }

    /**
     * @brief Magnitud continua teórica |H(j*omega)| para la etapa analítica.
     */
    [[nodiscard]] double getAnalyticalMagnitude(double freqHz) const noexcept
    {
        if (cutoffHz_ <= 0.0 || freqHz < 0.0) return 0.0;
        double w = freqHz / cutoffHz_;
        double w2 = w * w;
        double damping = 1.0 / resonanceQ_;
        double denom = std::sqrt((1.0 - w2) * (1.0 - w2) + (damping * damping * w2));
        if (denom < 1e-12) return 1e6;

        double magStage = 1.0;
        if (type_ == TptFilterType::Lowpass)
            magStage = 1.0 / denom;
        else if (type_ == TptFilterType::Highpass)
            magStage = w2 / denom;
        else if (type_ == TptFilterType::Bandpass)
            magStage = (damping * w) / denom;

        return (slope_ == TptFilterSlope::Order4_24dB) ? (magStage * magStage) : magStage;
    }

    void process(const float* in, float* out, size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            double sample = static_cast<double>(in[i]);
            double y1 = processSingleStage(sample, s1_stage1_, s2_stage1_);
            if (slope_ == TptFilterSlope::Order4_24dB)
            {
                double y2 = processSingleStage(y1, s1_stage2_, s2_stage2_);
                out[i] = static_cast<float>(y2);
            }
            else
            {
                out[i] = static_cast<float>(y1);
            }
        }
    }

    std::vector<float> process(const std::vector<float>& input)
    {
        std::vector<float> output(input.size());
        process(input.data(), output.data(), input.size());
        return output;
    }

private:
    void updateCoefficients() noexcept
    {
        double clampedCutoff = std::clamp(cutoffHz_, 10.0, sampleRate_ * 0.499);
        g_ = std::tan(std::numbers::pi * clampedCutoff / sampleRate_);
        damping_ = 1.0 / (2.0 * std::max(0.1, resonanceQ_));
        h_ = 1.0 / (1.0 + 2.0 * damping_ * g_ + g_ * g_);
    }

    double processSingleStage(double x, double& s1, double& s2) noexcept
    {
        // Vadim Zavalishin TPT SVF Equations
        double hp = (x - (2.0 * damping_ + g_) * s1 - s2) * h_;
        double v1 = g_ * hp;
        double bp = v1 + s1;
        s1 = bp + v1;
        double v2 = g_ * bp;
        double lp = v2 + s2;
        s2 = lp + v2;

        if (type_ == TptFilterType::Highpass) return hp;
        if (type_ == TptFilterType::Bandpass) return bp;
        return lp;
    }

    double cutoffHz_ { 1000.0 };
    double resonanceQ_ { 0.70710678 };
    TptFilterType type_ { TptFilterType::Lowpass };
    TptFilterSlope slope_ { TptFilterSlope::Order2_12dB };
    double sampleRate_ { 96000.0 };

    double g_ { 0.0 };
    double damping_ { 0.0 };
    double h_ { 0.0 };

    double s1_stage1_ { 0.0 };
    double s2_stage1_ { 0.0 };
    double s1_stage2_ { 0.0 };
    double s2_stage2_ { 0.0 };
};

/**
 * @enum PolynomialSaturatorMode
 * @brief Modalidad de no-linealidad estática sin memoria.
 */
enum class PolynomialSaturatorMode
{
    CubicOnly,       ///< y = x - a * x^3
    CubicQuintic,    ///< y = x - a * x^3 + b * x^5
    NormalizedTanh   ///< y = tanh(k * x) / tanh(k)
};

/**
 * @class StaticPolynomialSaturatorFixture
 * @brief Módulo 3: Saturador Polinómico Estático sin memoria.
 *
 * Modelo analítico: y[n] = x[n] - a * x[n]^3 (+ b * x[n]^5)
 * Cota superior para validación de identificación Wiener-Hammerstein LNL (R^2 > 0.999).
 */
class StaticPolynomialSaturatorFixture
{
public:
    explicit StaticPolynomialSaturatorFixture(double coeffA = 0.15,
                                              double coeffB = 0.02,
                                              PolynomialSaturatorMode mode = PolynomialSaturatorMode::CubicOnly)
        : a_(coeffA), b_(coeffB), mode_(mode)
    {
    }

    void setCoeffA(double a) noexcept { a_ = a; }
    [[nodiscard]] double getCoeffA() const noexcept { return a_; }

    void setCoeffB(double b) noexcept { b_ = b; }
    [[nodiscard]] double getCoeffB() const noexcept { return b_; }

    void setMode(PolynomialSaturatorMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] PolynomialSaturatorMode getMode() const noexcept { return mode_; }

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 96000.0;
    }

    void reset() noexcept {} // Sin memoria interna

    [[nodiscard]] inline float evaluateTransfer(float x) const noexcept
    {
        if (mode_ == PolynomialSaturatorMode::NormalizedTanh)
        {
            double k = std::max(0.01, a_ * 10.0);
            double norm = std::tanh(k);
            return static_cast<float>(std::tanh(k * static_cast<double>(x)) / norm);
        }
        else if (mode_ == PolynomialSaturatorMode::CubicQuintic)
        {
            double xVal = static_cast<double>(x);
            double x2 = xVal * xVal;
            double x3 = xVal * x2;
            double x5 = x3 * x2;
            return static_cast<float>(xVal - a_ * x3 + b_ * x5);
        }
        // CubicOnly: y = x - a * x^3
        double xVal = static_cast<double>(x);
        return static_cast<float>(xVal - a_ * (xVal * xVal * xVal));
    }

    void process(const float* in, float* out, size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = evaluateTransfer(in[i]);
        }
    }

    std::vector<float> process(const std::vector<float>& input)
    {
        std::vector<float> output(input.size());
        process(input.data(), output.data(), input.size());
        return output;
    }

private:
    double a_ { 0.15 };
    double b_ { 0.02 };
    PolynomialSaturatorMode mode_ { PolynomialSaturatorMode::CubicOnly };
    double sampleRate_ { 96000.0 };
};

/**
 * @class DynamicMemorySystemFixture
 * @brief Módulo 4: Sistema Dinámico no lineal con Memoria de Estado e Histéresis.
 *
 * Circuito rectificador/diodo con condensador RC asimétrico de carga/descarga y compresión dinámica.
 * Permite auditar y delimitar la frontera entre modelos estáticos (LUT / LNL) y modelos con memoria dinámica.
 */
class DynamicMemorySystemFixture
{
public:
    DynamicMemorySystemFixture(double attackSec = 0.005,
                               double releaseSec = 0.080,
                               double feedbackBeta = 1.5,
                               double diodeThreshold = 0.25)
        : attackSec_(attackSec), releaseSec_(releaseSec), beta_(feedbackBeta), threshold_(diodeThreshold)
    {
        updateCoeffs();
    }

    void setAttackSec(double sec) noexcept
    {
        attackSec_ = std::max(0.0001, sec);
        updateCoeffs();
    }
    [[nodiscard]] double getAttackSec() const noexcept { return attackSec_; }

    void setReleaseSec(double sec) noexcept
    {
        releaseSec_ = std::max(0.0001, sec);
        updateCoeffs();
    }
    [[nodiscard]] double getReleaseSec() const noexcept { return releaseSec_; }

    void setFeedbackBeta(double beta) noexcept { beta_ = beta; }
    [[nodiscard]] double getFeedbackBeta() const noexcept { return beta_; }

    void setThreshold(double th) noexcept { threshold_ = th; }
    [[nodiscard]] double getThreshold() const noexcept { return threshold_; }

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 96000.0;
        updateCoeffs();
        reset();
    }

    void reset() noexcept
    {
        stateVc_ = 0.0;
    }

    void process(const float* in, float* out, size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            double x = static_cast<double>(in[i]);
            double absX = std::abs(x);

            // Seguidor asimétrico con constante RC de carga/descarga
            if (absX > stateVc_)
                stateVc_ = alphaAttack_ * stateVc_ + (1.0 - alphaAttack_) * absX;
            else
                stateVc_ = alphaRelease_ * stateVc_;

            // Saturación dependiente del estado que introduce histéresis
            double denom = 1.0 + beta_ * stateVc_;
            double vDiff = x;
            if (absX > threshold_)
            {
                double sign = (x > 0.0) ? 1.0 : -1.0;
                vDiff = sign * (threshold_ + (absX - threshold_) / denom);
            }

            out[i] = static_cast<float>(std::tanh(vDiff));
        }
    }

    std::vector<float> process(const std::vector<float>& input)
    {
        std::vector<float> output(input.size());
        process(input.data(), output.data(), input.size());
        return output;
    }

    /**
     * @brief Mide el área del bucle de histéresis en el plano de fase (x, y) ante un seno puro.
     * Retorna > 0 si existe memoria dinámica; colapsa a 0 si beta == 0 (modelo estático).
     */
    [[nodiscard]] double measureHysteresisArea(double freqHz = 100.0, double amplitude = 0.8, int numPeriods = 10)
    {
        reset();
        if (freqHz <= 0.0 || sampleRate_ <= 0.0 || numPeriods < 2) return 0.0;
        const int samplesPerPeriod = static_cast<int>(sampleRate_ / freqHz);
        std::vector<float> sine(static_cast<size_t>(samplesPerPeriod * numPeriods));
        for (size_t n = 0; n < sine.size(); ++n)
        {
            double t = static_cast<double>(n) / sampleRate_;
            sine[n] = static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * freqHz * t));
        }

        std::vector<float> output = process(sine);

        // Integración de línea por regla trapezoidal sobre el último ciclo estacionario: Area = sum(y_avg * dx)
        double loopArea = 0.0;
        size_t start = static_cast<size_t>(samplesPerPeriod * (numPeriods - 1));
        size_t end = sine.size();
        for (size_t i = start + 1; i < end; ++i)
        {
            double inDx = static_cast<double>(sine[i]) - static_cast<double>(sine[i - 1]);
            double yAvg = 0.5 * (static_cast<double>(output[i]) + static_cast<double>(output[i - 1]));
            loopArea += yAvg * inDx;
        }

        return std::abs(loopArea);
    }

private:
    void updateCoeffs() noexcept
    {
        alphaAttack_ = std::exp(-1.0 / (sampleRate_ * attackSec_));
        alphaRelease_ = std::exp(-1.0 / (sampleRate_ * releaseSec_));
    }

    double attackSec_ { 0.005 };
    double releaseSec_ { 0.080 };
    double beta_ { 1.5 };
    double threshold_ { 0.25 };
    double sampleRate_ { 96000.0 };

    double alphaAttack_ { 0.0 };
    double alphaRelease_ { 0.0 };
    double stateVc_ { 0.0 };
};

} // namespace abdaudiolab::synth::ground_truth
