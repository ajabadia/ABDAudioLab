# Matriz de Presets Acústicos Canónicos (Fase 5D)

**Documento:** `docs/qa/audio-ab-5d-canonical-preset-matrix.md`  
**Hito Rector:** `HITO-AUDIO-AB-5D: Acceptance Matrix and Canonical Preset Validation`  
**Estado:** V1.0 — Congelado para Diseño y Especificación  
**Fecha:** 2026-09-29  

---

## 1. Principios de Diseño del Catálogo Canónico

1. **Totalmente Sintético y Hermético:** Cada preset se define exclusivamente por parámetros numéricos declarativos. Cero dependencias de archivos de muestras (`.wav`), patches de hardware físico o configuraciones locales.
2. **Determinismo Absoluto:** Parámetros fijos con inicialización de fase y semilla pseudoaleatoria declaradas explícitamente en el fixture.
3. **Representatividad Acústica:** Cubren los cinco regímenes fundamentales de respuesta en síntesis analógica y virtual-analógica (VA).

---

## 2. Definición Detallada de los 5 Presets Canónicos

### 2.1 Preset 1: `PRESET_CLEAN_REF_01` (Clase: `CleanReference`)
- **Propósito:** Línea base de linealidad matemática, calibración de ganancia, seguimiento de envolvente y verificación de pureza espectral.
- **Configuración DSP:**
  - **Oscilador:** DCO Onda Diente de Sierra (PolyBLEP o analítica) a tono nominal. Sub-oscilador desactivado. Ruido desactivado.
  - **Filtro (VCF):** TPT Low-Pass 24 dB/oct totalmente abierto ($f_c = 20.000\text{ Hz}$), resonancia nula ($Q = 0.707$), modulación de envolvente nula.
  - **Amplificador (VCA):** Envolvente ADSR limpia: Attack = 5 ms, Decay = 50 ms, Sustain = 1.0 (0 dBFS a máxima velocidad nominal), Release = 50 ms.
  - **Efectos:** Bypass total (Chorus OFF, Drive OFF).
- **Riesgos que Detecta:** Desalineación de fases fundamentales, error de escalado de ganancia, truncamiento de envolvente, jitter en el sample clock del render.
- **Expectativa de Veredicto:** `PASS` estricto ($\text{Correlación} \ge 0.999$, $\Delta\text{RMS} < 0.1\text{ dB}$, $\Delta\text{Espectral} < 0.5\text{ dB}$).

---

### 2.2 Preset 2: `PRESET_GENTLE_MOD_02` (Clase: `GentleModulation`)
- **Propósito:** Caracterización de dispersión de fase, modulación temporal periódica y conservación de energía en efectos de chorus/ensemble analógico.
- **Configuración DSP:**
  - **Oscilador:** Onda Diente de Sierra + Sub-oscilador cuadrado (-1 octava a -6 dB).
  - **Filtro (VCF):** Low-Pass 24 dB/oct en $f_c = 4.000\text{ Hz}$, $Q = 1.2$, Key-tracking 100%.
  - **Amplificador (VCA):** Attack = 10 ms, Decay = 100 ms, Sustain = 0.85, Release = 150 ms.
  - **Efectos:** BBD Analog Chorus activo (Modo Juno I: LFO = 0.5 Hz, retardo nominal 4 ms, excursión $\pm 1.5\text{ ms}$, mezcla 50/50).
- **Riesgos que Detecta:** Desfase no acotado por LFO no determinista, cancelación de fase destructiva por suma mono errónea, degradación de alta frecuencia por filtro antialiasing del BBD.
- **Expectativa de Veredicto:** `PASS` o `WARN` justificado por fase ($\text{Correlación} \ge 0.85$, $\Delta\text{RMS} < 0.3\text{ dB}$, $\text{EnvelopeDiff} < 0.5\text{ dB}$).

---

### 2.3 Preset 3: `PRESET_AGGR_NONLIN_03` (Clase: `AggressiveNonlinear`)
- **Propósito:** Validación de respuesta ante armónicos no lineales, compresión de rango dinámico por saturación y estabilidad de filtros auto-oscilantes.
- **Configuración DSP:**
  - **Oscilador:** Doble oscilador (Sierra + Pulso con modulación PWM al 35%), ligera desafinación relativa de 3 cents.
  - **Filtro (VCF):** Ladder o Sallen-Key no lineal con saturación por tangente hiperbólica ($\tanh$), $f_c = 800\text{ Hz}$, resonancia agresiva cercana a auto-oscilación ($Q = 8.5$), modulación por envolvente profunda ($+36\text{ semitonos}$, Attack = 2 ms, Decay = 200 ms).
  - **Amplificador / Sat:** Etapa de saturación analógica suave impulsada a $+6\text{ dB}$ con limitador soft-clip en el retorno.
- **Riesgos que Detecta:** Inestabilidad numérica, *blow-up* de filtros ZDF/TPT, aliasing por repliegue espectral descontrolado, clipping digital duro ($> 0.0\text{ dBFS}$).
- **Expectativa de Veredicto:** `PASS` o `WARN` por dispersión armónica ($\text{Correlación} \ge 0.75$, $\Delta\text{RMS} < 0.6\text{ dB}$, 0 clipping digital no lineal).

---

### 2.4 Preset 4: `PRESET_LOW_LEVEL_04` (Clase: `LowLevelDynamic`)
- **Propósito:** Detección de problemas de cuantización, presencia de números subnormales (*denormals*), comportamiento de compuertas y piso de ruido.
- **Configuración DSP:**
  - **Oscilador:** Onda Seno pura a nivel muy bajo ($-48\text{ dBFS}$ de referencia).
  - **Filtro (VCF):** Bypass o totalmente abierto sin resonancia.
  - **Amplificador (VCA):** Envolvente lenta: Attack = 50 ms, Decay = 500 ms, Sustain = 0.1 ($-20\text{ dB}$ adicionales, total $-68\text{ dBFS}$), Release = 800 ms hasta silencio total.
  - **Efectos:** Bypass total.
- **Riesgos que Detecta:** Picos de CPU por denormals en decaimiento a cero, silenciamiento abrupto por truncamiento prematuro de envelopes, desbalance de bits de precisión en punto flotante simple.
- **Expectativa de Veredicto:** `PASS` ($\text{Correlación} \ge 0.98$ en régimen activo, SNR $\ge 90\text{ dB}$ relativo a la señal mínima, 0 denormals).

---

### 2.5 Preset 5: `PRESET_HIGH_DENSITY_05` (Clase: `HighDensitySpectral`)
- **Propósito:** Respuesta ante acumulación simultánea de voces, coherencia de suma multivoz, aliasing y respuesta de transitorios rápidos.
- **Configuración DSP:**
  - **Polifonía:** 6 voces activas simultáneamente (acorde mayor de novena extendido).
  - **Oscilador:** 2 osciladores por voz (Sierra + Sub) con modelo de dispersión estocástica multivoz activo (`VoiceDispersionModel`: varianza de cutoff $\pm 1.8\%$, varianza de ganancia $\pm 1.2\%$).
  - **Filtro (VCF):** Low-Pass $f_c = 2.500\text{ Hz}$, $Q = 2.0$, seguimiento de teclado 100%.
  - **Amplificador (VCA):** Attack = 1 ms, Decay = 150 ms, Sustain = 0.7, Release = 200 ms.
- **Riesgos que Detecta:** Saturación por suma destructiva en bus maestro, aliasing en agudos, divergencia en el orden de asignación de voces (`VoiceAllocator`), pérdida de energía por modulación cruzada.
- **Expectativa de Veredicto:** `PASS` con tolerancia estocástica documentada ($\text{Correlación} \ge 0.80$, $\Delta\text{RMS} < 0.5\text{ dB}$, $\Delta\text{Espectral}_{\text{total}} < 1.2\text{ dB}$).

---

## 3. Matriz Resumen de Presets Canónicos

| Preset ID | Clase Acústica | Estímulo Principal | Tolerancia Fase | Tolerancia RMS | Tolerancia Espectral | Criterio Crítico |
|---|---|---|:---:|:---:|:---:|---|
| `PRESET_CLEAN_REF_01` | `CleanReference` | Sierra Mono $f_c=20\text{k}$ | $\ge 0.999$ | $< 0.10\text{ dB}$ | $< 0.50\text{ dB}$ | Identidad matemática y 0 clipping |
| `PRESET_GENTLE_MOD_02` | `GentleModulation` | Sierra+Sub + BBD Chorus | $\ge 0.850$ | $< 0.30\text{ dB}$ | $< 1.50\text{ dB}$ | Envolvente idéntica, dispersión BBD |
| `PRESET_AGGR_NONLIN_03` | `AggressiveNonlinear` | Dual Osc + VCF $\tanh$ + Drive | $\ge 0.750$ | $< 0.60\text{ dB}$ | $< 2.50\text{ dB}$ | Estabilidad ZDF y soft-clipping |
| `PRESET_LOW_LEVEL_04` | `LowLevelDynamic` | Seno $-48\text{ dBFS}$ a $-68\text{ dBFS}$ | $\ge 0.980$ | $< 0.20\text{ dB}$ | $< 1.00\text{ dB}$ | Cero denormals y decaimiento suave |
| `PRESET_HIGH_DENSITY_05` | `HighDensitySpectral` | Acorde 6 voces con dispersión | $\ge 0.800$ | $< 0.50\text{ dB}$ | $< 1.20\text{ dB}$ | Suma multivoz sin intermodulación |
