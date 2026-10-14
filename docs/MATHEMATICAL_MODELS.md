# Fundamentos Matemáticos y Modelos DSP — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión:** 2.0.0  
**Actualizado:** 2026-09-13  

---

## 1. Técnica de Barrido Senoidal Logarítmico (Farina Sweep)

*(Basado en Angelo Farina, AES 108th Convention, París, 2000 — [134-AES00.pdf](../docs/google%20ia%20research/134-AES00.pdf))*

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

*(Basado en Takeo Sasai et al., Optics Express 2020 / arXiv:2012.08046v1 — [Wiener-Hammerstein model...pdf](../docs/google%20ia%20research/Wiener-Hammerstein%20model%20and%20its%20learning%20for%20nonlinear%20digital%20pre-distortion%20of%20optical%20transmitters-with-annotations.pdf))*

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

---

## 9. Fixtures Analíticos de Referencia Determinista (Synthetic Ground Truth)

*(Fase 19.2 — SyntheticGroundTruthFixtures.h)*

Para certificar la precisión del instrumental algorítmico y delimitar la cota superior del identificador de sistemas, se implementan 4 módulos analíticos exactos en memoria:

### 9.1 Bypass Lineal Puro con Retardo Discreto
$$y[n] = g \cdot x[n - \tau], \quad \tau \in \mathbb{N}$$
- Error residual nulo: $\text{ESR} \le -180.0\text{ dB}$, $\text{RMSE} = 0.0$, $R^2 = 1.0$.
- Retardo fraccionario sub-muestra mediante interpolación sinc trunca / lagrange para validación de alineamiento.

### 9.2 Filtros Bilineales TPT/ZDF (Vadim Zavalishin)
Implementación en espacio de estados de topología preservada (Topology-Preserving Transform):
$$\begin{aligned}
v_1[n] &= \frac{x[n] - s_1[n] - 2R \cdot s_2[n]}{1 + 2R \cdot g + g^2} \\
v_2[n] &= g \cdot v_1[n] + s_2[n]
\end{aligned}$$
donde $g = \tan(\pi f_c / f_s)$, $2R = 1/Q$, y los estados se actualizan como:
$$s_1[n+1] = 2v_1[n] - s_1[n], \quad s_2[n+1] = 2v_2[n] - s_2[n]$$
- Atenuación analítica exacta: $f = f_c \implies |H(j\omega)| = -3.01\text{ dB}$ (2º orden) y $-6.02\text{ dB}$ (4º orden en cascada).
- Distorsión armónica nula LTI: $\text{THD} < -100\text{ dB}$.

### 9.3 Saturadores Polinómicos Estáticos
Modelos no lineales estrictamente sin memoria con simetría impar $f(-x) = -f(x)$:
- Polinomio cúbico: $y = x - a \cdot x^3$
- Polinomio quintico: $y = x - a \cdot x^3 + b \cdot x^5$
- Tangente hiperbólica normalizada: $y = \frac{\tanh(k \cdot x)}{\tanh(k)}$
- Generación armónica exacta para entrada senoidal $x(t) = A \sin(\omega t)$:
  $$x^3 = \frac{3A^3}{4}\sin(\omega t) - \frac{A^3}{4}\sin(3\omega t)$$
  Permite la comprobación analítica de la relación entre fundamental H1 y armónico H3 sin artefactos de aliasing.

### 9.4 Sistema Dinámico No Lineal con Memoria e Histéresis
Circuito rectificador de media onda con filtro RC asimétrico y realimentación dinámica:
$$\begin{aligned}
x_{\text{rect}}[n] &= \max(0, x[n] - V_{\text{th}}) \\
\alpha &= \begin{cases} e^{-\frac{1}{\tau_{\text{attack}} f_s}} & \text{si } x_{\text{rect}}[n] > s[n-1] \\ e^{-\frac{1}{\tau_{\text{release}} f_s}} & \text{si } x_{\text{rect}}[n] \le s[n-1] \end{cases} \\
s[n] &= \alpha \cdot s[n-1] + (1 - \alpha) \cdot x_{\text{rect}}[n] \\
y[n] &= \tanh(x[n] - \beta \cdot s[n])
\end{aligned}$$
- Comportamiento de ciclo cerrado en plano de fase $(x, y)$:
  $$\oint y \, dx > 0 \quad (\text{para } \beta > 0)$$
  colapsando a $\oint y \, dx = 0$ cuando $\beta = 0$ (modo estático), delimitando la frontera analítica entre modelos de tabla (LUT) y redes recurrentes (NAM).

---

## 10. Función de Coste Multiobjetivo y Selección de Modelos

*(Fase 19.3 — MultiObjectiveModelSelector.h/.cpp)*

El motor evalúa un conjunto finito de arquitecturas candidatas $\mathcal{M} = \{ m_1, m_2, \dots, m_K \}$ frente a un conjunto reservado de validación ciego (*Holdout Dataset*).

### 10.1 Función de Coste Normalizada
$$J(m) = E_{\text{val}}(m) + \lambda \cdot C(m) + \mu \cdot U(m)$$

donde cada componente está acotado en $[0.0, 1.0]$:
1. **Error Fuera de Muestra ($E_{\text{val}}$)**:
   $$E_{\text{val}}(m) = \text{clamp}(1.0 - R^2(m), 0.0, 1.0)$$
   complementado con el cálculo logarítmico del error-to-signal ratio:
   $$\text{ESR}_{\text{dB}}(m) = 10 \log_{10}\left( \frac{\sum (y_{\text{target}}[n] - \hat{y}_m[n])^2}{\sum y_{\text{target}}[n]^2} \right)$$

2. **Huella de Recursos Computacionales ($C(m)$)**:
   $$C(m) = 0.60 \cdot \frac{\text{CPU}\%}{10.0} + 0.20 \cdot \frac{\text{RAM}}{2\text{ MB}} + 0.20 \cdot \frac{\text{Latencia}}{128\text{ muestras}}$$

3. **Incertidumbre y Estructura del Residuo ($U(m)$)**:
   $$U(m) = 0.60 \cdot \rho_1(m) + 0.40 \cdot E_{\text{val}}(m)$$
   donde $\rho_1 = \left| \frac{R_{ee}(1)}{R_{ee}(0)} \right|$ es el coeficiente de autocorrelación a lag-1 del residuo $e[n] = y[n] - \hat{y}[n]$.

### 10.2 Presets Operativos Canónicos
| Preset | $\lambda$ (Coste) | $\mu$ (Incertidumbre) | Objetivo Primario |
| :--- | :---: | :---: | :--- |
| **`StudioMaster`** | $0.05$ | $0.15$ | Máxima fidelidad y precisión metrológica; tolera alto consumo. |
| **`LivePerformance`** | $0.60$ | $0.20$ | Mínimo impacto de CPU y cero latencia para sintetizadores polifónicos RT. |
| **`Balanced`** | $0.25$ | $0.20$ | Compromiso óptimo estándar. |

### 10.3 Criterio de Indecisión Formal (`Inconclusive`)
Sean $m_1$ y $m_2$ el primer y segundo candidato ordenados por $J(m)$ ascendente:
$$\Delta_{\text{gap}} = J(m_2) - J(m_1)$$
Si $\Delta_{\text{gap}} < \sigma$ (con $\sigma = 0.03$), la decisión se declara formalmente **`SelectionStatus::Inconclusive`**, documentando que ambos modelos son estadísticamente equivalentes dentro de la resolución de medición.

### 10.4 Diagnóstico de Residuo
- Si $\rho_1 < 0.15 \implies$ `UnstructuredResidual`: el residuo es compatible con ruido blanco plano; la arquitectura estática es adecuada.
- Si $\rho_1 \ge 0.15 \implies$ `CorrelatedDynamicResidual`: se detecta correlación temporal o memoria interna no modelada, alertando al usuario de la conveniencia de evaluar modelos recurrentes / NAM.

---

## 11. Batería de Validación Ciega Fuera de Muestra (*Out-of-Sample Holdout Engine*)

*(Fase 19.4 — HoldoutStimulusGenerator.h/.cpp, OutOfSampleHoldoutEngine.h/.cpp)*

El motor de validación ciega genera conjuntos de datos de prueba independientes e inmutables, garantizando la evaluación no sesgada de modelos no lineales y previniendo el sobreajuste (*overfitting*).

### 11.1 Sub-baterías de Estímulos Metrológicos
1. **Escaleras de Ráfagas Dinámicas** ($D_1$):
   Pulsos senoidales escalonados a niveles discretos $[-30, -24, -18, -12, -6, -3]\text{ dBFS}$ con ventanas de Hann para auditar el comportamiento a través de diferentes regímenes de ganancia.
2. **Transitorios con Zonas de Reposo** ($D_2$):
   Ráfagas de envolvente rápida ($T_{\text{att}} = 2\text{ ms}$, $T_{\text{rel}} = 10\text{ ms}$) seguidas de silencios absolutos $> 33\%$ de la duración para auditar colas de relajación, ringing y descargas de estados internos.
3. **Multitono Armónicamente Denso con Fases de Schroeder** ($D_3$):
   Suma de $M$ componentes espectrales no conmensurables con desfase cuadrático:
   $$\theta_k = \frac{\pi \cdot k^2}{M}, \quad k \in \{0, 1, \dots, M-1\}$$
   $$x[n] = \sum_{k=0}^{M-1} \cos(2\pi f_k t_n + \theta_k)$$
   Esta distribución minimiza la coincidencia en fase de los armónicos, manteniendo el factor de cresta controlado ($\text{CF}_{\text{dB}} < 9.0\text{ dB}$) a la vez que excita simultáneamente productos de intermodulación de alto orden.
4. **Secuencia Pseudo-musical Representativa** ($D_4$):
   Arpegios de onda triangular/compleja con salto modal de intervalos (fundamental, quinta, octava) y modulación de amplitud para validar el comportamiento psicoacústico en régimen cuasi-estacionario.

### 11.2 Control Criptográfico e Inmutabilidad
Cada sub-batería y el `HoldoutDataset` completo generan un hash criptográfico SHA-256 canónico (RFC 8785) sobre sus coeficientes normalizados:
$$H_{\text{dataset}} = \text{SHA-256}\Big(\bigoplus_{i=1}^N \text{IEEE-754LE}(x_i)\Big)$$
Cualquier modificación posterior invalida la firma de auditoría metrológica.

### 11.3 Auditoría de Fugas de Información (*Data Leakage Detection*)
Para verificar que el conjunto de validación es genuinamente fuera de muestra (*out-of-sample*), se evalúa la distancia en el espacio de parámetros de excitación frente a las secuencias de entrenamiento:
$$d(p_{\text{holdout}}, p_{\text{train}}) = \sqrt{ \sum_{j} w_j \left( \frac{p_{\text{holdout}, j} - p_{\text{train}, j}}{\sigma_j} \right)^2 }$$
Si $d < \epsilon_{\text{threshold}}$, se emite un fallo por contaminación de datos (`DataLeakageDetected`), requiriendo la regeneración de los estímulos de test.

### 11.4 Descomposición Formal de Incertidumbre Metrológica
La incertidumbre combinada total de la validación se descompone según la Guía para la Expresión de la Incertidumbre de Medida (GUM / ISO/IEC Guide 98-3):
$$u_{\text{combined}}^2 = u_{\text{inst}}^2 + u_{\text{struct}}^2$$
- **Incertidumbre Instrumental** ($u_{\text{inst}}$): Derivada del límite metrológico de la cadena de captura (ruido de cuantización, jitter, SNR de la interfaz analógica).
- **Incertidumbre Estructural** ($u_{\text{struct}}$): Varianza residual intrínseca entre la respuesta predicha por la arquitectura candidata $\hat{y}[n]$ y la respuesta de referencia $y_{\text{target}}[n]$ en datos no observados.


