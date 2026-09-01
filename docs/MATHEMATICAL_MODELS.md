# Fundamentos Matemáticos y Modelos DSP — ABDAudioLab

**Proyecto:** ABDAudioLab  
**Versión:** 1.0.0  
**Fecha:** 2026-09-01  

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

*(Basado en Takeo Sasai et al., Optics Express 2020 — [Wiener-Hammerstein model...pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Wiener-Hammerstein%20model%20and%20its%20learning%20for%20nonlinear%20digital%20pre-distortion%20of%20optical%20transmitters-with-annotations.pdf))*

Los circuitos de hardware analógico y de modelado ACB se descomponen en cascadas de 3 etapas no conmutativas (**Linear – Nonlinear – Linear**):

$$\text{Entrada } x[n] \longrightarrow \text{Filtro Lineal FIR } h_1[n] \longrightarrow \text{Saturador Estático } f(y) \longrightarrow \text{Filtro Lineal FIR } h_2[n] \longrightarrow \text{Salida } y[n]$$

Donde la no-linealidad estática se modela mediante:
$$f(y) = y + a \cdot y^3 \quad \text{o} \quad f(y) = \tanh(G \cdot y)$$

---

## 3. Extracción Estadística Dual: Media ($\mu$) y Desviación Estándar ($\sigma$)

Para cada punto de control $(\text{knob}_1, \text{knob}_2)$, el laboratorio ejecuta $P$ pasadas de medición idénticas ($P \ge 3$) y calcula:

1. **Media Muestral ($\mu$) — Valor Nominal Estable**:
   $$\mu = \frac{1}{P} \sum_{i=1}^{P} x_i$$

2. **Desviación Estándar ($\sigma$) — Factor de Ruido Térmico / Deriva ACB**:
   $$\sigma = \sqrt{\frac{1}{P - 1} \sum_{i=1}^{P} (x_i - \mu)^2}$$

### Interpretación de Resultados:
* $\sigma \approx 0$: Componente digital determinista perfecto.
* $\sigma > 0$: Hardware con inestabilidad analógica, fluctuaciones térmicas reales o algoritmos de modelado por componentes analógicos (Roland ACB).

---

## 4. Pre-Roll de Calibración y Sincronización Temporal (Secuencia de 3 Tonos)

*(Inspirado en los protocolos de calibración de Neural Amp Modeler y Audio Latency Examiner — ALEX)*

Antes de emitir el estímulo principal o al calibrar la sesión, se inyecta una secuencia de **3 ráfagas senoidales a 1 kHz** de 40 ms de duración cada una, separadas por 40 ms de silencio y con envolventes de Hann de 5 ms en los bordes para eliminar clics:

$$\text{Pre-Roll}(t) = \begin{cases}
\sin(2\pi \cdot 1000 \cdot t) \cdot w(t) & \text{en } [0, 40\text{ ms}], [80, 120\text{ ms}], [160, 200\text{ ms}] \\
0.0 & \text{en intervalos de silencio}
\end{cases}$$

### Objetivos del Pre-Roll de 3 Tonos:
1. **Marcador de Inicio Inequívoco ($t_0$)**: Permite al receptor identificar con precisión de muestra (*sample-accurate*) el inicio exacto de la prueba en la grabación entrante, independientemente de los retardos de búfer del sistema operativo.
2. **Medición de Latencia de Ida y Vuelta (Round-Trip Latency)**: Cuenta el número de muestras entre el disparo digital y el primer pico de retorno.
3. **Verificación de Margen Dinámico (*Gain Staging*)**: Comprueba que el hardware no esté saturando el previo de la tarjeta (asegurando picos a $-3\text{ dBfs}$).
4. **Comprobación de Fase Estéreo**: Mide la correlación entre canales L y R para evitar cancelaciones de fase acústicas o eléctricas.
