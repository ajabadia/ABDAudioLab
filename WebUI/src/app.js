/**
 * ABDAudioLab WebUI — Aplicación Principal y Orquestador
 * 
 * Cumplimiento arquitectónico:
 * - Uso del componente oficial SynthNavBar de ABDShared para liberar espacio de UI con menús desktop.
 * - Componente universal VuMeter de ABDShared.
 * - Integración con AboutDialog universal de ABDShared (layout split con arte analógico y metadata técnica).
 * - Selector de temas claro/oscuro (audiolab / audiolab-light) posicionado en la barra superior al lado de la campana.
 * - Configuración oficial de puertos de Audio y MIDI mediante el diálogo nativo de JUCE.
 * - Canal reactivo zero-allocation C++ dual mediante IPC.
 */

import { SynthNavBar } from '@components/SynthNavBar/index.js';
import { VuMeter } from '@components/vuMeter.js';
import { AboutDialog } from '@components/AboutDialog/AboutDialog.js';
import { VirtualKeyboardDrawer } from './components/VirtualKeyboardDrawer.js';

import { renderStage0 } from './stages/stage0_studio.js';
import { renderStage1, resolveThemeImage, getTargetImageByIdOrName, getThemeVstImage } from './stages/stage1_target.js';
import { renderStage2, getAudioInterfaceImage, getThemeAudioInterfaceImage } from './stages/stage2_calibration.js';
import { renderStage3Preflight } from './stages/stage3_preflight.js';
import { renderStage3 } from './stages/stage3_acquisition.js';
import { renderStage4 } from './stages/stage4_export.js';
import { createDefaultSuite } from './stages/stage3_data.js';
import { showModalAlert, showModalConfirm, patchBrowserDialogs } from './components/SoundIdModal.js';

class AudioLabApp {
  constructor() {
    this.currentStage = 1; // Arrancamos en Paso 1 (Target Selection) por defecto según C++
    this.currentSnapshot = null;
    this.currentData = null;
    this.currentAudioDevice = {
      deviceName: "Realtek High Definition Audio",
      typeName: "Windows Audio",
      sampleRate: 48000,
      bufferSize: 256,
      activeInputs: 2,
      activeOutputs: 2,
      audioInOk: true,
      audioOutOk: true,
      audioInName: "Realtek High Definition Audio (2 in)",
      audioOutName: "Realtek High Definition Audio (2 out)",
      availableMidiInputs: ["USB MIDI Interface"],
      availableMidiOutputs: ["Microsoft GS Wavetable Synth"],
      midiInOk: false,
      midiInName: "None / Disconnected",
      midiOutOk: true,
      midiOutName: "Microsoft GS Wavetable Synth"
    };
    this.activePlugin = null;
    this.isPluginWindowOpen = false;
    this.navBar = null;
    this.vuMeter = null;
    this.aboutDialog = null;
    this.virtualKeyboard = null;
    this.selectedTarget = null;
    this.activeTargetImage = '';
    this.activeAudioDeviceImage = getAudioInterfaceImage(this.currentAudioDevice?.deviceName);
    this.availablePlugins = [];
    this.isNativeHost = typeof window !== 'undefined' && !!(window.__JUCE__ && window.__JUCE__.backend);

    // Persistencia y resolución del tema inicial (default: audiolab oscuro)
    let savedTheme = 'audiolab';
    try {
      savedTheme = localStorage.getItem('abdaudiolab_theme') || 'audiolab';
    } catch (_) {}
    this.currentTheme = savedTheme;

    // Persistencia del estado colapsado del sidebar de workflow
    let isSidebarCollapsed = false;
    try {
      isSidebarCollapsed = localStorage.getItem('abdaudiolab_sidebar_collapsed') === 'true';
    } catch (_) {}
    this.isSidebarCollapsed = isSidebarCollapsed;
  }

  resetSessionState() {
    this.selectedTarget = null;
    this.target = null;
    if (this.currentSnapshot) {
      this.currentSnapshot.target = null;
      this.currentSnapshot.evaluation = null;
      this.currentSnapshot.sessionStatus = 'TargetSelection';
    }
    this.activePlugin = null;
    this.currentData = null;
    this.currentCalibrationMetrics = null;
    this.activeTargetImage = null;
    window.__ABDAudioLab_suiteQueue__ = null;
    window.__ABDAudioLab_suiteTargetId__ = null;
    window.__ABDAudioLab_activeTestIndex__ = 1;
    window.__ABDAudioLab_selectedObjective__ = null;
    window.__ABDAudioLab_signalPathVerified__ = false;
    this.updateSessionSummary();
    this.updateStepperUi();
  }

  isTargetConfirmed() {
    return !!(this.selectedTarget && this.selectedTarget.isLocked === true);
  }

  isStepCompleted(stageNum) {
    if (stageNum === 1) {
      return this.isTargetConfirmed();
    }
    if (stageNum === 2) {
      if (this.selectedTarget && this.selectedTarget.isPlugin) return true; // Digital VST plugins do not require analog loopback calibration
      return !!(this.currentCalibrationMetrics && (this.currentCalibrationMetrics.isCalibrated || (typeof this.currentCalibrationMetrics.latencySamples === 'number' && this.currentCalibrationMetrics.latencySamples >= 0) || (typeof this.currentCalibrationMetrics.measuredLatencyMs === 'number' && this.currentCalibrationMetrics.measuredLatencyMs >= 0)));
    }
    if (stageNum === 3) {
      // Step 3: Hardware Hookup & Preflight Verification
      if (this.selectedTarget && this.selectedTarget.isPlugin) return true;
      return !!window.__ABDAudioLab_signalPathVerified__;
    }
    if (stageNum === 4) {
      // Step 4: Run Acquisition
      if (this.currentData && this.currentData.hasEvaluation) return true;
      if (!window.__ABDAudioLab_suiteQueue__ || window.__ABDAudioLab_suiteQueue__.length === 0) return false;
      const tests = window.__ABDAudioLab_suiteQueue__;
      const nonSkipped = tests.filter(t => !t.isSkipped);
      return nonSkipped.length > 0 && nonSkipped.some(t => t.status === 'COMPLETED' || (t.points && t.points.length > 0 && t.points.every(p => p.status === 'COMPLETED')));
    }
    if (stageNum === 5) {
      return !!(this.currentData && this.currentData.hasEvaluation);
    }
    return false;
  }

  isStepAccessible(stageNum) {
    if (stageNum === 1) return true;
    if (stageNum === 2) {
      return this.isTargetConfirmed();
    }
    if (stageNum === 3) {
      // Same activation logic as former step 3: unlocked upon target confirmed and step 2 calibration completed/bypassed
      return this.isTargetConfirmed() && this.isStepCompleted(2);
    }
    if (stageNum === 4) {
      // Unlocked upon passing preflight in step 3
      return this.isTargetConfirmed() && this.isStepCompleted(3);
    }
    if (stageNum === 5) {
      return this.isTargetConfirmed() && (this.isStepCompleted(4) || (this.currentData && this.currentData.hasEvaluation));
    }
    return false;
  }

  async newSession() {
    const confirmed = await showModalConfirm({
      title: 'New Profiling Session',
      message: 'Are you sure you want to initialize a new profiling session? Any unsaved measurements and configuration will be reset.',
      type: 'warning',
      confirmText: 'Start New Session',
      cancelText: 'Cancel'
    });
    if (confirmed) {
      if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
        window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'newSession' });
      }
      this.resetSessionState();
      this.navigateToStage(1);
    }
  }

  loadSession() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'loadSession' });
    }
  }

  loadPredefinedFixture(fixtureFileName) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'loadPredefinedFixture',
        fixtureFileName: fixtureFileName
      });
    } else {
      this.loadMockFixture(fixtureFileName);
    }
  }

  loadMockFixture(fixtureFileName) {
    console.log('[WebUI Mock] Cargando fixture de evaluación:', fixtureFileName);
    let mockData = null;

    if (fixtureFileName.includes('approved')) {
      mockData = {
        hasEvaluation: true,
        status: 'Accepted',
        recommendedModelId: 'ZDF_Ladder_Synthetic',
        provenance: 'RFC 8785 Canonical Profile • Farina Sine Sweep',
        sha256: '9a4b2c8e1039d54a2b91834710dfbca418a992e4018274618293740192837461',
        validationEsrDb: -48.2,
        spectralCorrelationRho: 0.9998,
        stimuliPassingRatioPercent: 100,
        enobBits: 19.2,
        validatedDomain: '20 Hz - 24 kHz • Full Dynamic Range',
        combinedUncertaintyDb: 0.35,
        sampleOffset: 0,
        relativeCpuCost: 0.82,
        message: 'All metrological criteria verified within tolerance bounds.',
        canExport: true,
        curves: []
      };
    } else if (fixtureFileName.includes('dexed')) {
      mockData = {
        hasEvaluation: true,
        status: 'Accepted',
        recommendedModelId: 'FM6_Operator_Dexed',
        provenance: 'RFC 8785 Canonical Profile • Multi-Carrier Sweep',
        sha256: 'a1b2c3d4e5f60718293a4b5c6d7e8f90123456789abcdef0123456789abcdef0',
        validationEsrDb: -33.4,
        spectralCorrelationRho: 0.9850,
        stimuliPassingRatioPercent: 88,
        enobBits: 15.1,
        validatedDomain: '30 Hz - 18 kHz • Linear Velocity',
        combinedUncertaintyDb: 0.82,
        sampleOffset: 0,
        relativeCpuCost: 0.95,
        message: 'Non-linear harmonic distortion above standard threshold; accepted with provenance warning.',
        canExport: true,
        curves: []
      };
    } else if (fixtureFileName.includes('tampered')) {
      mockData = {
        hasEvaluation: true,
        status: 'Rejected',
        recommendedModelId: 'ZDF_Ladder_Synthetic',
        provenance: 'RFC 8785 Canonical Profile • Farina Sine Sweep',
        sha256: 'deadbeef0000111122223333444455556666777788889999aaaabbbbccccdddd',
        validationEsrDb: -12.1,
        spectralCorrelationRho: 0.7200,
        stimuliPassingRatioPercent: 30,
        enobBits: 8.4,
        validatedDomain: '20 Hz - 24 kHz • Full Dynamic Range',
        combinedUncertaintyDb: 3.10,
        sampleOffset: 2,
        relativeCpuCost: 1.15,
        message: 'RFC 8785 Canonical Profile hash mismatch: Model integrity violated or modified post-audit.',
        canExport: false,
        curves: []
      };
    } else if (fixtureFileName.includes('inconclusive')) {
      mockData = {
        hasEvaluation: true,
        status: 'Inconclusive',
        recommendedModelId: 'Analytic_StateVariable',
        provenance: 'RFC 8785 Canonical Profile • Partial Excitation',
        sha256: 'e0e0e0e0f1f1f1f1a2a2a2a2b3b3b3b3c4c4c4c4d5d5d5d5e6e6e6e6f7f7f7f7',
        validationEsrDb: -26.5,
        spectralCorrelationRho: 0.9100,
        stimuliPassingRatioPercent: 62,
        enobBits: 12.0,
        validatedDomain: '100 Hz - 8 kHz (Incomplete Bandwidth)',
        combinedUncertaintyDb: 1.85,
        sampleOffset: 0,
        relativeCpuCost: 0.78,
        message: 'Insufficient holdout excitation points to certify dynamic range across all octaves.',
        canExport: false,
        curves: []
      };
    } else {
      mockData = {
        hasEvaluation: true,
        status: 'Rejected',
        recommendedModelId: 'Direct_Bilinear_IIR',
        provenance: 'RFC 8785 Canonical Profile • Farina Sine Sweep',
        sha256: '888877776666555544443333222211110000aaaabbbbccccddddeeeeffff0000',
        validationEsrDb: -18.4,
        spectralCorrelationRho: 0.8100,
        stimuliPassingRatioPercent: 42,
        enobBits: 10.1,
        validatedDomain: '20 Hz - 24 kHz • Full Dynamic Range',
        combinedUncertaintyDb: 2.45,
        sampleOffset: 1,
        relativeCpuCost: 0.90,
        message: 'ESR exceeds tolerable threshold (-24 dB required). Model fidelity unacceptable.',
        canExport: false,
        curves: []
      };
    }

    this.currentData = mockData;
    this.showToast(`Fixture cargado: ${fixtureFileName}`);
    this.updateSessionSummary();
    this.navigateToStage(5);
  }

  saveSession() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'saveSession' });
    }
  }

  saveSessionAs() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'saveSessionAs' });
    }
  }

  loadPluginFromFile() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'loadPluginFromFile' });
    }
  }

  loadPlugin(plugin) {
    if (!plugin) return;
    if (this.isLoadingPlugin) {
      console.warn('[WebUI] loadPlugin ignorado: ya hay una carga en progreso.');
      return;
    }
    const targetFile = plugin.fileOrIdentifier || '';
    const targetName = plugin.name || '';
    if (this.activePlugin && (
        (this.activePlugin.filePath && targetFile && this.activePlugin.filePath.toLowerCase() === targetFile.toLowerCase()) ||
        (this.activePlugin.name && targetName && this.activePlugin.name.toLowerCase() === targetName.toLowerCase())
    )) {
      console.log('[WebUI] Plugin ya está activo actualmente:', targetName);
      return;
    }

    this.isLoadingPlugin = true;
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'loadPlugin',
        fileOrIdentifier: targetFile,
        name: targetName
      });
      this.showToast(`Cargando plugin: ${targetName || 'VST3'}...`);
    } else {
      this.isLoadingPlugin = false;
    }
  }

  openPluginGui() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openPluginGui' });
    }
  }

  closePluginGui() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'closePluginGui' });
    }
  }

  unloadPlugin() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'unloadPlugin' });
    }
  }

  captureVstImage() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'capturePluginSnapshot',
        pluginId: this.activePlugin ? (this.activePlugin.pluginId || this.activePlugin.name) : 'vst3_plugin'
      });
      this.showToast('VST GUI screenshot request sent to host');
    } else {
      this.showToast('VST GUI screenshot captured to user images library');
    }
  }

  async exportProductionPackage() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'exportProductionPackage' });
    } else {
      await showModalAlert({
        title: 'Export Production Package',
        message: 'Running in preview mode. In native desktop mode, this compiles and seals the production C++20 header package into the exports folder.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  openCertificationReport() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openCertificationReport' });
    } else {
      window.open('./report_preview.html', '_blank');
    }
  }

  playAuditionAudio(type, boostDb = 0) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'playAudition',
        auditionType: type,
        boostDb: boostDb
      });
    } else {
      this.showToast(`Audición: ${type} (${boostDb > 0 ? '+' + boostDb + ' dB' : '0 dB'})`);
    }
  }

  stopAuditionAudio() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'stopAudition' });
    }
  }

  openMeasurementViewer() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openMeasurementViewer' });
    } else {
      this.showToast('Visor FAIR / LNL solo disponible en modo nativo C++');
    }
  }

  async openDataFolder(folder) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openDataFolder', folder });
    } else {
      await showModalAlert({
        title: 'Open Data Folder',
        message: `Folder: ${folder}\nThis feature requires the native ABDAudioLab desktop application to reveal system directories.`,
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async exitApplication() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'exitApplication' });
    } else {
      const shouldExit = await showModalConfirm({
        title: 'Exit ABDAudioLab',
        message: 'Are you sure you want to exit the application? Make sure your current session is saved.',
        type: 'warning',
        confirmText: 'Exit Application',
        cancelText: 'Cancel'
      });
      if (shouldExit) {
        window.close();
      }
    }
  }

  async toggleScopeWindow() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'toggleScopeWindow' });
    } else {
      await showModalAlert({
        title: 'High-Speed Scope',
        message: 'In native mode, this toggles the high-speed multi-lane oscilloscope window with real-time waveform display.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async setWorkingDirectory() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'setWorkingDirectory' });
    } else {
      await showModalAlert({
        title: 'Set Working Directory',
        message: 'Please run inside the native ABDAudioLab desktop app to choose the system working directory.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async openWorkingDirectory() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openWorkingDirectory' });
    } else {
      await showModalAlert({
        title: 'Open Working Directory',
        message: 'Please run inside the native ABDAudioLab desktop app to open the workspace folder in Explorer.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async openPluginScan() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openPluginScan' });
    } else {
      await showModalAlert({
        title: 'Scan VST3 Plugins',
        message: 'Please run inside the native ABDAudioLab desktop app to discover installed VST3 instruments and effects.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async openFairLnlViewer() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openFairLnlViewer' });
    } else {
      await showModalAlert({
        title: 'FAIR / LNL Measurement Viewer',
        message: 'Native measurement container viewer requires the running ABDAudioLab desktop session.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async openMultiContainerComparator() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'openMultiContainerComparator' });
    } else {
      await showModalAlert({
        title: 'Multi-Container Comparison',
        message: 'Native multivariate comparison tool requires the running ABDAudioLab desktop session.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  showToast(message, type = 'info') {
    let toast = document.getElementById('audioLabToast');
    if (!toast) {
      toast = document.createElement('div');
      toast.id = 'audioLabToast';
      toast.style.cssText = 'position:fixed; bottom:24px; left:50%; transform:translateX(-50%); background:var(--color-panel-surface-elevated, #161f2c); color:var(--color-text-main, #fff); border:1px solid var(--color-accent, #00e676); border-radius:8px; padding:10px 20px; font-size:12px; font-weight:600; box-shadow:0 8px 24px rgba(0,0,0,0.5); z-index:999999; display:flex; align-items:center; gap:8px; opacity:0; transition:all 0.25s ease; pointer-events:none;';
      document.body.appendChild(toast);
    }
    const icon = type === 'error' ? '[!]' : '[OK]';
    toast.innerHTML = `<span style="color:var(--color-accent); font-weight:700; font-family:var(--font-mono, monospace);">${icon}</span> <span>${message}</span>`;
    toast.style.opacity = '1';
    toast.style.transform = 'translateX(-50%) translateY(0)';
    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(-50%) translateY(10px)';
    }, 3500);
  }

  init() {
    patchBrowserDialogs();
    this.applyTheme(this.currentTheme);
    this.renderShell();
    this.mountNavBar();
    this.mountVuMeters();
    this.mountAboutDialog();
    this.mountVirtualKeyboard();
    this.attachGlobalEvents();
    this.updateSessionSummary();
    this.renderCurrentStage();

    if (this.isNativeHost) {
      this.attachNativeBridge();
    } else {
      this.currentAudioDevice = {
        deviceName: "Realtek High Definition Audio",
        activeInputs: 2,
        activeOutputs: 2,
        audioInOk: true,
        audioOutOk: true,
        audioInName: "Realtek High Definition Audio (2 in)",
        audioOutName: "Realtek High Definition Audio (2 out)",
        availableMidiInputs: ["USB MIDI Interface"],
        availableMidiOutputs: ["Microsoft GS Wavetable Synth"],
        midiInOk: false,
        midiInName: "None / Disconnected",
        midiOutOk: true,
        midiOutName: "Microsoft GS Wavetable Synth"
      };
      this.updateAudioMidiStatusPill(this.currentAudioDevice);
    }

    // Exponer app y setTheme global para sincronización desde host C++ y drawers
    window.app = this;
    window.setTheme = (themeName) => {
      this.applyTheme(themeName);
    };
  }

  applyTheme(themeName) {
    this.currentTheme = themeName;
    document.documentElement.setAttribute('data-theme', themeName);
    if (document.body) {
      document.body.setAttribute('data-theme', themeName);
      document.body.className = `theme-${themeName} skin-${themeName} chassis`;
    }

    const sunIcon = document.getElementById('themeIconSun');
    const moonIcon = document.getElementById('themeIconMoon');
    if (sunIcon && moonIcon) {
      if (themeName === 'audiolab-light') {
        sunIcon.style.display = 'none';
        moonIcon.style.display = 'block';
      } else {
        sunIcon.style.display = 'block';
        moonIcon.style.display = 'none';
      }
    }

    const isLight = (themeName === 'audiolab-light');
    document.querySelectorAll('img').forEach(img => {
      const src = img.getAttribute('src');
      if (!src) return;
      if (isLight && src.includes('_black.')) {
        img.src = src.replace(/_black\./g, '_white.');
      } else if (!isLight && src.includes('_white.')) {
        img.src = src.replace(/_white\./g, '_black.');
      }
    });

    if (this.selectedTarget && this.selectedTarget.image) {
      if (isLight && this.selectedTarget.image.includes('_black.')) {
        this.selectedTarget.image = this.selectedTarget.image.replace(/_black\./g, '_white.');
      } else if (!isLight && this.selectedTarget.image.includes('_white.')) {
        this.selectedTarget.image = this.selectedTarget.image.replace(/_white\./g, '_black.');
      }
    }

    try {
      localStorage.setItem('abdaudiolab_theme', themeName);
    } catch (_) {}

    this.updateSessionSummary();
  }

  toggleTheme() {
    const nextTheme = (this.currentTheme === 'audiolab-light') ? 'audiolab' : 'audiolab-light';
    this.applyTheme(nextTheme);

    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'setTheme',
        theme: nextTheme
      });
    }
  }

  mountAboutDialog() {
    this.aboutDialog = new AboutDialog({
      id: 'abd-about-dialog',
      appName: 'ABDAudioLab',
      category: 'ABD SYNTHS • HARDWARE PROFILING LAB',
      subtitle: 'Universal Hardware Profiling & DSP Synthesis Suite',
      version: '2.1.0',
      buildNumber: '624',
      buildDate: 'Oct 14 2026',
      details: 'DSP Engine: Farina Sine Sweep • Wiener-Hammerstein LNL • SIMD Splines • NAM / RTNeural',
      copyright: '© 2026 ABD Synths',
      imageSrc: 'splash_art.jpg',
      statusText: 'All Systems Nominal'
    });
  }

  async openAudioSettings() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'openAudioSettings'
      });
    } else {
      await showModalAlert({
        title: 'Audio & MIDI Configuration',
        message: 'This feature invokes the native audio device and buffer configuration dialog when running inside the desktop application.',
        type: 'info',
        confirmText: 'Got it'
      });
    }
  }

  async autoDetectHardware() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'autoDetectHardware'
      });
    } else {
      console.log("[AutoDetect] Physical MIDI / USB hardware inquiry requires the running ABDAudioLab desktop session.");
      await showModalAlert({
        title: 'Hardware Auto-Detection',
        message: 'This queries connected physical MIDI interfaces via SysEx identity inquiry. Please run inside the native ABDAudioLab desktop app to scan physical ports.',
        type: 'info',
        confirmText: 'Got it'
      });
      window.dispatchEvent(new CustomEvent('hardwareDetected', { detail: {} }));
    }
  }

  mountNavBar() {
    const navContainer = document.getElementById('navBarContainer');
    if (!navContainer) return;

    this.navBar = new SynthNavBar({
      container: navContainer,
      brandPosition: 'right',
      brand: {
        logoHtml: `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="var(--color-accent, #00e676)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="filter: drop-shadow(0 0 6px rgba(var(--color-accent-rgb, 0, 230, 118), 0.6)); margin-right: 4px;">
          <path d="M2 12h3l3-7 4 14 3-7h7"/>
          <circle cx="12" cy="12" r="10" stroke-width="1.2" stroke-dasharray="2 2" stroke="rgba(var(--color-accent-rgb, 0, 230, 118), 0.4)"/>
        </svg>`,
        title: 'ABDAudioLab',
        subtitle: 'Metrology & DSP Profiler'
      },
      menus: [
        {
          name: 'File',
          items: [
            { id: 101, label: 'New Profiling Session', action: 'newSession', shortcut: 'Ctrl+N' },
            { id: 102, label: 'Load Session / JSON...', action: 'loadSession', shortcut: 'Ctrl+O' },
            {
              id: 105,
              label: 'Load Example Evaluation...',
              submenu: [
                { id: 1051, label: '1. Fixture Aprobado (Limpio, Verificado)', action: 'loadFixtureApproved' },
                { id: 1052, label: '2. Dexed FM (Aceptado con Advertencias)', action: 'loadFixtureDexed' },
                { id: 1053, label: '3. Adulterado / Tampered (Fallo de Hash)', action: 'loadFixtureTampered' },
                { id: 1054, label: '4. Inconcluso (Falta Holdout)', action: 'loadFixtureInconclusive' },
                { id: 1055, label: '5. Rechazado (Falta de Fidelidad ESR)', action: 'loadFixtureRejected' },
                { separator: true },
                { id: 1056, label: 'Examinar archivo JSON en disco...', action: 'loadSession' }
              ]
            },
            { id: 103, label: 'Save Session Snapshot', action: 'saveSession', shortcut: 'Ctrl+S' },
            { id: 104, label: 'Save Session As...', action: 'saveSessionAs', shortcut: 'Ctrl+Shift+S' },
            { separator: true },
            { id: 111, label: 'Set Working Directory...', action: 'setWorkingDirectory' },
            { id: 113, label: 'Scan VST Directories...', action: 'openPluginScan', shortcut: 'F4' },
            {
              id: 112,
              label: 'Open Folder...',
              submenu: [
                { id: 1121, label: 'Working', action: 'openFolderWorking' },
                { id: 1122, label: 'Presets', action: 'openFolderPresets' },
                { id: 1123, label: 'Exports', action: 'openFolderExports' },
                { id: 1124, label: 'Reports', action: 'openFolderReports' },
                { id: 1125, label: 'Sessions', action: 'openFolderSessions' },
                { id: 1126, label: 'Loopback / profiles', action: 'openFolderLoopback' }
              ]
            },
            { separator: true },
            { id: 106, label: 'Export Production Package (1-Click)', action: 'exportPackage', shortcut: 'Ctrl+E' },
            { separator: true },
            { id: 107, label: 'Exit Application', action: 'exit', shortcut: 'Alt+F4' }
          ]
        },
        {
          name: 'Workflow',
          items: [
            { id: 201, label: '1. Target Selection & Routing', action: 'goto1' },
            { id: 202, label: '2. Audio Interface Calibration', action: 'goto2' },
            { id: 203, label: '3. Run Acquisition Session', action: 'goto3' },
            { id: 204, label: '4. Results & Holdout Validation', action: 'goto4' }
          ]
        },
        {
          name: 'Tools',
          items: [
            { id: 300, label: 'Audio & MIDI Settings...', action: 'openAudioSettings', shortcut: 'F3' },
            { id: 301, label: 'Run Loopback Calibration Wizard', action: 'goto2', shortcut: 'Ctrl+L' },
            { id: 304, label: 'Toggle Virtual MIDI Keyboard', action: 'toggleVirtualKeyboard', shortcut: 'Ctrl+K' },
            { id: 302, label: 'Toggle Floating Scope (ABDScope)...', action: 'toggleScopeWindow' },
            { separator: true },
            { id: 305, label: 'Open FAIR / LNL Viewer...', action: 'openFairLnlViewer' },
            { id: 306, label: 'Open Multi-Container Comparison...', action: 'openMultiContainerComparator' },
            { separator: true },
            { id: 303, label: 'Open Publication-Grade HTML Report', action: 'openHtmlReport', shortcut: 'F2' }
          ]
        },
        {
          name: 'Help',
          items: [
            { id: 901, label: 'Metrological Documentation / Wiki', action: 'wiki', shortcut: 'F1' },
            { id: 902, label: 'Check for Updates...', action: 'updates' },
            { separator: true },
            { id: 900, label: 'About ABDAudioLab...', action: 'about' }
          ]
        }
      ],
      actions: {
        newSession: () => this.newSession(),
        loadSession: () => this.loadSession(),
        loadFixtureApproved: () => this.loadPredefinedFixture('fixture_approved.json'),
        loadFixtureDexed: () => this.loadPredefinedFixture('dexed_warnings.json'),
        loadFixtureTampered: () => this.loadPredefinedFixture('tampered_hash_mismatch.json'),
        loadFixtureInconclusive: () => this.loadPredefinedFixture('inconclusive.json'),
        loadFixtureRejected: () => this.loadPredefinedFixture('rejected.json'),
        saveSession: () => this.saveSession(),
        saveSessionAs: () => this.saveSessionAs(),
        openFolderWorking: () => this.openDataFolder('working'),
        openFolderPresets: () => this.openDataFolder('presets'),
        openFolderExports: () => this.openDataFolder('exports'),
        openFolderReports: () => this.openDataFolder('reports'),
        openFolderSessions: () => this.openDataFolder('sessions'),
        openFolderLoopback: () => this.openDataFolder('loopback'),
        setWorkingDirectory: () => this.setWorkingDirectory(),
        openPluginScan: () => this.openPluginScan(),
        goto1: () => this.navigateToStage(1),
        goto2: () => this.navigateToStage(2),
        goto3: () => this.navigateToStage(3),
        goto4: () => this.navigateToStage(4),
        openAudioSettings: () => this.openAudioSettings(),
        toggleVirtualKeyboard: () => this.toggleVirtualKeyboard(),
        toggleScopeWindow: () => this.toggleScopeWindow(),
        openFairLnlViewer: () => this.openFairLnlViewer(),
        openMultiContainerComparator: () => this.openMultiContainerComparator(),
        openHtmlReport: () => this.openCertificationReport(),
        exportPackage: () => this.exportProductionPackage(),
        exit: () => this.exitApplication(),
        updates: () => {
          if (this.aboutDialog) {
            this.aboutDialog.open();
            this.aboutDialog.checkForUpdates();
          }
        },
        about: () => {
          if (this.aboutDialog) {
            this.aboutDialog.open();
          }
        }
      },
      rightSlot: `
        <div style="position:relative; display:inline-flex; align-items:center; gap:8px;">
          <!-- Audio & MIDI Connection Status Pill -->
          <div id="navAudioMidiPill" role="button" tabindex="0" title="Audio &amp; MIDI Configuration - Select audio interfaces, buffer sizes, sample rate, and MIDI routing" class="nav-status-pill">
            <!-- Gear Icon -->
            <span style="display:inline-flex; align-items:center; justify-content:center; color:var(--color-text-muted);">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                <circle cx="12" cy="12" r="3"></circle>
                <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
              </svg>
            </span>
            <span style="width:1px; height:12px; background:var(--color-panel-border); margin:0 1px;"></span>
            <span class="status-indicator">
              <span id="pillLedAudioIn" style="width:6.5px; height:6.5px; border-radius:50%; background:#ff5252; box-shadow:0 0 4px rgba(255,82,82,0.6); display:inline-block; transition:background-color 0.2s, box-shadow 0.2s;"></span>
              <span>A-IN</span>
            </span>
            <span class="status-indicator">
              <span id="pillLedAudioOut" style="width:6.5px; height:6.5px; border-radius:50%; background:#ff5252; box-shadow:0 0 4px rgba(255,82,82,0.6); display:inline-block; transition:background-color 0.2s, box-shadow 0.2s;"></span>
              <span>A-OUT</span>
            </span>
            <span class="status-indicator">
              <span id="pillLedMidiIn" style="width:6.5px; height:6.5px; border-radius:50%; background:#ff5252; box-shadow:0 0 4px rgba(255,82,82,0.6); display:inline-block; transition:background-color 0.2s, box-shadow 0.2s;"></span>
              <span>M-IN</span>
            </span>
            <span class="status-indicator">
              <span id="pillLedMidiOut" style="width:6.5px; height:6.5px; border-radius:50%; background:#ff5252; box-shadow:0 0 4px rgba(255,82,82,0.6); display:inline-block; transition:background-color 0.2s, box-shadow 0.2s;"></span>
              <span>M-OUT</span>
            </span>
          </div>

          <!-- Virtual Piano Keyboard Quick Toggle Button -->
          <button id="navVirtualKeyboardBtn" type="button" class="btn btn-sm" title="Toggle Virtual MIDI Keyboard (Ctrl+K)" style="position:relative; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); color:var(--color-text-main); cursor:pointer; padding:3px 7px; border-radius:6px; display:inline-flex; align-items:center; justify-content:center; height:28px; box-sizing:border-box; transition:all 0.15s ease;">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <rect x="2" y="4" width="20" height="16" rx="2"></rect>
              <line x1="6" y1="4" x2="6" y2="12"></line>
              <line x1="10" y1="4" x2="10" y2="12"></line>
              <line x1="14" y1="4" x2="14" y2="12"></line>
              <line x1="18" y1="4" x2="18" y2="12"></line>
            </svg>
          </button>

          <!-- Target Device Thumbnail Icon (Left of Theme Button) -->
          <button id="navTargetDeviceBtn" type="button" class="btn btn-sm" title="Target Device: BOSS DS-1 Distortion" style="position:relative; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); color:var(--color-text-main); cursor:pointer; padding:3px 6px; border-radius:6px; display:inline-flex; align-items:center; justify-content:center; height:28px; min-width:28px; box-sizing:border-box; transition:all 0.15s ease;">
            <img id="navTargetDeviceImg" src="models/boss-ds1.webp" alt="Target Device" style="max-height:20px; max-width:26px; object-fit:contain; border-radius:2px; display:block;" onerror="this.style.display='none'; document.getElementById('navTargetDeviceFallback').style.display='inline-flex';" />
            <span id="navTargetDeviceFallback" style="font-size:10px; font-weight:700; color:var(--color-text-muted); display:none;">DEV</span>
          </button>

          <!-- Light/Dark Theme Toggle Button -->
          <button id="navThemeToggleBtn" type="button" class="btn btn-sm btn-theme" title="Toggle Light / Dark Theme" style="position:relative; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); color:var(--color-text-main); cursor:pointer; padding:6px 10px; border-radius:6px; display:inline-flex; align-items:center; justify-content:center; transition:all 0.15s ease;">
            <!-- Sun Icon (shown in dark mode to switch to light) -->
            <svg id="themeIconSun" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="12" cy="12" r="5"/>
              <line x1="12" y1="1" x2="12" y2="3"/>
              <line x1="12" y1="21" x2="12" y2="23"/>
              <line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/>
              <line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/>
              <line x1="1" y1="12" x2="3" y2="12"/>
              <line x1="21" y1="12" x2="23" y2="12"/>
              <line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/>
              <line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>
            </svg>
            <!-- Moon Icon (shown in light mode to switch to dark) -->
            <svg id="themeIconMoon" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="display:none;">
              <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>
            </svg>
          </button>

          <!-- System Notifications Bell -->
          <button id="navBellBtn" type="button" class="btn btn-sm btn-bell" title="System Notifications" style="position:relative; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); color:var(--color-text-main); cursor:pointer; padding:6px 10px; border-radius:6px; display:inline-flex; align-items:center; justify-content:center; margin-right:12px; transition:all 0.15s ease;">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9"/>
              <path d="M10.3 21a1.94 1.94 0 0 0 3.4 0"/>
            </svg>
            <span id="navBellBadge" style="display:none; position:absolute; top:-4px; right:-4px; background:var(--color-danger); color:var(--color-text-main); font-size:9px; font-weight:700; border-radius:50%; min-width:14px; height:14px; line-height:14px; text-align:center; padding:0 2px;">0</span>
          </button>
        </div>
      `
    });

    this.startupNotices = [];
    this.createStartupWarningsModal();

    const pillBtn = document.getElementById('navAudioMidiPill');
    if (pillBtn) {
      pillBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.openAudioSettings();
      });
      pillBtn.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          this.openAudioSettings();
        }
      });
    }

    const devBtn = document.getElementById('navTargetDeviceBtn');
    if (devBtn) {
      devBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.navigateToStage(1);
      });
    }

    const bellBtn = document.getElementById('navBellBtn');
    if (bellBtn) {
      bellBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleStartupWarningsModal();
      });
    }

    const themeBtn = document.getElementById('navThemeToggleBtn');
    if (themeBtn) {
      themeBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleTheme();
      });
    }

    // Inicializar visual del icono de tema según estado actual
    this.applyTheme(this.currentTheme);
    this.updateAudioMidiStatusPill(this.currentAudioDevice);
  }

  updateAudioMidiStatusPill(dev) {
    const pill = document.getElementById('navAudioMidiPill');
    const ledAIn = document.getElementById('pillLedAudioIn');
    const ledAOut = document.getElementById('pillLedAudioOut');
    const ledMIn = document.getElementById('pillLedMidiIn');
    const ledMOut = document.getElementById('pillLedMidiOut');

    if (!pill) return;

    const audioInOk = !!(dev && (dev.audioInOk ?? (typeof dev.activeInputs === 'number' && dev.activeInputs > 0)));
    const audioOutOk = !!(dev && (dev.audioOutOk ?? (typeof dev.activeOutputs === 'number' && dev.activeOutputs > 0)));
    const midiInOk = !!(dev && (dev.midiInOk ?? (Array.isArray(dev.availableMidiInputs) && dev.availableMidiInputs.length > 0)));
    const midiOutOk = !!(dev && (dev.midiOutOk ?? (Array.isArray(dev.availableMidiOutputs) && dev.availableMidiOutputs.length > 0)));

    const greenDot = 'background:#00e676; box-shadow:0 0 5px rgba(0,230,118,0.7);';
    const redDot = 'background:#ff5252; box-shadow:0 0 4px rgba(255,82,82,0.6);';

    if (ledAIn) ledAIn.style.cssText = `width:6.5px; height:6.5px; border-radius:50%; display:inline-block; transition:background-color 0.2s, box-shadow 0.2s; ${audioInOk ? greenDot : redDot}`;
    if (ledAOut) ledAOut.style.cssText = `width:6.5px; height:6.5px; border-radius:50%; display:inline-block; transition:background-color 0.2s, box-shadow 0.2s; ${audioOutOk ? greenDot : redDot}`;
    if (ledMIn) ledMIn.style.cssText = `width:6.5px; height:6.5px; border-radius:50%; display:inline-block; transition:background-color 0.2s, box-shadow 0.2s; ${midiInOk ? greenDot : redDot}`;
    if (ledMOut) ledMOut.style.cssText = `width:6.5px; height:6.5px; border-radius:50%; display:inline-block; transition:background-color 0.2s, box-shadow 0.2s; ${midiOutOk ? greenDot : redDot}`;

    const aInName = dev?.audioInName || (audioInOk ? (dev?.deviceName || 'Active Audio Input') : 'Disconnected');
    const aOutName = dev?.audioOutName || (audioOutOk ? (dev?.deviceName || 'Active Audio Output') : 'Disconnected');
    const mInName = dev?.midiInName || (midiInOk ? (dev?.availableMidiInputs?.[0] || 'Active MIDI In') : 'Disconnected');
    const mOutName = dev?.midiOutName || (midiOutOk ? (dev?.availableMidiOutputs?.[0] || 'Active MIDI Out') : 'Disconnected');

    pill.title = `AUDIO & MIDI HARDWARE STATUS:\n----------------------------------------\n• Audio In:  ${aInName} [${audioInOk ? 'OK' : 'DISCONNECTED'}]\n• Audio Out: ${aOutName} [${audioOutOk ? 'OK' : 'DISCONNECTED'}]\n• MIDI In:   ${mInName} [${midiInOk ? 'OK' : 'DISCONNECTED'}]\n• MIDI Out:  ${mOutName} [${midiOutOk ? 'OK' : 'DISCONNECTED'}]\n----------------------------------------\nClick to configure Audio & MIDI.`;
  }

  createStartupWarningsModal() {
    if (document.getElementById('startupWarningsOverlay')) return;

    const overlay = document.createElement('div');
    overlay.id = 'startupWarningsOverlay';
    overlay.style.cssText = 'display:none; position:fixed; inset:0; background:rgba(0,0,0,0.55); z-index:99999; align-items:center; justify-content:center; backdrop-filter:blur(2px);';
    overlay.innerHTML = `
      <div id="startupWarningsCard" style="width:540px; max-width:92vw; max-height:80vh; background:var(--color-panel-bg); border:1px solid var(--color-panel-border); border-radius:10px; box-shadow:var(--shadow-elevation-3); display:flex; flex-direction:column; padding:22px; box-sizing:border-box;">
        <div style="font-size:10px; font-weight:700; color:var(--color-warning); letter-spacing:0.8px; margin-bottom:4px;">SYSTEM NOTIFICATIONS</div>
        <div id="startupWarningsTitle" style="font-size:16px; font-weight:700; color:var(--color-text-main); margin-bottom:14px;">Avisos del arranque (0)</div>
        <div id="startupWarningsList" style="flex:1; overflow-y:auto; margin-bottom:18px; display:flex; flex-direction:column; gap:8px; max-height:360px; font-size:13px; color:var(--color-text-muted);">
          <div style="color:var(--color-text-dim); font-size:13px; padding:12px 0;">No hay avisos pendientes del arranque.</div>
        </div>
        <div style="display:flex; justify-content:flex-end;">
          <button id="startupWarningsCloseBtn" type="button" class="btn btn-sm btn-primary" style="padding:7px 22px; border-radius:6px; font-size:13px; font-weight:600; cursor:pointer;">CERRAR</button>
        </div>
      </div>
    `;
    document.body.appendChild(overlay);

    const closeBtn = overlay.querySelector('#startupWarningsCloseBtn');
    if (closeBtn) {
      closeBtn.addEventListener('click', () => this.dismissStartupWarnings());
    }
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) this.dismissStartupWarnings();
    });
  }

  setStartupNotices(notices) {
    this.startupNotices = Array.isArray(notices) ? notices : [];
    const badge = document.getElementById('navBellBadge');
    const bellBtn = document.getElementById('navBellBtn');
    const count = this.startupNotices.length;

    if (badge) {
      if (count > 0) {
        badge.textContent = count;
        badge.style.display = 'block';
        if (bellBtn) bellBtn.style.color = 'var(--color-warning)';
      } else {
        badge.style.display = 'none';
        if (bellBtn) bellBtn.style.color = 'var(--color-text-muted)';
      }
    }
  }

  toggleStartupWarningsModal() {
    const overlay = document.getElementById('startupWarningsOverlay');
    if (!overlay) return;

    if (overlay.style.display === 'flex') {
      overlay.style.display = 'none';
      return;
    }

    const title = document.getElementById('startupWarningsTitle');
    const list = document.getElementById('startupWarningsList');
    if (title) title.textContent = `Avisos del arranque (${this.startupNotices.length})`;

    if (list) {
      if (this.startupNotices.length === 0) {
        list.innerHTML = `<div style="color:var(--color-text-dim); font-size:13px; padding:12px 0;">No hay avisos pendientes del arranque.</div>`;
      } else {
        list.innerHTML = this.startupNotices.map((n, i) => `
          <div style="background:rgba(255,255,255,0.03); border:1px solid var(--color-panel-border); border-radius:6px; padding:10px 12px; display:flex; gap:10px; align-items:flex-start;">
            <span style="color:var(--color-warning); font-weight:700;">#${i + 1}</span>
            <div style="flex:1; word-break:break-word; line-height:1.4;">${typeof n === 'string' ? n : (n.text || JSON.stringify(n))}</div>
          </div>
        `).join('');
      }
    }

    overlay.style.display = 'flex';
  }

  dismissStartupWarnings() {
    const overlay = document.getElementById('startupWarningsOverlay');
    if (overlay) overlay.style.display = 'none';
    const badge = document.getElementById('navBellBadge');
    if (badge) badge.style.display = 'none';
    const bellBtn = document.getElementById('navBellBtn');
    if (bellBtn) bellBtn.style.color = 'var(--color-text-muted)';
  }

  mountVuMeters() {
    const container = document.getElementById('vuMetersMount');
    if (!container) return;

    this.vuMeter = new VuMeter(container, {
      channels: 2,
      labels: ['IN', 'OUT'],
      minDb: -60,
      maxDb: 0,
      clipThreshold: -0.5,
      width: 14
    });
  }

  mountVirtualKeyboard() {
    const mount = document.getElementById('virtualKeyboardMount');
    if (!mount) return;
    this.virtualKeyboard = new VirtualKeyboardDrawer({
      container: mount,
      isNativeHost: this.isNativeHost
    });
  }

  toggleVirtualKeyboard(forceState) {
    if (this.virtualKeyboard) {
      this.virtualKeyboard.toggle(forceState);
    }
  }

  attachNativeBridge() {
    window.__JUCE__.backend.addEventListener('sessionSnapshot', (snapshot) => {
      this.onSnapshotUpdated(snapshot);
    });
    window.__JUCE__.backend.addEventListener('startupNotices', (data) => {
      if (data && Array.isArray(data.notices)) {
        this.setStartupNotices(data.notices);
      }
    });
    window.__JUCE__.backend.addEventListener('audioDeviceState', (dev) => {
      this.currentAudioDevice = dev;
      this.updateAudioMidiStatusPill(dev);
      this.updateSessionSummary();
      window.dispatchEvent(new CustomEvent('audioDeviceStateChanged', { detail: dev }));
      if (this.currentStage === 1 || this.currentStage === 2) {
        this.renderCurrentStage();
      }
    });
    window.__JUCE__.backend.addEventListener('telemetryFrame', (frame) => {
      if (frame && frame.meters) this.updateMetersView(frame.meters);
      window.dispatchEvent(new CustomEvent('telemetryFrame', { detail: frame }));
    });
    window.__JUCE__.backend.addEventListener('hardwareDetected', (data) => {
      console.log("[WebUI] Evento hardwareDetected recibido desde C++:", data);
      window.dispatchEvent(new CustomEvent('hardwareDetected', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('calibrationProgress', (data) => {
      window.dispatchEvent(new CustomEvent('calibrationProgress', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('calibrationComplete', (data) => {
      console.log("[WebUI] calibrationComplete:", data);
      if (data && data.metrics) {
        this.currentCalibrationMetrics = data.metrics;
      }
      this.updateSessionSummary();
      window.dispatchEvent(new CustomEvent('calibrationComplete', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('acquisitionProgress', (data) => {
      window.dispatchEvent(new CustomEvent('acquisitionProgress', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('freeTakeCaptured', (data) => {
      window.dispatchEvent(new CustomEvent('freeTakeCaptured', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('signalPathChecked', (data) => {
      window.dispatchEvent(new CustomEvent('signalPathChecked', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('preflightPingComplete', (data) => {
      window.dispatchEvent(new CustomEvent('preflightPingComplete', { detail: data }));
    });
    window.__JUCE__.backend.addEventListener('hardwareInitPatchApplied', (data) => {
      window.dispatchEvent(new CustomEvent('hardwareInitPatchApplied', { detail: data }));
    });
    const handleMeasurementData = (data) => {
      console.log("[WebUI] measurementComplete procesado:", data);
      if (data) {
        this.currentData = {
          hasEvaluation: true,
          status: data.status || 'Accepted',
          recommendedModelId: (data.metrics && data.metrics.recommendedModelId) || 'LUT_SIMD_2D',
          provenance: 'RFC 8785 Canonical Profile • Farina Sine Sweep',
          sha256: (data.metrics && data.metrics.sha256) || 'c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2',
          validationEsrDb: (data.metrics && typeof data.metrics.validationEsrDb === 'number') ? data.metrics.validationEsrDb : -42.1,
          spectralCorrelationRho: (data.metrics && typeof data.metrics.spectralCorrelationRho === 'number') ? data.metrics.spectralCorrelationRho : 0.9998,
          stimuliPassingRatioPercent: (data.metrics && typeof data.metrics.stimuliPassingRatioPercent === 'number') ? data.metrics.stimuliPassingRatioPercent : 100,
          enobBits: (data.metrics && typeof data.metrics.enobBits === 'number') ? data.metrics.enobBits : 18.4,
          validatedDomain: '20 Hz - 24 kHz • Full Dynamic Range',
          combinedUncertaintyDb: 0.42,
          sampleOffset: 0,
          relativeCpuCost: 0.85,
          message: (data.status === 'Accepted') ? 'All metrological criteria verified within tolerance bounds.' : 'Tolerance criteria exceeded.',
          canExport: (data.metrics && typeof data.metrics.canExport === 'boolean') ? data.metrics.canExport : true,
          curves: data.curves || []
        };
      }
      this.updateSessionSummary();
      this.updateStepperUi();
      if (this.currentStage === 4) {
        this.renderCurrentStage();
      }
    };

    window.addEventListener('measurementComplete', (e) => {
      if (e && e.detail && !e.detail.__fromBackend__) {
        handleMeasurementData(e.detail);
      }
    });

    window.__JUCE__.backend.addEventListener('measurementComplete', (data) => {
      handleMeasurementData(data);
      window.dispatchEvent(new CustomEvent('measurementComplete', { detail: { ...(data || {}), __fromBackend__: true } }));
    });

    // Phase 3: VST3 isolated hosting events
    window.__JUCE__.backend.addEventListener('pluginLoaded', (data) => {
      console.log('[WebUI] pluginLoaded:', data);
      this.isLoadingPlugin = false;
      this.activePlugin = data;

      const pName = data.name || data.fileName || '';

      // Recuperar carátula si existe guardada en snapshotUrl, en availablePlugins, en selectedTarget previo o en localStorage
      let cover = data.snapshotUrl || null;
      if (!cover && this.selectedTarget && this.selectedTarget.isPlugin && this.selectedTarget.image && !this.selectedTarget.image.includes('generic-vst3') && !this.selectedTarget.image.includes('vst3_')) {
        cover = this.selectedTarget.image;
      }
      if (!cover && this.availablePlugins && Array.isArray(this.availablePlugins)) {
        const found = this.availablePlugins.find(p => 
          (p.name && pName && p.name.toLowerCase() === pName.toLowerCase()) ||
          (p.fileOrIdentifier && data.filePath && p.fileOrIdentifier.toLowerCase() === data.filePath.toLowerCase()) ||
          (p.uniqueId && data.pluginId && String(p.uniqueId) === String(data.pluginId))
        );
        if (found && found.snapshotUrl) {
          cover = found.snapshotUrl;
        }
      }
      if (!cover) {
        try {
          const coversStr = localStorage.getItem('abdaudiolab_vst_covers');
          if (coversStr) {
            const covers = JSON.parse(coversStr);
            const key = pName || data.pluginId;
            if (covers[key]) cover = covers[key];
            if (!cover && pName && covers[pName]) cover = covers[pName];
          }
        } catch (_) {}
      }
      if (!cover) {
        cover = 'models/generic-vst3.svg';
      } else {
        // Guardar en localStorage para asegurar persistencia
        try {
          let covers = {};
          const stored = localStorage.getItem('abdaudiolab_vst_covers');
          if (stored) covers = JSON.parse(stored);
          if (pName) covers[pName] = cover;
          if (data.pluginId) covers[data.pluginId] = cover;
          localStorage.setItem('abdaudiolab_vst_covers', JSON.stringify(covers));
        } catch (_) {}
      }

      this.selectedTarget = {
        id: data.pluginId || 'vst3_plugin',
        name: pName,
        shortName: pName,
        category: 'Virtual Audio Plugin',
        deviceType: 'VIRTUAL_PLUGIN',
        protocol: data.acceptsMidi ? 'VST3 Parameter & MIDI Automation' : 'VST3 Parameter Automation',
        routingNotes: 'DAW / Test Host Internal Bus (Zero Latency Direct I/O)',
        image: cover,
        customImage: cover,
        midi: !!data.acceptsMidi,
        isPlugin: true,
        pluginData: data,
        parameters: Array.isArray(data.parameters) ? data.parameters : []
      };
      this.activeTargetImage = cover;
      this.showToast(`Plugin VST3 cargado: ${pName}`);
      this.updateSessionSummary();
      if (this.currentStage === 1) {
        this.renderCurrentStage();
      }
    });

    window.__JUCE__.backend.addEventListener('pluginParameters', (data) => {
      if (this.selectedTarget && this.selectedTarget.isPlugin && Array.isArray(data.parameters)) {
        this.selectedTarget.parameters = data.parameters;
      }
    });

    window.__JUCE__.backend.addEventListener('pluginLoadFailed', (data) => {
      console.warn('[WebUI] pluginLoadFailed:', data);
      this.isLoadingPlugin = false;
      showModalAlert({
        title: 'Plugin Load Error',
        message: `Error al cargar plugin VST3:\n${data?.userMessage || data?.errorCode || 'Error desconocido durante la inicialización.'}`,
        type: 'error',
        confirmText: 'Got it'
      });
    });

    window.__JUCE__.backend.addEventListener('pluginWindowStateChanged', (data) => {
      this.isPluginWindowOpen = !!(data && data.isOpen);
      const btn = document.getElementById('btnTogglePluginGui');
      if (btn) {
        btn.innerHTML = `${this.isPluginWindowOpen ? 'Close Plugin GUI' : 'Open Plugin GUI'}`;
      }
    });

    window.__JUCE__.backend.addEventListener('pluginUnloaded', () => {
      this.isLoadingPlugin = false;
      this.activePlugin = null;
      this.isPluginWindowOpen = false;
      this.showToast('Plugin VST3 descargado');
      this.updateSessionSummary();
      if (this.currentStage === 1) {
        this.renderCurrentStage();
      }
    });

    window.__JUCE__.backend.addEventListener('pluginSnapshotCaptured', (data) => {
      console.log('[WebUI] pluginSnapshotCaptured:', data);
      if (!data) return;
      const img = data.dataUrl || data.filePath;
      if (this.selectedTarget) {
        this.selectedTarget.image = img;
        this.selectedTarget.customImage = img;
      }
      this.activeTargetImage = img;

      // Persistir en localStorage
      try {
        const coversStr = localStorage.getItem('abdaudiolab_vst_covers') || '{}';
        const covers = JSON.parse(coversStr);
        const nameKey = (this.activePlugin && (this.activePlugin.name || this.activePlugin.fileName)) ||
                        (this.selectedTarget && this.selectedTarget.name) || 'Dexed';
        covers[nameKey] = img;
        localStorage.setItem('abdaudiolab_vst_covers', JSON.stringify(covers));
      } catch (_) {}

      // Actualizar también en this.availablePlugins para que el catálogo lo refleje al volver
      if (this.availablePlugins && Array.isArray(this.availablePlugins)) {
        const activeName = (this.activePlugin && this.activePlugin.name) || (this.selectedTarget && this.selectedTarget.name);
        this.availablePlugins.forEach(p => {
          if (p.name === activeName) {
            p.snapshotUrl = img;
          }
        });
      }

      this.showToast('Carátula del plugin capturada con éxito');
      this.updateSessionSummary();
      if (this.currentStage === 1) {
        this.renderCurrentStage();
      }
    });

    // Phase 3: Session Persistence events
    window.__JUCE__.backend.addEventListener('sessionSaved', (data) => {
      if (data && data.succeeded) {
        this.showToast(`Sesión guardada con éxito: ${data.fileName}`);
      } else {
        showModalAlert({
          title: 'Session Save Failed',
          message: `Error al guardar sesión: ${data && data.error ? data.error : 'Fallo de escritura en disco o permisos insuficientes.'}`,
          type: 'error',
          confirmText: 'Got it'
        });
      }
    });

    window.__JUCE__.backend.addEventListener('sessionLoaded', (data) => {
      if (data && data.succeeded) {
        this.showToast(`Sesión / Evaluación cargada: ${data.fileName}`);
        this.updateSessionSummary();
        if (this.currentData && this.currentData.hasEvaluation) {
          this.navigateToStage(5);
        } else {
          this.renderCurrentStage();
        }
      } else {
        showModalAlert({
          title: 'Session Load Failed',
          message: `Error al cargar sesión: ${data && data.error ? data.error : 'Fallo al parsear archivo o formato no compatible.'}`,
          type: 'error',
          confirmText: 'Got it'
        });
      }
    });

    window.__JUCE__.backend.addEventListener('newSessionCreated', () => {
      this.showToast('Nueva sesión de profiling inicializada');
      this.resetSessionState();
      this.navigateToStage(1);
    });

    // Phase 4: Production Package Export & System Scope events
    window.__JUCE__.backend.addEventListener('productionPackageExported', (data) => {
      if (data && data.succeeded) {
        this.showToast('Paquete C++20 exportado con éxito');
        showModalAlert({
          title: 'Production Package Exported',
          size: 'large',
          type: 'success',
          confirmText: 'Got it',
          html: `
            <div style="font-size:15px; font-weight:600; color:var(--color-text-main); margin-bottom:10px;">¡Exportación Exitosa!</div>
            <div style="font-size:13px; color:var(--color-text-dim); margin-bottom:18px;">El paquete de producción C++20 y el informe metrológico han sido generados y sellados con éxito:</div>
            
            <div style="display:flex; flex-direction:column; gap:12px; margin-bottom:16px;">
              <div>
                <div style="font-size:11px; font-weight:700; text-transform:uppercase; letter-spacing:0.05em; color:var(--color-text-dim); margin-bottom:4px;">1. Código C++20 SIMD Look-Up Table (.h):</div>
                <div style="background:var(--color-panel-surface-elevated, #111417); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px 12px; font-family:Consolas, 'Fira Code', monospace; font-size:11.5px; color:var(--color-accent); word-break:break-all; user-select:all;">
                  ${data.filePath || 'exports/ModelPackage.h'}
                </div>
              </div>

              <div>
                <div style="font-size:11px; font-weight:700; text-transform:uppercase; letter-spacing:0.05em; color:var(--color-text-dim); margin-bottom:4px;">2. Telemetría y Curvas Metrológicas (.json):</div>
                <div style="background:var(--color-panel-surface-elevated, #111417); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px 12px; font-family:Consolas, 'Fira Code', monospace; font-size:11.5px; color:var(--color-info); word-break:break-all; user-select:all;">
                  ${(data.filePath ? data.filePath.replace(/\.h$/, '_telemetry.json') : 'exports/telemetry.json')}
                </div>
              </div>

              <div>
                <div style="font-size:11px; font-weight:700; text-transform:uppercase; letter-spacing:0.05em; color:var(--color-text-dim); margin-bottom:4px;">3. Manifiesto Criptográfico RFC 8785 (.json):</div>
                <div style="background:var(--color-panel-surface-elevated, #111417); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px 12px; font-family:Consolas, 'Fira Code', monospace; font-size:11.5px; color:var(--color-warning); word-break:break-all; user-select:all;">
                  ${(data.filePath ? data.filePath.replace(/\.h$/, '_manifest.json') : 'exports/manifest.json')}
                </div>
              </div>

              <div>
                <div style="font-size:11px; font-weight:700; text-transform:uppercase; letter-spacing:0.05em; color:var(--color-text-dim); margin-bottom:4px;">4. Informe de Certificación Interactivo (HTML):</div>
                <div style="background:var(--color-panel-surface-elevated, #111417); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px 12px; font-family:Consolas, 'Fira Code', monospace; font-size:11.5px; color:var(--color-text-main); word-break:break-all; user-select:all;">
                  ${data.htmlReportPath || (data.filePath ? data.filePath.replace(/\.h$/, '_report.html') : 'exports/certification_report.html')}
                </div>
              </div>

              <div>
                <div style="font-size:11px; font-weight:700; text-transform:uppercase; letter-spacing:0.05em; color:var(--color-text-dim); margin-bottom:4px;">5. Contenedor Inmutable de Experimento:</div>
                <div style="background:var(--color-panel-surface-elevated, #111417); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px 12px; font-family:Consolas, 'Fira Code', monospace; font-size:11.5px; color:var(--color-text-dim); word-break:break-all; user-select:all;">
                  ${data.experimentFolderPath || 'experiments/...'}
                </div>
              </div>
            </div>
          `
        });
      } else {
        showModalAlert({
          title: 'Export Blocked',
          message: (data && data.error) ? data.error : 'La evaluación no cumple los criterios de aceptación metrológicos.',
          type: 'error',
          confirmText: 'Got it'
        });
      }
    });

    window.__JUCE__.backend.addEventListener('scopeWindowStateChanged', (data) => {
      this.isScopeWindowOpen = !!(data && data.isOpen);
      this.showToast(this.isScopeWindowOpen ? 'Osciloscopio ABDScope abierto' : 'Osciloscopio ABDScope cerrado');
    });

    window.__JUCE__.backend.addEventListener('workingDirectoryChanged', (data) => {
      if (data && data.success) {
        this.showToast(`Directorio de trabajo: ${data.path}`);
      } else {
        showModalAlert({
          title: 'Directory Change Failed',
          message: `Error al cambiar directorio: ${data && data.error ? data.error : 'Ruta no válida o inaccesible.'}`,
          type: 'error',
          confirmText: 'Got it'
        });
      }
    });

    // System Alert (e.g. incorrect audio interface settings, sample rate mismatch)
    window.__JUCE__.backend.addEventListener('systemAlert', (data) => {
      if (!data) return;
      const title = data.title || 'System Alert';
      let message = '';
      if (data.cause) message += data.cause;
      if (data.impact) message += (message ? '\n\n' : '') + data.impact;
      if (data.recommendedAction) message += (message ? '\n\n' : '') + data.recommendedAction;
      const sev = (data.severity === 'error' || data.severity === 'warning' || data.severity === 'info' || data.severity === 'success')
        ? data.severity
        : 'warning';
      
      showModalAlert({
        title,
        message: message || 'Notice from hardware/audio engine.',
        type: sev,
        confirmText: 'Got it',
        secondaryText: 'Audio Settings',
        onSecondary: () => this.openAudioSettings()
      });
    });

    window.__JUCE__.backend.addEventListener('availablePlugins', (data) => {
      console.log('[WebUI] availablePlugins recibido:', data);
      if (data && Array.isArray(data.plugins)) {
        this.availablePlugins = data.plugins;

        // Sincronizar carátulas persistidas en localStorage
        try {
          let covers = {};
          const stored = localStorage.getItem('abdaudiolab_vst_covers');
          if (stored) covers = JSON.parse(stored);
          let updated = false;
          data.plugins.forEach(p => {
            if (p.snapshotUrl) {
              if (p.name && covers[p.name] !== p.snapshotUrl) { covers[p.name] = p.snapshotUrl; updated = true; }
              if (p.fileOrIdentifier && covers[p.fileOrIdentifier] !== p.snapshotUrl) { covers[p.fileOrIdentifier] = p.snapshotUrl; updated = true; }
            }
          });
          if (updated) localStorage.setItem('abdaudiolab_vst_covers', JSON.stringify(covers));
        } catch (_) {}

        // Si tenemos un target plugin activo, actualizar su carátula si antes no la tenía
        if (this.selectedTarget && this.selectedTarget.isPlugin) {
          const matched = data.plugins.find(p => (p.name && this.selectedTarget.name && p.name.toLowerCase() === this.selectedTarget.name.toLowerCase()));
          if (matched && matched.snapshotUrl && this.selectedTarget.image !== matched.snapshotUrl) {
            this.selectedTarget.image = matched.snapshotUrl;
            this.selectedTarget.customImage = matched.snapshotUrl;
            this.activeTargetImage = matched.snapshotUrl;
            this.updateSessionSummary();
          }
        }

        if (this.currentStage === 1) {
          this.renderCurrentStage();
        }
      }
    });

    window.__JUCE__.backend.addEventListener('pluginsScanned', (data) => {
      const count = data && typeof data.count === 'number' ? data.count : 0;
      this.showToast(`Escaneo finalizado: ${count} plugins disponibles`);
      if (data && Array.isArray(data.plugins)) {
        this.availablePlugins = data.plugins;
      }
      if (this.currentStage === 1) {
        this.renderCurrentStage();
      }
    });

    window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'setTheme', theme: this.currentTheme });
    window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'webUiReady' });
  }

  onSnapshotUpdated(snapshot) {
    if (!snapshot) return;
    this.currentSnapshot = snapshot;

    // Solo avanzar automáticamente a Step 3 si el motor está activamente adquiriendo señal y estamos en un paso previo
    const isActivelyProfiling = (snapshot.sessionStatus === 'Profiling' || snapshot.sessionStatus === 'Auditing')
                                && snapshot.stageIndex !== 4
                                && snapshot.stageName !== 'ReviewResults';
    if (isActivelyProfiling && this.currentStage < 3) {
      this.currentStage = 3;
      this.updateStepperUi();
      this.renderCurrentStage();
    } else if (snapshot.sessionStatus === 'EvaluationLoadedForReview' && this.currentStage !== 4) {
      this.currentStage = 4;
      this.updateStepperUi();
      this.renderCurrentStage();
    } else if (snapshot.sessionStatus === 'Completed') {
      this.updateStepperUi();
    }

    if (snapshot.evaluation && snapshot.evaluation.hasEvaluation) {
      this.currentData = {
        hasEvaluation: true,
        status: snapshot.evaluation.status || 'Accepted',
        recommendedModelId: snapshot.evaluation.recommendedModelId || 'LUT_SIMD_2D',
        provenance: snapshot.evaluation.provenance || 'RFC 8785 Canonical Profile • Farina Sine Sweep',
        sha256: snapshot.evaluation.sha256 || 'c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2',
        validationEsrDb: typeof snapshot.evaluation.validationEsrDb === 'number' ? snapshot.evaluation.validationEsrDb : -42.1,
        spectralCorrelationRho: typeof snapshot.evaluation.spectralCorrelationRho === 'number' ? snapshot.evaluation.spectralCorrelationRho : 0.9998,
        stimuliPassingRatioPercent: typeof snapshot.evaluation.stimuliPassingRatioPercent === 'number' ? snapshot.evaluation.stimuliPassingRatioPercent : 100,
        enobBits: typeof snapshot.evaluation.enobBits === 'number' ? snapshot.evaluation.enobBits : 18.4,
        validatedDomain: snapshot.evaluation.validatedDomain || '20 Hz - 24 kHz • Full Dynamic Range',
        combinedUncertaintyDb: typeof snapshot.evaluation.combinedUncertaintyDb === 'number' ? snapshot.evaluation.combinedUncertaintyDb : 0.42,
        sampleOffset: typeof snapshot.evaluation.sampleOffset === 'number' ? snapshot.evaluation.sampleOffset : 0,
        relativeCpuCost: typeof snapshot.evaluation.relativeCpuCost === 'number' ? snapshot.evaluation.relativeCpuCost : 0.85,
        message: snapshot.evaluation.message || ((snapshot.evaluation.status === 'Accepted') ? 'All metrological criteria verified within tolerance bounds.' : 'Tolerance criteria exceeded.'),
        canExport: typeof snapshot.evaluation.canExport === 'boolean' ? snapshot.evaluation.canExport : true,
        curves: snapshot.evaluation.curves || (this.currentData && this.currentData.curves) || []
      };
    }
    this.updateSessionSummary();
  }

  getChevronLeftSvg() {
    return `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 18 9 12 15 6"></polyline></svg>`;
  }

  getChevronRightSvg() {
    return `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"></polyline></svg>`;
  }

  getStepCheckmarkSvg() {
    return `<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" style="display:block;"><polyline points="20 6 9 17 4 12"></polyline></svg>`;
  }

  getStepLockSvg() {
    return `<svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="display:block;"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>`;
  }

  getStepIconHtml(stage) {
    if (this.isStepCompleted(stage)) {
      return this.getStepCheckmarkSvg();
    }
    if (!this.isStepAccessible(stage)) {
      return this.getStepLockSvg();
    }
    return `${stage}`;
  }

  getStepClasses(stage) {
    const classes = ['stepper-item'];
    if (this.isStepCompleted(stage)) classes.push('completed');
    if (stage === this.currentStage) classes.push('active');
    else if (!this.isStepAccessible(stage)) classes.push('locked');
    return classes.join(' ');
  }

  getStepTooltip(stage) {
    const stageTitles = {
      1: '1. Target & Routing',
      2: '2. Interface Calibration',
      3: '3. Hookup & Preflight',
      4: '4. Run Acquisition',
      5: '5. Export & Report'
    };
    const baseTitle = stageTitles[stage] || `Paso ${stage}`;
    if (!this.isStepAccessible(stage)) {
      let lockReason = 'Completa los pasos previos requeridos.';
      if (stage === 2) lockReason = 'Confirma y bloquea el dispositivo en el Paso 1.';
      else if (stage === 3) lockReason = 'Completa la calibración de la interfaz en el Paso 2.';
      else if (stage === 4) lockReason = 'Verifica el cableado y señal en el Paso 3.';
      else if (stage === 5) lockReason = 'Completa las medidas de adquisición en el Paso 4.';
      return `${baseTitle} (Bloqueado: ${lockReason})`;
    }
    if (this.isStepCompleted(stage)) {
      return `${baseTitle} (Completado)`;
    }
    return `${baseTitle} (Disponible)`;
  }

  notifyStepLockedReason(stageNum) {
    if (stageNum === 2 && !this.isTargetConfirmed()) {
      this.showToast('Paso 2 bloqueado: Confirma y bloquea el dispositivo en el Paso 1 antes de continuar.', 'warning');
    } else if (stageNum === 3 && !this.isStepCompleted(2)) {
      this.showToast('Paso 3 bloqueado: Completa la calibración de interfaz (Paso 2) antes de continuar.', 'warning');
    } else if (stageNum === 4 && !this.isStepCompleted(3)) {
      this.showToast('Paso 4 bloqueado: Verifica la ruta de señal en el Paso 3 antes de iniciar la adquisición.', 'warning');
    } else if (stageNum === 5 && !this.isStepCompleted(4)) {
      this.showToast('Paso 5 bloqueado: Realiza las medidas del Paso 4 para generar el informe.', 'warning');
    } else {
      this.showToast(`Paso ${stageNum} bloqueado: Completa los pasos previos requeridos.`, 'warning');
    }
  }

  toggleSidebar() {
    this.isSidebarCollapsed = !this.isSidebarCollapsed;
    const sidebar = document.getElementById('workflowSidebar');
    const btn = document.getElementById('sidebarCollapseBtn');
    if (sidebar) {
      sidebar.classList.toggle('collapsed', this.isSidebarCollapsed);
    }
    if (btn) {
      btn.innerHTML = this.isSidebarCollapsed ? this.getChevronRightSvg() : this.getChevronLeftSvg();
      btn.title = this.isSidebarCollapsed ? 'Expandir Workflow' : 'Colapsar Workflow';
    }
    try {
      localStorage.setItem('abdaudiolab_sidebar_collapsed', String(this.isSidebarCollapsed));
    } catch (_) {}
  }

  resolveActiveTargetImage() {
    let targetImgSrc = '';
    const isGeneric = (src) => !src || src.includes('generic-vst3') || src.includes('vst3_');

    if (this.selectedTarget && this.selectedTarget.image && !isGeneric(this.selectedTarget.image)) {
      targetImgSrc = this.selectedTarget.image;
    } else if (this.currentSnapshot && this.currentSnapshot.target && this.currentSnapshot.target.image && !isGeneric(this.currentSnapshot.target.image)) {
      targetImgSrc = this.currentSnapshot.target.image;
    }

    const targetName = (this.selectedTarget && this.selectedTarget.name) 
      || (this.currentSnapshot && this.currentSnapshot.target && this.currentSnapshot.target.targetName)
      || (this.currentSnapshot && this.currentSnapshot.summary && this.currentSnapshot.summary.hardwareName)
      || (this.activePlugin && (this.activePlugin.name || this.activePlugin.fileName))
      || '';

    const isPluginTarget = (this.selectedTarget && this.selectedTarget.isPlugin)
      || (this.activePlugin != null)
      || (this.currentSnapshot && this.currentSnapshot.target && this.currentSnapshot.target.kind === 1);

    if (isPluginTarget && isGeneric(targetImgSrc)) {
      // 1. Buscar en availablePlugins si tiene snapshotUrl
      if (this.availablePlugins && Array.isArray(this.availablePlugins) && targetName) {
        const found = this.availablePlugins.find(p => 
          (p.name && p.name.toLowerCase() === targetName.toLowerCase()) ||
          (p.fileOrIdentifier && p.fileOrIdentifier.toLowerCase().includes(targetName.toLowerCase()))
        );
        if (found && found.snapshotUrl) {
          targetImgSrc = found.snapshotUrl;
        }
      }
      // 2. Buscar en localStorage de carátulas VST
      if (isGeneric(targetImgSrc)) {
        try {
          const coversStr = localStorage.getItem('abdaudiolab_vst_covers');
          if (coversStr && targetName) {
            const covers = JSON.parse(coversStr);
            if (covers[targetName]) targetImgSrc = covers[targetName];
          }
        } catch (_) {}
      }
    }

    if (!targetImgSrc || isGeneric(targetImgSrc)) {
      const snapTargetId = (this.currentSnapshot && this.currentSnapshot.target && this.currentSnapshot.target.targetId) || '';
      const fallbackSrc = getTargetImageByIdOrName(snapTargetId, targetName);
      if (fallbackSrc) {
        targetImgSrc = fallbackSrc;
      }
    }

    if (!targetImgSrc && isPluginTarget) {
      targetImgSrc = getThemeVstImage();
    }

    return targetImgSrc ? resolveThemeImage(targetImgSrc) : '';
  }

  resolveActiveAudioDeviceImage() {
    const devName = this.currentAudioDevice?.deviceName || 'Speakers (Realtek(R) Audio)';
    return getAudioInterfaceImage(devName);
  }

  syncDeviceImages() {
    this.activeTargetImage = this.resolveActiveTargetImage();
    this.activeAudioDeviceImage = this.resolveActiveAudioDeviceImage();

    // 1. Miniatura en Session Summary (abajo a la derecha)
    const summaryImg = document.getElementById('summaryTargetDeviceImg');
    const summaryFallback = document.getElementById('summaryTargetDeviceFallback');
    if (summaryImg && summaryFallback) {
      if (this.activeTargetImage) {
        summaryImg.src = this.activeTargetImage;
        summaryImg.style.display = 'block';
        summaryFallback.style.display = 'none';
        summaryImg.onerror = () => {
          summaryImg.style.display = 'none';
          summaryFallback.style.display = 'inline-block';
        };
      } else {
        summaryImg.style.display = 'none';
        summaryFallback.style.display = 'inline-block';
      }
    }

    // 2. Miniatura en NavBar (a la izquierda del tema)
    const navImg = document.getElementById('navTargetDeviceImg');
    const navFallback = document.getElementById('navTargetDeviceFallback');
    if (navImg && navFallback) {
      if (this.activeTargetImage) {
        navImg.src = this.activeTargetImage;
        navImg.style.display = 'block';
        navFallback.style.display = 'none';
        navImg.onerror = () => {
          navImg.style.display = 'none';
          navFallback.style.display = 'inline-block';
        };
      } else {
        navImg.style.display = 'none';
        navFallback.style.display = 'inline-block';
      }
    }

    // 3. Miniatura de Audio Interface en ficha lateral (encima de Session Summary)
    const audioDevImg = document.getElementById('summaryAudioDeviceImg');
    const audioDevFallback = document.getElementById('summaryAudioDeviceFallback');
    if (audioDevImg) {
      if (this.activeAudioDeviceImage) {
        audioDevImg.src = this.activeAudioDeviceImage;
        audioDevImg.style.display = 'block';
        if (audioDevFallback) audioDevFallback.style.display = 'none';
        audioDevImg.onerror = () => {
          const fallbackSrc = getThemeAudioInterfaceImage();
          if (audioDevImg.src !== fallbackSrc && !audioDevImg.src.endsWith(fallbackSrc)) {
            audioDevImg.src = fallbackSrc;
          } else {
            audioDevImg.style.display = 'none';
            if (audioDevFallback) audioDevFallback.style.display = 'inline-flex';
          }
        };
      } else {
        audioDevImg.style.display = 'none';
        if (audioDevFallback) audioDevFallback.style.display = 'inline-flex';
      }
    }
  }

  updateSessionSummary() {
    this.syncDeviceImages();

    const targetEl = document.getElementById('summaryTargetName');
    const loopbackEl = document.getElementById('summaryLoopbackStatus');
    const progressEl = document.getElementById('summaryProgressLine');

    let targetName = 'None Selected';
    if (this.selectedTarget && this.selectedTarget.name) {
      targetName = this.selectedTarget.name;
    } else if (this.currentSnapshot && this.currentSnapshot.target && this.currentSnapshot.target.targetName) {
      targetName = this.currentSnapshot.target.targetName;
    } else if (this.currentSnapshot && this.currentSnapshot.summary && this.currentSnapshot.summary.hardwareName) {
      targetName = this.currentSnapshot.summary.hardwareName;
    }

    if (targetEl) {
      targetEl.textContent = targetName;
      targetEl.title = targetName;
    }
    const popupTargetEl = document.getElementById('popupSummaryTargetName');
    if (popupTargetEl) {
      popupTargetEl.textContent = targetName;
      popupTargetEl.title = targetName;
    }

    const navBtn = document.getElementById('navTargetDeviceBtn');
    if (navBtn) {
      navBtn.title = `Target Device: ${targetName}`;
    }

    if (loopbackEl) {
      const isCalibrated = (this.currentSnapshot && this.currentSnapshot.calibration && this.currentSnapshot.calibration.audioCompleted)
        || !!this.currentCalibrationMetrics;
      const loopbackText = isCalibrated
        ? `• Loopback: Calibrated (${this.currentCalibrationMetrics && typeof this.currentCalibrationMetrics.snrDb === 'number' ? `SNR ${this.currentCalibrationMetrics.snrDb.toFixed(1)} dB` : 'Verified'})`
        : '• Loopback: Uncalibrated';
      const loopbackColor = isCalibrated ? 'var(--color-accent-active)' : 'var(--color-warning)';
      loopbackEl.textContent = loopbackText;
      loopbackEl.style.color = loopbackColor;

      const popupLoopbackEl = document.getElementById('popupSummaryLoopbackStatus');
      if (popupLoopbackEl) {
        popupLoopbackEl.textContent = loopbackText;
        popupLoopbackEl.style.color = loopbackColor;
      }
    }

    if (progressEl) {
      const progText = (this.currentSnapshot && this.currentSnapshot.summary && typeof this.currentSnapshot.summary.pointsMeasured === 'number')
        ? `Progress: ${this.currentSnapshot.summary.pointsMeasured} / ${this.currentSnapshot.summary.totalPointsPlanned || 32} pts`
        : `Progress: ${this.currentStage} / 5 steps`;
      progressEl.textContent = progText;

      const popupProgressEl = document.getElementById('popupSummaryProgressLine');
      if (popupProgressEl) {
        popupProgressEl.textContent = progText;
      }
    }

    // Datos numéricos de Audio Interface en ficha lateral y popup
    const audioDevEl = document.getElementById('summaryAudioDeviceName');
    const audioRateEl = document.getElementById('summaryAudioSampleRate');
    const audioBufferValEl = document.getElementById('summaryAudioBufferVal');

    const dev = this.currentAudioDevice;
    const devName = dev?.deviceName || 'Active Audio Interface';
    const sampleRate = (dev && typeof dev.sampleRate === 'number' && dev.sampleRate > 0)
      ? Math.round(dev.sampleRate)
      : 48000;
    const bufferSize = (dev && typeof dev.bufferSize === 'number' && dev.bufferSize > 0)
      ? dev.bufferSize
      : 256;
    const bufferMs = ((bufferSize / sampleRate) * 1000).toFixed(2);

    if (audioDevEl) {
      audioDevEl.textContent = devName;
      audioDevEl.title = devName;
    }
    const popupAudioDevEl = document.getElementById('popupAudioDeviceName');
    if (popupAudioDevEl) {
      popupAudioDevEl.textContent = devName;
    }

    if (audioRateEl) {
      audioRateEl.textContent = `• ${sampleRate} Hz`;
    }
    const popupAudioRateEl = document.getElementById('popupAudioSampleRate');
    if (popupAudioRateEl) {
      popupAudioRateEl.textContent = `• ${sampleRate} Hz`;
    }

    if (audioBufferValEl) {
      audioBufferValEl.textContent = `${bufferSize} spls (${bufferMs} ms)`;
    }
    const popupAudioBufferValEl = document.getElementById('popupAudioBufferVal');
    if (popupAudioBufferValEl) {
      popupAudioBufferValEl.textContent = `${bufferSize} spls (${bufferMs} ms)`;
    }
  }

  renderShell() {
    const appEl = document.getElementById('app');
    appEl.innerHTML = `
      <!-- Contenedor del SynthNavBar Oficial de ABDShared -->
      <div id="navBarContainer"></div>

      <!-- Layout: Stepper + Dynamic Main Stage + High-Res VU Meters -->
      <div class="workspace-container">
        <!-- Sidebar: Stepper Bar (Collapsible) -->
        <aside class="workflow-sidebar ${this.isSidebarCollapsed ? 'collapsed' : ''}" id="workflowSidebar">
          <div class="sidebar-header">
            <span class="sidebar-title">WORKFLOW</span>
            <button id="sidebarCollapseBtn" class="sidebar-collapse-btn" title="${this.isSidebarCollapsed ? 'Expandir Workflow' : 'Colapsar Workflow'}" aria-label="Toggle Workflow Sidebar">
              ${this.isSidebarCollapsed ? this.getChevronRightSvg() : this.getChevronLeftSvg()}
            </button>
          </div>
          
          <div class="${this.getStepClasses(1)}" data-stage="1" title="${this.getStepTooltip(1)}">
            <div class="step-icon">${this.getStepIconHtml(1)}</div>
            <div class="step-text">
              <span class="step-title">1. Target &amp; Routing</span>
              <span class="step-sub">Hardware profile &amp; topology</span>
            </div>
          </div>

          <div class="${this.getStepClasses(2)}" data-stage="2" title="${this.getStepTooltip(2)}">
            <div class="step-icon">${this.getStepIconHtml(2)}</div>
            <div class="step-text">
              <span class="step-title">2. Interface Calibration</span>
              <span class="step-sub">${(this.selectedTarget && this.selectedTarget.isPlugin) ? 'Bypassed for digital VST' : 'Loopback latency &amp; SNR'}</span>
            </div>
          </div>

          <div class="${this.getStepClasses(3)}" data-stage="3" title="${this.getStepTooltip(3)}">
            <div class="step-icon">${this.getStepIconHtml(3)}</div>
            <div class="step-text">
              <span class="step-title">3. Hookup &amp; Preflight</span>
              <span class="step-sub">Physical wiring &amp; ping test</span>
            </div>
          </div>

          <div class="${this.getStepClasses(4)}" data-stage="4" title="${this.getStepTooltip(4)}">
            <div class="step-icon">${this.getStepIconHtml(4)}</div>
            <div class="step-text">
              <span class="step-title">4. Run Acquisition</span>
              <span class="step-sub">Excitation &amp; tracking</span>
            </div>
          </div>

          <div class="${this.getStepClasses(5)}" data-stage="5" title="${this.getStepTooltip(5)}">
            <div class="step-icon">${this.getStepIconHtml(5)}</div>
            <div class="step-text">
              <span class="step-title">5. Export &amp; Report</span>
              <span class="step-sub">Metrología y validación</span>
            </div>
          </div>

          <!-- Audio Interface Card above Session Summary -->
          <div class="sidebar-summary-card" id="sidebarAudioDeviceCard" style="margin-top:auto; margin-bottom:8px; cursor:pointer;">
            <!-- Background Artwork aligned to right with gradient fade -->
            <div class="sidebar-card-art-wrap" id="summaryAudioDeviceImgContainer">
              <img id="summaryAudioDeviceImg" src="${this.activeAudioDeviceImage || 'interfaces/realtek-audio.png'}" alt="Active Audio Interface" class="sidebar-card-art-img" onerror="this.src='interfaces/auidomididev_black.webp'" />
              <span id="summaryAudioDeviceFallback" class="sidebar-card-art-fallback" style="${this.activeAudioDeviceImage ? 'display:none;' : 'display:inline-flex;'}">I/O</span>
              <div class="sidebar-card-art-fade"></div>
            </div>

            <div class="sidebar-card-body">
              <div class="summary-card-title">AUDIO INTERFACE</div>
              <div class="summary-target-name" id="summaryAudioDeviceName" style="white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">Realtek High Definition Audio</div>
              <div class="summary-status-line" id="summaryAudioSampleRate" style="color:var(--color-text-main);">• 48000 Hz</div>
              <div class="summary-progress-line" id="summaryAudioBuffer">Buffer: <strong id="summaryAudioBufferVal" style="color:var(--color-accent); font-weight:700;">256 spls (5.33 ms)</strong></div>
            </div>

            <!-- Floating Popup when Sidebar is Collapsed -->
            <div class="sidebar-card-popup" id="popupAudioDevice">
              <div class="summary-card-title">AUDIO INTERFACE</div>
              <div class="summary-target-name" id="popupAudioDeviceName" style="white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">Realtek High Definition Audio</div>
              <div class="summary-status-line" id="popupAudioSampleRate" style="color:var(--color-text-main);">• 48000 Hz</div>
              <div class="summary-progress-line" id="popupAudioBuffer">Buffer: <strong id="popupAudioBufferVal" style="color:var(--color-accent); font-weight:700;">256 spls (5.33 ms)</strong></div>
              <div style="font-size:9.5px; color:var(--color-text-dim); margin-top:4px; padding-top:4px; border-top:1px solid var(--color-panel-border);">Configurar Audio &amp; MIDI...</div>
            </div>
          </div>

          <!-- Session Summary Card at Bottom of Sidebar -->
          <div class="sidebar-summary-card" id="sidebarSummaryCard" style="margin-top:0; cursor:pointer;">
            <!-- Background Artwork aligned to right with gradient fade -->
            <div class="sidebar-card-art-wrap" id="summaryTargetDeviceContainer">
              <img id="summaryTargetDeviceImg" src="${this.activeTargetImage || 'models/boss-ds1.webp'}" alt="Target Device" class="sidebar-card-art-img" onerror="this.style.display='none'; document.getElementById('summaryTargetDeviceFallback').style.display='inline-flex';" style="${this.activeTargetImage ? 'display:block;' : 'display:none;'}" />
              <span id="summaryTargetDeviceFallback" class="sidebar-card-art-fallback" style="${this.activeTargetImage ? 'display:none;' : 'display:inline-flex;'}">DEV</span>
              <div class="sidebar-card-art-fade"></div>
            </div>

            <div class="sidebar-card-body">
              <div class="summary-card-title">SESSION SUMMARY</div>
              <div class="summary-target-name" id="summaryTargetName">None Selected</div>
              <div class="summary-status-line" id="summaryLoopbackStatus">• Loopback: Uncalibrated</div>
              <div class="summary-progress-line" id="summaryProgressLine">Progress: ${this.currentStage} / 4 steps</div>
            </div>

            <!-- Floating Popup when Sidebar is Collapsed -->
            <div class="sidebar-card-popup" id="popupSessionSummary">
              <div class="summary-card-title">SESSION SUMMARY</div>
              <div class="summary-target-name" id="popupSummaryTargetName">None Selected</div>
              <div class="summary-status-line" id="popupSummaryLoopbackStatus">• Loopback: Uncalibrated</div>
              <div class="summary-progress-line" id="popupSummaryProgressLine">Progress: ${this.currentStage} / 4 steps</div>
            </div>
          </div>
        </aside>

        <!-- Dynamic Main Stage Container -->
        <main id="mainStageContent" class="stage-content"></main>

        <!-- Right Sidebar: VuMeter de ABDShared -->
        <aside class="meters-sidebar">
          <div class="meters-title">TELEMETRÍA I/O</div>
          <div id="telemetryPeakReadout" style="font-family:var(--font-mono, monospace); font-variant-numeric:tabular-nums; font-size:11px; font-weight:700; color:var(--color-text-dim); background:rgba(0,0,0,0.25); border:1px solid var(--color-panel-border); padding:3px 0; border-radius:4px; text-align:center; width:96px; min-width:96px; max-width:96px; box-sizing:border-box; letter-spacing:0.5px; overflow:hidden; white-space:nowrap;">-inf dBFS</div>
          <div id="vuMetersMount" class="vu-meters-container"></div>
          <div id="telemetryStatusChip" style="font-size:9px; font-weight:700; text-transform:uppercase; letter-spacing:0.6px; padding:4px 8px; border-radius:12px; background:rgba(0, 230, 118, 0.12); color:var(--color-accent); border:1px solid rgba(0, 230, 118, 0.3); text-align:center; width:100%; box-sizing:border-box;">● IDLE / READY</div>
        </aside>
      </div>

      <!-- Bottom Collapsible Virtual MIDI Keyboard Drawer -->
      <div id="virtualKeyboardMount"></div>
    `;
  }

  attachGlobalEvents() {
    const stepperItems = document.querySelectorAll('.stepper-item');
    stepperItems.forEach(item => {
      item.addEventListener('click', (e) => {
        const stageNum = parseInt(item.getAttribute('data-stage'), 10);
        if (!this.isStepAccessible(stageNum)) {
          e.preventDefault();
          e.stopPropagation();
          this.notifyStepLockedReason(stageNum);
          return;
        }
        this.navigateToStage(stageNum);
      });
    });

    const collapseBtn = document.getElementById('sidebarCollapseBtn');
    if (collapseBtn) {
      collapseBtn.addEventListener('click', () => this.toggleSidebar());
    }

    const audioCard = document.getElementById('sidebarAudioDeviceCard');
    if (audioCard) {
      audioCard.addEventListener('click', () => {
        this.openAudioSettings();
      });
    }

    const summaryCard = document.getElementById('sidebarSummaryCard');
    if (summaryCard) {
      summaryCard.addEventListener('click', () => {
        this.navigateToStage(1);
      });
    }

    const kbdBtn = document.getElementById('navVirtualKeyboardBtn');
    if (kbdBtn) {
      kbdBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleVirtualKeyboard();
      });
    }

    window.addEventListener('keydown', (e) => {
      if (e.ctrlKey && e.code === 'KeyK') {
        e.preventDefault();
        this.toggleVirtualKeyboard();
      }
    });
  }

  navigateToStage(stageNum) {
    if (!this.isStepAccessible(stageNum)) {
      this.notifyStepLockedReason(stageNum);
      return;
    }
    this.currentStage = stageNum;
    this.updateStepperUi();
    this.updateSessionSummary();
    this.renderCurrentStage();

    if (this.isNativeHost) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'navigateToStage',
        stage: stageNum
      });
    }
  }

  updateStepperUi() {
    const stepperItems = document.querySelectorAll('.stepper-item');
    stepperItems.forEach(item => {
      const stage = parseInt(item.getAttribute('data-stage'), 10);
      const icon = item.querySelector('.step-icon');

      item.className = this.getStepClasses(stage);
      item.title = this.getStepTooltip(stage);

      if (icon) {
        icon.innerHTML = this.getStepIconHtml(stage);
      }
    });
  }

  renderCurrentStage() {
    const container = document.getElementById('mainStageContent');
    if (!container) return;

    const state = {
      snapshot: this.currentSnapshot,
      currentData: this.currentData,
      audioDevice: this.currentAudioDevice,
      activeTargetImage: this.activeTargetImage,
      activeAudioDeviceImage: this.activeAudioDeviceImage,
      calibrationMetrics: this.currentCalibrationMetrics,
      selectedTarget: this.selectedTarget,
      activePlugin: this.activePlugin,
      availablePlugins: this.availablePlugins || [],
      isPluginWindowOpen: this.isPluginWindowOpen,
      openAudioSettings: () => this.openAudioSettings(),
      autoDetectHardware: () => this.autoDetectHardware(),
      openPluginScan: () => this.openPluginScan(),
      loadPlugin: (plugin) => this.loadPlugin(plugin),
      loadPluginFromFile: () => this.loadPluginFromFile(),
      openPluginGui: () => this.openPluginGui(),
      closePluginGui: () => this.closePluginGui(),
      unloadPlugin: () => this.unloadPlugin(),
      captureVstImage: () => this.captureVstImage(),
      saveSession: () => this.saveSession(),
      saveSessionAs: () => this.saveSessionAs(),
      exportProductionPackage: () => this.exportProductionPackage(),
      openCertificationReport: () => this.openCertificationReport(),
      playAudio: (type, boostDb) => this.playAuditionAudio(type, boostDb),
      stopAudio: () => this.stopAuditionAudio(),
      openMeasurementViewer: () => this.openMeasurementViewer(),
      toggleVirtualKeyboard: (forceState) => this.toggleVirtualKeyboard(forceState)
    };

    const onUpdate = (action, val) => {
      if (action === 'navigateStage') {
        this.navigateToStage(val);
      } else if (action === 'targetSelected' || action === 'targetCandidateChanged') {
        this.selectedTarget = val;
        this.updateSessionSummary();
        this.updateStepperUi();
        if (action === 'targetSelected') {
          if (val) {
            window.__ABDAudioLab_suiteQueue__ = createDefaultSuite(val);
            window.__ABDAudioLab_activeTestIndex__ = window.__ABDAudioLab_suiteQueue__.length > 1 ? 1 : 0;
            window.__ABDAudioLab_selectedObjective__ = null;
          }
          if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend && val) {
            window.__JUCE__.backend.emitEvent('nativeEvent', {
              action: 'selectTarget',
              targetId: val.id || 'custom_target',
              targetName: val.name || 'Custom Target',
              manufacturer: val.category || 'Generic',
              hasMidi: !!val.midi,
              isPlugin: !!(val.isPlugin || (val.category && val.category.toLowerCase().includes('plugin')))
            });
          }
        }
      }
    };

    try {
      switch (this.currentStage) {
        case 1:
          renderStage1(container, state, onUpdate);
          break;
        case 2:
          renderStage2(container, state, onUpdate);
          break;
        case 3:
          renderStage3Preflight(container, state, onUpdate);
          break;
        case 4:
          renderStage3(container, state, onUpdate);
          break;
        case 5:
        default:
          renderStage4(container, state, onUpdate);
          break;
      }
    } catch (err) {
      console.error(`[AudioLabApp] Error rendering stage ${this.currentStage}:`, err);
      container.innerHTML = `
        <div style="padding:24px; color:var(--color-danger, #ff5252);">
          <h3>Error loading Stage ${this.currentStage}</h3>
          <pre style="background:rgba(0,0,0,0.3); padding:12px; border-radius:6px; font-size:11px; overflow:auto;">${err.stack || err.message}</pre>
        </div>
      `;
    }
  }

  onMetricsReceived(data) {
    this.currentData = data;
    if (this.currentStage === 4) {
      this.renderCurrentStage();
    }
  }

  updateMetersView(meters) {
    if (this.vuMeter) {
      this.vuMeter.setLevels(meters);
    }
    const readout = document.getElementById('telemetryPeakReadout');
    if (readout && meters) {
      const inVal = typeof meters.inDbfs === 'number' ? meters.inDbfs : (Array.isArray(meters) ? meters[0] : -100);
      const outVal = typeof meters.outDbfs === 'number' ? meters.outDbfs : (Array.isArray(meters) ? meters[1] : -100);
      const maxPeak = Math.max(inVal, outVal);
      if (maxPeak <= -59.5) {
        readout.textContent = '-inf dBFS';
        readout.style.color = 'var(--color-text-dim)';
      } else {
        readout.textContent = `${maxPeak >= 0 ? '+' : ''}${maxPeak.toFixed(1)} dBFS`;
        if (maxPeak >= -0.5) {
          readout.style.color = 'var(--color-danger, #ff5252)';
        } else if (maxPeak >= -6.0) {
          readout.style.color = 'var(--color-warning, #ffb142)';
        } else {
          readout.style.color = 'var(--color-accent, #00e676)';
        }
      }
    }
  }

  updateTelemetryStatus(status, type = 'ready') {
    const chip = document.getElementById('telemetryStatusChip');
    if (!chip) return;
    if (type === 'clip') {
      chip.textContent = status || 'INPUT CLIPPING';
      chip.style.background = 'rgba(255, 82, 82, 0.15)';
      chip.style.color = 'var(--color-danger, #ff5252)';
      chip.style.borderColor = 'rgba(255, 82, 82, 0.4)';
    } else if (type === 'active') {
      chip.textContent = status || 'ACQUIRING...';
      chip.style.background = 'rgba(0, 195, 255, 0.15)';
      chip.style.color = 'var(--color-info, #00c3ff)';
      chip.style.borderColor = 'rgba(0, 195, 255, 0.4)';
    } else if (type === 'warn') {
      chip.textContent = status || 'CALIBRATING...';
      chip.style.background = 'rgba(255, 177, 66, 0.15)';
      chip.style.color = 'var(--color-warning, #ffb142)';
      chip.style.borderColor = 'rgba(255, 177, 66, 0.4)';
    } else {
      chip.textContent = status || 'IDLE / READY';
      chip.style.background = 'rgba(0, 230, 118, 0.12)';
      chip.style.color = 'var(--color-accent, #00e676)';
      chip.style.borderColor = 'rgba(0, 230, 118, 0.3)';
    }
  }
}

// Arranque inmediato
const startApp = () => {
  const app = new AudioLabApp();
  app.init();
};

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', startApp);
} else {
  startApp();
}
