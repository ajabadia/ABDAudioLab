/**
 * Stage 3A: Test Parameterization & Matrix Design
 * Sonarworks SoundID Reference Style Modal Wizard:
 * - Step 1: Preset Configurations & Stimulus Typology (art1_white / art1_black)
 * - Step 2: Signal Burst Duration & Capture Mode (art2_white / art2_black)
 * - Step 3: Measurement Matrix Resolution (art3_white / art3_black)
 * - Step 4: Plan Estimation & Execution Review (art4_white / art4_black)
 */

import {
  MASTER_PRESETS,
  PRESET_CATEGORIES,
  STIMULUS_TYPES,
  DURATION_PRESETS,
  RESOLUTION_PRESETS,
  calculatePlanEstimate,
  hasStandardRecipe,
  getRecipeForTarget,
  getCompatiblePresetsForTarget,
  getCompatibleCategoriesForTarget
} from './stage3_data.js';

function getWizardArtImg(stepNumber) {
  const isLight = document.body.classList.contains('theme-light') || 
                  document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body.getAttribute('data-theme') === 'audiolab-light';
  const suffix = isLight ? 'white' : 'black';
  return `wizard/art${stepNumber}_${suffix}.webp`;
}

function escapeHtml(str) {
  return String(str || '').replace(/[&<>"']/g, m => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[m]);
}

function renderControlIconSvg(type) {
  if (type === 'Slider') {
    return `<svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" title="Slider Control"><line x1="12" y1="2" x2="12" y2="22"></line><rect x="6" y="9" width="12" height="6" rx="2" fill="currentColor"></rect></svg>`;
  }
  if (type === 'Switch' || type === 'Toggle') {
    return `<svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" title="Toggle Switch"><rect x="3" y="6" width="18" height="12" rx="6"></rect><circle cx="9" cy="12" r="3" fill="currentColor"></circle></svg>`;
  }
  // Knob / Potentiometer / RotarySwitch
  return `<svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" title="Rotary Potentiometer / Knob"><circle cx="12" cy="12" r="9"></circle><line x1="12" y1="12" x2="16" y2="8" stroke-width="2.5"></line></svg>`;
}

/**
 * Opens the Test Configuration & Parameter Matrix Modal
 * @param {Object} options
 * @param {Object} options.initialConfig - Test configuration to edit
 * @param {Object} [options.targetInfo] - Current selected hardware or plugin target
 * @param {boolean} [options.isNewTest=false] - True if adding a new test profile
 * @param {Function} [options.onSave] - Callback when configuration is confirmed
 * @param {Function} [options.onCancel] - Callback when modal is dismissed
 */
export function openTestConfigModal({
  initialConfig = {},
  targetInfo = null,
  isNewTest = false,
  onSave = () => {},
  onCancel = () => {}
}) {
  let config = JSON.parse(JSON.stringify(initialConfig || {}));

  // Ensure default structures
  if (!config.testName) config.testName = isNewTest ? 'Custom Profile' : 'Test Profile';
  if (!config.stimulusType) config.stimulusType = 'LogFarinaSweep';
  if (typeof config.burstDurationSec !== 'number') config.burstDurationSec = 1.0;
  if (!config.captureMode) config.captureMode = 'FIXED_TIME';
  if (!Array.isArray(config.controls)) config.controls = [];

  let showAllPresets = false;
  const compatiblePresets = getCompatiblePresetsForTarget(targetInfo);
  const compatibleCategories = getCompatibleCategoriesForTarget(targetInfo);
  const isStandard = !!(config.isStandard && (hasStandardRecipe(targetInfo) || compatiblePresets.some(p => !p.isUserRecipe)));

  // If custom test, start directly on Step 3 (Matrix Resolution)
  let currentStep = !isStandard ? 3 : 1;
  const activeRecipe = getRecipeForTarget(targetInfo);
  const hardwareImage = config.hardwareImage || (targetInfo ? targetInfo.image : null) || (activeRecipe ? activeRecipe.hardwareImage : null);

  // Request VST plugin parameters if active plugin target does not have them cached yet
  const onPluginParamsReceived = (data) => {
    if (data && Array.isArray(data.parameters) && data.parameters.length > 0) {
      if (targetInfo) targetInfo.parameters = data.parameters;
      render();
    }
  };
  if (typeof window !== 'undefined' && window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.addEventListener('pluginParameters', onPluginParamsReceived);
    if (targetInfo && targetInfo.isPlugin && (!targetInfo.parameters || targetInfo.parameters.length === 0)) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'getPluginParameters' });
    }
  }

  // Create Modal Overlay
  const overlay = document.createElement('div');
  overlay.className = 'soundid-modal-overlay test-config-modal-overlay';
  overlay.setAttribute('role', 'dialog');
  overlay.setAttribute('aria-modal', 'true');

  function render() {
    const estimate = calculatePlanEstimate(config.controls, config.burstDurationSec, config.isManual);

    overlay.innerHTML = `
      <div class="test-config-modal-card">
        
        <!-- Hardware Watermark Background (when standard test or hardware target exists) -->
        ${isStandard && hardwareImage ? `
          <div class="test-config-hardware-watermark" style="background-image: url('${hardwareImage}');"></div>
        ` : ''}

        <!-- MODAL HEADER -->
        <div class="test-config-modal-header">
          <div style="display:flex; align-items:center; gap:12px;">
            <span style="font-size:13px; font-weight:800; letter-spacing:0.5px; text-transform:uppercase; color:var(--color-text-main);">
              ${isNewTest ? (isStandard ? '+ Add Standard Test Profile' : '+ Add Custom Test Profile') : 'Edit Test Configuration'}
            </span>
            <span class="badge badge-accent badge-lg" style="font-family:var(--font-mono, monospace);">
              ${escapeHtml(config.testName)}
            </span>
          </div>
          <button id="btnModalCloseConfig" type="button" class="btn btn-outline btn-circle btn-sm" title="Close dialog">✕</button>
        </div>

        <!-- SONARWORKS-STYLE SUB-STEPPER BAR -->
        <div style="padding:12px 22px 0 22px; position:relative; z-index:1;">
          <div class="stage3-wizard-stepper">
            <div class="wizard-step ${currentStep === 1 ? 'active' : (currentStep > 1 ? 'completed' : '')}" data-step="1">
              <span class="wizard-step-dot"></span>
              <span class="wizard-step-label">Preset Configurations</span>
            </div>
            <div class="wizard-step-line ${currentStep > 1 ? 'completed' : ''}"></div>

            <div class="wizard-step ${currentStep === 2 ? 'active' : (currentStep > 2 ? 'completed' : '')}" data-step="2">
              <span class="wizard-step-dot"></span>
              <span class="wizard-step-label">Signal Burst &amp; Capture</span>
            </div>
            <div class="wizard-step-line ${currentStep > 2 ? 'completed' : ''}"></div>

            <div class="wizard-step ${currentStep === 3 ? 'active' : (currentStep > 3 ? 'completed' : '')}" data-step="3">
              <span class="wizard-step-dot"></span>
              <span class="wizard-step-label">Matrix Resolution</span>
            </div>
            <div class="wizard-step-line ${currentStep > 3 ? 'completed' : ''}"></div>

            <div class="wizard-step ${currentStep === 4 ? 'active' : ''}" data-step="4">
              <span class="wizard-step-dot"></span>
              <span class="wizard-step-label">Plan Estimate</span>
            </div>
          </div>
        </div>

        <!-- SCROLLABLE MODAL BODY -->
        <div class="test-config-modal-body">
          ${renderStepBody(estimate)}
        </div>

        <!-- MODAL FOOTER -->
        <div class="test-config-modal-footer">
          <div>
            <button id="btnModalCancel" type="button" class="btn btn-outline stage3-nav-btn">
              Cancel
            </button>
          </div>
          <div class="stage3-wizard-footer-right">
            <button id="btnWizardBack" type="button" class="btn btn-outline stage3-nav-btn" ${currentStep === 1 ? 'disabled' : ''} style="min-width:100px;">
              Back
            </button>

            ${currentStep < 4 ? `
              <button id="btnWizardNext" type="button" class="btn btn-primary stage3-nav-btn" ${(currentStep === 3 && estimate.exceedsLimit) ? 'disabled style="min-width:120px; opacity:0.45; cursor:not-allowed;" title="Total combinations exceed limit of 64"' : 'style="min-width:120px;"'}>
                Next &rarr;
              </button>
            ` : `
              <button id="btnWizardApply" type="button" class="btn btn-primary stage3-nav-btn" ${estimate.exceedsLimit ? 'disabled style="min-width:210px; opacity:0.45; cursor:not-allowed;" title="Total combinations exceed limit of 64"' : 'style="min-width:210px;"'}>
                Add to Session Plan &rarr;
              </button>
            `}
          </div>
        </div>

      </div>
    `;

    bindEvents();
  }

  function renderStepBody(estimate) {
    if (currentStep === 1) {
      const activePresets = showAllPresets ? MASTER_PRESETS : compatiblePresets;
      const activeCategories = showAllPresets ? PRESET_CATEGORIES : compatibleCategories;

      return `
        <div class="stage3-wizard-body">
          <!-- Left: Data & Edit Form -->
          <div class="stage3-param-card">
            <div style="display:flex; justify-content:space-between; align-items:center;">
              <div style="display:flex; align-items:center; gap:10px;">
                <span class="badge badge-accent badge-square">1</span>
                <div>
                  <span class="stage3-section-title">PRESET CONFIGURATIONS (QUICK SETUP)</span>
                  <span class="stage3-section-sub">${showAllPresets ? 'All Available Presets' : 'Filtered for Target Hardware'}</span>
                </div>
              </div>
              <label style="font-size:11px; font-weight:600; color:var(--color-text-dim); display:flex; align-items:center; gap:6px; cursor:pointer;">
                <input type="checkbox" id="chkShowAllPresets" ${showAllPresets ? 'checked' : ''} style="cursor:pointer;" />
                Show All Presets
              </label>
            </div>

            <!-- Master Preset Selector -->
            <div>
              <select id="selPresetQuick" class="abd-select form-control" style="width:100%; height:38px; min-height:38px; text-align:left; background:var(--color-panel-surface-elevated); color:var(--color-text-main); font-weight:600; font-size:12px; padding:6px 12px; border:1px solid var(--color-panel-border); border-radius:var(--radius-md, 6px); line-height:normal; box-sizing:border-box;">
                <optgroup label="Blank / Custom Setup">
                  <option value="blank_scratch" ${config.controls.length === 0 ? 'selected' : ''}>[BLANK] Blank Matrix (Start from Scratch - 0 Parameters)</option>
                </optgroup>
                ${activeCategories.map(cat => {
                  const presets = activePresets.filter(p => p.category === cat.id);
                  if (presets.length === 0) return '';
                  return `
                    <optgroup label="${cat.label}">
                      ${presets.map(p => `
                        <option value="${p.typologyId}" ${p.typologyId === config.typologyId ? 'selected' : ''}>[${p.badgeText}] ${p.displayName}</option>
                      `).join('')}
                    </optgroup>
                  `;
                }).join('')}
              </select>
              ${activePresets.length === 0 ? `
                <div style="padding:10px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:11.5px; color:var(--color-text-dim); line-height:1.4; margin-top:8px;">
                  No certified presets registered for <strong>${escapeHtml(targetInfo ? (targetInfo.name || targetInfo.shortName) : 'this target')}</strong>. Blank profile selected — configure parameters in <strong>Step 3 (Matrix Resolution)</strong>.
                </div>
              ` : ''}
            </div>

            <div style="display:grid; grid-template-columns: 1fr 1fr; gap:16px; margin-top:4px;">
              <div>
                <label style="font-size:10px; font-weight:700; color:var(--color-text-dim); display:block; margin-bottom:5px; text-transform:uppercase; letter-spacing:0.5px;">Test Name:</label>
                <input id="txtTestName" type="text" value="${escapeHtml(config.testName || 'Custom Hardware Test')}" style="width:100%; padding:8px 12px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:600; color:var(--color-text-main); box-sizing:border-box;" />
              </div>

              <div>
                <label style="font-size:10px; font-weight:700; color:var(--color-text-dim); display:block; margin-bottom:5px; text-transform:uppercase; letter-spacing:0.5px;">Stimulus Type:</label>
                <select id="selStimulusType" style="width:100%; padding:8px 12px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:600; color:var(--color-text-main); box-sizing:border-box;">
                  ${STIMULUS_TYPES.map(s => `
                    <option value="${s.id}" ${s.id === config.stimulusType ? 'selected' : ''}>${s.label}</option>
                  `).join('')}
                </select>
                <div id="lblStimulusDesc" style="font-size:10px; font-style:italic; color:var(--color-text-dim); margin-top:5px;">
                  ${(STIMULUS_TYPES.find(s => s.id === config.stimulusType) || STIMULUS_TYPES[0]).desc}
                </div>
              </div>
            </div>
          </div>

          <!-- Right: Representative Square Artwork Card (Art 1) -->
          <div class="stage3-wizard-art-card">
            <img src="${getWizardArtImg(1)}" alt="Preset Typology &amp; Excitation" class="wizard-art-img-full" onerror="this.src='wizard/art1_white.webp'" />
            <div class="wizard-art-gradient-overlay"></div>
            <div class="wizard-art-content-bottom">
              <div class="wizard-art-title">Preset Typology &amp; Excitation</div>
              <div class="wizard-art-sub">Calibrated stepped rotary sweep for high-SNR transfer function acquisition.</div>
            </div>
          </div>
        </div>
      `;
    }

    if (currentStep === 2) {
      return `
        <div class="stage3-wizard-body">
          <!-- Left: Data & Edit Form -->
          <div class="stage3-param-card">
            <div style="display:flex; align-items:center; gap:10px;">
              <span class="badge badge-accent badge-square">2</span>
              <div>
                <span class="stage3-section-title">SIGNAL BURST DURATION &amp; CAPTURE MODE</span>
                <span class="stage3-section-sub">Excitation window duration and auto-tail threshold</span>
              </div>
            </div>

            <div style="display:grid; grid-template-columns: 1fr auto; gap:16px; align-items:flex-end;">
              <div>
                <label style="font-size:10px; font-weight:700; color:var(--color-text-dim); display:block; margin-bottom:5px; text-transform:uppercase; letter-spacing:0.5px;">Excitation Profile:</label>
                <select id="selDurationPreset" style="width:100%; padding:8px 12px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:600; color:var(--color-text-main); box-sizing:border-box;">
                  ${(() => {
                    const isPresetMatch = DURATION_PRESETS.some(d => d.val !== 'custom' && Math.abs(d.val - config.burstDurationSec) < 0.01);
                    return DURATION_PRESETS.map(d => {
                      const isSelected = d.val === 'custom' ? !isPresetMatch : (isPresetMatch && Math.abs(d.val - config.burstDurationSec) < 0.01);
                      return `<option value="${d.val}" ${isSelected ? 'selected' : ''}>${d.label}</option>`;
                    }).join('');
                  })()}
                </select>
              </div>

              <div>
                <label style="font-size:10px; font-weight:700; color:var(--color-text-dim); display:block; margin-bottom:5px; text-transform:uppercase; letter-spacing:0.5px;">Custom Time:</label>
                <div style="display:flex; align-items:center; gap:6px;">
                  ${(() => {
                    const isCustom = !DURATION_PRESETS.some(d => d.val !== 'custom' && Math.abs(d.val - config.burstDurationSec) < 0.01);
                    return `<input id="txtManualDuration" type="number" step="0.1" min="0.1" max="60.0" value="${config.burstDurationSec.toFixed(2)}" ${isCustom ? '' : 'disabled'} style="width:74px; padding:7px 8px; text-align:center; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:700; color:var(--color-text-main); font-family:var(--font-mono, monospace); ${isCustom ? '' : 'opacity:0.45; cursor:not-allowed;'}" />`;
                  })()}
                  <span style="font-size:11px; font-weight:600; color:var(--color-text-dim);">sec</span>
                </div>
              </div>
            </div>

            <div style="display:flex; align-items:center; gap:10px; padding:12px 14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border-dim); border-radius:var(--radius-md); margin-top:8px;">
              <input type="checkbox" id="chkAdaptiveTail" ${config.captureMode === 'ADAPTIVE_ENVELOPE' ? 'checked' : ''} style="cursor:pointer;" />
              <label for="chkAdaptiveTail" style="font-size:11.5px; font-weight:600; color:var(--color-text-main); cursor:pointer;">
                Adaptive Auto-Tail Silence Cutoff (for ADSR / Reverb) &mdash; <span style="font-weight:400; color:var(--color-text-dim);">cuts capture when envelope drops below -60 dBFS</span>
              </label>
            </div>
          </div>

          <!-- Right: Representative Square Artwork Card (Art 2) -->
          <div class="stage3-wizard-art-card">
            <img src="${getWizardArtImg(2)}" alt="Acoustic Burst &amp; Time Windowing" class="wizard-art-img-full" onerror="this.src='wizard/art2_white.webp'" />
            <div class="wizard-art-gradient-overlay"></div>
            <div class="wizard-art-content-bottom">
              <div class="wizard-art-title">Acoustic Burst &amp; Time Windowing</div>
              <div class="wizard-art-sub">${config.burstDurationSec.toFixed(2)}s excitation window with auto-tail acoustic gating.</div>
            </div>
          </div>
        </div>
      `;
    }

    if (currentStep === 3) {
      return `
        <div class="stage3-wizard-body">
          <!-- Left: Data & Edit Form -->
          <div class="stage3-param-card">
            <div style="display:flex; justify-content:space-between; align-items:center;">
              <div style="display:flex; align-items:center; gap:10px;">
                <span class="badge badge-accent badge-square">3</span>
                <div>
                  <span class="stage3-section-title">MEASUREMENT MATRIX RESOLUTION (SET STEPS PER CONTROL)</span>
                  <span class="stage3-section-sub">Step resolution per control widget &amp; hardware/plugin protocol binding</span>
                </div>
              </div>
              <div style="display:flex; align-items:center; gap:8px;">
                <button id="btnClearMatrixControls" type="button" class="btn btn-outline btn-sm" style="color:var(--color-text-dim);" title="Clear all parameters to start completely blank">
                  🗑 Clear All (Blank)
                </button>
                <button id="btnAddParamRow" type="button" class="btn btn-accent btn-sm">
                  + Add Parameter
                </button>
              </div>
            </div>

            <div style="border:1px solid var(--color-panel-border); border-radius:var(--radius-md); overflow:hidden; background:var(--color-bg-base);">
              <div style="display:grid; grid-template-columns: 28px 1fr auto minmax(150px, 215px) 32px; background:var(--color-panel-surface-elevated); padding:8px 12px; font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.5px; border-bottom:1px solid var(--color-panel-border); gap:8px; align-items:center;">
                <span>Icon</span>
                <span>Parameter</span>
                <span>Binding</span>
                <span>Step Resolution</span>
                <span style="text-align:right;">Adv.</span>
              </div>

              <div id="matrixTableRows">
                ${config.controls.length === 0 ? `
                  <div style="padding:32px 16px; text-align:center; color:var(--color-text-dim); font-size:12px;">
                    Parameter matrix is blank (0 states).<br />
                    Click <strong>+ Add Parameter</strong> to bind a ${targetInfo?.isPlugin ? 'VST3 parameter' : 'MIDI CC / NRPN'} or manual control.
                  </div>
                ` : config.controls.map((ctrl, idx) => renderControlRow(ctrl, idx)).join('')}
              </div>
            </div>

            <!-- Plan Estimate Banner Matching C++ Original -->
            <div style="display:flex; justify-content:space-between; align-items:center; padding:10px 14px; background:${estimate.exceedsLimit ? 'rgba(255, 77, 77, 0.12)' : 'var(--color-panel-surface-elevated)'}; border:1px solid ${estimate.exceedsLimit ? 'var(--color-danger, #ff5252)' : 'var(--color-panel-border)'}; border-radius:var(--radius-md); margin-top:8px;">
              <div>
                <div style="display:flex; align-items:center; gap:8px; margin-bottom:3px;">
                  <span class="badge ${estimate.exceedsLimit ? 'badge-danger' : 'badge-accent'} badge-sm">${estimate.exceedsLimit ? 'LÍMITE EXCEDIDO' : 'PLAN ESTIMATE'}</span>
                  <span style="font-size:11.5px; font-weight:700; color:${estimate.exceedsLimit ? 'var(--color-danger, #ff5252)' : 'var(--color-text-main)'}; font-family:var(--font-mono, monospace);">
                    ${estimate.states} measurement states (${estimate.formula})
                  </span>
                </div>
                <div style="font-size:10.5px; font-weight:600; color:${estimate.exceedsLimit ? 'var(--color-danger, #ff5252)' : (estimate.isManual ? 'var(--color-warning)' : 'var(--color-accent)')};">
                  ${estimate.exceedsLimit 
                    ? `⚠️ El límite máximo soportado es de ${estimate.maxLimit} combinaciones por perfil (${estimate.states} calculadas). Reduce la resolución de pasos por control (ej. 8 × 8 = 64).` 
                    : (estimate.isManual ? `✓ Manual profiling • ${estimate.states + config.controls.length} control adjustments` : '✓ Fully automated profiling. No manual adjustments required.')}
                </div>
              </div>
              <div style="text-align:right;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; display:block;">Estimated Time:</span>
                <span style="font-size:12.5px; font-weight:800; color:${estimate.exceedsLimit ? 'var(--color-danger, #ff5252)' : 'var(--color-accent)'}; font-family:var(--font-mono, monospace);">~${estimate.durationStr}</span>
              </div>
            </div>
          </div>

          <!-- Right: Representative Square Artwork Card (Art 3) -->
          <div class="stage3-wizard-art-card">
            <img src="${getWizardArtImg(3)}" alt="Multidimensional Parameter Grid" class="wizard-art-img-full" onerror="this.src='wizard/art3_white.webp'" />
            <div class="wizard-art-gradient-overlay"></div>
            <div class="wizard-art-content-bottom">
              <div class="wizard-art-title">Multidimensional Parameter Grid</div>
              <div class="wizard-art-sub">${estimate.states} discrete physical measurement points across ${config.controls.length} controls (${estimate.formula}).</div>
            </div>
          </div>
        </div>
      `;
    }

    // Step 4: Plan Estimation & Review
    return `
      <div class="stage3-wizard-body">
        <!-- Left: Summary & Review -->
        <div class="stage3-param-card">
          <div style="display:flex; align-items:center; gap:10px;">
            <span class="badge badge-accent badge-square">4</span>
            <div>
              <span class="stage3-section-title">PLAN ESTIMATION &amp; EXECUTION REVIEW</span>
              <span class="stage3-section-sub">Verify states, estimated duration, and acquisition mode</span>
            </div>
          </div>

          <div style="display:flex; flex-direction:column; gap:10px; margin-top:8px;">
            <div style="padding:12px 14px; border-radius:var(--radius-md); background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); display:flex; justify-content:space-between; align-items:center;">
              <div>
                <strong style="font-size:12px; color:var(--color-text-main); display:block;">Target &amp; Typology</strong>
                <span style="font-size:11px; color:var(--color-text-dim);">${escapeHtml(config.testName)} &bull; ${config.typologyId || 'Generic'}</span>
              </div>
              <span class="badge badge-success badge-xs">VERIFIED</span>
            </div>

            <div style="padding:12px 14px; border-radius:var(--radius-md); background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); display:flex; justify-content:space-between; align-items:center;">
              <div>
                <strong style="font-size:12px; color:var(--color-text-main); display:block;">Excitation Stimulus &amp; Duration</strong>
                <span style="font-size:11px; color:var(--color-text-dim);">${config.burstDurationSec.toFixed(2)} sec / point &bull; ${(STIMULUS_TYPES.find(s => s.id === config.stimulusType) || STIMULUS_TYPES[0]).label}</span>
              </div>
              <span class="badge badge-info badge-xs">${config.captureMode === 'ADAPTIVE_ENVELOPE' ? 'ADAPTIVE' : 'FIXED'}</span>
            </div>

            ${estimate.exceedsLimit ? `
              <div style="padding:12px 14px; border-radius:var(--radius-md); background:rgba(255, 77, 77, 0.12); border:1px solid var(--color-danger, #ff5252); display:flex; align-items:center; gap:10px;">
                <span style="font-size:16px;">⚠️</span>
                <div style="font-size:11.5px; color:var(--color-danger, #ff5252); font-weight:600;">
                  No es posible añadir a la cola: se han superado las ${estimate.maxLimit} combinaciones máximas (${estimate.states} calculadas). Vuelve al paso 3 para reducir la resolución de los controles.
                </div>
              </div>
            ` : ''}

            <div style="padding:12px 14px; border-radius:var(--radius-md); background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); display:flex; justify-content:space-between; align-items:center;">
              <div>
                <strong style="font-size:12px; color:var(--color-text-main); display:block;">Matrix Permutations</strong>
                <span style="font-size:11px; color:var(--color-text-dim);">${estimate.states} states across ${config.controls.length} controls</span>
              </div>
              <span class="badge ${estimate.exceedsLimit ? 'badge-danger' : 'badge-warning'} badge-xs">${estimate.exceedsLimit ? 'EXCEEDED' : (estimate.isManual ? 'MANUAL' : 'AUTOMATED')}</span>
            </div>
          </div>
        </div>

        <!-- Right: Representative Square Artwork Card (Art 4) -->
        <div class="stage3-wizard-art-card">
          <img src="${getWizardArtImg(4)}" alt="Plan Orchestration &amp; Estimation" class="wizard-art-img-full" onerror="this.src='wizard/art4_white.webp'" />
          <div class="wizard-art-gradient-overlay"></div>
          <div class="wizard-art-content-bottom">
            <div style="font-size:9.5px; font-weight:800; text-transform:uppercase; letter-spacing:0.8px; color:var(--color-accent); margin-bottom:2px;">
              Total Estimated Time
            </div>
            <div style="font-size:24px; font-weight:800; color:var(--color-accent); font-family:var(--font-mono, monospace); margin-bottom:2px;">
              ~${estimate.durationStr}
            </div>
            <div class="wizard-art-title">${estimate.states} measurement states</div>
            <div class="wizard-art-sub">${estimate.isManual ? 'Manual operator adjustment required during testing' : 'Fully automated continuous acquisition'}</div>
          </div>
        </div>
      </div>
    `;
  }

  function resolveBoundParameter(ctrl, targetInfo) {
    if (!targetInfo || !targetInfo.parameters || targetInfo.parameters.length === 0) {
      return null;
    }
    // 1. If ctrl already has a valid numeric paramIndex, check if it matches an existing param
    if (typeof ctrl.paramIndex === 'number') {
      const existing = targetInfo.parameters.find(p => p.index === ctrl.paramIndex);
      if (existing) return existing;
    }

    // 2. Fuzzy match against control name
    const ctrlNameLower = (ctrl.name || '').toLowerCase().trim();
    if (!ctrlNameLower) return targetInfo.parameters[0];

    const tokens = ctrlNameLower.split(/[\s\-_/]+/).filter(Boolean);
    let bestParam = null;
    let bestScore = -1;

    for (const p of targetInfo.parameters) {
      const pNameLower = ((p.name || '') + ' ' + (p.label || '')).toLowerCase();
      let score = 0;

      if (pNameLower === ctrlNameLower) score += 100;

      for (const t of tokens) {
        if (pNameLower.includes(t)) score += 20;
      }

      // Domain heuristics for synthesizer parameters:
      if (ctrlNameLower.includes('cutoff') && (pNameLower.includes('cutoff') || pNameLower.includes('cut') || pNameLower.includes('freq'))) {
        score += 40;
      }
      if (ctrlNameLower.includes('resonance') && (pNameLower.includes('reso') || pNameLower.includes('res') || pNameLower.includes('q') || pNameLower.includes('width'))) {
        score += 40;
      }
      if (ctrlNameLower.includes('eg int') || ctrlNameLower.includes('env') || ctrlNameLower.includes('filter eg')) {
        if (pNameLower.includes('eg') || pNameLower.includes('env') || pNameLower.includes('amount') || pNameLower.includes('int')) {
          score += 35;
        }
      }
      if (ctrlNameLower.includes('attack') && (pNameLower.includes('attack') || pNameLower.includes('att'))) score += 35;
      if (ctrlNameLower.includes('decay') && (pNameLower.includes('decay') || pNameLower.includes('dec'))) score += 35;
      if (ctrlNameLower.includes('sustain') && (pNameLower.includes('sustain') || pNameLower.includes('sus'))) score += 35;
      if (ctrlNameLower.includes('release') && (pNameLower.includes('release') || pNameLower.includes('rel'))) score += 35;

      if (score > bestScore) {
        bestScore = score;
        bestParam = p;
      }
    }

    return bestScore > 0 ? bestParam : null;
  }

  function getBindingBadge(ctrl, resolvedParam = null) {
    const method = ctrl.controlMethod || (targetInfo?.isPlugin ? 'VST_PARAM' : (typeof ctrl.cc !== 'undefined' ? 'MIDI_CC' : (typeof ctrl.nrpn !== 'undefined' ? 'MIDI_NRPN' : 'MANUAL')));
    if (method === 'VST_PARAM') {
      const effectiveIndex = (resolvedParam && typeof resolvedParam.index === 'number') 
        ? resolvedParam.index 
        : (typeof ctrl.paramIndex !== 'undefined' ? ctrl.paramIndex : 0);
      return `<span class="badge badge-accent badge-xs" style="font-family:var(--font-mono, monospace);" title="VST3 Parameter Automation">VST #${effectiveIndex}</span>`;
    }
    if (method === 'MIDI_CC') {
      const ccNum = typeof ctrl.cc !== 'undefined' ? ctrl.cc : 74;
      return `<span class="badge badge-outline badge-xs" style="color:#64b5f6; border-color:#64b5f6; font-family:var(--font-mono, monospace);" title="MIDI Continuous Controller">CC #${ccNum}</span>`;
    }
    if (method === 'MIDI_NRPN') {
      const nrpnNum = typeof ctrl.nrpn !== 'undefined' ? ctrl.nrpn : 0;
      return `<span class="badge badge-outline badge-xs" style="color:#ba68c8; border-color:#ba68c8; font-family:var(--font-mono, monospace);" title="14-bit High-Resolution NRPN">NRPN #${nrpnNum}</span>`;
    }
    if (method === 'SYSEX') {
      return `<span class="badge badge-outline badge-xs" style="color:#ffb74d; border-color:#ffb74d; font-family:var(--font-mono, monospace);" title="MIDI System Exclusive Message">SysEx</span>`;
    }
    return `<span class="badge badge-outline badge-xs" style="color:var(--color-text-dim);" title="Manual Physical Potentiometer Adjustment">Manual</span>`;
  }

  function renderControlRow(ctrl, idx) {
    const isExpanded = !!ctrl.isExpanded;
    const isCustomSteps = ctrl.isCustomSteps || !RESOLUTION_PRESETS.some(r => r.steps !== 'custom' && r.steps === ctrl.steps);

    // Auto-resolve parameter for VST3 target
    let resolvedParam = null;
    if (targetInfo?.isPlugin && targetInfo?.parameters && targetInfo.parameters.length > 0) {
      resolvedParam = resolveBoundParameter(ctrl, targetInfo);
      if (typeof ctrl.paramIndex !== 'number' && resolvedParam) {
        ctrl.paramIndex = resolvedParam.index;
      } else if (typeof ctrl.paramIndex === 'number') {
        resolvedParam = targetInfo.parameters.find(p => p.index === ctrl.paramIndex) || resolvedParam;
      }
    }

    // Compute human-readable label of the real bound parameter
    let resolvedLabel = '';
    const method = ctrl.controlMethod || (targetInfo?.isPlugin ? 'VST_PARAM' : (typeof ctrl.cc !== 'undefined' ? 'MIDI_CC' : (typeof ctrl.nrpn !== 'undefined' ? 'MIDI_NRPN' : 'MANUAL')));
    if (method === 'VST_PARAM') {
      if (resolvedParam) {
        resolvedLabel = `#${resolvedParam.index}: ${resolvedParam.name}${resolvedParam.label ? ' (' + resolvedParam.label + ')' : ''}`;
      } else {
        resolvedLabel = `VST #${ctrl.paramIndex ?? 0}`;
      }
    } else if (method === 'MIDI_CC') {
      resolvedLabel = `MIDI CC #${typeof ctrl.cc !== 'undefined' ? ctrl.cc : 74}`;
    } else if (method === 'MIDI_NRPN') {
      resolvedLabel = `MIDI NRPN #${typeof ctrl.nrpn !== 'undefined' ? ctrl.nrpn : 0}`;
    } else if (method === 'SYSEX') {
      resolvedLabel = 'SysEx Hex Message';
    } else {
      resolvedLabel = 'Manual Potentiometer';
    }

    return `
      <div style="border-bottom:1px solid var(--color-panel-border-dim);">
        <div style="display:grid; grid-template-columns: 28px 1fr auto minmax(150px, 215px) 32px; align-items:center; padding:8px 12px; gap:8px;">
          <span style="display:flex; align-items:center; justify-content:center; color:var(--color-text-dim);">
            ${renderControlIconSvg(ctrl.type)}
          </span>

          <div style="display:flex; flex-direction:column; gap:2px; min-width:0;">
            <input type="text" data-ctrl-field="name" data-idx="${idx}" value="${escapeHtml(ctrl.name)}" ${isStandard ? 'readonly title="Standard preset parameter name (read-only)"' : ''} style="background:transparent; border:none; color:var(--color-text-main); font-size:11.5px; font-weight:600; outline:none; text-overflow:ellipsis; ${isStandard ? 'cursor:default;' : ''}" placeholder="Parameter Name" />
            <div style="font-size:10px; color:var(--color-text-dim); display:flex; align-items:center; gap:4px; font-family:var(--font-mono, monospace); white-space:nowrap; overflow:hidden; text-overflow:ellipsis;" title="Real bound parameter: ${escapeHtml(resolvedLabel)}">
              <span style="color:var(--color-accent); font-weight:700;">↳ Real:</span>
              <span style="color:var(--color-text-main);">${escapeHtml(resolvedLabel)}</span>
            </div>
          </div>

          <div style="display:flex; align-items:center;">
            ${getBindingBadge(ctrl, resolvedParam)}
          </div>

          <div style="display:flex; align-items:center; gap:6px; justify-content:flex-end;">
            <select data-ctrl-field="steps" data-idx="${idx}" style="${isCustomSteps ? 'width:105px;' : 'width:150px;'} padding:5px 6px; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:4px; font-size:11px; color:var(--color-text-main); font-weight:600;">
              ${RESOLUTION_PRESETS.map(r => {
                const isSelected = r.steps === 'custom' ? isCustomSteps : (!isCustomSteps && r.steps === ctrl.steps);
                return `<option value="${r.steps}" ${isSelected ? 'selected' : ''}>${r.label}</option>`;
              }).join('')}
            </select>

            ${isCustomSteps ? `
              <div style="display:flex; align-items:center; gap:3px;">
                <input type="number" min="1" max="64" data-ctrl-field="customSteps" data-idx="${idx}" value="${ctrl.steps || 4}" style="width:44px; padding:3.5px 4px; text-align:center; font-size:11px; font-weight:700; background:var(--color-bg-base); border:1px solid var(--color-accent); border-radius:4px; color:var(--color-accent); font-family:var(--font-mono, monospace);" title="Specify custom steps/points (1 - 64)" />
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim);">pts</span>
              </div>
            ` : ''}
          </div>

          <button type="button" data-action="toggle-adv" data-idx="${idx}" class="btn btn-ghost btn-xs" title="Toggle protocol binding &amp; advanced settings">
            ${isExpanded ? '▲' : '▼'}
          </button>
        </div>

        <!-- Advanced Settings Subpanel with Protocol Binding -->
        <div style="display:${isExpanded ? 'flex' : 'none'}; flex-direction:column; gap:10px; padding:10px 14px 12px 44px; background:var(--color-panel-surface-elevated); border-top:1px dashed var(--color-panel-border-dim);">
          ${isStandard ? `
            <div style="display:flex; flex-direction:column; gap:8px;">
              <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:8px; padding:6px 10px; background:rgba(0,0,0,0.18); border-radius:4px; border:1px solid var(--color-panel-border-dim);">
                <div style="display:flex; align-items:center; gap:8px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">Protocol Binding:</span>
                  <span class="badge badge-accent badge-sm" style="font-family:var(--font-mono, monospace);">
                    ${escapeHtml(resolvedLabel)}
                  </span>
                  <span style="font-size:10.5px; color:var(--color-text-dim);">(Pre-calibrated for Standard Profile)</span>
                </div>
                <div style="font-size:10.5px; color:var(--color-text-dim); font-family:var(--font-mono, monospace);">
                  Range: <strong>${ctrl.minPct || 0}% &mdash; ${ctrl.maxPct || 100}%</strong>
                </div>
              </div>
            </div>
          ` : `
            <!-- Protocol and Channel Assignment for Custom Profiles -->
            <div style="display:flex; flex-wrap:wrap; align-items:center; gap:14px;">
              <div style="display:flex; align-items:center; gap:6px;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">Protocol:</span>
                <select data-ctrl-field="controlMethod" data-idx="${idx}" style="padding:4px 8px; font-size:11px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main); font-weight:600;">
                  ${targetInfo?.isPlugin ? `
                    <option value="VST_PARAM" ${(ctrl.controlMethod === 'VST_PARAM' || !ctrl.controlMethod) ? 'selected' : ''}>VST3 Parameter Automation</option>
                    <option value="MIDI_CC" ${ctrl.controlMethod === 'MIDI_CC' ? 'selected' : ''}>MIDI CC</option>
                    <option value="MANUAL" ${ctrl.controlMethod === 'MANUAL' ? 'selected' : ''}>Manual Knob</option>
                  ` : `
                    <option value="MIDI_CC" ${(ctrl.controlMethod === 'MIDI_CC' || !ctrl.controlMethod) ? 'selected' : ''}>MIDI CC (Continuous Controller)</option>
                    <option value="MIDI_NRPN" ${ctrl.controlMethod === 'MIDI_NRPN' ? 'selected' : ''}>MIDI NRPN (14-bit High-Res)</option>
                    <option value="SYSEX" ${ctrl.controlMethod === 'SYSEX' ? 'selected' : ''}>SysEx (System Exclusive)</option>
                    <option value="MANUAL" ${ctrl.controlMethod === 'MANUAL' ? 'selected' : ''}>Manual Potentiometer</option>
                  `}
                </select>
              </div>

              ${(ctrl.controlMethod === 'VST_PARAM' || (!ctrl.controlMethod && targetInfo?.isPlugin)) ? `
                <div style="display:flex; align-items:center; gap:6px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">VST Param:</span>
                  ${(targetInfo?.parameters && targetInfo.parameters.length > 0) ? `
                    <div style="display:flex; align-items:center; gap:4px;">
                      <input type="text" data-action="filter-vst-params" data-idx="${idx}" placeholder="🔍 Search param..." value="${ctrl.paramFilterQuery || ''}" style="width:115px; padding:3.5px 7px; font-size:10.5px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);" title="Type to filter VST parameters by name or index" />
                      <select data-ctrl-field="paramIndex" data-idx="${idx}" style="max-width:240px; padding:4px 8px; font-size:11px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);">
                        ${(ctrl.paramFilterQuery 
                            ? targetInfo.parameters.filter(p => (`#${p.index} ${p.name || ''} ${p.label || ''}`).toLowerCase().includes(ctrl.paramFilterQuery.toLowerCase()))
                            : targetInfo.parameters
                          ).map(p => `
                          <option value="${p.index}" ${ctrl.paramIndex === p.index ? 'selected' : ''}>
                            #${p.index}: ${escapeHtml(p.name)} ${p.label ? '(' + escapeHtml(p.label) + ')' : ''}
                          </option>
                        `).join('')}
                      </select>
                    </div>
                  ` : `
                    <input type="number" min="0" data-ctrl-field="paramIndex" data-idx="${idx}" value="${ctrl.paramIndex ?? 0}" style="width:60px; padding:4px 6px; font-size:11px; text-align:center; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);" placeholder="Index (0)" />
                  `}
                </div>
              ` : ''}

              ${ctrl.controlMethod === 'MIDI_CC' ? `
                <div style="display:flex; align-items:center; gap:6px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">CC #:</span>
                  <input type="number" min="0" max="127" data-ctrl-field="cc" data-idx="${idx}" value="${ctrl.cc ?? 74}" style="width:55px; padding:4px 6px; font-size:11px; text-align:center; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);" placeholder="74" />
                </div>
              ` : ''}

              ${ctrl.controlMethod === 'MIDI_NRPN' ? `
                <div style="display:flex; align-items:center; gap:6px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">NRPN #:</span>
                  <input type="number" min="0" max="16383" data-ctrl-field="nrpn" data-idx="${idx}" value="${ctrl.nrpn ?? 0}" style="width:70px; padding:4px 6px; font-size:11px; text-align:center; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);" placeholder="288" />
                </div>
              ` : ''}

              ${ctrl.controlMethod === 'SYSEX' ? `
                <div style="display:flex; align-items:center; gap:6px; flex:1;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">SysEx Hex:</span>
                  <input type="text" data-ctrl-field="sysex" data-idx="${idx}" value="${escapeHtml(ctrl.sysex || '')}" style="flex:1; min-width:140px; padding:4px 8px; font-size:11px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); border-radius:4px; color:var(--color-text-main);" placeholder="F0 41 10 ... F7" />
                </div>
              ` : ''}
            </div>

            <!-- Range, Order and Remove -->
            <div style="display:flex; justify-content:space-between; align-items:center; padding-top:4px;">
              <div style="display:flex; align-items:center; gap:16px;">
                <div style="display:flex; align-items:center; gap:6px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">Range:</span>
                  <input type="number" min="0" max="100" data-ctrl-field="minPct" data-idx="${idx}" value="${ctrl.minPct || 0}" style="width:48px; padding:4px 6px; text-align:center; font-size:10.5px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); color:var(--color-text-main);" />
                  <span style="font-size:10px; color:var(--color-text-dim);">&mdash;</span>
                  <input type="number" min="0" max="100" data-ctrl-field="maxPct" data-idx="${idx}" value="${ctrl.maxPct || 100}" style="width:48px; padding:4px 6px; text-align:center; font-size:10.5px; background:var(--color-bg-base); border:1px solid var(--color-panel-border); color:var(--color-text-main);" />
                  <span style="font-size:10px; color:var(--color-text-dim);">%</span>
                </div>

                <div style="display:flex; align-items:center; gap:4px;">
                  <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase;">Order:</span>
                  <button type="button" data-action="move-up" data-idx="${idx}" class="btn btn-outline btn-xs" ${idx === 0 ? 'disabled' : ''}>▲ Earlier</button>
                  <button type="button" data-action="move-down" data-idx="${idx}" class="btn btn-outline btn-xs" ${idx === config.controls.length - 1 ? 'disabled' : ''}>▼ Later</button>
                </div>
              </div>

              <button type="button" data-action="remove-row" data-idx="${idx}" class="btn btn-ghost btn-xs" style="color:var(--color-danger);">
                ✕ Remove Control
              </button>
            </div>
          `}
        </div>
      </div>
    `;
  }

  function dismiss() {
    window.removeEventListener('keydown', onKeyDown);
    if (typeof window !== 'undefined' && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.removeEventListener('pluginParameters', onPluginParamsReceived);
    }
    overlay.classList.remove('active');
    setTimeout(() => {
      if (overlay.parentNode) {
        overlay.parentNode.removeChild(overlay);
      }
    }, 220);
  }

  function onKeyDown(e) {
    if (e.key === 'Escape') {
      dismiss();
      onCancel();
    }
  }

  function bindEvents() {
    // Top-right close button
    const btnClose = overlay.querySelector('#btnModalCloseConfig');
    if (btnClose) {
      btnClose.addEventListener('click', () => {
        dismiss();
        onCancel();
      });
    }

    // Bottom Cancel button
    const btnCancel = overlay.querySelector('#btnModalCancel');
    if (btnCancel) {
      btnCancel.addEventListener('click', () => {
        dismiss();
        onCancel();
      });
    }

    // Stepper header navigation
    overlay.querySelectorAll('.wizard-step').forEach(stepEl => {
      stepEl.addEventListener('click', (e) => {
        const stepNum = parseInt(e.currentTarget.dataset.step, 10);
        if (stepNum >= 1 && stepNum <= 4) {
          currentStep = stepNum;
          render();
        }
      });
    });

    // Wizard Back / Next
    const btnBack = overlay.querySelector('#btnWizardBack');
    if (btnBack) {
      btnBack.addEventListener('click', () => {
        if (currentStep > 1) {
          currentStep--;
          render();
        }
      });
    }

    const btnNext = overlay.querySelector('#btnWizardNext');
    if (btnNext) {
      btnNext.addEventListener('click', () => {
        if (currentStep < 4) {
          currentStep++;
          render();
        }
      });
    }

    // Apply to Suite Queue
    const btnApply = overlay.querySelector('#btnWizardApply');
    if (btnApply) {
      btnApply.addEventListener('click', () => {
        dismiss();
        onSave(config);
      });
    }

    // Preset Quick Selector (Step 1)
    const selPreset = overlay.querySelector('#selPresetQuick');
    if (selPreset) {
      selPreset.addEventListener('change', (e) => {
        if (e.target.value === 'blank_scratch') {
          config.typologyId = 'Custom';
          config.testName = 'Custom Profile';
          config.controls = [];
          render();
          return;
        }
        const found = MASTER_PRESETS.find(p => p.typologyId === e.target.value);
        if (found) {
          config.typologyId = found.typologyId;
          config.testName = found.displayName;
          config.stimulusType = found.stimulusType;
          config.burstDurationSec = found.burstDurationSec;
          config.captureMode = found.captureMode;
          config.controls = JSON.parse(JSON.stringify(found.defaultControls));
          render();
        }
      });
    }

    const chkShowAll = overlay.querySelector('#chkShowAllPresets');
    if (chkShowAll) {
      chkShowAll.addEventListener('change', (e) => {
        showAllPresets = e.target.checked;
        render();
      });
    }

    // Test Name (Step 1)
    const txtName = overlay.querySelector('#txtTestName');
    if (txtName) {
      txtName.addEventListener('input', (e) => {
        config.testName = e.target.value;
      });
    }

    // Stimulus Type (Step 1)
    const selStim = overlay.querySelector('#selStimulusType');
    if (selStim) {
      selStim.addEventListener('change', (e) => {
        config.stimulusType = e.target.value;
        const sObj = STIMULUS_TYPES.find(s => s.id === config.stimulusType);
        const descEl = overlay.querySelector('#lblStimulusDesc');
        if (descEl && sObj) descEl.textContent = sObj.desc;
      });
    }

    // Duration Preset (Step 2)
    const selDur = overlay.querySelector('#selDurationPreset');
    const txtDur = overlay.querySelector('#txtManualDuration');
    if (selDur && txtDur) {
      selDur.addEventListener('change', (e) => {
        if (e.target.value === 'custom') {
          txtDur.disabled = false;
          txtDur.style.opacity = '1';
          txtDur.style.cursor = 'text';
          txtDur.focus();
          txtDur.select();
        } else {
          txtDur.disabled = true;
          txtDur.style.opacity = '0.45';
          txtDur.style.cursor = 'not-allowed';
          config.burstDurationSec = parseFloat(e.target.value);
          txtDur.value = config.burstDurationSec.toFixed(2);
          render();
        }
      });

      txtDur.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        if (!isNaN(val) && val >= 0.1) {
          config.burstDurationSec = val;
        }
      });
    }

    // Adaptive Tail Checkbox (Step 2)
    const chkTail = overlay.querySelector('#chkAdaptiveTail');
    if (chkTail) {
      chkTail.addEventListener('change', (e) => {
        config.captureMode = e.target.checked ? 'ADAPTIVE_ENVELOPE' : 'FIXED_TIME';
      });
    }

    // Clear All (Blank) button (Step 3)
    const btnClearMat = overlay.querySelector('#btnClearMatrixControls');
    if (btnClearMat) {
      btnClearMat.addEventListener('click', () => {
        config.controls = [];
        render();
      });
    }

    // Add Parameter button (Step 3)
    const btnAdd = overlay.querySelector('#btnAddParamRow');
    if (btnAdd) {
      btnAdd.addEventListener('click', () => {
        const newIdx = config.controls.length + 1;
        let defaultMethod = targetInfo?.isPlugin ? 'VST_PARAM' : (targetInfo?.midi ? 'MIDI_CC' : 'MANUAL');
        let defaultParamIdx = 0;
        let defaultName = `Parameter ${newIdx}`;
        let defaultCc = 74;

        if (targetInfo?.isPlugin && Array.isArray(targetInfo.parameters) && targetInfo.parameters.length > 0) {
          const usedIndices = new Set(config.controls.map(c => c.paramIndex));
          const nextParam = targetInfo.parameters.find(p => !usedIndices.has(p.index)) || targetInfo.parameters[0];
          defaultParamIdx = nextParam.index;
          defaultName = nextParam.name;
        } else if (defaultMethod === 'MIDI_CC') {
          const usedCcs = new Set(config.controls.map(c => c.cc));
          const commonCcs = [74, 71, 73, 72, 1, 7, 10, 11];
          defaultCc = commonCcs.find(cc => !usedCcs.has(cc)) || (70 + newIdx);
          defaultName = `CC #${defaultCc}`;
        }

        config.controls.push({
          id: `param_${Date.now()}_${newIdx}`,
          name: defaultName,
          type: 'Knob',
          steps: 4,
          minPct: 0,
          maxPct: 100,
          controlMethod: defaultMethod,
          paramIndex: defaultParamIdx,
          cc: defaultCc,
          nrpn: 0,
          sysex: '',
          isExpanded: true
        });
        render();
      });
    }

    // Dynamic Control Row Inputs & Actions (Step 3)
    overlay.querySelectorAll('[data-ctrl-field]').forEach(el => {
      el.addEventListener('change', (e) => {
        const idx = parseInt(e.target.dataset.idx, 10);
        const field = e.target.dataset.ctrlField;
        if (!config.controls[idx]) return;

        if (field === 'steps') {
          if (e.target.value === 'custom') {
            config.controls[idx].isCustomSteps = true;
            if (typeof config.controls[idx].steps !== 'number' || config.controls[idx].steps < 1) {
              config.controls[idx].steps = 4;
            }
          } else {
            config.controls[idx].isCustomSteps = false;
            config.controls[idx].steps = parseInt(e.target.value, 10) || 1;
          }
          render();
        } else if (field === 'customSteps') {
          const val = parseInt(e.target.value, 10);
          config.controls[idx].steps = Math.max(1, Math.min(64, isNaN(val) ? 4 : val));
          config.controls[idx].isCustomSteps = true;
          render();
        } else if (field === 'minPct' || field === 'maxPct') {
          config.controls[idx][field] = parseFloat(e.target.value) || 0;
          render();
        } else if (field === 'name') {
          config.controls[idx].name = e.target.value;
        } else if (field === 'controlMethod') {
          config.controls[idx].controlMethod = e.target.value;
          render();
        } else if (field === 'paramIndex') {
          const pIdx = parseInt(e.target.value, 10);
          config.controls[idx].paramIndex = pIdx;
          if (targetInfo?.parameters) {
            const found = targetInfo.parameters.find(p => p.index === pIdx);
            if (found && (config.controls[idx].name.startsWith('Param') || !config.controls[idx].name)) {
              config.controls[idx].name = found.name;
            }
          }
          render();
        } else if (field === 'cc') {
          config.controls[idx].cc = parseInt(e.target.value, 10) || 0;
          render();
        } else if (field === 'nrpn') {
          config.controls[idx].nrpn = parseInt(e.target.value, 10) || 0;
          render();
        } else if (field === 'sysex') {
          config.controls[idx].sysex = e.target.value;
        }
      });
    });

    // Live search filter typing for VST parameters (Step 3)
    overlay.querySelectorAll('input[data-action="filter-vst-params"]').forEach(filterInput => {
      filterInput.addEventListener('input', (e) => {
        const idx = parseInt(e.target.dataset.idx, 10);
        const query = e.target.value.toLowerCase().trim();
        if (!config.controls[idx]) return;
        config.controls[idx].paramFilterQuery = e.target.value;

        const parentContainer = filterInput.parentElement;
        const selectEl = parentContainer ? parentContainer.querySelector('select[data-ctrl-field="paramIndex"]') : null;
        if (selectEl && targetInfo?.parameters) {
          const filtered = query
            ? targetInfo.parameters.filter(p => (`#${p.index} ${p.name || ''} ${p.label || ''}`).toLowerCase().includes(query))
            : targetInfo.parameters;

          selectEl.innerHTML = filtered.map(p => `
            <option value="${p.index}" ${config.controls[idx].paramIndex === p.index ? 'selected' : ''}>
              #${p.index}: ${escapeHtml(p.name)} ${p.label ? '(' + escapeHtml(p.label) + ')' : ''}
            </option>
          `).join('');

          if (filtered.length > 0 && !filtered.some(p => p.index === config.controls[idx].paramIndex)) {
            config.controls[idx].paramIndex = filtered[0].index;
            config.controls[idx].name = filtered[0].name || config.controls[idx].name;
            const rowHeader = filterInput.closest('[style*="border-bottom"]');
            const nameInput = rowHeader ? rowHeader.querySelector('input[data-ctrl-field="name"]') : null;
            if (nameInput) nameInput.value = config.controls[idx].name;
          }
        }
      });
    });

    overlay.querySelectorAll('[data-action]').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const act = e.currentTarget.dataset.action;
        const idx = parseInt(e.currentTarget.dataset.idx, 10);
        if (act === 'toggle-adv') {
          config.controls[idx].isExpanded = !config.controls[idx].isExpanded;
          render();
        } else if (act === 'remove-row') {
          config.controls.splice(idx, 1);
          render();
        } else if (act === 'move-up' && idx > 0) {
          const temp = config.controls[idx];
          config.controls[idx] = config.controls[idx - 1];
          config.controls[idx - 1] = temp;
          render();
        } else if (act === 'move-down' && idx < config.controls.length - 1) {
          const temp = config.controls[idx];
          config.controls[idx] = config.controls[idx + 1];
          config.controls[idx + 1] = temp;
          render();
        }
      });
    });
  }

  // Dismiss on backdrop click
  overlay.addEventListener('click', (e) => {
    if (e.target === overlay) {
      dismiss();
      onCancel();
    }
  });

  window.addEventListener('keydown', onKeyDown);

  // Clean up any existing test config overlay in document
  const existing = document.querySelector('.test-config-modal-overlay');
  if (existing && existing.parentNode) {
    existing.parentNode.removeChild(existing);
  }

  document.body.appendChild(overlay);
  render();

  overlay.classList.add('active');
  requestAnimationFrame(() => {
    overlay.classList.add('active');
  });
}

/**
 * Backward compatibility wrapper
 */
export function renderStage3Params(container, testConfig, onConfigChanged, onContinueToRun, targetInfo = null) {
  openTestConfigModal({
    initialConfig: testConfig,
    targetInfo,
    isNewTest: false,
    onSave: (saved) => {
      if (typeof onConfigChanged === 'function') onConfigChanged(saved, true);
      if (typeof onContinueToRun === 'function') onContinueToRun(saved);
    },
    onCancel: () => {
      if (typeof onContinueToRun === 'function') onContinueToRun(testConfig);
    }
  });
}
