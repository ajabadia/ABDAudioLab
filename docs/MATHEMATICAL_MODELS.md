# Fundamentos Matemáticos y Modelos DSP — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión:** 2.0.0  
**Actualizado:** 2026-09-13  

---

## 1. Técnica de Barrido Senoidal Logarítmico (Farina Sweep)

*(Basado en Angelo Farina, AES 108th Convention, París, 2000 — [134-AES00.pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/134-AES00.pdf))*

### 1.1 Ecuación de la Señal de Excitación
La frecuencia instantánea $\omega(t)$ varía exponencialmente con el tiempo entre $\omega_1 = 2\pi f_1$ y $\omega_2 = 2\pi f_2$ en una duración total $T$:

$$x(t) = \sin\left[ \frac{\omega_1 T}{\ln(\omega_2/\omega_1)} \left( e^{\frac{t}{T} \ln(\omega_2/\omega_1)} - 1 \right) \right]$$

Constantes de escala:
$$K = \frac{\omega_1 T}{\ln(\omega_2/\omega_1)}, \quad L = \frac{T}{\ln(\omega_2/\omega_1)}$$

### 1.2 Filtro Inverso para Deconvolución Lineal
Para comprimir la respuesta del sistema en un impulso Dirac Delta y desacoplar las distorsiones no lineales, se genera el filtro inverso $f(t)$ como la señal de excitación **invertida en el tiempo** y modulada con una atenuación de **$-6\text{ dB/octava}$**:

$$f(t) = x(T - t) \cdot e^{-\frac{t}{T} \ln(\omega_2/\omega_1)}$$

### 1.3 Separación Temporal de Armónicos de Distorsión
Al realizar la convolución en el dominio de la frecuencia mediante FFT:
$$h_{\text{total}}(t) = y(t) \ast f(t) = \text{IFFT}\left( \text{FFT}(y) \cdot \text{FFT}(f) \right)$$

Los armónicos de distorsión no lineal de orden $N$ ($2^{\circ}, 3^{\circ}, 4^{\circ}\dots$) se concentran en impulsos previos al impulso de respuesta lineal principal, con desfases temporales exactos:

$$\Delta t_N = T \cdot \frac{\ln(N)}{\ln(\omega_2 / \omega_1)}$$

* **Ganancia en SNR**: $> 60 \text{ dB}$ frente a impulsos Dirac individuales.
* **Cálculo de Distorsión Armónica Total (THD %)**:
  $$\text{THD} = \sqrt{\frac{\sum_{N=2}^{M} E(h_N)}{E(h_{\text{lineal}})}} \times 100\%$$

---

## 2. Modelado por Bloques Wiener-Hammerstein (LNL)

*(Basado en Takeo Sasai et al., Optics Express 2020 / arXiv:2012.08046v1 — [Wiener-Hammerstein model...pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Wiener-Hammerstein%20model%20and%20its%20learning%20for%20nonlinear%20digital%20pre-distortion%20of%20optical%20transmitters-with-annotations.pdf))*

Los circuitos de hardware analógico, pedales de saturación y modelado ACB se descomponen en cascadas de 3 etapas no conmutativas (**Linear – Nonlinear – Linear**):

$$\text{Entrada } x[n] \longrightarrow \text{Filtro Lineal FIR } h_1[n] \longrightarrow \text{Saturador Estático } f(u) \longrightarrow \text{Filtro Lineal FIR } h_2[n] \longrightarrow \text{Salida } y[n]$$

Donde la no-linealidad estática sin memoria se modela mediante la expansión polinomial cúbica canónica:
$$f(u) = u + a \cdot u^3$$

### 2.1 Algoritmo de Ajuste Supervisado (Backpropagation con Adam)

Implementado en C++20 en `WienerHammersteinFitter`:
1. **Función de Pérdida de Mínimos Cuadrados**:
   $$E = \frac{1}{2N} \sum_{n=0}^{N-1} (y_2[n] - \hat{y}[n])^2$$
2. **Gradientes Analíticos por Regla de la Cadena**:
   * Error de salida: $e[n] = y_2[n] - \hat{y}[n]$
   * Gradiente filtro salida: $\frac{\partial E}{\partial h_2[k]} = \frac{1}{N} \sum_n e[n] \cdot x_2[n - k + M_2]$
   * Retropropagación a no-linealidad: $\frac{\partial E}{\partial x_2[n]} = \sum_k e[n + k - M_2] \cdot h_2[k]$
   * Gradiente del parámetro $a$: $\frac{\partial E}{\partial a} = \frac{1}{N} \sum_n \frac{\partial E}{\partial x_2[n]} \cdot (y_1[n])^3$
   * Retropropagación a filtro entrada: $\frac{\partial E}{\partial y_1[n]} = \frac{\partial E}{\partial x_2[n]} \cdot (1 + 3a \cdot (y_1[n])^2)$
   * Gradiente filtro entrada: $\frac{\partial E}{\partial h_1[k]} = \frac{1}{N} \sum_n \frac{\partial E}{\partial y_1[n]} \cdot x[n - k + M_1]$
3. **Optimizador Adam**:
   * Momentos de primer y segundo orden ($m, v$) con decaimientos $\beta_1 = 0.9, \beta_2 = 0.999$.
   * Tasa de aprendizaje adaptativa $\alpha = 0.015$.
4. **Métricas de Calidad**:
   * Coeficiente de determinación $R^2 = 1 - \frac{\sum (y_2 - \hat{y})^2}{\sum (\hat{y} - \bar{\hat{y}})^2}$.
   * Error cuadrático medio residual (RMSE).

---

## 3. Extracción Estadística Dual: Media ($\mu$) y Desviación Estándar ($\sigma$)

Para cada punto de control $(\text{knob}_1, \text{knob}_2)$, el laboratorio ejecuta $P$ pasadas de medición idénticas ($P \ge 3$) y calcula:

1. **Media Muestral ($\mu$) — Valor Nominal Estable**:
   $$\mu = \frac{1}{P} \sum_{i=1}^{P} x_i$$

2. **Desviación Estándar ($\sigma$) — Factor de Ruido Térmico / Deriva ACB**:
   $$\sigma = \sqrt{\frac{1}{P - 1} \sum_{i=1}^{P} (x_i - \mu)^2}$$

---

## 4. Pre-Roll de Calibración y Sincronización Temporal (Secuencia de 3 Tonos)

Antes de emitir el estímulo principal, se inyecta una secuencia de **3 ráfagas senoidales a 1 kHz** de 40 ms separadas por 40 ms de silencio con ventanas de Hann de 5 ms:
- Marcador de inicio inequívoco ($t_0$) sample-accurate.
- Medición de latencia de ida y vuelta (*round-trip latency*).
- Calibración de ganancia dinámica a $-3\text{ dBFS}$.
- Comprobación de correlación de fase estéreo.

---

## 5. Estimación de Tono y Desplazamiento Sub-muestra (NSDF Parabólico)

*(Implementado en `SynthPitchEstimator.cpp` — Fase 20)*

Para sintetizadores y osciladores donde no existe señal de sincronismo externa, la frecuencia fundamental $f_0$ se estima mediante la **Normalized Square Difference Function (NSDF)**:

$$r_{xx}[n, \tau] = \frac{2 \sum_{j} x[j] x[j + \tau]}{\sum_{j} x[j]^2 + \sum_{j} x[j + \tau]^2}$$

### 5.1 Vértice Parabólico Sub-muestra Corregido
Una vez localizado el primer pico de autocorrelación significativo por encima del umbral de claridad ($r \ge 0.70$) en el desfase discreto $\tau = k$, se calcula el desplazamiento fraccionario $\delta \in [-0.5, 0.5]$ evaluando los tres puntos $(\alpha, \beta, \gamma) = (r[k-1], r[k], r[k+1])$:

$$\delta = \frac{\gamma - \alpha}{2(2\beta - \alpha - \gamma)}$$

El periodo fundamental exacto es $\tau_{\text{exact}} = k + \delta$, y la frecuencia estimada es:
$$f_{\text{est}} = \frac{f_s}{\tau_{\text{exact}}}$$

### 5.2 Error de Afinación en Cents
Respecto a la frecuencia MIDI de referencia $f_{\text{ref}} = 440 \cdot 2^{(\text{note} - 69)/12}$:

$$\Delta_{\text{cents}} = 1200 \log_2\left( \frac{f_{\text{est}}}{f_{\text{ref}}} \right)$$

Garantiza una precisión teórica de $\pm 0.005$ cents frente al ground truth analítico.

---

## 6. Extracción de Envolvente y Desacoplamiento de Portadora (`AnalysisPolicy`)

*(Implementado en `SynthEnvelopeAnalyzer.cpp` — Fase 20)*

Para evitar retardos de fase o artefactos inducidos por filtros paso bajo analógicos que contaminan la detección de ataques rápidos, el perfilado utiliza un **seguidor de picos instantáneo con decaimiento exponencial**:

$$y[n] = \max\left( |x[n]|, \; y[n-1] \cdot \alpha \right), \quad \alpha = e^{-\frac{1}{\tau_{\text{decay}} \cdot f_s}}$$

### 6.1 Hiperparámetros de la Política de Análisis
- **$\tau_{\text{decay}} = 40\text{ ms}$**: Salva holgadamente los valles inter-ciclo de la fundamental (ej. 1.91 ms para C4 261.6 Hz) sin fluctuaciones de rizado espurias.
- **Primer Pico de Ataque**: Se localiza el primer índice donde la envolvente cruza el umbral del $97\%$ del valor máximo observado en la compuerta activa:
  $$t_{\text{peak}} = \min \{ t \mid y[t] \ge 0.97 \cdot y_{\max} \}$$
  $$T_{\text{attack}} = t_{\text{peak}} - t_{\text{onset}}$$
- **Umbral de Fin de Decaimiento**:
  $$t_{\text{decay\_end}} = \min \{ t > t_{\text{peak}} \mid y[t] \le S_{\text{linear}} + 0.02 \}$$
- **Umbral de Fin de Liberación (Norma IEC)**:
  $$t_{\text{release\_end}} = \min \{ t > t_{\text{note\_off}} \mid y[t] \le 0.04 \cdot S_{\text{linear}} \}$$

> [!NOTE]
> Estos umbrales pertenecen a la **`AnalysisPolicy`** metrológica y se serializan en el manifiesto (`analysisPolicyId`, `analysisPolicyVersion`) para asegurar comparabilidad histórica estricta.

---

## 7. Estimación del Jacobiano Local por Perturbación Simétrica ($\pm\Delta$)

*(Fase 20.3 & 20.5 — Identificabilidad y Sensibilidad)*

Para estimar la influencia causal de cada parámetro $p_j \in [0.0, 1.0]$ sobre el vector de rasgos acústicos $\Phi(y) = [f_0, T_{\text{attack}}, S_{\text{level}}, T_{\text{release}}, \text{Centroid}, \text{THD}]^T$, se calcula el gradiente central alrededor de un punto base $p^*$:

$$J_{ij} = \frac{\partial \Phi_i(y)}{\partial p_j} \approx \frac{\Phi_i\left(y(p^* + \Delta \cdot \mathbf{e}_j)\right) - \Phi_i\left(y(p^* - \Delta \cdot \mathbf{e}_j)\right)}{2\Delta}$$

Con $\Delta = 0.02$ (rango normalizado).

### 7.1 Detección de Redundancias y Simetrías
Si dos columnas del Jacobiano son colineales:
$$\frac{\mathbf{J}_{\cdot, j}^T \mathbf{J}_{\cdot, k}}{\|\mathbf{J}_{\cdot, j}\| \|\mathbf{J}_{\cdot, k}\|} \approx 1.0$$

El sistema marca **`MultipleEquivalentSolutions`** o redundancia funcional entre los controles $p_j$ y $p_k$.

---

## 8. Diseño Óptimo de Experimentos y Active Learning

*(Fase 20.4 — AdaptiveExperimentPlanner)*

Para evitar la explosión combinatoria factorial ($N^K$), el planificador activo selecciona iterativamente el siguiente punto de prueba $x^*$ maximizando la relación entre ganancia de información y coste de medición:

$$x^* = \arg\max_{x \in \mathcal{X}} \frac{\text{ExpectedInformationGain}(x)}{\text{MeasurementCost}(x)}$$

### 8.1 Criterio de Parada Formal
El proceso de exploración concluye cuando la reducción marginal esperada de incertidumbre cae por debajo del umbral de coste:

$$\frac{\Delta U(x)}{\text{Coste}(x)} < \epsilon \quad \lor \quad U_{\text{paramétrica}} < U_{\text{target}}$$

### 8.2 Descomposición Tripartita de Incertidumbre
El reporte metrológico descompone explícitamente la incertidumbre total en tres componentes ortogonales:

$$U_{\text{total}} = U_{\text{aleatoria}} + U_{\text{paramétrica}} + U_{\text{estructural}}$$

1. **Incertidumbre Aleatoria ($U_{\text{aleatoria}}$)**: Variabilidad intrínseca no explicable (ruido analógico, jitter de transmisión, deriva estocástica).
2. **Incertidumbre Paramétrica ($U_{\text{paramétrica}}$)**: Intervalo de confianza estadístico ($\text{CI}_{95\%}$) sobre los coeficientes del modelo.
3. **Incertidumbre Estructural ($U_{\text{estructural}}$)**: Discrepancia insoslayable cuando el modelo hipotético (ej. LNL estático) no puede explicar la física observada (ej. sistema con memoria de envolvente o histéresis).
