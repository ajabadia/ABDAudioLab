# Fundamentos Matemáticos y Modelos DSP — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión:** 2.4.0 (Fase 24 Implementada y Verificada)  
**Actualizado:** 2026-10-16  

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

### 1.4 Enventanado Asimétrico Anti-Gibbs y Silencios de Guarda
*(FarinaDeconvolver.h/.cpp)*

Un inicio o corte abrupto de $x(t)$ en los extremos de banda ($f_1 = 20\text{ Hz}$, $f_2 = 20\text{ kHz}$) introduce discontinuidades de derivada que generan rizado espectral (*fenómeno de Gibbs*) y transitorios de conmutación en convertidores y etapas analógicas acopladas por condensador. Para erradicarlo, se aplica una envolvente de ventana Tukey asimétrica (*Half-Hann*) diferenciada entre emisión y captura:

$$w_{\text{out}}[n] = \begin{cases} 
\frac{1}{2}\left(1 - \cos\left(\frac{\pi n}{N_{\text{in}}}\right)\right) & 0 \le n < N_{\text{in}} \\
1.0 & N_{\text{in}} \le n < N - N_{\text{out}} \\
\frac{1}{2}\left(1 + \cos\left(\frac{\pi (n - (N - N_{\text{out}}))}{N_{\text{out}}}\right)\right) & N - N_{\text{out}} \le n < N
\end{cases}$$

Parámetros temporales canónicos:
* **Rampa de entrada de emisión (`outputFadeIn`)**: $\tau_{\text{in,out}} = 30\text{ ms}$ ($N_{\text{in}} = \lfloor 0.030 \cdot f_s \rfloor$), suavizando el arranque sub-grave en $f_1$.
* **Rampa de salida de emisión (`outputFadeOut`)**: $\tau_{\text{out,out}} = 5\text{ ms}$ ($N_{\text{out}} = \lfloor 0.005 \cdot f_s \rfloor$), extinguiendo limpiamente los últimos ciclos en $f_2$ sin recortar ancho de banda agudo.
* **Rampas de ventana de captura (`inputFadeIn` / `inputFadeOut`)**: $\tau_{\text{in,cap}} = \tau_{\text{out,cap}} = 10\text{ ms}$ en los extremos de la ventana de grabación antes de la FFT.
* **Márgenes de silencio de guarda**:
  - `safeSilence` = $15\text{ ms}$ (pre-roll antes de la excitación para asegurar estabilidad del buffer DMA).
  - `interSignalSilence` = $650\text{ ms}$ (post-roll entre barridos sucesivos para garantizar la extinción completa de colas de reverberación/relajación de condensadores por debajo de $-90\text{ dBFS}$).

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

---

## 12. Síntesis de Filtros FIR de Fase Mínima por Cepstrum Real, Ventana de Kaiser y Convolución Overlap-Add

*(Fase 24.1 — LoopbackCalibrator.h/.cpp)*

La compensación inversa de bucle cerrado en bloques FFT contiguos sin solapamiento y con fase cero ($H_{\text{inv}}(e^{j\omega}) \in \mathbb{R}^+$) produce *pre-ringing* acausal simétrico y discontinuidades de borde por convolución circular. Para garantizar causalidad física estricta, mínima latencia de grupo y ausencia total de artefactos de bloque, el ecualizador inverso de calibración se sintetiza como un filtro FIR de fase mínima enventanado mediante Kaiser y aplicado por **Overlap-Add (OLA)**.

### 12.1 Reconstrucción de Fase Mínima vía Cepstrum Real (Hilbert Discreto)
Dada la respuesta en magnitud inversa regularizada y acotada $|H_{\text{inv}}[k]|$ sobre una rejilla FFT de tamaño $N$ ($N = 4096$):

1. **Cepstrum Real** $\hat{c}[n]$:
   $$\hat{c}[n] = \text{IFFT}\Big( \ln\big(\max(|H_{\text{inv}}[k]|, \epsilon)\big) \Big), \quad \epsilon = 10^{-6}$$
2. **Enventanado Causal en el Dominio Quefrencial (*Minimum-Phase Lifter*)** $w_{\text{min}}[n]$:
   $$w_{\text{min}}[n] = \begin{cases} 
   1 & n = 0 \;\text{ ó }\; n = N/2 \\
   2 & 1 \le n < N/2 \\
   0 & N/2 < n < N
   \end{cases}$$
3. **Espectro Complejo de Fase Mínima y Respuesta al Impulso Causal**:
   $$H_{\text{min}}[k] = \exp\Big( \text{FFT}\big( w_{\text{min}}[n] \cdot \hat{c}[n] \big) \Big), \qquad h_{\text{min}}[n] = \Re\Big\{ \text{IFFT}(H_{\text{min}}[k]) \Big\}$$

### 12.2 Truncado FIR con Ventana de Kaiser y Aproximación de Abramowitz & Stegun para $I_0(x)$
Para truncar el núcleo causal $h_{\text{min}}[n]$ a $M$ coeficientes ($M = 1024$) maximizando la concentración de energía en el lóbulo principal y suprimiendo los lóbulos laterales por debajo de $-90\text{ dB}$, se aplica una ventana de Kaiser con parámetro de forma $\beta \in [7.0, 9.0]$:

$$w_K[n] = \frac{I_0\left( \beta \sqrt{1 - \left(\frac{2n}{M - 1} - 1\right)^2} \right)}{I_0(\beta)}, \quad 0 \le n < M$$

La función de Bessel modificada de primera especie y orden cero $I_0(x)$ se evalúa en punto flotante de doble precisión mediante la aproximación polinomial de Chebyshev de **Abramowitz & Stegun (9.8.1 y 9.8.2)** (error relativo $|\epsilon| < 1.6 \times 10^{-7}$):

* **Régimen axial** $|x| < 3.75$, definiendo $t = (x / 3.75)^2 = x^2 / 14.0625$:
  $$I_0(x) \approx 1 + 3.5156229\,t + 3.0899424\,t^2 + 1.2067492\,t^3 + 0.2659732\,t^4 + 0.0360768\,t^5 + 0.0045813\,t^6$$
* **Régimen asintótico** $|x| \ge 3.75$, definiendo $t = 3.75 / |x|$:
  $$I_0(x) \approx \frac{e^{|x|}}{\sqrt{|x|}} \Big( 0.39894228 + 0.01328592\,t + 0.00225319\,t^2 - 0.00157565\,t^3 + 0.00916281\,t^4 - 0.02057706\,t^5 + 0.02635537\,t^6 - 0.01647633\,t^7 + 0.00392377\,t^8 \Big)$$

### 12.3 Convolución Lineal Rápida por Overlap-Add (OLA) y Techo Dinámico Anti-Clipping
Con un kernel FIR $h_{\text{cal}}[n] = h_{\text{min}}[n] \cdot w_K[n]$ de longitud $M = 1024$ y bloques de entrada de longitud $L = 1024$, se emplea una FFT de longitud $N_{\text{OLA}} \ge L + M - 1$ ($N_{\text{OLA}} = 2048$) rellenando con ceros (*zero-padding*) los últimos $N_{\text{OLA}} - L$ puntos de cada bloque, y sumando la cola transitoria de $M - 1$ muestras al bloque siguiente.

Adicionalmente, el sintetizador calcula dos métricas acopladas sobre $h_{\text{cal}}[n]$:
1. **Nivel de potencia media del filtro** ($L_{\text{pow}}$):
   $$L_{\text{pow, dB}} = 10 \log_{10}\left( \frac{1}{K_{\text{band}}} \sum_{k \in [20\text{ Hz}, 20\text{ kHz}]} |H_{\text{cal}}[k]|^2 \right)$$
2. **Límite de realce seguro (`maxBoost` / `avoidClippingEnabled`)**:
   $$G_{\text{headroom, dB}} = -20 \log_{10}\left( \max_{k} |H_{\text{cal}}[k]| \right)$$

---

## 13. Suavizado Psicoacústico ERB (Glasberg & Moore) e Interpolación por Splines Cúbicos

*(Fase 24.2 — LoopbackCalibrator.h/.cpp, AudioABComparator.h)*

El suavizado de media móvil uniforme en bins lineales de la FFT sobre-suaviza la región modal grave ($20\text{–}300\text{ Hz}$) y deja rizado espurio de peine en alta frecuencia ($> 5\text{ kHz}$). Para alinear la inversión espectral y la comparación espectral A/B con la resolución de los filtros auditivos humanos, se adopta la escala de **Ancho de Banda Rectangular Equivalente (ERB)** de **Glasberg & Moore (Hearing Research, 1990)**.

### 13.1 Escala ERB y Tasa de Bandas Auditivas $E(f)$
Para una frecuencia central $f$ expresada en hercios ($\text{Hz}$):
$$\text{ERB}(f) = 24.7 \cdot (0.00437\,f + 1) = 0.107939 \cdot (f + 228.833)\;\text{[Hz]}$$

El número de banda ERB (escala perceptual logarítmica-lineal $E(f)$ en $\text{Cams}$) viene dado por:
$$E(f) = 21.4 \log_{10}(0.00437\,f + 1) = 9.2939\,\ln(0.00437\,f + 1)$$

Y su transformación inversa para proyectar nodos equiespaciados en la escala perceptual de vuelta al eje de frecuencias físicas:
$$f(E) = \frac{10^{E / 21.4} - 1}{0.00437}$$

### 13.2 Suavizado en Potencia con Ventana Proporcional a $\text{ERB}(f_k)$ ó $1/12$ de Octava
Para cada frecuencia objetivo $f_k$, la magnitud suavizada $|\tilde{H}(f_k)|$ se calcula integrando la densidad espectral de potencia con un kernel gaussiano o triangular cuyo semi-ancho de banda $\Delta f(f_k)$ es proporcional a $\text{ERB}(f_k)$ o a una fracción de octava ($\Delta_{\text{oct}} = 1/12 \approx 0.083333336$):
$$|\tilde{H}(f_k)|^2 = \frac{\sum_{i=i_{\min}(k)}^{i_{\max}(k)} W\left(\frac{f_i - f_k}{\Delta f(f_k)}\right) |H(f_i)|^2}{\sum_{i=i_{\min}(k)}^{i_{\max}(k)} W\left(\frac{f_i - f_k}{\Delta f(f_k)}\right)}$$

### 13.3 Interpolación por Splines Cúbicos Naturales ($C^2$)
Para evaluar la curva de corrección sobre cualquier tamaño de FFT o tasa de muestreo ($44.1\text{ kHz} \dots 192\text{ kHz}$) sin esquinas de derivada discontinua, la curva suavizada se parametriza sobre $K$ nodos estrictamente crecientes $\{(x_j, y_j)\}_{j=0}^{K-1}$ con $x_j = \ln(f_j)$ e $y_j = 20\log_{10}|\tilde{H}(f_j)|$ mediante **Splines Cúbicos Naturales** ($S''(x_0) = S''(x_{K-1}) = 0$):
$$S_j(x) = a_j + b_j(x - x_j) + c_j(x - x_j)^2 + d_j(x - x_j)^3, \quad x \in [x_j, x_{j+1}]$$
resolviendo el sistema tridiagonal en $\mathcal{O}(K)$ mediante el algoritmo de Thomas.

---

## 14. Integración Inversa de Schroeder con Truncado de Rodilla de Ruido y Regresión Adaptativa ($R^2 \ge 0.95$)

*(Fase 24.3 — LabEnvelopeCurveAnalyzer.h/.cpp, FarinaDeconvolver.h/.cpp)*

De acuerdo con la norma **ISO 3382** y el método de **M. R. Schroeder (JASA, 1965)**, la curva de decaimiento energético (*Energy Decay Curve*, EDC) se obtiene integrando hacia atrás el cuadrado de la respuesta al impulso $h[n]$. Sin embargo, la presencia de un piso de ruido estacionario $\sigma_{\text{noise}}^2$ en la mitad final de la captura desvía la cola hacia una rampa lineal artificial que falsea el tiempo de decaimiento si no se trunca en la rodilla de intersección (*noise-floor knee*).

### 14.1 Estimación del Piso de Ruido en la Segunda Mitad de la Cola
Dada una respuesta al impulso deconvolucionada $h[n]$ de longitud $N$, se estima la potencia media de ruido por muestra a partir de la mitad terminal $[N/2, N-1]$:
$$P_{\text{noise}} = \frac{1}{N - \lfloor N/2 \rfloor} \sum_{n=\lfloor N/2 \rfloor}^{N-1} h^2[n], \qquad L_{\text{noise, dB}} = 10 \log_{10}(P_{\text{noise}} + \epsilon)$$

### 14.2 Detección de la Rodilla de Ruido (*Noise-Floor Knee*) e Integración Truncada
Se evalúa la envolvente suavizada local de energía en bloques cortos y se localiza el índice de truncado $n_{\text{knee}}$ donde la relación señal-ruido local cae por debajo de un margen de seguridad $\Delta_{\text{knee}} = 6.0\text{ dB}$ sobre $L_{\text{noise, dB}}$. La integral inversa de Schroeder compensada se evalúa únicamente desde $n_{\text{knee}}$ hasta el pico directo $n_{\text{peak}}$:
$$E_{\text{EDC}}[n] = \sum_{k=n}^{n_{\text{knee}}} \max\big(h^2[k] - P_{\text{noise}},\, 0\big), \qquad L_{\text{EDC}}[n] = 10 \log_{10}\left( \frac{E_{\text{EDC}}[n]}{E_{\text{EDC}}[n_{\text{peak}}]} \right)\;\text{[dB]}$$

### 14.3 Regresión Lineal Adaptativa ($T_{30} \to T_{20} \to T_{10}$) y Criterio de Calificación $R^2 \ge 0.95$
Sobre la curva $L_{\text{EDC}}(t)$ en $\text{dB}$, se ajusta una recta por mínimos cuadrados ordinarios $L_{\text{EDC}}(t) \approx \alpha + \beta\,t$ (con pendiente $\beta < 0$ en $\text{dB/s}$) seleccionando adaptativamente el rango dinámico disponible antes de la rodilla de ruido:
1. **$T_{30}$ (Preferente)**: Rango $[-5\text{ dB}, -35\text{ dB}]$ si $L_{\text{EDC}}[n_{\text{knee}}] \le -40\text{ dB}$.
2. **$T_{20}$ (Fallback intermedio)**: Rango $[-5\text{ dB}, -25\text{ dB}]$ si $L_{\text{EDC}}[n_{\text{knee}}] \le -30\text{ dB}$.
3. **$T_{10}$ (Fallback corto)**: Rango $[-5\text{ dB}, -15\text{ dB}]$ si $L_{\text{EDC}}[n_{\text{knee}}] \le -20\text{ dB}$.

El tiempo de caída extrapolado a $-60\text{ dB}$ es $\text{RT}_{60} = -60.0 / \beta$, y su validez metrológica exige superar el umbral del coeficiente de determinación:
$$R^2 = 1 - \frac{\sum_{i} \big(L_{\text{EDC}}(t_i) - (\alpha + \beta t_i)\big)^2}{\sum_{i} \big(L_{\text{EDC}}(t_i) - \bar{L}_{\text{EDC}}\big)^2} \ge 0.95$$
Si $R^2 < 0.95$, la medición de decaimiento se marca como `NonExponentialOrNoisyDecay` para prevenir inferencias espurias.

---

## 15. Controlador PID de Bucle Cerrado con Compensación de Tiempo Muerto (Auto-Trim Analógico)

*(Fase 24.4 — LoopbackCalibrator.h/.cpp)*

El ajuste automático del nivel de envío analógico (*Auto-Trim*) debe converger rápidamente al nivel objetivo $L_{\text{target}}$ (ej. $-12.0\text{ dBFS} \pm 0.5\text{ dB}$) a pesar del retardo de transporte round-trip $\tau_d$ de la interfaz de audio, evitando oscilaciones de sobreimpulso (*overshoot*) y el bloqueo por saturación del integrador (*integral windup*).

### 15.1 Ganancias PID Parametrizadas por Razón de Tiempo Muerto
Sea $\Delta T_c$ la duración de cada bloque de medición RMS (en $\text{ms}$) y $\tau_d$ la latencia round-trip estimada del bucle analógico. Se define el factor de amortiguamiento por tiempo muerto $\eta = \frac{\Delta T_c}{\Delta T_c + \tau_d} \in (0, 1]$. Dado el error instantáneo en decibelios $e[k] = L_{\text{target}} - L_{\text{meas}}[k]$:

$$u_{\text{dB}}[k] = K_p(\eta)\,e[k] + K_i(\eta)\,I[k] + K_d(\eta)\,\frac{e[k] - e[k-1]}{\Delta T_c}$$

donde las ganancias se escalan proporcionalmente a $\eta$:
$$K_p(\eta) = K_{p,0}\cdot \eta, \qquad K_i(\eta) = K_{i,0}\cdot \eta^2 \cdot \Delta T_c, \qquad K_d(\eta) = K_{d,0}\cdot \eta \cdot \Delta T_c$$

### 15.2 Anti-Windup con Reset en Saturación y Criterio de Convergencia por Ventana Estable
1. **Protección Anti-Windup (`[LIMIT-PID RESET]`)**:
   Si la ganancia acumulada $G_{\text{dB}}[k] = G_{\text{dB}}[k-1] + u_{\text{dB}}[k]$ alcanza los límites de seguridad $[G_{\min}, G_{\max}]$ (ej. $[-36\text{ dB}, +12\text{ dB}]$) o si el pico instantáneo en la entrada supera $-1.0\text{ dBFS}$ (*clipping guard*), el acumulador integral $I[k]$ se purga inmediatamente a $0.0$ y se aplica una atenuación de seguridad inmediata de $-6.0\text{ dB}$.
2. **Convergencia Estadística (`stabilityMinChunks`)**:
   El bucle cerrado sólo declara `CalibrationTrimLocked` cuando el error se mantiene dentro de la banda de tolerancia $|e[k]| \le \epsilon_{\text{tol}}$ ($\pm 0.5\text{ dB}$) y la desviación estándar inter-bloque es $\sigma_L \le 0.2\text{ dB}$ durante al menos $N_{\text{stable}} \ge 4$ bloques consecutivos.

---

## 16. Detección de Deriva de Reloj por Doble Piloto, Inversión de Kirkeby y Convolución Particionada de Latencia Cero

*(Fase 24.7 — FarinaDeconvolver.h/.cpp, LoopbackCalibrator.h/.cpp)*

### 16.1 Detección Sub-Muestra de Deriva de Reloj (*Clock Drift*) y *Dropouts* ASIO
Se enmarca el barrido logarítmico entre dos pulsos piloto sinc pasabanda enventanados con Hann ($p[n] = w_{\text{Hann}}[n]\cdot (2 f_{H}\operatorname{sinc}(2 f_{H} x) - 2 f_{L}\operatorname{sinc}(2 f_{L} x))$) separados por una distancia nominal exacta $\Delta N_{\text{nom}}$. En recepción se evalúa la correlación cruzada $R_{yp}[\tau]$ en las ventanas anterior y posterior al barrido y se refina la posición de cada pico mediante interpolación parabólica de 3 puntos:

$$\hat{\tau} = \tau_0 + \frac{R[\tau_0 - 1] - R[\tau_0 + 1]}{2\left(R[\tau_0 - 1] - 2 R[\tau_0] + R[\tau_0 + 1]\right)}$$

La deriva de reloj en partes por millón y la condición de *dropout* se obtienen como:

$$\Delta N_{\text{drift}} = (\hat{\tau}_{\text{post}} - \hat{\tau}_{\text{pre}}) - \Delta N_{\text{nom}}, \qquad \text{Drift}_{\text{ppm}} = 10^6 \cdot \frac{\Delta N_{\text{drift}}}{\Delta N_{\text{nom}}}$$

### 16.2 Deconvolución Regularizada Dependiente de la Frecuencia (Método de Kirkeby)
Para evitar la amplificación de ruido fuera de la banda excitada $[f_1, f_2]$, la inversión espectral aplica un perfil de regularización $\epsilon(f_k)$ con transición *raised-cosine* en octavas logarítmicas entre $\epsilon_{\text{in}}$ (dentro de banda) y $\epsilon_{\text{out}}$ (en DC y Nyquist):

$$H(k) = \frac{Y(k)\cdot X^*(k)}{|X(k)|^2 + \epsilon(f_k)\cdot \max_m |X(m)|^2}$$

### 16.3 Convolución Particionada Híbrida de Latencia Cero (Gardner, 1995)
Para aplicar filtros FIR de fase mínima $h[0\dots M-1]$ en tiempo real con **latencia algorítmica nula** ($0\text{ muestras}$) y cero asignaciones de memoria en el hilo de audio (`ZeroLatencyPartitionedConvolver`), se descompone el núcleo en una cabecera directa de longitud $B$ (ej. $B = 64$) y $P-1$ particiones de cola en una línea de retardo en dominio frecuencial (*Frequency-Domain Delay Line*, FDL):

$$y[n] = \underbrace{\sum_{m=0}^{B-1} h[m]\,x[n-m]}_{\text{Cabecera directa (latencia 0)}} + \underbrace{\text{OLA}_{p=1}^{P-1} \text{IFFT}\left( H_p[k]\cdot X_{b-p}[k] \right)}_{\text{Cola FDL particionada}}$$

### 16.4 Promediado Síncrono en el Dominio del Tiempo (*Synchronous Time-Domain Averaging*, STA)
En capturas multi-pase ($K \ge 2$ repeticiones sincronizadas con el estímulo), mientras las métricas escalares $(\mu, \sigma)$ de corte, resonancia y THD se evalúan sobre cada pase individual para cuantificar la deriva térmica analógica, la curva de transferencia representativa se extrae deconvolucionando el promedio coherente en el dominio del tiempo (`FilterAnalytics::computeSynchronousTimeDomainAverage`):

$$\bar{y}[n] = \frac{1}{K} \sum_{k=0}^{K-1} y_k[n] = s[n] + \frac{1}{K}\sum_{k=0}^{K-1} \eta_k[n] \implies \Delta \text{SNR}_{\text{STA}} = +10\log_{10}(K)\text{ dB}$$
