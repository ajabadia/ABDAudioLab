/**
 * VirtualKeyboardDrawer.js
 * Collapsible bottom virtual MIDI keyboard drawer for ABDAudioLab.
 * Powered by ABDShared official @abdsynths/midi-keyb (single source of truth).
 *
 * Features:
 *   - Responsive keybed (adaptive octave count, touch, mouse, QWERTY, velocity)
 *   - Animated filmstrip Pitch & Mod Wheels (@abdsynths/shared/components/wheel.js)
 *   - Octave shift (-3 to +3), Sustain pedal, and Panic (All Notes Off)
 *   - Zero heap allocation real-time MIDI streaming via C++ IPC (midiNoteOn, midiNoteOff, midiPitchBend, midiController, midiAllNotesOff)
 *   - Theme synchronized with ABDAudioLab dark/light palettes
 */

import { createKeyboard } from '@abdsynths/midi-keyb';
import '@abdsynths/midi-keyb/keyboard.css';
import '@abdsynths/shared/styles/components/keyboard.css';

export class VirtualKeyboardDrawer {
  constructor(options = {}) {
    this.container = options.container || document.body;
    this.isNativeHost = options.isNativeHost !== undefined
      ? options.isNativeHost
      : (typeof window !== 'undefined' && !!(window.__JUCE__ && window.__JUCE__.backend));
    this.midiChannel = options.midiChannel || 1;
    this.isExpanded = false;
    this.keyboardInstance = null;
    this.onToggleCallback = options.onToggle || null;
    this.activeNoteNames = new Map();

    // Restore persistent drawer state
    try {
      this.isExpanded = localStorage.getItem('abdaudiolab_keyboard_expanded') === 'true';
    } catch (_) {}

    this.init();
  }

  init() {
    this.drawerEl = document.createElement('div');
    this.drawerEl.id = 'virtualKeyboardDrawer';
    this.drawerEl.className = `virtual-keyboard-drawer ${this.isExpanded ? 'expanded' : 'collapsed'}`;
    this.container.appendChild(this.drawerEl);

    this.render();
    this.mountSharedKeyboard();
    this.attachDrawerEvents();
  }

  render() {
    this.drawerEl.innerHTML = `
      <!-- Collapsible Top Handle Bar -->
      <div class="kbd-drawer-handle" id="kbdDrawerHandle">
        <div class="kbd-handle-left">
          <button id="kbdExpandToggleBtn" type="button" class="kbd-toggle-chevron-btn" title="Toggle Virtual MIDI Keyboard (Ctrl+K)">
            ${this.isExpanded ? this.getChevronDownSvg() : this.getChevronUpSvg()}
          </button>
          <button id="kbdHeaderTitleBtn" type="button" class="kbd-title-btn">
            <span class="kbd-title-text">Virtual MIDI Keyboard</span>
            <span class="kbd-title-badge">ABDShared</span>
          </button>
          <span id="kbdActiveNotePill" class="kbd-note-pill">IDLE</span>
        </div>

        <div class="kbd-handle-right">
          <!-- Octave Controls -->
          <div class="kbd-control-group">
            <button id="abdKbdOctDown" type="button" class="kbd-mini-btn" title="Octave Down (Shift+Z / Down Arrow)">OCT &minus;</button>
            <span id="abdKbdLedDown" class="kbd-octave-led"></span>
            <span id="kbdOctaveDisplay" class="kbd-octave-pill">OCT 0</span>
            <span id="abdKbdLedUp" class="kbd-octave-led"></span>
            <button id="abdKbdOctUp" type="button" class="kbd-mini-btn" title="Octave Up (Shift+X / Up Arrow)">OCT +</button>
          </div>

          <!-- Sustain Pedal -->
          <button id="abdKbdSustainBtn" type="button" class="kbd-mini-btn" title="Sustain Pedal (Ctrl+Space)">
            SUSTAIN
          </button>

          <!-- Panic Button -->
          <button id="abdKbdPanicBtn" type="button" class="kbd-mini-btn kbd-btn-danger" title="Panic - All Notes Off (Ctrl+Q)">
            PANIC
          </button>
        </div>
      </div>

      <!-- Drawer Content: Wheels + Keys -->
      <div class="kbd-drawer-content" id="kbdDrawerContent">
        <div class="kbd-wheels-wrapper">
          <div id="abdKbdPitchWheel" class="kbd-wheel-container"></div>
          <div id="abdKbdModWheel" class="kbd-wheel-container"></div>
        </div>
        <div id="piano-keyboard" class="kbd-keybed-container"></div>
      </div>
    `;
  }

  mountSharedKeyboard() {
    this.keyboardInstance = createKeyboard({
      containerId: 'piano-keyboard',
      wheelPitchId: 'abdKbdPitchWheel',
      wheelModId: 'abdKbdModWheel',
      octUpId: 'abdKbdOctUp',
      octDownId: 'abdKbdOctDown',
      ledUpId: 'abdKbdLedUp',
      ledDownId: 'abdKbdLedDown',
      panicBtnId: 'abdKbdPanicBtn',
      sustainBtnId: 'abdKbdSustainBtn',
      onNoteOn: (note, velocity) => {
        this.sendMidiNoteOn(note, velocity);
        this.addActiveNoteVisual(note);
      },
      onNoteOff: (note) => {
        this.sendMidiNoteOff(note);
        this.removeActiveNoteVisual(note);
      },
      onPitchBend: (normalized) => {
        this.sendMidiPitchBend(normalized);
      },
      onModWheel: (normalized) => {
        this.sendMidiModWheel(normalized);
      },
      onPanic: () => {
        this.sendMidiPanic();
        this.activeNoteNames.clear();
        this.updateNotePill();
      },
      onOctaveChange: (oct) => {
        const pill = this.drawerEl.querySelector('#kbdOctaveDisplay');
        if (pill) {
          pill.textContent = `OCT ${oct > 0 ? '+' : ''}${oct}`;
        }
      },
      config: {
        numOctaves: 3,
        startNote: 48, // C3
        fixedOctaves: false, // adapt to available width
        velocitySource: 'y-pos',
        velocityCurve: 'normal',
        enableQwerty: true,
        enableTouch: true,
        enablePressureDisplay: true,
        enablePitchBendDisplace: true,
        enableIvoryTexture: false,
        enableVintageWear: false
      }
    });
  }

  attachDrawerEvents() {
    const expandBtn = this.drawerEl.querySelector('#kbdExpandToggleBtn');
    const titleBtn = this.drawerEl.querySelector('#kbdHeaderTitleBtn');

    if (expandBtn) expandBtn.addEventListener('click', () => this.toggle());
    if (titleBtn) titleBtn.addEventListener('click', () => this.toggle());

    // Global toggle shortcut Ctrl+K
    window.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
        const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
        if (tag === 'input' || tag === 'textarea') return;
        e.preventDefault();
        this.toggle();
      }
    });
  }

  toggle(forceState) {
    if (typeof forceState === 'boolean') {
      this.isExpanded = forceState;
    } else {
      this.isExpanded = !this.isExpanded;
    }

    this.drawerEl.classList.toggle('expanded', this.isExpanded);
    this.drawerEl.classList.toggle('collapsed', !this.isExpanded);

    try {
      localStorage.setItem('abdaudiolab_keyboard_expanded', String(this.isExpanded));
    } catch (_) {}

    const toggleBtn = this.drawerEl.querySelector('#kbdExpandToggleBtn');
    if (toggleBtn) {
      toggleBtn.innerHTML = this.isExpanded ? this.getChevronDownSvg() : this.getChevronUpSvg();
    }

    if (this.onToggleCallback) {
      this.onToggleCallback(this.isExpanded);
    }
  }

  // --- MIDI IPC Handlers (Zero Heap Allocation) ---

  sendMidiNoteOn(note, velocity) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'midiNoteOn',
        note: Math.round(note),
        velocity: Math.max(1, Math.min(127, Math.round((velocity || 0.8) * 127))),
        channel: this.midiChannel
      });
    }
  }

  sendMidiNoteOff(note) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'midiNoteOff',
        note: Math.round(note),
        channel: this.midiChannel
      });
    }
  }

  sendMidiPitchBend(normalized) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'midiPitchBend',
        value: normalized, // float -1.0 .. +1.0
        channel: this.midiChannel
      });
    }
  }

  sendMidiModWheel(normalized) {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'midiController',
        controller: 1, // CC#1 Modulation Wheel
        value: Math.max(0, Math.min(127, Math.round((normalized || 0) * 127))),
        channel: this.midiChannel
      });
    }
  }

  sendMidiPanic() {
    if (this.isNativeHost && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('nativeEvent', {
        action: 'midiAllNotesOff',
        channel: this.midiChannel
      });
    }
  }

  // --- UI Helpers ---

  addActiveNoteVisual(note) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const name = names[note % 12] + (Math.floor(note / 12) - 1);
    this.activeNoteNames.set(note, name);
    this.updateNotePill();
  }

  removeActiveNoteVisual(note) {
    this.activeNoteNames.delete(note);
    this.updateNotePill();
  }

  updateNotePill() {
    const pill = this.drawerEl.querySelector('#kbdActiveNotePill');
    if (!pill) return;
    if (this.activeNoteNames.size === 0) {
      pill.textContent = 'IDLE';
      pill.classList.remove('active');
    } else {
      const activeList = Array.from(this.activeNoteNames.values()).slice(-3).join(' ');
      pill.textContent = activeList;
      pill.classList.add('active');
    }
  }

  getChevronUpSvg() {
    return `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="18 15 12 9 6 15"></polyline></svg>`;
  }

  getChevronDownSvg() {
    return `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>`;
  }

  destroy() {
    if (this.keyboardInstance && typeof this.keyboardInstance.destroy === 'function') {
      this.keyboardInstance.destroy();
    }
    if (this.drawerEl && this.drawerEl.parentNode) {
      this.drawerEl.parentNode.removeChild(this.drawerEl);
    }
  }
}
