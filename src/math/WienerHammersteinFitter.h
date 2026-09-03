/**
 * @file WienerHammersteinFitter.h
 * @brief System identification of Wiener-Hammerstein (Linear-NonLinear-Linear) models.
 * @details Implements gradient-based optimization with backpropagation and Adam optimizer
 *          based on Takeo Sasai et al. (Optics Express 2020 / arXiv:2012.08046v1).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include <cmath>
#include <string>

namespace abdaudiolab::math
{

/**
 * @struct WienerHammersteinModel
 * @brief Encapsulates the parameters of a fitted LNL system.
 */
struct WienerHammersteinModel
{
    std::vector<float> h1Taps;           /**< Linear input FIR filter impulse response. */
    float nonLinearCoeffA { 0.0f };      /**< 3rd-order static nonlinearity coefficient: f(u) = u + a * u^3. */
    std::vector<float> h2Taps;           /**< Linear output FIR filter impulse response. */

    float preFilterCentroidHz { 0.0f };  /**< Spectral centroid / cutoff estimation of h1. */
    float postFilterCentroidHz { 0.0f }; /**< Spectral centroid / cutoff estimation of h2. */
    float goodnessOfFitR2 { 0.0f };      /**< Coefficient of determination R^2 [0.0, 1.0]. */
    float residualErrorRms { 0.0f };     /**< Root-mean-square error between model and target. */
    int iterationsRun { 0 };             /**< Number of gradient descent iterations performed. */
};

/**
 * @struct WienerHammersteinConfig
 * @brief Configuration settings for the optimizer.
 */
struct WienerHammersteinConfig
{
    int numTapsH1 { 31 };                /**< Number of taps for pre-filter (odd number recommended). */
    int numTapsH2 { 31 };                /**< Number of taps for post-filter. */
    int maxEpochs { 120 };               /**< Maximum training epochs. */
    float learningRate { 0.015f };       /**< Adam learning rate. */
    float beta1 { 0.9f };                /**< Adam first moment decay. */
    float beta2 { 0.999f };              /**< Adam second moment decay. */
    float epsilon { 1e-8f };             /**< Numerical stability epsilon. */
    float tolerance { 1e-5f };           /**< Early stopping loss delta tolerance. */
};

/**
 * @class WienerHammersteinFitter
 * @brief Supervised optimizer that learns h1, a, and h2 from input/output audio pairs.
 */
class WienerHammersteinFitter
{
public:
    explicit WienerHammersteinFitter(WienerHammersteinConfig config = {});

    /**
     * @brief Fit Wiener-Hammerstein model to match inputSignal -> targetSignal.
     * @param inputSignal Excitation signal buffer x[n].
     * @param targetSignal Measured hardware response buffer y_hat[n].
     * @param sampleRate Sampling rate in Hz.
     * @return Fitted model structure with FIR taps, a, and diagnostics.
     */
    WienerHammersteinModel fit(const std::vector<float>& inputSignal,
                              const std::vector<float>& targetSignal,
                              double sampleRate);

    /**
     * @brief Run inference through an existing model.
     * @param model Model parameters.
     * @param input Input signal buffer.
     * @param output Output buffer (resized to input size).
     */
    static void process(const WienerHammersteinModel& model,
                        const std::vector<float>& input,
                        std::vector<float>& output);

    /**
     * @brief Compute spectral centroid of a discrete FIR filter.
     */
    static float calculateCentroidHz(const std::vector<float>& firTaps, double sampleRate);

private:
    WienerHammersteinConfig config;

    static void convolveSame(const std::vector<float>& x,
                             const std::vector<float>& h,
                             std::vector<float>& y);

    static void correlateValid(const std::vector<float>& signal,
                               const std::vector<float>& error,
                               int filterTaps,
                               std::vector<float>& gradH);
};

} // namespace abdaudiolab::math
