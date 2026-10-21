/**
 * ABDAudioLab WebUI — Mock Telemetry & IPC Bridge
 * Simula el canal de telemetría de audio y metrología C++ para desarrollo en navegador a 60 FPS.
 */

export const MOCK_FIXTURES = {
  accepted: {
    hasEvaluation: true,
    recommendedModelId: "LUT_SIMD_2D",
    status: "Accepted",
    verdictClass: "status-accepted",
    provenance: "SyntheticSynthFixture_V1 (Holdout Calibrado)",
    sha256: "4e37e03b9f12d8a57c1264b914e626d2e8b91a7c0384591e0a2938475618293a",
    hashVerified: true,
    validationEsrDb: -120.0,
    spectralCorrelationRho: 1.0000,
    stimuliPassingRatioPercent: 100,
    validatedDomain: "C1–C6, Vel 1–127, Cutoff [20 Hz..20 kHz]",
    sampleOffset: 0,
    relativeCpuCost: 0.50,
    combinedUncertaintyDb: 0.45,
    enobBits: 19.6,
    canExport: true,
    message: "Modelo verificado y dentro de tolerancias metrológicas internacionales."
  },
  rejected: {
    hasEvaluation: true,
    recommendedModelId: "LUT_SIMD_2D",
    status: "Rejected",
    verdictClass: "status-rejected",
    provenance: "SyntheticSynthFixture_Failed (Out-of-Tolerance)",
    sha256: "993491b6e82a47c19f8021d4882496b4c1029384756102938475610293847561",
    hashVerified: true,
    validationEsrDb: -5.3,
    spectralCorrelationRho: 0.7061,
    stimuliPassingRatioPercent: 50,
    validatedDomain: "C1–C6, Vel 1–127",
    sampleOffset: 0,
    relativeCpuCost: 0.50,
    combinedUncertaintyDb: 2.85,
    enobBits: 8.4,
    canExport: false,
    message: "RECHAZADO: El dictamen 'Rejected' bloquea la exportación de producción."
  }
};

export class MockIpcBridge {
  constructor(onMetricsUpdate, onMetersUpdate) {
    this.onMetricsUpdate = onMetricsUpdate;
    this.onMetersUpdate = onMetersUpdate;
    this.currentFixture = 'accepted';
    this.isRunning = false;
    this.animId = null;
    this.t = 0;
  }

  start() {
    this.isRunning = true;
    this.emitCurrentMetrics();

    const loop = () => {
      if (!this.isRunning) return;
      this.t += 0.05;

      // Simulación de VU-meters con balística natural
      const inLevel = -18.0 + Math.sin(this.t * 2.0) * 8.0 + (Math.random() - 0.5) * 3.0;
      const outLevel = inLevel - 1.2 + Math.cos(this.t * 1.5) * 4.0;

      if (this.onMetersUpdate) {
        this.onMetersUpdate({
          inDbfs: Math.max(-60, Math.min(0, inLevel)),
          outDbfs: Math.max(-60, Math.min(0, outLevel)),
          inPeakDbfs: Math.max(-60, Math.min(0, inLevel + 3.0)),
          outPeakDbfs: Math.max(-60, Math.min(0, outLevel + 2.5))
        });
      }

      this.animId = requestAnimationFrame(loop);
    };

    this.animId = requestAnimationFrame(loop);
  }

  stop() {
    this.isRunning = false;
    if (this.animId) cancelAnimationFrame(this.animId);
  }

  setFixture(name) {
    if (MOCK_FIXTURES[name]) {
      this.currentFixture = name;
      this.emitCurrentMetrics();
    }
  }

  emitCurrentMetrics() {
    if (this.onMetricsUpdate) {
      this.onMetricsUpdate(MOCK_FIXTURES[this.currentFixture]);
    }
  }
}
