/**
 * Stage 2: Audio Interface Calibration (Noise Baseline 2A & Loopback 2B)
 * Metrological round-trip latency measurement, SNR validation, and gain calibration.
 * Clean, professional SoundID-grade architecture with zero emojis.
 * Features two dedicated, independent modal wizards for Step 2A and Step 2B.
 */

export function getThemeAudioInterfaceImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'interfaces/auidomididev_white.webp' : 'interfaces/auidomididev_black.webp';
}

export function getThemeNoDeviceImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'interfaces/nodevice2_white.webp' : 'interfaces/nodevice2_black.webp';
}

function isGenericInterface(devName) {
  const lower = (devName || '').toLowerCase();
  return !(lower.includes('realtek') || lower.includes('presonus') || lower.includes('audiobox') || lower.includes('studio one') || lower.includes('mx-1') || lower.includes('aira') || lower.includes('roland'));
}

function isNoDeviceInterface(devName) {
  const lower = (devName || '').toLowerCase();
  return lower.includes('no device') || lower.includes('disabled') || lower.includes('none') || lower.includes('sin dispositivo') || !devName;
}

export function getAudioInterfaceImage(devName) {
  const lower = (devName || '').toLowerCase();
  if (isNoDeviceInterface(devName)) return getThemeNoDeviceImage();
  if (lower.includes('realtek')) return 'interfaces/realtek-audio.png';
  if (lower.includes('presonus') || lower.includes('audiobox')) return 'interfaces/presonus-audiobox-usb.png';
  if (lower.includes('studio one')) return 'interfaces/presonus-studio-one-usb.png';
  if (lower.includes('mx-1') || lower.includes('aira') || lower.includes('roland')) return 'interfaces/roland-mx-1.png';
  if (
    lower.includes('focusrite') || lower.includes('scarlett') ||
    lower.includes('audient') || lower.includes('motu') ||
    lower.includes('behringer') || lower.includes('volt') ||
    lower.includes('komplete') || lower.includes('steinberg') ||
    lower.includes('yamaha') || lower.includes('m-audio') ||
    lower.includes('asio') || lower.includes('usb') ||
    lower.includes('wasapi') || lower.includes('directsound') ||
    lower.includes('audio') || lower.includes('sound')
  ) {
    return 'interfaces/generic-audio-midi-interface.png';
  }
  return getThemeAudioInterfaceImage();
}

export function getNoiseFloorImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'calibration/noisefloor_white.jpg' : 'calibration/noisefloor_black.jpg';
}

export function getLoopbackImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'calibration/loopback_white.jpg' : 'calibration/loopback_black.jpg';
}

export function getMetrologyImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'calibration/metrology_white.webp' : 'calibration/metrology_black.webp';
}

function getInterfaceThemeClass(devName) {
  if (isGenericInterface(devName)) return 'theme-img-interface';
  return '';
}

export function renderStage2(container, state, onUpdate) {
  const metrics = state.calibrationMetrics || null;
  const isAudioCompleted = Boolean(
    (state.snapshot && state.snapshot.calibration && state.snapshot.calibration.audioCompleted) ||
    metrics
  );

  const sampleRate = (state.audioDevice && state.audioDevice.sampleRate) ? Math.round(state.audioDevice.sampleRate / 1000) : 48;
  const audioDevName = (state.audioDevice && state.audioDevice.deviceName) ? state.audioDevice.deviceName : 'Speakers (Realtek(R) Audio)';
  const bufferSize = (state.audioDevice && state.audioDevice.bufferSize) ? state.audioDevice.bufferSize : 256;

  // Determine Target Profile Nature strictly: selectedTarget takes precedence over any background plugin
  const target = state.selectedTarget || (state.activePlugin ? {
    name: state.activePlugin.name || 'VST3 Plugin',
    category: 'Virtual Audio Plugin',
    deviceType: 'VIRTUAL_PLUGIN',
    isPlugin: true
  } : null);

  const isVirtual = Boolean(
    target && (target.isPlugin || target.deviceType === 'VIRTUAL_PLUGIN' || (target.category && target.category.toLowerCase().includes('plugin')))
  );

  const isSynth = Boolean(
    target && !isVirtual && (target.midi || (target.category && (target.category.toLowerCase().includes('synth') || target.category.toLowerCase().includes('sintetizador'))))
  );

  let step2APassed = isAudioCompleted;
  let step2BPassed = isAudioCompleted;
  let currentNoiseFloor = metrics ? metrics.noiseFloorDbfs : -51.8;

  const rtlSamplesText = metrics ? `${metrics.latencySamples}` : '--';
  const rtlMsText = metrics ? `${metrics.latencyMs.toFixed(2)} ms @ ${sampleRate} kHz` : `-- ms @ ${sampleRate} kHz`;
  const snrText = metrics ? `${metrics.snrDb.toFixed(1)}` : '--';
  const noiseFloorText = metrics ? `${metrics.noiseFloorDbfs.toFixed(1)}` : '--';
  const trimText = metrics ? `${metrics.gainTrimDb.toFixed(2)}` : '--';

  let lcdLine1Text = isAudioCompleted
    ? 'CALIBRATION STATUS: VERIFIED & ACTIVE'
    : 'CALIBRATION STATUS: PENDING HARDWARE MEASUREMENT';

  let lcdLine2Text = isAudioCompleted && metrics
    ? `Hardware loopback aligned (${metrics.latencySamples} spls, ${metrics.latencyMs.toFixed(2)} ms) • SNR: ${metrics.snrDb.toFixed(1)} dB`
    : 'Step 2A: Check input noise baseline. Step 2B: Connect loopback cable and run sweep.';

  if (isVirtual) {
    lcdLine1Text = 'CALIBRATION STATUS: ZERO PHYSICAL LATENCY (VST3)';
    lcdLine2Text = 'Direct memory block call • Analogue converters & loopback calibration bypassed.';
  } else if (isSynth) {
    lcdLine1Text = isAudioCompleted ? 'CALIBRATION STATUS: VERIFIED' : 'CALIBRATION STATUS: SYNTH LINE INPUT MODE';
    lcdLine2Text = 'Check baseline input noise (2A). Physical audio loopback is optional for hardware synthesizers.';
  }

  const isThemeDev = isGenericInterface(audioDevName) || isNoDeviceInterface(audioDevName);
  const devThemeClass = getInterfaceThemeClass(audioDevName);

  container.innerHTML = `
    <div class="stage-header" style="display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:14px;">
      <div>
        <h2>Step 2: Interface Calibration &amp; Noise Verification</h2>
        <p>Noise baseline safety check (2A), loopback sample-latency compensation, and frequency alignment (2B).</p>
      </div>
    </div>

    ${isVirtual ? `
      <!-- Case 1: VST3 Virtual Plugin Bypass Banner -->
      <div class="module" style="background:linear-gradient(135deg, rgba(0, 230, 118, 0.05) 0%, rgba(0, 195, 255, 0.05) 100%); border:1px solid rgba(0, 230, 118, 0.3); padding:20px; border-radius:var(--radius-lg); text-align:center; margin-bottom:16px;">
        <div style="font-size:15px; font-weight:700; color:var(--color-text-main); margin-bottom:6px;">
          Virtual DSP Target Active: ${target ? target.name : 'VST3 Plugin'}
        </div>
        <div style="display:inline-block; font-size:10px; font-weight:800; letter-spacing:0.8px; text-transform:uppercase; background:rgba(0, 230, 118, 0.15); color:var(--color-accent); border:1px solid rgba(0, 230, 118, 0.4); padding:4px 12px; border-radius:9999px; margin-bottom:10px;">
          ANALOGUE CALIBRATION BYPASSED (DIRECT IN-MEMORY C++ PROCESSING)
        </div>
        <p style="font-size:12px; color:var(--color-text-muted); max-width:620px; margin:0 auto; line-height:1.5;">
          When profiling a VST3 digital plugin, audio samples are transferred directly through RAM buffers (Sample-Accurate). No physical DAC/ADC converters, patch cables, or acoustic propagation delays are involved. The system is calibrated by design.
        </p>
      </div>
    ` : ''}

    <!-- Top Row: Step Cards 2A and 2B Side by Side -->
    <div class="stage2-steps-grid">
      
      <!-- Card 2A: Input Noise Floor -->
      <div class="module stage2-step-card">
        <div class="stage2-card-art-wrap">
          <img src="${getNoiseFloorImage()}" class="stage2-card-art-img theme-img-calibration-noise" alt="Noise Floor Artwork" />
          <div class="stage2-card-art-fade"></div>
        </div>
        <div class="stage2-card-content">
          <div>
            <div class="stage2-card-header">
              <div style="display:flex; align-items:center; gap:8px;">
                <span class="badge badge-accent badge-pill">2A</span>
                <strong class="stage2-step-title">Input Noise Baseline</strong>
              </div>
              <span id="badgeStep2A" class="badge ${step2APassed ? 'badge-pass' : 'badge-pending'}">
                ${step2APassed ? 'PASS' : 'PENDING'}
              </span>
            </div>
            <div class="stage2-step-subtitle">Safety Baseline Check</div>
            <p class="stage2-step-desc">
              Disconnect the loopback cable and leave the input idle with no signal. The digital output remains at -inf dBFS. Measures 400 ms of input to verify noise floor is below -45 dBFS and prevent overload.
            </p>
          </div>
          <div class="stage2-card-footer">
            <span id="infoBaseline2A" class="stage2-card-status-info">
              ${step2APassed ? `Measured floor: ${currentNoiseFloor.toFixed(1)} dBFS [PASS]` : 'Status: Measurement pending'}
            </span>
            <button id="btnOpenModal2A" type="button" class="btn btn-primary stage2-action-btn" ${isVirtual ? 'disabled title="Analogue noise check bypassed for virtual VST3"' : ''}>
              ${isVirtual ? 'Noise Baseline Bypassed (VST3)' : 'Check Noise Floor (Step 2A)...'}
            </button>
          </div>
        </div>
      </div>

      <!-- Card 2B: Loopback Sweep Measurement -->
      <div class="module stage2-step-card">
        <div class="stage2-card-art-wrap">
          <img src="${getLoopbackImage()}" class="stage2-card-art-img theme-img-calibration-loop" alt="Loopback Sweep Artwork" />
          <div class="stage2-card-art-fade"></div>
        </div>
        <div class="stage2-card-content">
          <div>
            <div class="stage2-card-header">
              <div style="display:flex; align-items:center; gap:8px;">
                <span class="badge badge-info badge-pill">2B</span>
                <strong class="stage2-step-title">Physical Loopback Sweep</strong>
              </div>
              <span id="badgeStep2B" class="badge ${step2BPassed ? 'badge-pass' : (step2APassed ? 'badge-ready' : 'badge-locked')}">
                ${isVirtual ? 'BYPASSED' : (isSynth ? 'OPTIONAL' : (step2BPassed ? 'PASS' : (step2APassed ? 'READY' : 'REQUIRES 2A')))}
              </span>
            </div>
            <div class="stage2-step-subtitle">Latency Compensation &amp; SNR</div>
            <p class="stage2-step-desc">
              Connect a balanced patch cable directly from Audio Out 1 to Audio In 1 on your audio interface. The Farina sine sweep will measure exact fractional delay and frequency response for compensation.
            </p>
          </div>
          <div class="stage2-card-footer">
            <span id="infoLoopback2B" class="stage2-card-status-info">
              ${step2BPassed && metrics ? `RTL: ${metrics.latencySamples} spls (${metrics.latencyMs.toFixed(2)} ms) • SNR: ${metrics.snrDb.toFixed(1)} dB` : (step2APassed ? 'Status: Ready to connect patch cable' : 'Locked: Requires noise baseline 2A')}
            </span>
            <button id="btnOpenModal2B" type="button" class="btn btn-primary stage2-action-btn" ${(!step2APassed && !isSynth) || isVirtual ? 'disabled' : ''}>
              ${isVirtual ? 'Loopback Bypassed (VST3)' : 'Run Loopback Sweep (Step 2B)...'}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- Bottom Row: Unified Interface Metrology Module (Full Width) -->
    <div class="module stage2-merged-metrics-module">
      
      <!-- Metrology Artwork (Cube Signal Decomposition) -->
      <div class="stage2-metrics-art-wrap">
        <img src="${getMetrologyImage()}" class="stage2-metrics-art-img theme-img-calibration-metrology" alt="Interface Metrology Artwork" />
        <div class="stage2-metrics-art-fade"></div>
      </div>

      <div class="stage2-metrics-content">
        <!-- Section Title -->
        <div class="stage2-metrics-subheading">
          INTERFACE METROLOGY METRICS
        </div>

        <!-- 4 Metrology Metric Tiles across full width -->
        <div class="metrics-grid stage2-metrics-4col">
          <div class="metric-tile">
            <span class="metric-label">Round-Trip Latency</span>
            <div class="metric-value ${metrics ? 'good' : ''}" id="valLatencySamples">${rtlSamplesText}<span class="metric-unit">spls</span></div>
            <span id="valLatencyMs" style="font-size:10px; color:var(--color-text-dim);">${rtlMsText}</span>
          </div>

          <div class="metric-tile">
            <span class="metric-label">Signal-to-Noise (SNR)</span>
            <div class="metric-value ${metrics ? 'good' : ''}" id="valSnr">${snrText}<span class="metric-unit">dB</span></div>
            <span style="font-size:10px; color:var(--color-text-dim);">&gt; 90 dB tolerance</span>
          </div>

          <div class="metric-tile">
            <span class="metric-label">Thermal Noise Floor (2A)</span>
            <div class="metric-value ${metrics ? 'good' : ''}" id="valNoiseFloor">${noiseFloorText}<span class="metric-unit">dBFS</span></div>
            <span style="font-size:10px; color:var(--color-text-dim);">RMS baseline</span>
          </div>

          <div class="metric-tile">
            <span class="metric-label">Calibrated Return Level</span>
            <div class="metric-value ${metrics ? 'good' : ''}" id="valGainTrim">${trimText}<span class="metric-unit">dBFS</span></div>
            <span style="font-size:10px; color:var(--color-text-dim);">Auto-trim compensation</span>
          </div>
        </div>

        <!-- Metrology Status Console & Profile Actions -->
        <div class="stage2-merged-footer">
          <div class="stage2-status-console">
            <div class="stage2-console-header">
              <span class="stage2-console-dot"></span>
              <span class="stage2-console-title" id="lcdStatusLine1">${lcdLine1Text}</span>
            </div>
            <div class="stage2-console-desc" id="lcdStatusLine2">${lcdLine2Text}</div>
          </div>

          <div class="stage2-merged-actions">
            <button id="btnSaveCalProfile" type="button" class="btn btn-sm btn-outline stage2-action-profile-btn" ${!isAudioCompleted ? 'disabled' : ''}>
              Save Calibrated Profile
            </button>
            <button id="btnBypassCalibration" type="button" class="btn btn-sm btn-outline stage2-action-bypass-btn">
              Bypass Calibration
            </button>
          </div>
        </div>
      </div>

    </div>

    <!-- ===================================================================== -->
    <!-- DEDICATED MODAL 2A: SUELO DE RUIDO DE ENTRADA (WATERMARKED BACKGROUND) -->
    <!-- ===================================================================== -->
    <div id="modalOverlay2A" style="display:none; position:fixed; top:0; left:0; right:0; bottom:0; background:rgba(0, 0, 0, 0.75); backdrop-filter:blur(6px); z-index:99999; align-items:center; justify-content:center; padding:20px;">
      <div class="modal-dialog-watermarked" style="background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-lg, 12px); width:100%; max-width:820px; box-shadow:0 24px 60px rgba(0,0,0,0.7); display:flex; flex-direction:column; overflow:hidden; position:relative;">
        
        <!-- Watermark Art (Left, subtle, transparent) -->
        <div class="modal-watermark-art">
          <img src="${getNoiseFloorImage()}" class="modal-watermark-img theme-img-calibration-noise" alt="Watermark" />
          <div class="modal-watermark-fade"></div>
        </div>

        <!-- Modal 2A Header -->
        <div style="position:relative; z-index:2; display:flex; justify-content:space-between; align-items:center; padding:16px 20px; border-bottom:1px solid var(--color-panel-border); background:rgba(0,0,0,0.2);">
          <div style="display:flex; align-items:center; gap:12px;">
            <img src="${getAudioInterfaceImage(audioDevName)}" alt="Device" class="${devThemeClass}" style="width:36px; height:36px; object-fit:contain; border:none; background:transparent; padding:0;" onerror="this.src='interfaces/auidomididev_black.webp'" />
            <div>
              <div style="font-size:14px; font-weight:700; color:var(--color-text-main);">Step 2A: Input Noise Floor (Safety Baseline)</div>
              <div style="font-size:10.5px; color:var(--color-text-muted);">${audioDevName} • ${sampleRate}000 Hz • Buffer ${bufferSize} spls</div>
            </div>
          </div>
          <button id="btnModalClose2A" type="button" class="btn btn-outline btn-circle btn-sm" title="Close dialog">
            ✕
          </button>
        </div>

        <!-- Modal 2A Body: Two Columns (Instructions Checklist & Live Meter / Report) -->
        <div style="position:relative; z-index:2; display:grid; grid-template-columns:1.2fr 0.8fr; gap:18px; padding:20px;">
          
          <!-- Left Column: 2A Checklist -->
          <div style="display:flex; flex-direction:column; gap:12px;">
            <div style="font-size:10.5px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">
              PHYSICAL PREPARATION INSTRUCTIONS (2A)
            </div>

            <div style="display:flex; flex-direction:column; gap:12px; font-size:12px;">
              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-accent); color:var(--color-accent); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">1</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Disconnect Loopback Cable</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Disconnect loopback cable. Unplug instruments and microphones from test inputs.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-accent); color:var(--color-accent); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">2</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Digital Output Muted</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Digital output remains muted at 0.0f (-inf dBFS). Guarantees zero signal injection.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-accent); color:var(--color-accent); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">3</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Set Input Gain</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Adjust preamp gain to nominal operating level. Do not modify after calibration.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-accent); color:var(--color-accent); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">4</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Noise Baseline Check (400 ms)</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Measures 400 ms of input to verify noise floor is below -45 dBFS and rule out feedback.</span>
                </div>
              </div>
            </div>
          </div>

          <!-- Right Column: Live Meter & 2A Report -->
          <div style="display:flex; flex-direction:column; gap:14px;">
            
            <div style="background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:14px; display:flex; flex-direction:column; gap:10px;">
              <div style="display:flex; justify-content:space-between; align-items:center;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">REAL-TIME SIGNAL MONITOR</span>
                <span id="signalBadge2A" style="font-size:10px; font-weight:700; color:var(--color-accent);">[Idle / Silent]</span>
              </div>

              <div style="background:rgba(0,0,0,0.4); border:1px solid var(--color-panel-border-dim); border-radius:4px; height:12px; position:relative; overflow:hidden; padding:2px;">
                <div id="signalBar2A" style="width:0%; height:100%; border-radius:2px; background:linear-gradient(90deg, var(--color-accent) 0%, var(--color-warning) 80%, var(--color-danger) 100%); transition:width 0.08s ease-out;"></div>
              </div>

              <div style="display:flex; justify-content:space-between; font-size:11px;">
                <span style="color:var(--color-text-muted);">Input Level:</span>
                <strong id="signalLevelText2A" style="font-family:var(--font-mono, monospace); color:var(--color-text-main);">-inf dBFS</strong>
              </div>
            </div>

            <div style="background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:14px; display:flex; flex-direction:column; gap:8px; flex:1;">
              <div style="display:flex; justify-content:space-between; align-items:center;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">2A NOISE BASELINE REPORT</span>
                <span id="reportStatus2A" style="font-size:10px; font-weight:800; color:var(--color-warning);">${step2APassed ? '[ PASS ]' : '[ PENDING ]'}</span>
              </div>
              <div id="reportDetails2A" style="font-size:11.5px; color:var(--color-text-muted); line-height:1.45; background:rgba(0,0,0,0.2); border:1px solid var(--color-panel-border-dim); border-radius:4px; padding:10px; flex:1;">
                ${step2APassed
                  ? `Input noise baseline verified (${currentNoiseFloor.toFixed(1)} dBFS). Idle level is nominal and safe.`
                  : `Noise floor has not been verified yet. Click [Measure Noise Floor (400 ms)].`}
              </div>
            </div>

          </div>

        </div>

        <!-- Modal 2A Footer -->
        <div style="position:relative; z-index:2; display:flex; justify-content:space-between; align-items:center; padding:14px 20px; border-top:1px solid var(--color-panel-border); background:rgba(0,0,0,0.2);">
          <button id="btnModalBypass2A" type="button" class="btn btn-sm btn-outline">
            Bypass Calibration
          </button>
          
          <div style="display:flex; gap:10px; align-items:center;">
            <button id="btnModalCancel2A" type="button" class="btn btn-sm btn-outline">
              Cancel
            </button>
            <button id="btnModalAction2A" type="button" class="btn btn-primary btn-lg">
              ${step2APassed ? 'Accept & Close' : 'Measure Noise Floor (400 ms)'}
            </button>
          </div>
        </div>

      </div>
    </div>

    <!-- ===================================================================== -->
    <!-- DEDICATED MODAL 2B: MEDICION FISICA DE LOOPBACK (WATERMARKED)         -->
    <!-- ===================================================================== -->
    <div id="modalOverlay2B" style="display:none; position:fixed; top:0; left:0; right:0; bottom:0; background:rgba(0, 0, 0, 0.75); backdrop-filter:blur(6px); z-index:99999; align-items:center; justify-content:center; padding:20px;">
      <div class="modal-dialog-watermarked" style="background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-lg, 12px); width:100%; max-width:820px; box-shadow:0 24px 60px rgba(0,0,0,0.7); display:flex; flex-direction:column; overflow:hidden; position:relative;">
        
        <!-- Watermark Art (Left, subtle, transparent) -->
        <div class="modal-watermark-art">
          <img src="${getLoopbackImage()}" class="modal-watermark-img theme-img-calibration-loop" alt="Watermark" />
          <div class="modal-watermark-fade"></div>
        </div>

        <!-- Modal 2B Header -->
        <div style="position:relative; z-index:2; display:flex; justify-content:space-between; align-items:center; padding:16px 20px; border-bottom:1px solid var(--color-panel-border); background:rgba(0,0,0,0.2);">
          <div style="display:flex; align-items:center; gap:12px;">
            <img src="${getAudioInterfaceImage(audioDevName)}" alt="Device" class="${devThemeClass}" style="width:36px; height:36px; object-fit:contain; border:none; background:transparent; padding:0;" onerror="this.src='interfaces/auidomididev_black.webp'" />
            <div>
              <div style="font-size:14px; font-weight:700; color:var(--color-text-main);">Step 2B: Physical Loopback Measurement (Latency &amp; SNR)</div>
              <div style="font-size:10.5px; color:var(--color-text-muted);">${audioDevName} • ${sampleRate}000 Hz • Buffer ${bufferSize} spls</div>
            </div>
          </div>
          <button id="btnModalClose2B" type="button" class="btn btn-outline btn-circle btn-sm" title="Close dialog">
            ✕
          </button>
        </div>

        <!-- Modal 2B Body: Two Columns (Instructions Checklist & Live Meter / Report) -->
        <div style="position:relative; z-index:2; display:grid; grid-template-columns:1.2fr 0.8fr; gap:18px; padding:20px;">
          
          <!-- Left Column: 2B Checklist -->
          <div style="display:flex; flex-direction:column; gap:12px;">
            <div style="font-size:10.5px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">
              PHYSICAL LOOPBACK INSTRUCTIONS (2B)
            </div>

            <div style="display:flex; flex-direction:column; gap:12px; font-size:12px;">
              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-info); color:var(--color-info); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">1</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Connect Patch Cable: Main Out -> Input</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Connect a direct balanced patch cable from Audio Out 1 to Audio In 1 on your audio interface.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-info); color:var(--color-info); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">2</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Disable Direct Monitoring</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Turn off Direct Monitor or set Mix control entirely to Playback to avoid feedback loops.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-info); color:var(--color-info); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">3</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Safety Preflight (200 ms)</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">Before emitting the sweep, 200 ms of muted output verifies connection stability.</span>
                </div>
              </div>

              <div style="display:flex; gap:12px; align-items:flex-start;">
                <span style="width:22px; height:22px; border-radius:50%; background:var(--color-panel-surface); border:1px solid var(--color-info); color:var(--color-info); font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; flex-shrink:0;">4</span>
                <div>
                  <strong style="color:var(--color-text-main); display:block;">Contractual Farina Sweep (1300 ms)</strong>
                  <span style="color:var(--color-text-muted); font-size:11px;">1300 ms capture (1000 ms Farina sweep + 200 ms latency margin) measures delay and frequency response.</span>
                </div>
              </div>
            </div>
          </div>

          <!-- Right Column: Live Meter & 2B Report -->
          <div style="display:flex; flex-direction:column; gap:14px;">
            
            <div style="background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:14px; display:flex; flex-direction:column; gap:10px;">
              <div style="display:flex; justify-content:space-between; align-items:center;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">REAL-TIME SIGNAL MONITOR</span>
                <span id="signalBadge2B" style="font-size:10px; font-weight:700; color:var(--color-accent);">[Cable Connected]</span>
              </div>

              <div style="background:rgba(0,0,0,0.4); border:1px solid var(--color-panel-border-dim); border-radius:4px; height:12px; position:relative; overflow:hidden; padding:2px;">
                <div id="signalBar2B" style="width:0%; height:100%; border-radius:2px; background:linear-gradient(90deg, var(--color-accent) 0%, var(--color-warning) 80%, var(--color-danger) 100%); transition:width 0.08s ease-out;"></div>
              </div>

              <div style="display:flex; justify-content:space-between; font-size:11px;">
                <span style="color:var(--color-text-muted);">Input Level:</span>
                <strong id="signalLevelText2B" style="font-family:var(--font-mono, monospace); color:var(--color-text-main);">-inf dBFS</strong>
              </div>
            </div>

            <div style="background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:14px; display:flex; flex-direction:column; gap:8px; flex:1;">
              <div style="display:flex; justify-content:space-between; align-items:center;">
                <span style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">2B PHYSICAL LOOPBACK REPORT</span>
                <span id="reportStatus2B" style="font-size:10px; font-weight:800; color:${step2BPassed ? 'var(--color-accent)' : 'var(--color-info)'};">${step2BPassed ? '[ PASS ]' : (step2APassed ? '[ READY ]' : '[ LOCKED ]')}</span>
              </div>
              <div id="reportDetails2B" style="font-size:11.5px; color:var(--color-text-muted); line-height:1.45; background:rgba(0,0,0,0.2); border:1px solid var(--color-panel-border-dim); border-radius:4px; padding:10px; flex:1;">
                ${step2BPassed && metrics
                  ? `Farina sweep verified successfully.<br/>• Latency: ${metrics.latencySamples} spls (${metrics.latencyMs.toFixed(2)} ms)<br/>• SNR: ${metrics.snrDb.toFixed(1)} dB • Trim: ${metrics.gainTrimDb.toFixed(2)} dBFS`
                  : `Step 2A verified (${currentNoiseFloor.toFixed(1)} dBFS). Connect patch cable from Audio Out 1 to Audio In 1 and click [Run Farina Sweep (1300 ms)].`}
              </div>
            </div>

          </div>

        </div>

        <!-- Modal 2B Footer -->
        <div style="position:relative; z-index:2; display:flex; justify-content:space-between; align-items:center; padding:14px 20px; border-top:1px solid var(--color-panel-border); background:rgba(0,0,0,0.2);">
          <button id="btnModalBypass2B" type="button" class="btn btn-sm btn-outline">
            Bypass Calibration
          </button>
          
          <div style="display:flex; gap:10px; align-items:center;">
            <button id="btnModalCancel2B" type="button" class="btn btn-sm btn-outline">
              Cancel
            </button>
            <button id="btnModalAction2B" type="button" class="btn btn-primary btn-lg">
              ${step2BPassed ? 'Apply Calibration & Close' : 'Run Farina Sweep (1300 ms)'}
            </button>
          </div>
        </div>

      </div>
    </div>
  `;

  // Main Page DOM References
  const btnOpenAudioSettings = container.querySelector('#btnOpenAudioSettings');
  const btnOpenModal2A = container.querySelector('#btnOpenModal2A');
  const btnOpenModal2B = container.querySelector('#btnOpenModal2B');
  const btnSaveCalProfile = container.querySelector('#btnSaveCalProfile');
  const btnBypassCalibration = container.querySelector('#btnBypassCalibration');

  const badgeStep2A = container.querySelector('#badgeStep2A');
  const badgeStep2B = container.querySelector('#badgeStep2B');
  const infoBaseline2A = container.querySelector('#infoBaseline2A');
  const infoLoopback2B = container.querySelector('#infoLoopback2B');

  const valLatencySamples = container.querySelector('#valLatencySamples');
  const valLatencyMs = container.querySelector('#valLatencyMs');
  const valSnr = container.querySelector('#valSnr');
  const valNoiseFloor = container.querySelector('#valNoiseFloor');
  const valGainTrim = container.querySelector('#valGainTrim');
  const lcdStatusLine1 = container.querySelector('#lcdStatusLine1');
  const lcdStatusLine2 = container.querySelector('#lcdStatusLine2');

  // Modal 2A DOM References
  const modalOverlay2A = container.querySelector('#modalOverlay2A');
  const btnModalClose2A = container.querySelector('#btnModalClose2A');
  const btnModalCancel2A = container.querySelector('#btnModalCancel2A');
  const btnModalBypass2A = container.querySelector('#btnModalBypass2A');
  const btnModalAction2A = container.querySelector('#btnModalAction2A');
  const signalBar2A = container.querySelector('#signalBar2A');
  const signalBadge2A = container.querySelector('#signalBadge2A');
  const signalLevelText2A = container.querySelector('#signalLevelText2A');
  const reportStatus2A = container.querySelector('#reportStatus2A');
  const reportDetails2A = container.querySelector('#reportDetails2A');

  // Modal 2B DOM References
  const modalOverlay2B = container.querySelector('#modalOverlay2B');
  const btnModalClose2B = container.querySelector('#btnModalClose2B');
  const btnModalCancel2B = container.querySelector('#btnModalCancel2B');
  const btnModalBypass2B = container.querySelector('#btnModalBypass2B');
  const btnModalAction2B = container.querySelector('#btnModalAction2B');
  const signalBar2B = container.querySelector('#signalBar2B');
  const signalBadge2B = container.querySelector('#signalBadge2B');
  const signalLevelText2B = container.querySelector('#signalLevelText2B');
  const reportStatus2B = container.querySelector('#reportStatus2B');
  const reportDetails2B = container.querySelector('#reportDetails2B');

  // Open & Close Handlers for Modal 2A
  function openModal2A() {
    modalOverlay2A.style.display = 'flex';
  }
  function closeModal2A() {
    modalOverlay2A.style.display = 'none';
  }

  // Open & Close Handlers for Modal 2B
  function openModal2B() {
    modalOverlay2B.style.display = 'flex';
  }
  function closeModal2B() {
    modalOverlay2B.style.display = 'none';
  }

  // Button Listeners to open dedicated modals
  if (btnOpenModal2A) btnOpenModal2A.addEventListener('click', openModal2A);
  if (btnOpenModal2B) {
    btnOpenModal2B.addEventListener('click', () => {
      if (!step2APassed && !isSynth) return;
      openModal2B();
    });
  }

  if (btnModalClose2A) btnModalClose2A.addEventListener('click', closeModal2A);
  if (btnModalCancel2A) btnModalCancel2A.addEventListener('click', closeModal2A);

  if (btnModalClose2B) btnModalClose2B.addEventListener('click', closeModal2B);
  if (btnModalCancel2B) btnModalCancel2B.addEventListener('click', closeModal2B);

  // Modal 2A Action: Measure Noise Baseline
  if (btnModalAction2A) {
    btnModalAction2A.addEventListener('click', () => {
      if (step2APassed) {
        closeModal2A();
        return;
      }

      btnModalAction2A.disabled = true;
      btnModalAction2A.textContent = 'Measuring Noise Baseline (400 ms)...';
      reportStatus2A.textContent = '[ MEASURING... ]';
      reportStatus2A.style.color = 'var(--color-info)';
      reportDetails2A.textContent = 'Testing 400 ms with output completely muted to verify baseline silence and absence of stray loops...';

      if (window.__JUCE__ && window.__JUCE__.backend) {
        window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'startNoiseBaselineCheck' });
      } else {
        // Mock mode fallback
        setTimeout(() => {
          updateStep2AStatus(true, -51.8);
          btnModalAction2A.disabled = false;
          btnModalAction2A.textContent = 'Accept & Close';
        }, 600);
      }
    });
  }

  // Modal 2B Action: Launch Loopback Farina Sweep
  if (btnModalAction2B) {
    btnModalAction2B.addEventListener('click', () => {
      if (step2BPassed) {
        // Already completed, save profile and close modal
        if (window.__JUCE__ && window.__JUCE__.backend) {
          window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'saveCalibrationProfile' });
        }
        closeModal2B();
        return;
      }

      btnModalAction2B.disabled = true;
      btnModalAction2B.textContent = 'Emitting Farina Sweep (1300 ms)...';
      reportStatus2B.textContent = '[ EMITTING SWEEP... ]';
      reportStatus2B.style.color = 'var(--color-info)';
      reportDetails2B.textContent = 'Emitting logarithmic sinusoidal sweep to measure round-trip delay, SNR, and frequency response...';

      if (window.__JUCE__ && window.__JUCE__.backend) {
        window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'startPhysicalLoopbackSweep' });
      } else {
        // Mock mode fallback
        setTimeout(() => {
          applyCompletedCalibrationMetrics({
            latencySamples: 72,
            latencyMs: 1.50,
            snrDb: 94.2,
            noiseFloorDbfs: -51.8,
            gainTrimDb: 0.05,
            flatnessDb: 0.12
          });
          btnModalAction2B.disabled = false;
          btnModalAction2B.textContent = 'Apply Calibration & Close';
        }, 1400);
      }
    });
  }

  function updateStep2AStatus(passed, noiseDbfs) {
    step2APassed = passed;
    currentNoiseFloor = noiseDbfs;

    badgeStep2A.textContent = passed ? 'PASS' : 'FAILED';
    badgeStep2A.style.background = passed ? 'rgba(0, 230, 118, 0.15)' : 'rgba(255, 82, 82, 0.15)';
    badgeStep2A.style.color = passed ? 'var(--color-accent)' : 'var(--color-danger)';

    infoBaseline2A.textContent = passed ? `Measured floor: ${noiseDbfs.toFixed(1)} dBFS [PASS]` : 'Baseline measurement failed';

    reportStatus2A.textContent = passed ? '[ PASS ]' : '[ FAILED ]';
    reportStatus2A.style.color = passed ? 'var(--color-accent)' : 'var(--color-danger)';
    reportDetails2A.textContent = passed
      ? `Input noise baseline verified (${noiseDbfs.toFixed(1)} dBFS). Idle level is nominal and safe. You can now proceed to Step 2B.`
      : `Noise floor exceeds the -45 dBFS safety threshold. Ensure no cables or instruments are plugged into the input.`;

    if (btnModalAction2A) {
      btnModalAction2A.textContent = 'Accept & Close';
    }

    if (!step2BPassed) {
      badgeStep2B.textContent = 'READY';
      badgeStep2B.style.background = 'var(--color-info-dim, rgba(0, 195, 255, 0.15))';
      badgeStep2B.style.color = 'var(--color-info)';
      infoLoopback2B.textContent = 'Status: Ready to connect patch cable';
      btnOpenModal2B.disabled = false;

      reportStatus2B.textContent = '[ READY ]';
      reportStatus2B.style.color = 'var(--color-info)';
      reportDetails2B.textContent = `Step 2A verified (${noiseDbfs.toFixed(1)} dBFS). Connect patch cable from Audio Out 1 to Audio In 1 and click [Run Farina Sweep (1300 ms)].`;
    }

    if (valNoiseFloor) {
      valNoiseFloor.className = 'metric-value good';
      valNoiseFloor.innerHTML = `${noiseDbfs.toFixed(1)}<span class="metric-unit">dBFS</span>`;
    }

    lcdStatusLine1.textContent = 'CALIBRATION STATUS: STEP 2A VERIFIED';
    lcdStatusLine2.textContent = `Baseline noise floor: ${noiseDbfs.toFixed(1)} dBFS. Proceed to Step 2B with loopback cable.`;
  }

  function applyCompletedCalibrationMetrics(m) {
    step2APassed = true;
    step2BPassed = true;

    badgeStep2A.textContent = 'PASS';
    badgeStep2A.style.background = 'rgba(0, 230, 118, 0.15)';
    badgeStep2A.style.color = 'var(--color-accent)';

    badgeStep2B.textContent = 'PASS';
    badgeStep2B.style.background = 'rgba(0, 230, 118, 0.15)';
    badgeStep2B.style.color = 'var(--color-accent)';

    infoLoopback2B.textContent = `RTL: ${m.latencySamples} spls (${m.latencyMs.toFixed(2)} ms) • SNR: ${m.snrDb.toFixed(1)} dB`;

    reportStatus2B.textContent = '[ PASS ]';
    reportStatus2B.style.color = 'var(--color-accent)';
    reportDetails2B.innerHTML = `
      Farina sweep verified successfully.<br/>
      • Round-Trip Latency: ${m.latencySamples} spls (${m.latencyMs.toFixed(2)} ms)<br/>
      • Signal-to-Noise Ratio (SNR): ${m.snrDb.toFixed(1)} dB (Tolerance: &gt; 90 dB OK)<br/>
      • Calibrated Gain Trim: ${m.gainTrimDb.toFixed(2)} dBFS
    `;

    if (btnModalAction2B) {
      btnModalAction2B.textContent = 'Apply Calibration & Close';
    }

    if (valLatencySamples) {
      valLatencySamples.className = 'metric-value good';
      valLatencySamples.innerHTML = `${m.latencySamples}<span class="metric-unit">spls</span>`;
    }
    if (valLatencyMs) {
      valLatencyMs.textContent = `${m.latencyMs.toFixed(2)} ms @ ${sampleRate} kHz`;
    }
    if (valSnr) {
      valSnr.className = 'metric-value good';
      valSnr.innerHTML = `${m.snrDb.toFixed(1)}<span class="metric-unit">dB</span>`;
    }
    if (valNoiseFloor) {
      valNoiseFloor.className = 'metric-value good';
      valNoiseFloor.innerHTML = `${m.noiseFloorDbfs.toFixed(1)}<span class="metric-unit">dBFS</span>`;
    }
    if (valGainTrim) {
      valGainTrim.className = 'metric-value good';
      valGainTrim.innerHTML = `${m.gainTrimDb.toFixed(2)}<span class="metric-unit">dBFS</span>`;
    }

    lcdStatusLine1.textContent = 'CALIBRATION STATUS: VERIFIED & ACTIVE';
    lcdStatusLine2.textContent = `Hardware loopback aligned (${m.latencySamples} spls, ${m.latencyMs.toFixed(2)} ms) • SNR: ${m.snrDb.toFixed(1)} dB`;

    if (btnSaveCalProfile) btnSaveCalProfile.disabled = false;
  }

  // Bypass Handler
  function handleBypass() {
    if (window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'skipCalibration' });
    }
    step2APassed = true;
    step2BPassed = true;
    badgeStep2A.textContent = 'BYPASSED';
    badgeStep2B.textContent = 'BYPASSED';
    infoBaseline2A.textContent = 'Noise Baseline: Calibration bypassed';
    infoLoopback2B.textContent = 'Loopback: Calibration bypassed';

    lcdStatusLine1.textContent = 'CALIBRATION STATUS: BYPASSED';
    lcdStatusLine2.textContent = 'Calibration bypassed by user. Nominal unity gain and 0 samples latency.';

    closeModal2A();
    closeModal2B();
  }

  if (btnBypassCalibration) btnBypassCalibration.addEventListener('click', handleBypass);
  if (btnModalBypass2A) btnModalBypass2A.addEventListener('click', handleBypass);
  if (btnModalBypass2B) btnModalBypass2B.addEventListener('click', handleBypass);

  // Settings
  if (btnOpenAudioSettings) {
    btnOpenAudioSettings.addEventListener('click', () => {
      if (typeof state.openAudioSettings === 'function') state.openAudioSettings();
    });
  }

  if (btnSaveCalProfile) {
    btnSaveCalProfile.addEventListener('click', () => {
      if (window.__JUCE__ && window.__JUCE__.backend) {
        window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'saveCalibrationProfile' });
        btnSaveCalProfile.textContent = 'Profile Saved';
        setTimeout(() => { btnSaveCalProfile.textContent = 'Save Calibrated Profile'; }, 2000);
      }
    });
  }

  // Live VU Meter telemetry frame listener for both modals
  const onTelemetry = (e) => {
    const frame = e.detail;
    if (!frame || !frame.meters) return;
    const meters = frame.meters;
    const inDb = typeof meters.inDbfs === 'number'
      ? meters.inDbfs
      : (Array.isArray(meters) ? meters[0] : (typeof meters[0] === 'number' ? meters[0] : -100));
    if (typeof inDb === 'number' && Number.isFinite(inDb)) {
      const pct = Math.max(0, Math.min(100, ((inDb + 60) / 60) * 100));
      const formattedLevel = inDb <= -59.5 ? '-inf dBFS' : `${inDb >= 0 ? '+' : ''}${inDb.toFixed(1)} dBFS`;

      // Update Modal 2A meter if active
      if (modalOverlay2A && modalOverlay2A.style.display !== 'none') {
        if (signalBar2A) signalBar2A.style.width = `${pct}%`;
        if (signalLevelText2A) signalLevelText2A.textContent = formattedLevel;
        if (signalBadge2A) {
          if (inDb > -45) {
            signalBadge2A.textContent = '[High Idle Level / Warning]';
            signalBadge2A.style.color = 'var(--color-warning)';
          } else {
            signalBadge2A.textContent = '[Idle / Silent]';
            signalBadge2A.style.color = 'var(--color-accent)';
          }
        }
      }

      // Update Modal 2B meter if active
      if (modalOverlay2B && modalOverlay2B.style.display !== 'none') {
        if (signalBar2B) signalBar2B.style.width = `${pct}%`;
        if (signalLevelText2B) signalLevelText2B.textContent = formattedLevel;
        if (signalBadge2B) {
          signalBadge2B.textContent = '[Active Signal]';
          signalBadge2B.style.color = 'var(--color-accent)';
        }
      }
    }
  };
  window.addEventListener('telemetryFrame', onTelemetry);

  // Calibration Event Listeners from C++ Native Calibration Panel
  const onProgress = (e) => {
    const detail = e.detail;
    if (!detail) return;
    if (detail.stage === 'NoiseBaseline_2A') {
      if (reportDetails2A) {
        reportDetails2A.textContent = `Analyzing noise baseline: ${detail.statusText || '400 ms silence'} (${detail.currentInDbfs ? detail.currentInDbfs.toFixed(1) + ' dBFS' : ''})`;
      }
    } else if (detail.stage && detail.stage.includes('2B')) {
      if (reportDetails2B) {
        reportDetails2B.textContent = `Loopback sweep: ${detail.statusText || 'Measuring response'} (${detail.currentInDbfs ? detail.currentInDbfs.toFixed(1) + ' dBFS' : ''})`;
      }
    }
  };

  const onComplete = (e) => {
    const detail = e.detail;
    if (!detail) return;

    if (btnModalAction2A) btnModalAction2A.disabled = false;
    if (btnModalAction2B) btnModalAction2B.disabled = false;

    if (detail.success) {
      const m = detail.metrics;
      applyCompletedCalibrationMetrics(m);
    } else {
      if (modalOverlay2A.style.display !== 'none') {
        reportStatus2A.textContent = '[ FAILED ]';
        reportStatus2A.style.color = 'var(--color-danger)';
        reportDetails2A.textContent = `Verification error: ${detail.errorReason || 'Noise level exceeds threshold. Ensure cable is unplugged.'}`;
      }
      if (modalOverlay2B.style.display !== 'none') {
        reportStatus2B.textContent = '[ FAILED ]';
        reportStatus2B.style.color = 'var(--color-danger)';
        reportDetails2B.textContent = `Sweep error: ${detail.errorReason || 'No signal detected or clipping. Check patch cable from Audio Out 1 to Audio In 1.'}`;
      }
    }
  };

  window.addEventListener('calibrationProgress', onProgress);
  window.addEventListener('calibrationComplete', onComplete);
}
