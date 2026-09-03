/**
 * ABDScope Lane Controller
 * ========================
 * Encapsulates an independent visual channel / lane within a Multi-Lane scope view.
 * Owns its dedicated canvas, renderer instance, mode buttons, probe selector,
 * per-lane Freeze & Snapshot controls, colSpan width toggle, and telemetry note badge.
 */

import { OscilloscopeRenderer } from '../renderers/OscilloscopeRenderer.js';
import { SpectrumRenderer } from '../renderers/SpectrumRenderer.js';
import { LissajousRenderer } from '../renderers/LissajousRenderer.js';
import { PhaseMeterRenderer } from '../renderers/PhaseMeterRenderer.js';
import { SpectrogramRenderer } from '../renderers/SpectrogramRenderer.js';
import { downloadCanvasAsPng } from '../utils/exportImage.js';

export class LaneController {
  constructor(options = {}) {
    this.index = options.index ?? 0;
    this.enabledModes = options.enabledModes || ['oscilloscope', 'spectrum', 'lissajous', 'phase', 'spectrogram'];
    this.availableTaps = options.availableTaps || [{ id: 'master', name: 'Master Out' }];
    this.activeMode = options.initialMode || this.enabledModes[0];
    this.activeTap = options.defaultTap || this.availableTaps[0]?.id || 'master';

    // Requested column span: 1 for compact modes (lissajous, phase), 2 for panoramic modes
    this.requestedColSpan = options.colSpan || (['lissajous', 'phase'].includes(this.activeMode) ? 1 : 2);
    this.effectiveColSpan = this.requestedColSpan;

    this.onModeChange = options.onModeChange || null;
    this.onTapChange = options.onTapChange || null;
    this.onColSpanChange = options.onColSpanChange || null;

    this.rootElement = null;
    this.headerElement = null;
    this.modeButtonsGroup = null;
    this.modeButtons = [];
    this.tapSelectElement = null;
    this.infoBadgeElement = null;
    this.freezeBtn = null;
    this.snapshotBtn = null;
    this.colSpanBtn = null;
    this.canvasElement = null;
    this.canvasWrap = null;

    this.renderers = new Map();
    this.activeRenderer = null;
    this.isFrozen = false;
    this.resizeObserver = null;

    this._buildDOM();
    this._initRenderers();
    this._initResizeObserver();
    this.setMode(this.activeMode);
    this.setEffectiveColSpan(this.effectiveColSpan);
  }

  _buildDOM() {
    this.rootElement = document.createElement('div');
    this.rootElement.className = 'abd-scope-lane';
    this.rootElement.setAttribute('data-lane-index', String(this.index));
    this.rootElement.setAttribute('data-col-span', String(this.effectiveColSpan));

    // Lane Header / Toolbar
    this.headerElement = document.createElement('div');
    this.headerElement.className = 'abd-scope-lane-header';

    // Left controls: Lane badge + Mode Buttons Group
    const leftGroup = document.createElement('div');
    leftGroup.className = 'abd-scope-lane-group-left';

    const laneBadge = document.createElement('span');
    laneBadge.className = 'abd-scope-lane-badge';
    laneBadge.textContent = `L${this.index + 1}`;
    laneBadge.title = `Lane ${this.index + 1}`;
    leftGroup.appendChild(laneBadge);

    if (this.enabledModes.length > 1) {
      this.modeButtonsGroup = document.createElement('div');
      this.modeButtonsGroup.className = 'abd-scope-lane-mode-group';

      this.enabledModes.forEach(mode => {
        const btn = document.createElement('button');
        btn.className = `abd-scope-lane-mode-btn ${mode === this.activeMode ? 'active' : ''}`;
        btn.dataset.mode = mode;
        btn.textContent = this._formatModeButtonLabel(mode);
        btn.title = this._formatModeName(mode);
        btn.addEventListener('click', () => {
          this.setMode(mode);
          // Auto-adjust default requested colSpan on mode change
          if (['lissajous', 'phase'].includes(mode)) {
            this.setRequestedColSpan(1);
          } else {
            this.setRequestedColSpan(2);
          }
          if (typeof this.onModeChange === 'function') {
            this.onModeChange(this.index, this.activeMode);
          }
        });
        this.modeButtonsGroup.appendChild(btn);
        this.modeButtons.push(btn);
      });

      leftGroup.appendChild(this.modeButtonsGroup);
    }

    // Right controls: Probe Selector + Info Badge + ColSpan + Freeze + Snapshot
    const rightGroup = document.createElement('div');
    rightGroup.className = 'abd-scope-lane-group-right';

    if (this.availableTaps.length > 1) {
      this.tapSelectElement = document.createElement('select');
      this.tapSelectElement.className = 'abd-scope-lane-select abd-scope-lane-tap-select';
      this.tapSelectElement.title = 'Signal Input / Telemetry Tap';
      this.availableTaps.forEach(tap => {
        const opt = document.createElement('option');
        opt.value = tap.id;
        opt.textContent = tap.name;
        if (tap.id === this.activeTap) opt.selected = true;
        this.tapSelectElement.appendChild(opt);
      });

      this.tapSelectElement.addEventListener('change', (e) => {
        this.setActiveTap(e.target.value);
        if (typeof this.onTapChange === 'function') {
          this.onTapChange(this.index, this.activeTap);
        }
      });
      rightGroup.appendChild(this.tapSelectElement);
    }

    this.infoBadgeElement = document.createElement('span');
    this.infoBadgeElement.className = 'abd-scope-lane-info';
    rightGroup.appendChild(this.infoBadgeElement);

    // Column Span Width Toggle (1/2 col vs 2/2 full width)
    this.colSpanBtn = document.createElement('button');
    this.colSpanBtn.className = 'abd-scope-lane-tool-btn abd-scope-lane-span-btn';
    this.colSpanBtn.title = 'Toggle Column Width (Full 100% / Half 50%)';
    this.colSpanBtn.textContent = this.requestedColSpan === 1 ? '½' : '1';
    this.colSpanBtn.addEventListener('click', () => {
      const nextSpan = this.requestedColSpan === 1 ? 2 : 1;
      this.setRequestedColSpan(nextSpan);
      if (typeof this.onColSpanChange === 'function') {
        this.onColSpanChange(this.index, nextSpan);
      }
    });
    rightGroup.appendChild(this.colSpanBtn);

    // Per-Lane Freeze Button
    this.freezeBtn = document.createElement('button');
    this.freezeBtn.className = 'abd-scope-lane-tool-btn abd-scope-lane-freeze-btn';
    this.freezeBtn.title = 'Freeze / Hold this Lane';
    this.freezeBtn.innerHTML = `
      <svg width="8" height="8" viewBox="0 0 24 24" fill="currentColor">
        <rect x="6" y="4" width="4" height="16"/>
        <rect x="14" y="4" width="4" height="16"/>
      </svg>
    `;
    this.freezeBtn.addEventListener('click', () => {
      this.freeze(!this.isFrozen);
    });
    rightGroup.appendChild(this.freezeBtn);

    // Per-Lane Snapshot Button
    this.snapshotBtn = document.createElement('button');
    this.snapshotBtn.className = 'abd-scope-lane-tool-btn abd-scope-lane-snapshot-btn';
    this.snapshotBtn.title = 'Export PNG Snapshot of this Lane';
    this.snapshotBtn.innerHTML = `
      <svg width="9" height="9" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/>
        <circle cx="12" cy="13" r="4"/>
      </svg>
    `;
    this.snapshotBtn.addEventListener('click', () => {
      if (this.canvasElement) {
        downloadCanvasAsPng(this.canvasElement, `abd-scope-lane${this.index + 1}-${this.activeMode}.png`);
      }
    });
    rightGroup.appendChild(this.snapshotBtn);

    this.headerElement.appendChild(leftGroup);
    this.headerElement.appendChild(rightGroup);

    // Canvas Viewport
    this.canvasWrap = document.createElement('div');
    this.canvasWrap.className = 'abd-scope-lane-canvas-wrap';

    this.canvasElement = document.createElement('canvas');
    this.canvasElement.className = 'abd-scope-canvas abd-scope-lane-canvas';
    this.canvasWrap.appendChild(this.canvasElement);

    this.rootElement.appendChild(this.headerElement);
    this.rootElement.appendChild(this.canvasWrap);
  }

  _formatModeButtonLabel(mode) {
    switch (mode) {
      case 'oscilloscope': return 'OSC';
      case 'spectrum':     return 'FFT';
      case 'lissajous':    return 'LISS';
      case 'phase':        return 'PHASE';
      case 'spectrogram':  return 'WATERFALL';
      default: return mode.slice(0, 4).toUpperCase();
    }
  }

  _formatModeName(mode) {
    switch (mode) {
      case 'oscilloscope': return 'Oscilloscope';
      case 'spectrum':     return 'FFT Spectrum';
      case 'lissajous':    return 'Lissajous';
      case 'phase':        return 'Phase Meter';
      case 'spectrogram':  return 'Spectrogram Waterfall';
      default: return mode.charAt(0).toUpperCase() + mode.slice(1);
    }
  }

  _initRenderers() {
    if (this.enabledModes.includes('oscilloscope')) {
      const osc = new OscilloscopeRenderer();
      osc.init(this.canvasElement);
      this.renderers.set('oscilloscope', osc);
    }
    if (this.enabledModes.includes('spectrum')) {
      const spec = new SpectrumRenderer();
      spec.init(this.canvasElement);
      this.renderers.set('spectrum', spec);
    }
    if (this.enabledModes.includes('lissajous')) {
      const liss = new LissajousRenderer();
      liss.init(this.canvasElement);
      this.renderers.set('lissajous', liss);
    }
    if (this.enabledModes.includes('phase')) {
      const ph = new PhaseMeterRenderer();
      ph.init(this.canvasElement);
      this.renderers.set('phase', ph);
    }
    if (this.enabledModes.includes('spectrogram')) {
      const sp = new SpectrogramRenderer();
      sp.init(this.canvasElement);
      this.renderers.set('spectrogram', sp);
    }
  }

  _initResizeObserver() {
    if (typeof ResizeObserver === 'undefined') return;

    this.resizeObserver = new ResizeObserver((entries) => {
      if (!entries[0] || !this.canvasWrap) return;
      const rect = entries[0].contentRect;
      if (rect.width > 0 && rect.height > 0) {
        const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;
        this._updateCanvasResolution(rect.width, rect.height, dpr);
      }
    });

    this.resizeObserver.observe(this.canvasWrap);
  }

  setRequestedColSpan(span = 1) {
    this.requestedColSpan = Math.max(1, Math.min(2, span));
    if (this.colSpanBtn) {
      this.colSpanBtn.textContent = this.requestedColSpan === 1 ? '½' : '1';
      this.colSpanBtn.classList.toggle('active', this.requestedColSpan === 2);
    }
  }

  setEffectiveColSpan(span = 1) {
    this.effectiveColSpan = Math.max(1, Math.min(2, span));
    this.rootElement.setAttribute('data-col-span', String(this.effectiveColSpan));
    if (this.canvasWrap) {
      const rect = this.canvasWrap.getBoundingClientRect();
      if (rect.width > 0 && rect.height > 0) {
        const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;
        this._updateCanvasResolution(rect.width, rect.height, dpr);
      }
    }
  }

  getColSpan() {
    return this.effectiveColSpan;
  }

  setColSpan(span = 1) {
    this.setRequestedColSpan(span);
    this.setEffectiveColSpan(span);
  }

  setMode(mode) {
    if (!this.renderers.has(mode)) return;
    this.activeMode = mode;
    this.activeRenderer = this.renderers.get(mode);
    this.modeButtons.forEach(btn => {
      btn.classList.toggle('active', btn.dataset.mode === mode);
    });
  }

  getActiveMode() {
    return this.activeMode;
  }

  setActiveTap(tapId) {
    this.activeTap = tapId;
    if (this.tapSelectElement && this.tapSelectElement.value !== tapId) {
      this.tapSelectElement.value = tapId;
    }
  }

  getActiveTap() {
    return this.activeTap;
  }

  setAvailableTaps(taps = []) {
    this.availableTaps = taps;
    if (this.tapSelectElement) {
      this.tapSelectElement.innerHTML = '';
      taps.forEach(tap => {
        const opt = document.createElement('option');
        opt.value = tap.id;
        opt.textContent = tap.name;
        if (tap.id === this.activeTap) opt.selected = true;
        this.tapSelectElement.appendChild(opt);
      });
    }
  }

  freeze(frozen = true) {
    this.isFrozen = frozen;
    if (this.freezeBtn) {
      this.freezeBtn.classList.toggle('active', frozen);
    }
    this.renderers.forEach(r => {
      if (typeof r.freeze === 'function') r.freeze(frozen);
    });
  }

  render(dataFrame, options = {}) {
    if (!this.activeRenderer || !dataFrame || this.isFrozen) return;

    // Update Info Badge if detected note or peak is present
    if (this.infoBadgeElement) {
      if (dataFrame.detectedNoteName && dataFrame.estimatedFrequencyHz > 0) {
        this.infoBadgeElement.textContent = `${dataFrame.detectedNoteName} (${Math.round(dataFrame.estimatedFrequencyHz)} Hz)`;
      } else if (dataFrame.rmsDb !== undefined && dataFrame.rmsDb > -96) {
        this.infoBadgeElement.textContent = `${Math.round(dataFrame.rmsDb)} dB`;
      } else {
        this.infoBadgeElement.textContent = '';
      }
    }

    this.activeRenderer.render(dataFrame, options);
  }

  _updateCanvasResolution(width, height, dpr = 1) {
    if (!this.canvasElement) return;

    this.canvasElement.width = Math.floor(width * dpr);
    this.canvasElement.height = Math.floor(height * dpr);
    this.canvasElement.style.width = `${width}px`;
    this.canvasElement.style.height = `${height}px`;

    this.renderers.forEach(r => {
      if (typeof r.resize === 'function') {
        r.resize(width, height, dpr);
      }
    });
  }

  resize(width, height, dpr = 1) {
    if (!this.canvasElement) return;

    const canvasH = Math.max(50, height - (this.headerElement?.offsetHeight ?? 26));
    const effectiveW = this.effectiveColSpan === 1 ? Math.floor(width / 2) : width;

    this._updateCanvasResolution(effectiveW, canvasH, dpr);
  }

  destroy() {
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
      this.resizeObserver = null;
    }
    this.renderers.forEach(r => {
      if (typeof r.destroy === 'function') r.destroy();
    });
    this.renderers.clear();
    this.activeRenderer = null;
    this.rootElement?.remove();
    this.rootElement = null;
  }
}
