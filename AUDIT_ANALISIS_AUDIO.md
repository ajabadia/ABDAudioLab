# Audit de Análisis de Audio — ABDAudioLab

Evaluación de cada función de análisis: algoritmo, corrección, y soluciones propuestas.

---

## Resumen

| Calidad | Cantidad | Nota |
|---------|----------|------|
| ✅ Profesional | 12 | Algoritmos correctos, estándar de la industria |
| ⚠️ Aproximación | 5 | Funcional pero con limitaciones conocidas |
| ❌ Fake | 5 | Datos hardcodeados o sintéticos |

---

## ✅ Funciones Profesionales

### 1. `calculateStatistics` — `LabAnalyticEngine.cpp:9`
- **Algoritmo:** Media + desviación estándar con corrección de Bessel (N-1)
- **Veredicto:** Correcto. Estimador insesgado de varianza.

### 2. `analyzeFilterPasses` — `LabAnalyticEngine.cpp:42`
- **Algoritmo:** Deconvolución Farina por pass + agregación estadística multi-pass
- **Veredicto:** Correcto. Cada pass se deconvoluciona independientemente, se extrae curva de frecuencia, y se promedia.

### 3. `analyzeDelayImpulses` — `LabAnalyticEngine.cpp:192`
- **Algoritmo:** Búsqueda de pico con guarda de 5ms para evitar señal directa
- **Veredicto:** Correcto. Método estándar de medición de latencia.

### 4. `analyzeWaveShaperRamps` (THD) — `LabAnalyticEngine.cpp:250`
- **Algoritmo:** Goertzel DFT en H1 (1kHz) + H2–H5, `THD = sqrt(ΣHn²) / H1 × 100%`
- **Veredicto:** Correcto. Goertzel es textbook-correct para DFT de bin único.

### 5. `analyzeGainTones` (ganancia) — `LabAnalyticEngine.cpp:300`
- **Algoritmo:** RMS → `20·log10(rms)` = ganancia en dBFS
- **Veredicto:** Correcto. Medición estándar de ganancia.

### 6. `analyzeCyclicModulator` (envolvente) — `LabAnalyticEngine.cpp:362`
- **Algoritmo:** Rectificador + LPF 30Hz (adecuado para LFOs ≤15Hz)
- **Veredicto:** Correcto.

### 7. `analyzeCyclicModulator` (rate) — `LabAnalyticEngine.cpp:373`
- **Algoritmo:** Autocorrelación bruta en envolvente, búsqueda de lags 0.5–30Hz
- **Veredicto:** Correcto. Autocorrelación es el método estándar de detección de periodicidad.

### 8. `analyzeCyclicModulator` (depth) — `LabAnalyticEngine.cpp:403`
- **Algoritmo:** `(peak - valley) / peak × 100%`
- **Veredicto:** Correcto. Definición estándar de profundidad de modulación.

### 9. `generateLogFarinaSweep` — `FarinaDeconvolver.cpp:13`
- **Algoritmo:** Fórmula exacta de Farina (2000): `phase(t) = K·(exp(t/L) - 1)`
- **Veredicto:** Correcto. Paper AES 2000, implementación fiel.

### 10. `generateInverseFilter` — `FarinaDeconvolver.cpp:36`
- **Algoritmo:** Sweep temporal invertido con envolvente exponencial `-6dB/oct`
- **Veredicto:** Correcto. Filtro inverso estándar de Farina.

### 11. `deconvolve` (FFT) — `FarinaDeconvolver.cpp:88`
- **Algoritmo:** Convolución FFT radix-2, zero-padding, normalización `1/N`
- **Veredicto:** Correcto. JUCE `dsp::FFT` con normalización adecuada.

### 12. `computeFrequencyResponse` — `FarinaDeconvolver.cpp:187`
- **Algoritmo:** FFT 4096pt, ventana Hann, magnitud en dB
- **Veredicto:** Correcto. JUCE aplica normalización internamente.

---

## ⚠️ Aproximaciones

### 1. `calculateSignalToNoiseRatioDb` — `LabAnalyticEngine.cpp:331`
- **Algoritmo:** `signalRmsDb - baselineNoiseRmsDb` (ruido asumido, no medido)
- **Problema:** No es SNR real. El buffer contiene señal+ruido, así que el RMS incluye ambos → sobreestima SNR.
- **Solución:**
```cpp
// Opción A: Medir ruido en segmentos de silencio del buffer
// Opción B: Usar notch filter en la fundamental para separar ruido
float measureTrueSnr(const float* buffer, int numSamples, float freqHz, double sampleRate)
{
    // 1. Calcular energía total
    float totalEnergy = computeRms(buffer, numSamples);

    // 2. Implementar notch notch en freqHz (filtro stop-band estrecho)
    // 3. Calcular energía del notch-filtered = noise energy
    float noiseEnergy = computeNotchedRms(buffer, numSamples, freqHz, sampleRate);

    // 4. SNR = 10·log10((totalEnergy² - noiseEnergy²) / noiseEnergy²)
    float signalOnly = std::sqrt(std::max(0.0f, totalEnergy * totalEnergy - noiseEnergy * noiseEnergy));
    return 20.0f * std::log10(signalOnly / std::max(noiseEnergy, 1e-10f));
}
```

### 2. `analyzeAdsrEnvelopes` — `LabAnalyticEngine.cpp:82`
- **Algoritmo:** Rectificador + LPF 100Hz + detección por umbrales
- **Problema:** El comentario dice "Hilbert" pero es rectificador (menos preciso). El trigger de release se asume en `sustainSearchEnd` (500ms post-peak), no en un evento MIDI real.
- **Solución:**
```cpp
// Mejora 1: Comentar correctamente (es envelope follower, no Hilbert)
// Mejora 2: Detectar release por derivada negativa sostenida, no por tiempo fijo
float releaseThreshold = peakValue * 0.608f; // -60dB relativo al pico
int releaseStart = sustainEndIdx;
for (int i = sustainEndIdx; i < numSamples - 1; ++i)
{
    if (envelope[i] < releaseThreshold && envelope[i] < envelope[i + 1])
    {
        releaseStart = i;
        break;
    }
}
```

### 3. `analyzeWaveShaperRamps` (curva transferencia) — `LabAnalyticEngine.cpp:236`
- **Algoritmo:** Nearest-sample lookup en 128 puntos
- **Problema:** Sin interpolación — pierde precisión entre muestras.
- **Solución:**
```cpp
// Reemplazar nearest-sample por interpolación lineal
float idxFloat = static_cast<float>(i) / 127.0f * static_cast<float>(numSamples - 1);
int idx0 = static_cast<int>(idxFloat);
float frac = idx0 < numSamples - 1 ? (idxFloat - static_cast<float>(idx0)) : 0.0f;
float inVal = static_cast<float>(i) / 127.0f;
float outVal = buffer[idx0] * (1.0f - frac) + buffer[idx0 + 1] * frac;
```

### 4. `deconvolve` (ventana IR) — `FarinaDeconvolver.cpp:133`
- **Algoritmo:** Ventana fija de 4096 muestras, 256 pre-pico
- **Problema:** A 96kHz = 42ms. No se adapta a sample rate ni a la naturaleza del IR.
- **Solución:**
```cpp
// Hacer la ventana proporcional al sample rate
int windowSamples = static_cast<int>(sampleRate * 0.05); // 50ms
int prePeak = static_cast<int>(sampleRate * 0.003);      // 3ms pre-pico
int startIdx = juce::jmax(0, peakIdx - prePeak);
int endIdx = juce::jmin(fftSize / 2, startIdx + windowSamples);
```

### 5. `deconvolve` (THD) — `FarinaDeconvolver.cpp:150`
- **Algoritmo:** Farina harmonic offset para H2 y H3
- **Problema:** Solo mide H2 y H3. wave shapers con H4+ significativos se subestiman.
- **Solución:**
```cpp
// Extender a H2–H5 usando la fórmula de Farina para cada armónico
for (int h = 2; h <= 5; ++h)
{
    double dt_h = static_cast<double>(sweepDuration) * std::log(static_cast<double>(h)) / logRatio;
    int offset = static_cast<int>(dt_h * sampleRate);
    // medir energía en ±window alrededor de peakIdx - offset
}
```

---

## ❌ Fakes

### 1. Asimetría en `analyzeCyclicModulator` — `LabAnalyticEngine.cpp:412`
```cpp
asymmetries.push_back(0.02f); // ← SIEMPRE 0.02, nunca se calcula
```
**Solución:**
```cpp
// Calcular asimetría real: ratio entre pendiente de subida y bajada de la envolvente
float risingSlope = 0.0f, fallingSlope = 0.0f;
int risingCount = 0, fallingCount = 0;

for (int i = peakIdx + 1; i < valleyIdx; ++i)
{
    float diff = envelope[i + 1] - envelope[i];
    if (diff > 0) { risingSlope += diff; ++risingCount; }
    else          { fallingSlope += std::abs(diff); ++fallingCount; }
}

float avgRising = (risingCount > 0) ? risingSlope / risingCount : 0.0f;
float avgFalling = (fallingCount > 0) ? fallingSlope / fallingCount : 0.0f;
float asymmetry = (avgRising + avgFalling > 1e-10f)
    ? std::abs(avgRising - avgFalling) / (avgRising + avgFalling)
    : 0.0f;
asymmetries.push_back(asymmetry);
```

### 2. Manifest `noiseFloorRmsDb` — `ProfilingSequencer.cpp:422`
```cpp
manifest.noiseFloorRmsDb = -80.0f; // ← hardcodeado
```
**Solución:**
```cpp
// Calcular del primer pass de mediciones (o de un pass de ruido dedicado)
float minRmsDb = 0.0f;
for (const auto& pt : measuredPoints)
{
    float rmsDb = 20.0f * std::log10(std::max(pt.rmsLevel, 1e-10f));
    if (rmsDb < minRmsDb || minRmsDb == 0.0f)
        minRmsDb = rmsDb;
}
manifest.noiseFloorRmsDb = (minRmsDb == 0.0f) ? -80.0f : minRmsDb;
```

### 3. Manifest `averageSnrDb` — `ProfilingSequencer.cpp:423`
```cpp
manifest.averageSnrDb = 32.5f; // ← hardcodeado
```
**Solución:**
```cpp
// Promediar SNR de todos los measured points que tengan SNR calculado
float sumSnr = 0.0f;
int snrCount = 0;
for (const auto& pt : measuredPoints)
{
    if (pt.signalToNoiseDb > 0.0f)
    {
        sumSnr += pt.signalToNoiseDb;
        ++snrCount;
    }
}
manifest.averageSnrDb = (snrCount > 0) ? sumSnr / snrCount : 32.5f; // fallback al hardcodeado
```

### 4. Curva de frecuencia en export HTML — `CertificationReportExporter.cpp:192`
```cpp
mags[i] = -6.0f * std::sin(norm * 3.14159f * 4.0f) - norm * 12.0f; // ← FICTICIO
```
**Solución:**
```cpp
// Usar datos reales de measuredPoints
// 1. Agrupar por frequencyHz (promediar passes)
// 2. Usar magnitudeDb directamente
std::map<float, float> freqMap; // frequency → avg magnitude
for (const auto& pt : measuredPoints)
{
    float freq = pt.frequencyHz;
    freqMap[freq] += pt.magnitudeDb;
}
for (auto& [f, m] : freqMap)
    m /= static_cast<float>(measuredPoints.size() / freqMap.size());

// 3. Renderizar freqMap en el SVG en vez de la curva sintética
```

### 5. Badge "CERTIFIED PASSED" — `CertificationReportExporter.cpp:240`
```cpp
file << "    <div class=\"badge\">CERTIFIED PASSED</div>\n"; // ← SIEMPRE PASA
```
**Solución:**
```cpp
// Evaluar resultados contra thresholds del hardware contract
bool passed = true;
for (const auto& pt : measuredPoints)
{
    if (pt.thdPercent > 1.0f) passed = false;  // THD > 1%
    if (pt.magnitudeDb < -3.0f) passed = false; // Atenuación > 3dB
    if (pt.signalToNoiseDb < 20.0f) passed = false; // SNR < 20dB
}

std::string badgeClass = passed ? "badge-pass" : "badge-fail";
std::string badgeText = passed ? "CERTIFIED PASSED" : "CERTIFIED FAILED";
file << "    <div class=\"" << badgeClass << "\">" << badgeText << "</div>\n";
```

---

## Verificación contra `docs/MATHEMATICAL_MODELS.md`

Cada sección del documento matemático verificada contra el código fuente.

### §1 Farina Sweep — ✅ IMPLEMENTACIÓN EXACTA

| Ecuación (doc) | Código | Estado |
|----------------|--------|--------|
| `x(t) = sin[K·(exp(t/L) - 1)]` | `FarinaDeconvolver.cpp:29` | ✅ Coincidencia exacta |
| `K = ω₁T / ln(ω₂/ω₁)` | `FarinaDeconvolver.cpp:23` | ✅ Coincidencia exacta |
| `L = T / ln(ω₂/ω₁)` | `FarinaDeconvolver.cpp:24` | ✅ Coincidencia exacta |
| `f(t) = x(T-t)·exp(-t·ln(ω₂/ω₁)/T)` | `FarinaDeconvolver.cpp:53-60` | ✅ Coincidencia exacta |
| `Δtₙ = T·ln(N)/ln(ω₂/ω₁)` | `FarinaDeconvolver.cpp:153-154` | ✅ Coincidencia exacta |
| `THD = √(ΣE(hₙ)/E(h_linear)) × 100%` | `FarinaDeconvolver.cpp:180` | ✅ Coincidencia exacta |

**Nota:** El doc define THD como Σ desde N=2 hasta M, pero el código solo calcula H2+H3 (M=2). La ecuación del doc es correcta; la implementación es una aproximación con M=2.

### §2 Wiener-Hammerstein (LNL) — ✅ IMPLEMENTADO (Sasai et al. 2020)

Implementado en C++20 en [`WienerHammersteinFitter.h`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/math/WienerHammersteinFitter.h) y [`LabAnalyticEngine.cpp`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/math/LabAnalyticEngine.cpp) siguiendo el paper canónico de Takeo Sasai et al. (Optics Express / arXiv:2012.08046v1):
- Modelo FIR $h_1$ ($K_1$ taps) $\rightarrow$ No-linealidad estática $f(u) = u + a \cdot u^3$ $\rightarrow$ FIR $h_2$ ($K_2$ taps).
- Optimización simultánea mediante retropropagación analítica y optimizador Adam ($\beta_1=0.9, \beta_2=0.999$).
- Extracción de métricas: Coeficiente no lineal $a$, bondad de ajuste $R^2$, RMSE y centroides espectrales en Hz.
- Validado mediante test unitario con Catch2 (`src/tests/test_WienerHammerstein.cpp`).

### §3 Estadística (μ, σ) — ✅ IMPLEMENTACIÓN EXACTA

| Ecuación (doc) | Código | Estado |
|----------------|--------|--------|
| `μ = (1/P)·Σxᵢ` | `LabAnalyticEngine.cpp:26` | ✅ Coincidencia exacta |
| `σ = √(1/(P-1)·Σ(xᵢ-μ)²)` | `LabAnalyticEngine.cpp:36` | ✅ Coincidencia exacta (Bessel N-1) |

### §4 Pre-Roll de 3 Tonos — ✅ IMPLEMENTACIÓN EXACTA

| Ecuación (doc) | Código | Estado |
|----------------|--------|--------|
| 3 ráfagas de 40ms ON + 40ms OFF | `LabStimulusGenerator.cpp:94-98` | ✅ `cycleTime=0.080`, `cycleIdx<3`, `tInCycle<0.040` |
| Frecuencia 1 kHz | `LabStimulusGenerator.cpp:100` | ✅ `twoPi * 1000.0 * t` |
| Envolvente Hann 5ms | `LabStimulusGenerator.cpp:103-106` | ✅ `0.5*(1-cos(π·t/0.005))` exacto |
| Amplitud -3 dBfs | `LabStimulusGenerator.cpp:108` | ✅ `sin(phase) * 0.707` |

### Resumen de verificación matemática

| Sección | Doc | Código | Estado |
|---------|-----|--------|--------|
| §1 Farina Sweep | Ecuaciones exactas | Implementación fiel | ✅ |
| §2 Wiener-Hammerstein | Modelo LNL (Sasai et al.) | Implementación Adam LNL | ✅ |
| §3 Estadística μ, σ | Fórmulas estándar | Implementación exacta | ✅ |
| §4 Pre-Roll 3 Tonos | Secuencia temporal | Implementación exacta | ✅ |

---

## Resumen de impacto

| Categoría | Funciones | Impacto |
|-----------|-----------|---------|
| Core de análisis | 12 prof. + 3 aprox. | ✅ Sólido — algoritmos correctos |
| Export de reportes | 4 fakes | ❌ Los reportes HTML muestran datos ficticios |
| Manifest de sesión | 2 fakes | ❌ Metadata del session package incorrecta |
| THD export | 1 aprox. | ⚠️ Subestima H4+ |

**Prioridad de arreglo:**
1. 🔴 Export HTML — curva sintética + badge siempre "PASSED" (los clientes ven esto)
2. 🔴 Manifest — noiseFloor y SNR hardcodeados (afecta el .json del package)
3. 🟠 Asimetría LFO — fake pero bajo impacto (pocos clientes miden esto)
4. 🟡 THD H2–H3 → H2–H5 (mejora de precisión)
5. 🟡 SNR real con notch filter (mejora de precisión)
