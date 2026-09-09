#include <catch2/catch_test_macros.hpp>
#include "../math/LabAnalyticEngine.h"

TEST_CASE ("LabAnalyticEngine Plateau Collapse Filter Validation", "[math][manual]")
{
    using namespace abdaudiolab::math;

    std::vector<PreScanPoint> rawSweep;
    const int totalFrames = 100; // Simulación de 100 pasos de telemetría a lo largo de 10s
    
    // Crear una trayectoria manual artificial con errores
    for (int i = 0; i < totalFrames; ++i)
    {
        PreScanPoint pt;
        pt.timeSec = (static_cast<float>(i) / static_cast<float>(totalFrames)) * 10.0f;
        
        // Simular un parón de la mano del operador entre los frames 40 y 55 (Meseta estática)
        if (i >= 40 && i <= 55)
        {
            pt.primaryMetric = 0.45f; // Valor congelado
        }
        else
        {
            // Movimiento lineal ascendente base
            pt.primaryMetric = (static_cast<float>(i) / static_cast<float>(totalFrames));
        }

        // Inyectar micro-fluctuaciones analógicas controladas dentro del rango épsilon (ruido térmico)
        if (i % 2 == 0) pt.primaryMetric += 0.001f;
        else            pt.primaryMetric -= 0.001f;

        rawSweep.push_back(pt);
    }

    // Ejecutar el filtro de colapso de mesetas con una varianza de ruido simulada baja
    float simulatedSigma = 0.0015f; // 3-sigma = 0.0045f (menor que la guarda rígida de 0.005)
    PreScanResult processedResult = LabAnalyticEngine::applyPlateauCollapseFilter(rawSweep, simulatedSigma);

    SECTION ("De-duplication Integrity")
    {
        // El número de puntos purificados debe ser notablemente menor debido al colapso de la meseta
        REQUIRE(processedResult.trajectory.size() < rawSweep.size());
        
        // Verificar que los extremos absolutos se mantuvieron intactos
        REQUIRE(processedResult.trajectory.front().controlValue == 0.0f);
        REQUIRE(processedResult.trajectory.back().controlValue == 127.0f);
    }

    SECTION ("Time Axis Re-mapping Regularity")
    {
        // Comprobar que los puntos resultantes están perfectamente espaciados y ordenados
        for (size_t i = 1; i < processedResult.trajectory.size(); ++i)
        {
            REQUIRE(processedResult.trajectory[i].controlValue > processedResult.trajectory[i-1].controlValue);
            REQUIRE(processedResult.trajectory[i].timeSec > processedResult.trajectory[i-1].timeSec);
        }
    }
}

#include "../audio/LabStimulusGenerator.h"

TEST_CASE ("LabStimulusGenerator Metronome Tick Synthesis Validation", "[audio][metronome]")
{
    using namespace abdaudiolab::audio;

    const double sampleRate = 48000.0;
    const int blockSamples = 2048;
    std::vector<float> buffer(static_cast<size_t>(blockSamples), 0.0f);

    // Render 800 Hz tick at -24 dBFS (linear ≈ 0.063)
    LabStimulusGenerator::renderMetronomeTick(buffer.data(), blockSamples, sampleRate, 0.015, 800.0f, 0.0630957f);

    // 1. Verify non-zero output inside the 15ms window (15ms @ 48kHz = 720 samples)
    float maxAmp = 0.0f;
    for (int i = 0; i < 720; ++i)
    {
        maxAmp = std::max(maxAmp, std::abs(buffer[static_cast<size_t>(i)]));
    }
    REQUIRE(maxAmp > 0.01f);
    REQUIRE(maxAmp <= 0.07f); // Should stay safely around -24 dBFS

    // 2. Verify silence after 15ms
    float tailAmp = 0.0f;
    for (int i = 800; i < blockSamples; ++i)
    {
        tailAmp = std::max(tailAmp, std::abs(buffer[static_cast<size_t>(i)]));
    }
    REQUIRE(tailAmp == 0.0f);
}

