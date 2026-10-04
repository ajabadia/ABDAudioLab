# Especificación Técnica: R5-AUDIO1
**Adaptación de Barrido Farina y Ventana de Evaluación Espectral al Sample Rate ($f_s$)**

**Estado:** Aprobado para implementación  
**Fecha:** 5 de Octubre de 2026  
**Rama:** `main`  
**Base Commit:** `e6e7e74` (Cierre formal de R5-UX1)

---

## 1. Motivación y Principio de Ingeniería

Para cualquier sistema discreto muestreado a $f_s$, el límite de Nyquist es:
$$f_{\text{Nyq}} = \frac{f_s}{2}$$

Las interfaces de audio pueden presentar una atenuación natural y pronunciada en las inmediaciones de $f_{\text{Nyq}}$ debido a sus filtros analógicos antialiasing/reconstrucción y a los filtros digitales de sobremuestreo/diezmado. Exigir que una interfaz evaluada a $f_s = 44,1\text{ kHz}$ mantenga respuesta plana hasta 40 kHz genera un problema doble:
1. Se excitan frecuencias inexistentes o plegadas por aliasing en el dominio digital discreto.
2. La caída natural en la banda de guarda del convertidor provoca una variación aparente de decenas de decibelios ($\Delta_{\text{flatness}} \approx 69\text{ dB}$) que descalifica erróneamente un hardware en perfecto estado.

**Decisión de Ingeniería:**
La aplicación debe desacoplar claramente:
1. **Límite Teórico:** $f_{\text{Nyq}} = f_s / 2$.
2. **Límite Superior del Barrido ($f_{\text{sweep,max}}$):** Hasta dónde se excita y mide la interfaz física.
3. **Banda de Evaluación de Planitud ($[f_{\text{flat,low}}, f_{\text{flat,high}}]$):** Ventana acotada y segura donde se calcula $\Delta_{\text{flatness}}$ para decidir el veredicto PASS/FAIL.

---

## 2. Definición Matemática y Políticas por Sample Rate

### 2.1. Frecuencia Máxima de Barrido
$$f_{\text{sweep,max}} = \min(20000.0\text{ Hz},\; 0.45 \cdot f_s)$$
* Margen de seguridad respecto a Nyquist: $0.45 \cdot f_s = 0.90 \cdot f_{\text{Nyq}}$ (margen del 10%).

### 2.2. Ventana de Evaluación de Planitud
$$f_{\text{flat,low}} = 40.0\text{ Hz}$$
$$f_{\text{flat,high}} = \min(18000.0\text{ Hz},\; 0.40 \cdot f_s)$$
* Margen de evaluación respecto a Nyquist: $0.40 \cdot f_s = 0.80 \cdot f_{\text{Nyq}}$ (margen del 20%).
* La franja entre $f_{\text{flat,high}}$ y $f_{\text{sweep,max}}$ actúa como zona de amortiguación analógica: se mide y se visualiza, pero **no decide** la aceptación del hardware.

### 2.3. Matriz de Comportamiento Nominal
| Sample Rate ($f_s$) | Nyquist ($f_{\text{Nyq}}$) | Barrido Máximo ($f_{\text{sweep,max}}$) | Banda Evaluación Planitud ($[f_{\text{flat,low}}, f_{\text{flat,high}}]$) | Margen Evaluación vs Nyquist |
| :---: | :---: | :---: | :---: | :---: |
| **44.100 Hz** | 22.050 Hz | **19.845 Hz** | **40 Hz – 17.640 Hz** | 4.410 Hz (20%) |
| **48.000 Hz** | 24.000 Hz | **20.000 Hz** | **40 Hz – 18.000 Hz** | 6.000 Hz (25%) |
| **96.000 Hz** | 48.000 Hz | **20.000 Hz** | **40 Hz – 18.000 Hz** | 30.000 Hz (62.5%) |

---

## 3. Arquitectura y Contratos de Código

### 3.1. Helper Puro de Evaluación de Planitud
Para permitir pruebas unitarias directas, herméticas y desacopladas de la deconvolución Farina, se introduce en `LoopbackCalibrator`:

```cpp
namespace abdaudiolab::math
{

struct FlatnessEvaluation
{
    bool hasValidBins { false };
    float deltaDb { 0.0f };
    size_t validBinCount { 0 };
};

class LoopbackCalibrator
{
public:
    [[nodiscard]] static float computeSafeSweepMaxHz(double sampleRate) noexcept;
    [[nodiscard]] static std::pair<float, float> computeFlatnessBandHz(double sampleRate) noexcept;

    [[nodiscard]] static FlatnessEvaluation evaluateFlatnessInBand(
        const std::vector<float>& frequenciesHz,
        const std::vector<float>& magnitudesDb,
        float lowHz,
        float highHz) noexcept;
...
```

### 3.2. Reglas de Validación e Invariantes
1. **Contrato de Bins Válidos:**
   * Si `frequenciesHz.size() != magnitudesDb.size()` $\rightarrow$ `hasValidBins = false`, `deltaDb = std::numeric_limits<float>::infinity()`.
   * Si no existe al menos un bin de frecuencia dentro de $[f_{\text{flat,low}}, f_{\text{flat,high}}]$ $\rightarrow$ `hasValidBins = false`, `deltaDb = std::numeric_limits<float>::infinity()`.
   * **Bajo ninguna circunstancia la ausencia de bins válidos resultará en `deltaDb = 0.0f` ni en un falso PASS.**
2. **Contrato de Calibración Inmutable:**
   ```cpp
   result.frequencyFlatnessDb = flatness.deltaDb;
   result.isCalibrated = (flatness.hasValidBins &&
                          !result.clippingDetected &&
                          result.peakInDbfs > -40.0f &&
                          result.frequencyFlatnessDb < 6.0f);
   ```
3. **Persistencia de la Protección P3A:**
   * El clipping en la entrada analógica/digital continúa invalidando taxativamente la calibración (`isCalibrated == false`).
4. **Coherencia de Frecuencia Final en UI:**
   * `NativeCalibrationPanel` calcula `endFreq = LoopbackCalibrator::computeSafeSweepMaxHz(sr)` una sola vez por medición.
   * Se utiliza ese mismo valor exacto para configurar el generador Farina y como parámetro `endFreqHz` en `analyzeLoopback`.

---

## 4. Plan de Pruebas Unitarias Herméticas

El nuevo archivo de tests `src/tests/test_LoopbackSampleRateAdaptation.cpp` cubrirá:

1. **Pruebas de Límites y Bandas (`computeSafeSweepMaxHz`, `computeFlatnessBandHz`):**
   * Verificación exacta para $f_s \in \{44100, 48000, 96000\}$.
   * Manejo de entradas anómalas: $f_s \le 0.0$ o frecuencias absurdas.
2. **Pruebas del Helper Puro (`evaluateFlatnessInBand`):**
   * Curva perfectamente plana $\rightarrow$ `hasValidBins = true`, `deltaDb = 0.0f`.
   * Caída de $50\text{ dB}$ entre 18.000 Hz y 19.845 Hz a 44,1 kHz (fuera de $[40, 17640]$) $\rightarrow$ `hasValidBins = true`, `deltaDb < 1.0f`.
   * Caída $> 6\text{ dB}$ en 5 kHz (dentro de banda) $\rightarrow$ `hasValidBins = true`, `deltaDb > 6.0f`.
   * Vector vacío o banda sin intersección $\rightarrow$ `hasValidBins = false`, delta infinito.
   * Vectores con tamaños desiguales $\rightarrow$ `hasValidBins = false`, seguro contra excepciones.
3. **Pruebas de Integración con `analyzeLoopback`:**
   * Señal sintética con roll-off de Nyquist simulado a 44.100 Hz $\rightarrow$ `isCalibrated == true`.
   * Señal sintética saturada (clipping) a 44.100 Hz $\rightarrow$ `isCalibrated == false`.
   * Señal con nivel inferior a $-40\text{ dBFS} \rightarrow$ `isCalibrated == false`.

---

## 5. Criterio de Aceptación Física Posterior

Una vez que la suite completa pase en verde:
1. Se compilará `ABDAudioLab.exe`.
2. Se repetirá la calibración con la PreSonus AudioBox USB conectada físicamente (Output 1 ➔ Input 1).
3. Se verificará que el retorno a $-12,5\text{ dBFS}$ y $\approx 25\text{ ms}$ de latencia ya no es rechazado por la caída natural en agudos extremos, alcanzando `State::Success` de forma repetible y estable.
