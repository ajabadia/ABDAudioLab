/**
 * OperatorStepModal.js
 * Faithful JavaScript mirror of C++ native:
 * - src/gui/OperatorStepModalDialog.h
 * - src/gui/OperatorStepModalDialog.cpp
 * - src/gui/OperatorCardsContainerComponent.h
 * - src/gui/OperatorCardsContainerComponent.cpp
 * 
 * Strict specifications:
 * - Zero emojis (clean SVGs, semantic badges and typography).
 * - Exact vector fidelity for Knob, Slider, JackPort, Button, and Switch.
 * - Interactive controls guidance for manual hardware profiling (pedals, manual synths).
 * - Direct keyboard shortcut [Space] for step confirmation.
 * - Continuous zero-flicker updating during multi-step acquisition.
 * - Live real-time response mini-plotter and telemetry meters.
 * - Supports both active measurement and single-point inspection.
 */

import { renderOperatorCardsContainer } from './hardwareControls.js';
import { freqToNormX, dbToNormY } from '../stages/stage3_dsp_curves.js';

function computeSvgCurvePath(curveData, width = 480, height = 100) {
  if (curveData && Array.isArray(curveData.points) && curveData.points.length > 0) {
    let minDb = -60;
    let maxDb = 12;
    const mags = curveData.points.map(p => typeof p.mag === 'number' ? p.mag : 0);
    const measuredMin = Math.min(...mags);
    const measuredMax = Math.max(...mags);
    if (measuredMin < minDb) minDb = Math.floor(measuredMin / 10) * 10;
    if (measuredMax > maxDb) maxDb = Math.ceil(measuredMax / 6) * 6;
    if (maxDb - minDb < 12) maxDb = minDb + 12;

    return curveData.points.map((p, i) => {
      const x = (freqToNormX(p.freq) * width).toFixed(1);
      const clampedMag = Math.max(minDb, Math.min(maxDb, p.mag));
      const normY = (maxDb - clampedMag) / (maxDb - minDb);
      const y = (normY * (height - 16) + 8).toFixed(1);
      return `${i === 0 ? 'M' : 'L'} ${x} ${y}`;
    }).join(' ');
  }
  return '';
}

export class OperatorStepModal {
  constructor(options = {}) {
    this.options = options;
    this.onAccept = options.onAccept || null;
    this.onRepeat = options.onRepeat || null;
    this.onStepBack = options.onStepBack || null;
    this.onStepForward = options.onStepForward || null;
    this.onNavigatePoint = options.onNavigatePoint || null;
    this.onCancel = options.onCancel || null;
    this.onPause = options.onPause || null;
    this.onResume = options.onResume || null;
    this.onReMeasure = options.onReMeasure || null;
    this.onResetPoint = options.onResetPoint || null;
    this.onInvalidatePoint = options.onInvalidatePoint || null;
    this.onAutoRun = options.onAutoRun || null;
    this.isManual = typeof options.isManual === 'boolean' ? options.isManual : true;
    this.isInspectOnly = !!options.isInspectOnly;
    this.isMeasuring = false;
    this.isPaused = !!options.isPaused;
    this.isCollapsed = false;
    this.selectedParamIndex = 0;
    this.overlay = null;
    this.card = null;
    this.keyListener = null;

    this.currentData = {
      testTitle: options.testTitle || 'Measurement Step',
      typologyId: options.typologyId || 'pedal_overdrive_distortion',
      currentStep: options.currentStep || 1,
      totalSteps: options.totalSteps || 1,
      posPct: typeof options.posPct === 'number' ? options.posPct : 0,
      controls: options.controls || [],
      promptMessage: options.promptMessage || '',
      historyPoints: options.historyPoints || [],
      pointsList: options.pointsList || [],
      telemetry: options.telemetry || { peakDbfs: -18.2, rmsDbfs: -23.5, snrDb: 98.4 }
    };

    this.init();
  }

  init() {
    this.overlay = document.createElement('div');
    this.overlay.className = 'operator-modal-overlay';

    this.card = document.createElement('div');
    this.card.className = 'operator-modal-card';

    this.overlay.appendChild(this.card);
    document.body.appendChild(this.overlay);

    // Dismiss modal on background click
    this.overlay.addEventListener('click', (e) => {
      if (e.target === this.overlay) {
        this.triggerCancel();
      }
    });

    // Bind keyboard shortcuts
    this.keyListener = (e) => {
      const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
      if (tag === 'input' || tag === 'textarea' || tag === 'select') return;

      if (e.code === 'Space' || e.key === ' ') {
        e.preventDefault();
        e.stopPropagation();
        if (!this.isMeasuring && this.isManual && !this.isInspectOnly && !this.isPaused) {
          this.triggerAccept();
        }
      } else if (e.key === 'Escape') {
        e.preventDefault();
        this.triggerCancel();
      } else if (e.key === 'ArrowLeft') {
        if (this.isInspectOnly && typeof this.onStepBack === 'function' && this.currentData.currentStep > 1) {
          e.preventDefault();
          this.onStepBack();
        }
      } else if (e.key === 'ArrowRight') {
        if (this.isInspectOnly && typeof this.onStepForward === 'function' && this.currentData.currentStep < this.currentData.totalSteps) {
          e.preventDefault();
          this.onStepForward();
        }
      }
    };
    window.addEventListener('keydown', this.keyListener, true);

    this.render();

    requestAnimationFrame(() => {
      if (this.overlay) this.overlay.style.opacity = '1';
      if (this.card) this.card.style.transform = 'scale(1) translateY(0)';
    });
  }

  calculateParamSteps(controls, currentStep, totalSteps, posPct) {
    if (!controls || controls.length === 0) {
      return [{
        paramName: 'PARAMETER 1',
        controlType: 'Knob',
        normalizedValue: Math.max(0, Math.min(1, posPct / 100.0)),
        minNormalized: 0.0,
        maxNormalized: 1.0,
        steps: totalSteps || 1,
        totalSteps: totalSteps || 1,
        stepIndex: Math.max(0, (currentStep || 1) - 1),
        protocol: 'MANUAL'
      }];
    }

    const pointIdx = Math.max(0, (currentStep || 1) - 1);
    let divisor = 1;

    return controls.map((c, i) => {
      const sCount = Math.max(1, parseInt(c.steps || c.defaultSteps, 10) || 1);
      const stepIdx = Math.floor(pointIdx / divisor) % sCount;
      divisor *= sCount;

      const normFraction = sCount > 1 ? (stepIdx / (sCount - 1)) : 0.5;
      const minN = typeof c.minPct === 'number' ? c.minPct / 100.0 : 0.0;
      const maxN = typeof c.maxPct === 'number' ? c.maxPct / 100.0 : 1.0;
      const normVal = minN + normFraction * (maxN - minN);

      const proto = c.controlMethod || c.protocol || (typeof c.paramIndex !== 'undefined' ? 'VST_PARAM' : (typeof c.cc !== 'undefined' ? 'MIDI_CC' : 'MANUAL'));

      return {
        paramName: c.name || `Control ${i + 1}`,
        controlType: c.type || 'Knob',
        normalizedValue: Math.max(0, Math.min(1, normVal)),
        minNormalized: minN,
        maxNormalized: maxN,
        steps: sCount,
        totalSteps: sCount,
        stepIndex: stepIdx,
        protocol: proto,
        paramIndex: typeof c.paramIndex !== 'undefined' ? c.paramIndex : i,
        cc: typeof c.cc !== 'undefined' ? c.cc : (74 + i),
        nrpn: c.nrpn,
        sysex: c.sysex,
        unit: c.unit || '%'
      };
    });
  }

  render() {
    if (!this.card) return;
    const d = this.currentData;
    const steps = this.calculateParamSteps(d.controls, d.currentStep, d.totalSteps, d.posPct);
    const promptMsg = d.promptMessage || (this.isInspectOnly
      ? `State Inspection: Point #${d.currentStep} of ${d.totalSteps} (${d.posPct.toFixed(1)}% Position)`
      : (this.isManual 
        ? `Step #${d.currentStep} of ${d.totalSteps}: Set physical hardware knobs to indicated positions, then press [Space] to acquire.` 
        : `Step #${d.currentStep} of ${d.totalSteps}: Sweeping hardware parameters automatically...`));

    const currentCurve = computeSvgCurvePath(d.curve, 480, 95);
    const historyCurves = (d.historyPoints || []).map(hp => {
      const p = computeSvgCurvePath(hp.curve, 480, 95);
      return p ? `<path d="${p}" fill="none" stroke="rgba(0, 230, 118, 0.22)" stroke-width="1.2" />` : '';
    }).join('');

    const pctProgress = Math.round((d.currentStep / Math.max(1, d.totalSteps)) * 100);

    // Build the horizontal Takes Strip showing measured status and icons for every point
    // Build the takes matrix strip showing measured status and icons for every point
    let takesStripHtml = '';
    if (d.pointsList && d.pointsList.length > 0) {
      takesStripHtml = `
        <div class="operator-modal-takes-strip" title="Measurement Points Matrix (Click point to navigate/inspect)">
          ${d.pointsList.map((pt, idx) => {
            const ptNum = pt.index || (idx + 1);
            const isCurrent = ptNum === d.currentStep;
            let statusClass = 'chip-queued';
            let icon = '○';

            if (pt.status === 'COMPLETED') {
              statusClass = 'chip-completed';
              icon = '✓';
            } else if (pt.status === 'INVALID') {
              statusClass = 'chip-invalid';
              icon = '✕';
            } else if (!this.isInspectOnly && isCurrent) {
              statusClass = 'chip-running';
              icon = '●';
            } else if (pt.status === 'IN_PROGRESS' && !this.isInspectOnly) {
              statusClass = 'chip-running';
              icon = '●';
            }

            return `
              <div class="op-take-pill ${statusClass} ${isCurrent ? 'active' : ''}" data-modal-point-idx="${idx}" role="button" tabindex="0" title="Point #${ptNum} — Status: ${pt.status || 'QUEUED'} (${(pt.posPct || 0).toFixed(0)}%)">
                <span>${icon}</span>
                <span>${ptNum}</span>
              </div>
            `;
          }).join('')}
        </div>
      `;
    }

    // Estado del punto actual para condicionar la telemetría y botones
    const currentPt = (d.pointsList && d.pointsList[d.currentStep - 1]) || null;
    const currentStatus = currentPt ? (currentPt.status || 'QUEUED') : 'QUEUED';
    const isCompleted = currentStatus === 'COMPLETED';
    const isInvalid = currentStatus === 'INVALID';
    const isExecuted = isCompleted || isInvalid; // Solo si ha sido medido alguna vez
    const hasRecording = isCompleted || (currentPt && (currentPt.status === 'COMPLETED' || currentPt.hasRecording)) || (d.telemetry && typeof d.telemetry.peakDbfs === 'number' && d.telemetry.peakDbfs > -90 && isExecuted);

    let pointStatusBadge = '';
    if (this.isInspectOnly) {
      if (isCompleted) {
        pointStatusBadge = `<span class="badge badge-success badge-xs" style="margin-left:4px;">MEASURED</span>`;
      } else if (isInvalid) {
        pointStatusBadge = `<span class="badge badge-danger badge-xs" style="margin-left:4px; color:var(--color-danger); border-color:var(--color-danger);">INVALID</span>`;
      } else {
        pointStatusBadge = `<span class="badge badge-neutral badge-xs" style="margin-left:4px; opacity:0.75;">UNMEASURED</span>`;
      }
    }

    const resetDisabled = !isExecuted;
    const resetTooltip = resetDisabled
      ? 'This point has not been measured yet (already queued)'
      : 'Clear recorded telemetry and reset point back to Queued status';

    const invalidateDisabled = !isExecuted || isInvalid;
    let invalidateTooltip = 'Mark this point as invalid and flag for re-take';
    let invalidateText = '⚠ Invalidate (Re-Take)';
    if (!isExecuted) {
      invalidateTooltip = 'Cannot invalidate a point that has not been measured yet';
    } else if (isInvalid) {
      invalidateTooltip = 'This point is already marked as invalid for re-take';
      invalidateText = '⚠ Already Flagged (Re-Take)';
    }

    const measureText = isExecuted ? '↻ Re-Measure this Point' : '▶ Measure this Point';
    const measureTooltip = isExecuted 
      ? `Re-acquire acoustic response for Point #${d.currentStep}` 
      : `Acquire acoustic response for Point #${d.currentStep}`;

    this.card.innerHTML = `
      <!-- 1. Header -->
      <div class="operator-modal-header">
        <div style="display:flex; flex-direction:column; gap:3px;">
          <div style="display:flex; align-items:center; gap:8px;">
            <span class="badge ${this.isInspectOnly ? 'badge-info' : (this.isManual ? 'badge-accent' : 'badge-info')} badge-xs">
              ${this.isInspectOnly ? 'POINT INSPECTION' : (this.isManual ? 'MANUAL OPERATOR STEP' : 'AUTOMATED SWEEP')}
            </span>
            <span style="font-size:13px; font-weight:800; color:var(--color-text-main); text-transform:uppercase; letter-spacing:0.5px;">
              ${d.testTitle}
            </span>
          </div>
          <div style="font-size:11px; color:var(--color-text-dim);">
            ${this.isInspectOnly ? 'Review calibrated parameter positions and measured acoustic response' : 'Acoustic excitation and physical parameter alignment matrix'}
          </div>
        </div>

        <div style="display:flex; align-items:center; gap:10px;">
          <div style="padding:4px 12px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:6px; font-size:12px; font-weight:800; color:var(--color-accent); font-family:var(--font-mono, monospace); display:flex; align-items:center;">
            POINT ${d.currentStep} / ${d.totalSteps} <span style="font-weight:600; opacity:0.75; margin-left:4px;">(${d.posPct.toFixed(1)}%)</span>
            ${pointStatusBadge}
          </div>

          <button id="btnOpClose" type="button" class="btn btn-sm btn-ghost" title="Close dialog">
            ✕
          </button>
        </div>
      </div>

      <!-- Takes Status Strip (Every point with icon & color in multi-row wrap) -->
      ${takesStripHtml}

      <!-- Segmented Progress Line -->
      <div class="operator-modal-progress-bar">
        <div class="operator-modal-progress-fill" style="width:${pctProgress}%;"></div>
      </div>

      <!-- 2. Controls Body (Hardware Knobs / Sliders) -->
      <div id="operatorCardsMount" class="operator-modal-cards-mount">
        ${renderOperatorCardsContainer(steps, promptMsg, this.selectedParamIndex, 'operatorModalCardsContainer')}
      </div>

      <!-- 3. Live Response Mini-Plotter & Real-Time Telemetry -->
      <div class="operator-modal-telemetry-strip">
        <div class="operator-mini-plotter-box">
          <svg viewBox="0 0 480 100" preserveAspectRatio="none" style="width:100%; height:100%; display:block;">
            <!-- Grid lines -->
            <line x1="0" y1="25" x2="480" y2="25" stroke="var(--color-panel-border)" stroke-width="0.8" opacity="0.4"/>
            <line x1="0" y1="50" x2="480" y2="50" stroke="var(--color-panel-border)" stroke-width="1.2" opacity="0.6"/>
            <line x1="0" y1="75" x2="480" y2="75" stroke="var(--color-panel-border)" stroke-width="0.8" opacity="0.4"/>
            <line x1="120" y1="0" x2="120" y2="100" stroke="var(--color-panel-border)" stroke-width="0.8" opacity="0.4"/>
            <line x1="240" y1="0" x2="240" y2="100" stroke="var(--color-panel-border)" stroke-width="0.8" opacity="0.4"/>
            <line x1="360" y1="0" x2="360" y2="100" stroke="var(--color-panel-border)" stroke-width="0.8" opacity="0.4"/>
            
            <!-- History ghost curves -->
            ${historyCurves}

            <!-- Active Point Transfer Function: Solid if executed, dashed/reference if unmeasured -->
            ${currentCurve ? (isExecuted 
              ? `<path d="${currentCurve}" fill="none" stroke="var(--color-accent)" stroke-width="2.2" />`
              : `<path d="${currentCurve}" fill="none" stroke="var(--color-accent)" stroke-width="1.8" stroke-dasharray="4 3" opacity="0.5" />`) : ''}
          </svg>
          <span style="position:absolute; top:4px; left:8px; font-size:9px; font-family:var(--font-mono, monospace); color:var(--color-text-dim);">
            ${isExecuted ? `Measured Response • Point #${d.currentStep}` : `Target Reference Profile (Unmeasured) • Point #${d.currentStep}`}
          </span>
          <span style="position:absolute; bottom:4px; right:8px; font-size:9px; font-family:var(--font-mono, monospace); color:var(--color-text-dim);">20Hz — 20kHz Log</span>
        </div>

        <div class="operator-telemetry-metrics">
          <div style="font-size:9.5px; font-weight:800; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.5px;">
            ${isExecuted ? 'Measured Metrology' : 'Expected Metrology (Pending)'}
          </div>
          <div style="display:flex; justify-content:space-between; font-size:11px; font-family:var(--font-mono, monospace);">
            <span style="color:var(--color-text-dim);">Peak:</span>
            <span style="color:var(--color-text-main); font-weight:700;">
              ${isExecuted && d.telemetry && typeof d.telemetry.peakDbfs === 'number' ? d.telemetry.peakDbfs.toFixed(1) + ' dBFS' : '— (Pending)'}
            </span>
          </div>
          <div style="display:flex; justify-content:space-between; font-size:11px; font-family:var(--font-mono, monospace);">
            <span style="color:var(--color-text-dim);">RMS:</span>
            <span style="color:var(--color-text-main); font-weight:700;">
              ${isExecuted && d.telemetry && typeof d.telemetry.rmsDbfs === 'number' ? d.telemetry.rmsDbfs.toFixed(1) + ' dBFS' : '— (Pending)'}
            </span>
          </div>
          <div style="display:flex; justify-content:space-between; font-size:11px; font-family:var(--font-mono, monospace);">
            <span style="color:var(--color-text-dim);">Status:</span>
            <span style="color:${isCompleted ? 'var(--color-accent)' : (isInvalid ? 'var(--color-danger)' : 'var(--color-text-dim)')}; font-weight:700;">
              ${currentStatus}
            </span>
          </div>
        </div>
      </div>

      <!-- 4. Transport Actions Footer -->
      <div class="operator-modal-footer">
        <div style="display:flex; align-items:center; gap:8px;">
          ${!this.isInspectOnly ? `
            <button id="btnOpCancel" type="button" class="btn btn-danger btn-sm">
              ■ Stop
            </button>
            <button id="btnOpPause" type="button" class="btn btn-warning btn-sm">
              ${this.isPaused ? '▶ Resume' : '⏸ Pause'}
            </button>
          ` : `
            <button id="btnOpReset" type="button" class="btn btn-outline btn-sm" ${resetDisabled ? 'disabled' : ''} style="${resetDisabled ? 'opacity:0.38; cursor:not-allowed; border-color:var(--color-panel-border); color:var(--color-text-dim); pointer-events:none;' : ''}" title="${resetTooltip}">
              ↺ Reset Point
            </button>
            <button id="btnOpInvalidate" type="button" class="btn btn-outline btn-sm" ${invalidateDisabled ? 'disabled' : ''} style="${invalidateDisabled ? 'opacity:0.38; cursor:not-allowed; border-color:var(--color-panel-border); color:var(--color-text-dim); pointer-events:none;' : 'color:var(--color-danger); border-color:var(--color-danger);'}" title="${invalidateTooltip}">
              ${invalidateText}
            </button>
          `}
          <button id="btnOpPlayTake" type="button" class="btn btn-outline btn-sm" ${!hasRecording ? 'disabled style="opacity:0.38; cursor:not-allowed; border-color:var(--color-panel-border); color:var(--color-text-dim); pointer-events:none;"' : ''} title="${hasRecording ? `Play recorded take audio for Point #${d.currentStep}` : 'No audio recording available for this point yet'}">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor" style="display:inline-block; vertical-align:middle; margin-right:4px;"><polygon points="5 3 19 12 5 21 5 3"/></svg> Play Take
          </button>
        </div>

        <div style="display:flex; align-items:center; gap:8px;">
          ${this.isInspectOnly ? `
            <button id="btnOpReMeasure" type="button" class="btn btn-primary btn-sm" title="${measureTooltip}">
              ${measureText}
            </button>
            <button id="btnOpDone" type="button" class="btn btn-outline btn-sm" title="Close inspection dialog">
              Done
            </button>
          ` : (this.isManual ? `
            <button id="btnOpRepeat" type="button" class="btn btn-outline btn-sm" ${this.isMeasuring ? 'disabled' : ''} title="Re-measure current point">
              ↻ Repeat Step
            </button>

            <button id="btnOpAccept" type="button" class="btn btn-primary btn-sm" ${this.isMeasuring || this.isPaused ? 'disabled' : ''}>
              ${this.isMeasuring ? 'Measuring Audio...' : `▶ Measure Step #${d.currentStep} [Space] →`}
            </button>
          ` : `
            <div style="font-size:11.5px; font-weight:700; color:var(--color-info); display:flex; align-items:center; gap:6px;">
              <span class="pulse" style="width:7px; height:7px; border-radius:50%; background:var(--color-info); display:inline-block;"></span>
              Automated Sweeping in progress...
            </div>
          `)}
        </div>
      </div>
    `;

    this.bindEvents();
  }

  bindEvents() {
    if (!this.card) return;

    // 1. Close button
    const btnClose = this.card.querySelector('#btnOpClose');
    if (btnClose) {
      btnClose.addEventListener('click', () => {
        this.close();
      });
    }

    const btnDone = this.card.querySelector('#btnOpDone');
    if (btnDone) {
      btnDone.addEventListener('click', () => {
        this.close();
      });
    }

    const btnReset = this.card.querySelector('#btnOpReset');
    if (btnReset) {
      btnReset.addEventListener('click', () => {
        if (btnReset.disabled) return;
        if (typeof this.onResetPoint === 'function') {
          this.onResetPoint(this.currentData.currentStep - 1);
        }
      });
    }

    const btnInvalidate = this.card.querySelector('#btnOpInvalidate');
    if (btnInvalidate) {
      btnInvalidate.addEventListener('click', () => {
        if (btnInvalidate.disabled) return;
        if (typeof this.onInvalidatePoint === 'function') {
          this.onInvalidatePoint(this.currentData.currentStep - 1);
        }
      });
    }

    const btnPlayTake = this.card.querySelector('#btnOpPlayTake');
    if (btnPlayTake) {
      btnPlayTake.addEventListener('click', () => {
        if (btnPlayTake.disabled) return;
        const testIdx = typeof this.currentData.testIndex === 'number' ? this.currentData.testIndex : 0;
        const ptIdx = this.currentData.currentStep - 1;
        btnPlayTake.innerHTML = `<svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor" style="display:inline-block; vertical-align:middle; margin-right:4px;"><rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/></svg> Playing...`;
        if (window.__JUCE__ && window.__JUCE__.backend) {
          window.__JUCE__.backend.emitEvent('nativeEvent', {
            action: 'playTakeAudio',
            testIndex: testIdx,
            pointIndex: ptIdx
          });
        }
        setTimeout(() => {
          if (btnPlayTake) {
            btnPlayTake.innerHTML = `<svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor" style="display:inline-block; vertical-align:middle; margin-right:4px;"><polygon points="5 3 19 12 5 21 5 3"/></svg> Play Take`;
          }
        }, 3000);
      });
    }

    // 1b. Takes Strip point pills click
    this.card.querySelectorAll('[data-modal-point-idx]').forEach(pill => {
      pill.addEventListener('click', (e) => {
        e.stopPropagation();
        const ptIdx = parseInt(pill.getAttribute('data-modal-point-idx'), 10);
        if (!isNaN(ptIdx) && typeof this.onNavigatePoint === 'function') {
          this.onNavigatePoint(ptIdx);
        }
      });
    });

    // 2. Multi-card click
    const cardsMount = this.card.querySelector('#operatorCardsMount');
    if (cardsMount) {
      cardsMount.querySelectorAll('[data-param-card-idx]').forEach(el => {
        el.addEventListener('click', () => {
          this.selectedParamIndex = parseInt(el.getAttribute('data-param-card-idx'), 10) || 0;
          this.render();
        });
      });
    }

    // 3. Action buttons
    const btnAccept = this.card.querySelector('#btnOpAccept');
    if (btnAccept) {
      btnAccept.addEventListener('click', () => {
        if (!this.isMeasuring) this.triggerAccept();
      });
    }

    const btnRepeat = this.card.querySelector('#btnOpRepeat');
    if (btnRepeat) {
      btnRepeat.addEventListener('click', () => {
        if (!this.isMeasuring && typeof this.onRepeat === 'function') {
          this.setMeasuring(true);
          this.onRepeat();
        }
      });
    }

    const btnAuto = this.card.querySelector('#btnOpAutoRun');
    if (btnAuto) {
      btnAuto.addEventListener('click', () => {
        if (!this.isMeasuring && typeof this.onAutoRun === 'function') {
          this.onAutoRun();
        }
      });
    }

    const btnCancel = this.card.querySelector('#btnOpCancel');
    if (btnCancel) {
      btnCancel.addEventListener('click', () => {
        this.triggerCancel();
      });
    }

    const btnPause = this.card.querySelector('#btnOpPause');
    if (btnPause) {
      btnPause.addEventListener('click', () => {
        this.isPaused = !this.isPaused;
        if (this.isPaused && typeof this.onPause === 'function') {
          this.onPause();
        } else if (!this.isPaused && typeof this.onResume === 'function') {
          this.onResume();
        }
        this.render();
      });
    }

    const btnReMeasure = this.card.querySelector('#btnOpReMeasure');
    if (btnReMeasure) {
      btnReMeasure.addEventListener('click', () => {
        if (typeof this.onReMeasure === 'function') {
          this.onReMeasure(this.currentData.currentStep - 1);
        }
      });
    }
  }

  triggerAccept() {
    this.setMeasuring(true);
    if (typeof this.onAccept === 'function') {
      this.onAccept();
    }
  }

  triggerCancel() {
    if (typeof this.onCancel === 'function') {
      this.onCancel();
    }
    this.close();
  }

  setMeasuring(isMeasuring) {
    this.isMeasuring = isMeasuring;
    const btnAccept = this.card ? this.card.querySelector('#btnOpAccept') : null;
    const btnRepeat = this.card ? this.card.querySelector('#btnOpRepeat') : null;
    if (btnAccept) {
      btnAccept.disabled = isMeasuring;
      btnAccept.textContent = isMeasuring ? 'Measuring Audio...' : `▶ Measure Step #${this.currentData.currentStep} [Space] →`;
    }
    if (btnRepeat) btnRepeat.disabled = isMeasuring;
  }

  updateStep(data) {
    this.currentData = {
      ...this.currentData,
      ...data
    };
    this.isMeasuring = false;
    this.render();
  }

  close() {
    if (this.keyListener) {
      window.removeEventListener('keydown', this.keyListener, true);
      this.keyListener = null;
    }
    if (this.overlay) {
      if (this.overlay.parentNode) {
        this.overlay.parentNode.removeChild(this.overlay);
      }
      this.overlay = null;
      this.card = null;
    }
    if (typeof this.options?.onClose === 'function') {
      this.options.onClose();
    }
  }
}
