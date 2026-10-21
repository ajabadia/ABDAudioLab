/**
 * Stage 0: Studio Environment (Audio I/O & MIDI setup)
 * Official JUCE AudioDeviceManager configuration for low-latency audio & MIDI hardware.
 */

export function renderStage0(container, state, onUpdate) {
  const dev = state.audioDevice || {};
  const devName = dev.deviceName || 'Detecting audio interface...';
  const sampleRate = dev.sampleRate ? `${Math.round(dev.sampleRate)} Hz` : '48000 Hz';
  const bufferSize = dev.bufferSize ? `${dev.bufferSize} samples` : '256 samples';
  const activeIn = dev.activeInputs !== undefined ? `${dev.activeInputs} channels` : '2 channels';
  const activeOut = dev.activeOutputs !== undefined ? `${dev.activeOutputs} channels` : '2 channels';
  const midiIns = (dev.availableMidiInputs && dev.availableMidiInputs.length > 0)
    ? dev.availableMidiInputs.join(', ')
    : 'No MIDI inputs detected';
  const midiOuts = (dev.availableMidiOutputs && dev.availableMidiOutputs.length > 0)
    ? dev.availableMidiOutputs.join(', ')
    : 'No MIDI outputs detected';

  container.innerHTML = `
    <div class="stage-header">
      <h2>Step 0: Studio Environment &amp; Connectivity</h2>
      <p>Management of low-latency audio drivers (ASIO/WASAPI), master clock synchronization, and physical MIDI ports.</p>
    </div>

    <div class="panel-row" style="display:grid; grid-template-columns: 1fr 1fr; gap: 16px;">
      <!-- Active Audio Device (JUCE AudioDeviceManager) -->
      <div class="module">
        <div class="module-header">Active Audio Interface (JUCE Engine)</div>
        <div style="display:flex; flex-direction:column; gap: 14px; margin-top: 14px;">
          <div>
            <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">ACTIVE DRIVER / INTERFACE</label>
            <div style="padding:10px 14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-weight:700; color:var(--color-text-main); font-size:13px;">
              ${devName}
            </div>
          </div>

          <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <div>
              <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">SAMPLE RATE</label>
              <div style="padding:8px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-family:var(--font-mono, monospace); font-size:12px; font-weight:700; color:var(--color-accent-active);">
                ${sampleRate}
              </div>
            </div>
            <div>
              <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">BUFFER SIZE</label>
              <div style="padding:8px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-family:var(--font-mono, monospace); font-size:12px; font-weight:700; color:var(--color-accent-active);">
                ${bufferSize}
              </div>
            </div>
          </div>

          <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <div>
              <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">INPUT CHANNELS</label>
              <div style="padding:8px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:600; color:var(--color-text-main);">
                ${activeIn}
              </div>
            </div>
            <div>
              <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">OUTPUT CHANNELS</label>
              <div style="padding:8px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; font-weight:600; color:var(--color-text-main);">
                ${activeOut}
              </div>
            </div>
          </div>

          <div style="margin-top: 8px;">
            <button id="btnOpenAudioSettings" type="button" class="btn btn-primary" style="width:100%; padding: 12px; font-weight: 700; font-size: 13px; display:flex; align-items:center; justify-content:center; gap:8px;">
              Open Audio &amp; MIDI Settings
            </button>
            <p style="font-size: 11px; color: var(--color-text-muted); margin-top: 8px; line-height: 1.4;">
              Opens the native JUCE configuration dialog to switch ASIO/WASAPI drivers, clock rates, buffer sizes, and hardware MIDI ports.
            </p>
          </div>
        </div>
      </div>

      <!-- MIDI Ports & Physical Bus -->
      <div class="module">
        <div class="module-header">MIDI Ports &amp; Physical Automation</div>
        <div style="display:flex; flex-direction:column; gap: 14px; margin-top: 14px;">
          <div>
            <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">AVAILABLE MIDI INPUT PORTS</label>
            <div style="padding:10px 14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; color:var(--color-text-main);">
              ${midiIns}
            </div>
          </div>

          <div>
            <label style="font-size:11px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.5px;">AVAILABLE MIDI OUTPUT PORTS (AUTOMATION / SYSEX)</label>
            <div style="padding:10px 14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); font-size:12px; color:var(--color-text-main);">
              ${midiOuts}
            </div>
          </div>

          <div class="lcd-container" style="padding: 12px 16px; margin-top: 10px;">
            <div class="lcd-line-1" style="font-size: 12px;">DSP BUS STATUS: READY</div>
            <div class="lcd-line-2" style="font-size: 11px;">Zero-allocation real-time audio thread • Precision metrology mode</div>
          </div>
        </div>
      </div>
    </div>
  `;

  const btnOpenSettings = container.querySelector('#btnOpenAudioSettings');
  if (btnOpenSettings) {
    btnOpenSettings.addEventListener('click', () => {
      if (typeof state.openAudioSettings === 'function') {
        state.openAudioSettings();
      }
    });
  }
}
