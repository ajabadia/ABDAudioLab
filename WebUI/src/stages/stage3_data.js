/**
 * Stage 3 Data Models, Preset Catalogs and Estimation Engine
 * Canonical mirror of C++ backend:
 * - src/core/AutoTestPresetEngine.cpp
 * - src/gui/TestConfiguration.h
 * - src/gui/suite/SuiteDataModels.h
 */

export const STIMULUS_TYPES = [
  {
    id: 'LogFarinaSweep',
    numId: 1,
    label: 'Exponential Log-Farina Sweep (Standard for Filters/EQ)',
    desc: 'Log Sweep: Best SNR, precise 20Hz-20kHz response, extracts THD'
  },
  {
    id: 'AmplitudeRamp',
    numId: 2,
    label: 'Amplitude Ramp (Saturation & Non-Linear Knees)',
    desc: 'Amplitude Ramp: Non-linear saturation and compression knees'
  },
  {
    id: 'SyncPulses3',
    numId: 3,
    label: 'Sync Pulses (ADSR Gate & Timing)',
    desc: 'Sync Pulses: Gate step pulses for ADSR envelope timings'
  },
  {
    id: 'DiracDelta',
    numId: 4,
    label: 'Step Impulse / Dirac Delta (Transient & Phase Calibration)',
    desc: 'Step Impulse (Dirac Delta): Single 0 dBfs sample for transient/phase IR'
  },
  {
    id: 'SineWave1kHz',
    numId: 5,
    label: '1kHz Sine Wave (Saturator & Harmonic Distortion THD)',
    desc: '1kHz Sine Wave: Pure tone for harmonic distortion (THD) and saturation'
  },
  {
    id: 'WhiteNoise',
    numId: 6,
    label: 'White Noise Burst (Statistical Noise Floor & Broad Band)',
    desc: 'White Noise: Uniform energy density across all frequencies'
  },
  {
    id: 'NamCalibration',
    numId: 7,
    label: 'NAM / RTNeural Calibration (Sync, Chirp, Noise, Multitone)',
    desc: 'NAM Calibration: Multi-stage neural stimulus with sample-accurate sync pulses'
  },
  {
    id: 'LevelLadderSweep',
    numId: 8,
    label: 'Level-Ladder Multi-Amplitude Sweep (-24 to +6 dBu)',
    desc: 'Level-Ladder: Staged multi-amplitude sweeps to characterize non-linear saturation vs. level'
  },
  {
    id: 'MultitoneIMD',
    numId: 9,
    label: '31-Tone Multitone IMD (Intermodulation Distortion)',
    desc: 'Multitone: Simultaneous non-harmonic 31-tone excitation to isolate IMD and dynamic noise floor'
  }
];

export const DURATION_PRESETS = [
  { val: 0.25, label: 'Ultra-Fast (0.25 sec per point)' },
  { val: 0.50, label: 'Fast Standard (0.50 sec per point)' },
  { val: 1.00, label: 'High Precision (1.00 sec per point)' },
  { val: 2.00, label: 'Ultra High Precision (2.00 sec per point)' },
  { val: 4.00, label: 'Long Tail (4.00 sec per point)' },
  { val: 'custom', label: 'Custom Duration (Manual)...' }
];

export const RESOLUTION_PRESETS = [
  { steps: 1, label: 'Fixed (1 point)' },
  { steps: 3, label: 'Coarse (3 points)' },
  { steps: 5, label: 'Standard (5 points)' },
  { steps: 8, label: 'Detailed (8 points)' },
  { steps: 16, label: 'Fine (16 points)' },
  { steps: 32, label: 'Ultra-Fine (32 points)' },
  { steps: 64, label: 'Extreme (64 points)' },
  { steps: 'custom', label: 'Custom...' }
];

// --- HARDWARE CONTRACT FAMILIES (SSOT MIRROR OF contracts/hardware/*.json & profiles/targets/*.json) ---
export const CONTRACT_FAMILIES = {
  roland_aira: {
    id: 'roland_aira',
    contractId: 'roland_aira_submodules',
    label: 'Roland AIRA Modular Submodules',
    models: ['roland_aira_modular', 'roland_aira_bitrazer', 'roland_aira_demora', 'roland_aira_scooper', 'roland_aira_torcido', 'bitrazer', 'demora', 'scooper', 'torcido'],
    deviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX']
  },
  casio_cz: {
    id: 'casio_cz',
    contractId: 'casio_cz101',
    label: 'Casio Phase Distortion (CZ Series)',
    models: ['casio_cz101', 'casio_cz1000', 'casio_cz5000', 'casio_cz1', 'cz101', 'cz1000', 'cz5000', 'cz1'],
    deviceTypes: ['AUTOMATED_SYSEX', 'AUTOMATED_MIDI_CC', 'DIGITAL_SYNTH']
  },
  roland_juno: {
    id: 'roland_juno',
    contractId: 'roland_juno106',
    label: 'Roland Juno Series (IR3109 VCF & BBD Chorus)',
    models: ['roland_juno106', 'roland_juno60', 'roland_juno6', 'roland_hs60', 'juno106', 'juno60', 'juno6', 'hs60'],
    deviceTypes: ['AUTOMATED_SYSEX', 'AUTOMATED_MIDI_CC', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH']
  },
  behringer_deepmind: {
    id: 'behringer_deepmind',
    contractId: 'behringer_deepmind12',
    label: 'Behringer DeepMind Series',
    models: ['behringer_deepmind12', 'behringer_deepmind12d', 'behringer_deepmind6', 'deepmind12', 'deepmind12d', 'deepmind6'],
    deviceTypes: ['AUTOMATED_MIDI_CC', 'ANALOGUE_SYNTH']
  },
  korg_ms2000: {
    id: 'korg_ms2000',
    contractId: 'korg_ms2000',
    label: 'Korg MS2000 & microKORG Family',
    models: ['korg_ms2000', 'korg_microkorg', 'ms2000', 'microkorg'],
    deviceTypes: ['AUTOMATED_MIDI_CC', 'DIGITAL_SYNTH']
  },
  behringer_pro800: {
    id: 'behringer_pro800',
    contractId: 'behringer_pro800',
    label: 'Behringer PRO-800 Polyphonic Analog',
    models: ['behringer_pro800', 'pro800'],
    deviceTypes: ['AUTOMATED_MIDI_CC', 'ANALOGUE_SYNTH']
  },
  yamaha_dx7: {
    id: 'yamaha_dx7',
    contractId: 'yamaha_dx7',
    label: 'Yamaha DX7 FM Synthesis Family',
    models: ['yamaha_dx7', 'yamaha_dx7ii', 'dx7', 'dx7ii'],
    deviceTypes: ['AUTOMATED_SYSEX', 'DIGITAL_SYNTH']
  },
  boss_ds1: {
    id: 'boss_ds1',
    contractId: 'hw-boss-ds1-canonical',
    label: 'BOSS Analogue Distortion & Overdrive',
    models: ['boss_ds1', 'hw-boss-ds1', 'boss-ds1-distortion', 'ds1'],
    deviceTypes: ['ANALOGUE_PEDAL']
  }
};

export const PRESET_CATEGORIES = [
  { id: 'UserRecipes', label: 'User & Workspace Recipes' },
  { id: 'FiltersAndToneShapers', label: 'Filters & Tone Shapers' },
  { id: 'DynamicsAndNonLinear', label: 'Distortion, Saturation & Dynamics' },
  { id: 'EurorackAiraSubmodules', label: 'Modular & Submodules (Roland AIRA)', contractFamily: 'roland_aira' },
  { id: 'ModulatorsAndEnvelopes', label: 'Envelopes & Modulators' },
  { id: 'CasioCZ', label: 'Phase Distortion & Digital (Casio CZ)', contractFamily: 'casio_cz' }
];

export function getTargetFamily(target) {
  if (!target) return null;
  const tid = (target.id || '').toLowerCase().replace(/-/g, '_');
  const tname = (target.name || target.shortName || '').toLowerCase();

  for (const famKey of Object.keys(CONTRACT_FAMILIES)) {
    const fam = CONTRACT_FAMILIES[famKey];
    if (fam.models.some(m => tid === m || tid.includes(m) || m.includes(tid))) {
      return fam.id;
    }
    if (tname && fam.models.some(m => tname.includes(m))) {
      return fam.id;
    }
  }
  return null;
}

export function getTargetDeviceType(target) {
  if (!target) return 'UNKNOWN';
  if (target.deviceType) return target.deviceType;
  if (target.isPlugin) return 'VIRTUAL_PLUGIN';
  const cat = `${target.category || ''} ${target.name || ''}`.toLowerCase();
  if (cat.includes('pedal') || cat.includes('distortion') || cat.includes('overdrive')) return 'ANALOGUE_PEDAL';
  if (cat.includes('eurorack') || cat.includes('modular')) return 'MANUAL_EURORACK';
  if (cat.includes('synth')) return 'ANALOGUE_SYNTH';
  if (cat.includes('rack') || cat.includes('outboard')) return 'STUDIO_OUTBOARD';
  if (cat.includes('preamp')) return 'PREAMP';
  return 'GENERIC_HARDWARE';
}

export function getCompatiblePresetsForTarget(target) {
  if (!target) return MASTER_PRESETS;
  const targetFamily = getTargetFamily(target);
  const deviceType = getTargetDeviceType(target);
  const isPedal = deviceType === 'ANALOGUE_PEDAL' || (target.category && target.category.toLowerCase().includes('pedal')) || (target.name && target.name.toLowerCase().includes('pedal'));
  const isSynth = deviceType === 'ANALOGUE_SYNTH' || deviceType === 'DIGITAL_SYNTH' || deviceType === 'MANUAL_SYNTH' || (target.category && target.category.toLowerCase().includes('synth'));

  const tId = (target.id || '').toLowerCase().replace(/[-_\s]/g, '');
  const tName = (target.name || target.shortName || '').toLowerCase().replace(/[-_\s]/g, '');

  return MASTER_PRESETS.filter(preset => {
    // 0. User / Workspace Recipes strict validation:
    if (preset.isUserRecipe || preset.isUserPreset || preset.category === 'UserRecipes') {
      const pTid = (preset.targetId || '').toLowerCase().replace(/[-_\s]/g, '');
      const pTname = (preset.targetName || '').toLowerCase().replace(/[-_\s]/g, '');
      const pDisplayName = (preset.displayName || preset.recipeName || preset.typologyId || '').toLowerCase();

      // If preset has specific targetId or targetName:
      if (pTid || pTname) {
        const matchesId = pTid && (pTid === tId || tId.includes(pTid) || pTid.includes(tId));
        const matchesName = pTname && (pTname === tName || tName.includes(pTname) || pTname.includes(tName));
        if (matchesId || matchesName) {
          return true;
        }
        // Specific target does not match current target -> discard immediately
        return false;
      }

      // Check if preset display name mentions specific hardware brands/models:
      const hardwareKeywords = [
        { key: 'juno', matches: tId.includes('juno') || tName.includes('juno') },
        { key: 'dx7', matches: tId.includes('dx7') || tName.includes('dx7') },
        { key: 'ms2000', matches: tId.includes('ms2000') || tName.includes('ms2000') },
        { key: 'microkorg', matches: tId.includes('microkorg') || tName.includes('microkorg') },
        { key: 'pro800', matches: tId.includes('pro800') || tName.includes('pro800') },
        { key: 'deepmind', matches: tId.includes('deepmind') || tName.includes('deepmind') },
        { key: 'cz', matches: tId.includes('cz') || tName.includes('cz') },
        { key: 'ds1', matches: tId.includes('ds1') || tName.includes('ds1') },
        { key: 'distortion', matches: isPedal || tName.includes('distortion') },
        { key: 'overdrive', matches: isPedal || tName.includes('overdrive') }
      ];

      for (const hw of hardwareKeywords) {
        if (pDisplayName.includes(hw.key) && !hw.matches) {
          return false;
        }
      }

      // If target is an analog pedal:
      if (isPedal) {
        if (pDisplayName.includes('vcf') || pDisplayName.includes('synth') || pDisplayName.includes('osc') || pDisplayName.includes('dco') || pDisplayName.includes('env') || pDisplayName.includes('lfo')) {
          if (!pDisplayName.includes('distortion') && !pDisplayName.includes('overdrive') && !pDisplayName.includes('pedal')) {
            return false;
          }
        }
        if (preset.category === 'FiltersAndToneShapers' && !preset.typologyId?.includes('eq')) return false;
        if (preset.category === 'ModulatorsAndEnvelopes' || preset.category === 'CasioCZ' || preset.category === 'EurorackAiraSubmodules') return false;
        if (preset.compatibleDeviceTypes && !preset.compatibleDeviceTypes.includes('ANALOGUE_PEDAL')) {
          return false;
        }
      }

      // If target is a synth:
      if (isSynth) {
        if (preset.category === 'DynamicsAndNonLinear' && pDisplayName.includes('guitar pedal')) {
          return false;
        }
      }

      return true;
    }

    // 1. Contract family check: proprietary presets require matching contract family
    if (preset.contractFamily) {
      return targetFamily === preset.contractFamily;
    }

    // 2. Pedals must not receive synth VCFs or envelope generators
    if (isPedal) {
      if (preset.compatibleDeviceTypes && !preset.compatibleDeviceTypes.includes('ANALOGUE_PEDAL')) {
        return false;
      }
      return preset.category === 'DynamicsAndNonLinear' || preset.typologyId === 'eq_parametric_graphic';
    }

    // 3. Device type compatibility check
    if (preset.compatibleDeviceTypes && preset.compatibleDeviceTypes.length > 0) {
      if (!preset.compatibleDeviceTypes.includes(deviceType) && !preset.compatibleDeviceTypes.includes('ALL')) {
        return false;
      }
    }

    // 4. Virtual plugins or targets without a registered factory recipe do not have standard hardware presets
    if ((target.isPlugin || deviceType === 'VIRTUAL_PLUGIN') && !preset.isUserRecipe && !preset.isUserPreset) {
      if (!targetFamily || preset.contractFamily !== targetFamily) {
        return false;
      }
    }

    return true;
  });
}

export function getCompatibleCategoriesForTarget(target) {
  const compatiblePresets = getCompatiblePresetsForTarget(target);
  const activeCategoryIds = new Set(compatiblePresets.map(p => p.category));

  return PRESET_CATEGORIES.filter(cat => activeCategoryIds.has(cat.id));
}

export const MASTER_PRESETS = [
  // --- FILTERS & TONE SHAPERS ---
  {
    typologyId: 'vcf_ladder_ota_svf',
    displayName: 'VCF Filter (Ladder, OTA, Sallen-Key, SVF, Diode)',
    badgeText: 'VCF',
    category: 'FiltersAndToneShapers',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'VCF Cutoff Frequency', type: 'Knob', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'resonance', name: 'VCF Resonance (Q)', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'slope', name: 'VCF Slope (2-Pole / 4-Pole)', type: 'Switch', steps: 1, minPct: 0, maxPct: 100 },
      { id: 'boost', name: 'VCF Bass Boost', type: 'Switch', steps: 1, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'filter_tpt_state_variable',
    displayName: 'TPT / ZDF Multi-Mode State Variable Filter (LP/BP/HP/Notch)',
    badgeText: 'SVF',
    category: 'FiltersAndToneShapers',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'SVF Cutoff Freq', type: 'Knob', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'q', name: 'SVF Resonance (Q)', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'mode', name: 'Filter Mode (LP/BP/HP)', type: 'Switch', steps: 3, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'filter_formant_vocal',
    displayName: 'Formant / Vocal Resonator Filter',
    badgeText: 'FRM',
    category: 'FiltersAndToneShapers',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'STUDIO_OUTBOARD', 'VIRTUAL_PLUGIN'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.50,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'vowel', name: 'Formant Vowel Morph (A-E-I-O-U)', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'intensity', name: 'Resonator Intensity', type: 'Knob', steps: 3, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'eq_parametric_graphic',
    displayName: 'Parametric / Graphic Equalizer',
    badgeText: 'EQ',
    category: 'FiltersAndToneShapers',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['ALL', 'ANALOGUE_PEDAL', 'STUDIO_OUTBOARD', 'PREAMP', 'AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN', 'CABINET'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'freq', name: 'Bell Frequency', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'gain', name: 'Band Gain (-12dB to +12dB)', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'q', name: 'Q Bandwidth', type: 'Knob', steps: 3, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'filter_comb',
    displayName: 'Comb Filter (Positive / Negative Feedback)',
    badgeText: 'CMB',
    category: 'FiltersAndToneShapers',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'STUDIO_OUTBOARD', 'VIRTUAL_PLUGIN'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.50,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'frequency', name: 'Comb Frequency / Delay', type: 'Knob', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'feedback', name: 'Feedback Ratio', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },

  // --- ROLAND AIRA MODULAR SUBMODULES (CONTRACT: roland_aira_submodules.json) ---
  {
    typologyId: 'aira_01_ladder_lpf',
    displayName: 'Roland AIRA: 01. Ladder Low-Pass Filter (-24dB/Oct)',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'Cutoff Freq', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'resonance', name: 'Resonance', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_02_resonant_filter',
    displayName: 'Roland AIRA: 02. Resonant Filter (-18dB/Oct)',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'Cutoff Freq', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'resonance', name: 'Resonance', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_03_high_pass',
    displayName: 'Roland AIRA: 03. High-Pass Filter (-12dB/Oct)',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'HPF Cutoff Freq', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_04_band_pass',
    displayName: 'Roland AIRA: 04. Band-Pass Filter (BPF)',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'center_freq', name: 'Center Freq', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'bandwidth', name: 'Bandwidth (Q)', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_05_formant_filter',
    displayName: 'Roland AIRA: 05. Formant Vocal Filter',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'vowel', name: 'Vowel Frequency', type: 'Slider', steps: 5, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_06_svf_multimode',
    displayName: 'Roland AIRA: 06. State Variable Filter (SVF Multi-Mode)',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cutoff', name: 'Cutoff Freq', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'resonance', name: 'Resonance', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_12_adsr_envelope',
    displayName: 'Roland AIRA: 12. ADSR Envelope Generator',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'TimeDynamic',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'SyncPulses3',
    burstDurationSec: 2.00,
    captureMode: 'ADAPTIVE_ENVELOPE',
    defaultControls: [
      { id: 'attack', name: 'Attack Time', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'decay', name: 'Decay Time', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'sustain', name: 'Sustain Level', type: 'Slider', steps: 3, minPct: 0, maxPct: 100 },
      { id: 'release', name: 'Release Time', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_13_ar_envelope',
    displayName: 'Roland AIRA: 13. AR Attack-Release Envelope',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'TimeDynamic',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'SyncPulses3',
    burstDurationSec: 1.50,
    captureMode: 'ADAPTIVE_ENVELOPE',
    defaultControls: [
      { id: 'attack', name: 'Attack Time', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'release', name: 'Release Time', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_14_multiwave_lfo',
    displayName: 'Roland AIRA: 14. Multi-Wave LFO Modulator',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'CyclicModulator',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'AmplitudeRamp',
    burstDurationSec: 3.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'rate', name: 'LFO Rate / Freq', type: 'Slider', steps: 6, minPct: 0, maxPct: 100 },
      { id: 'waveform', name: 'LFO Waveform Shape', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_15_dual_lfo',
    displayName: 'Roland AIRA: 15. Dual Cross-Modulated LFO',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'CyclicModulator',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'AmplitudeRamp',
    burstDurationSec: 3.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'rate1', name: 'LFO 1 Rate', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'rate2', name: 'LFO 2 Rate', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'depth', name: 'Cross-Mod Depth', type: 'Slider', steps: 3, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_24_sample_hold',
    displayName: 'Roland AIRA: 24. Sample & Hold (S&H) Circuit',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'CyclicModulator',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'SyncPulses3',
    burstDurationSec: 2.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'rate', name: 'Clock Rate', type: 'Slider', steps: 5, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'aira_25_slew_limiter',
    displayName: 'Roland AIRA: 25. Slew Limiter / Portamento Glide',
    badgeText: 'AIRA',
    category: 'EurorackAiraSubmodules',
    contractFamily: 'roland_aira',
    blockType: 'TimeDynamic',
    compatibleDeviceTypes: ['MANUAL_EURORACK', 'AUTOMATED_SYSEX'],
    stimulusType: 'DiracDelta',
    burstDurationSec: 1.50,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'rise', name: 'Rise Slew Rate', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'fall', name: 'Fall Slew Rate', type: 'Slider', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },

  // --- CASIO CZ PHASE DISTORTION (CONTRACT: casio_cz101.json) ---
  {
    typologyId: 'cz_dcw_waveshaping',
    displayName: 'Casio CZ: DCW Phase Distortion Waveshaper (Virtual VCF)',
    badgeText: 'DCW',
    category: 'CasioCZ',
    contractFamily: 'casio_cz',
    blockType: 'SpectrumFilter',
    compatibleDeviceTypes: ['AUTOMATED_SYSEX', 'AUTOMATED_MIDI_CC', 'DIGITAL_SYNTH', 'ANALOGUE_SYNTH'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'dcw_depth', name: 'DCW Envelope Depth', type: 'Slider', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'wave_select', name: 'Waveform Select (Saw / Square / Resonant)', type: 'Switch', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },

  // --- ENVELOPES & MODULATORS ---
  {
    typologyId: 'env_generator_adsr',
    displayName: 'Envelope Generator (ADSR / AR / Function Gen)',
    badgeText: 'ENV',
    category: 'ModulatorsAndEnvelopes',
    blockType: 'TimeDynamic',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'SyncPulses3',
    burstDurationSec: 2.00,
    captureMode: 'ADAPTIVE_ENVELOPE',
    defaultControls: [
      { id: 'attack', name: 'Attack Time', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'decay', name: 'Decay Time', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'sustain', name: 'Sustain Level', type: 'Knob', steps: 3, minPct: 0, maxPct: 100 },
      { id: 'release', name: 'Release Time', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'lfo_sample_hold',
    displayName: 'LFO & Sample / Hold Circuit',
    badgeText: 'LFO',
    category: 'ModulatorsAndEnvelopes',
    blockType: 'CyclicModulator',
    compatibleDeviceTypes: ['AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'AmplitudeRamp',
    burstDurationSec: 3.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'rate', name: 'LFO Rate / Frequency', type: 'Knob', steps: 6, minPct: 0, maxPct: 100 }
    ]
  },

  // --- DYNAMICS & SATURATION ---
  {
    typologyId: 'vca_attenuverter',
    displayName: 'VCA & Attenuverter (Lin/Exp Response & Bleed)',
    badgeText: 'VCA',
    category: 'DynamicsAndNonLinear',
    blockType: 'AmplitudeGain',
    compatibleDeviceTypes: ['ANALOGUE_PEDAL', 'MANUAL_EURORACK', 'AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'STUDIO_OUTBOARD', 'PREAMP', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'AmplitudeRamp',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'cv_level', name: 'CV Bias / Gain Level', type: 'Knob', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'response', name: 'Response Curve (Lin/Exp)', type: 'Switch', steps: 2, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'saturator_fuzz_tube',
    displayName: 'Saturator / Fuzz / Tube Preamp / Wavefolder',
    badgeText: 'SAT',
    category: 'DynamicsAndNonLinear',
    blockType: 'WaveShaper',
    compatibleDeviceTypes: ['ANALOGUE_PEDAL', 'STUDIO_OUTBOARD', 'PREAMP', 'AUTOMATED_MIDI_CC', 'AUTOMATED_NRPN', 'AUTOMATED_SYSEX', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DIGITAL_SYNTH', 'MANUAL_EURORACK', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'SineWave1kHz',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'drive', name: 'Drive / Saturation Gain', type: 'Knob', steps: 8, minPct: 0, maxPct: 100 },
      { id: 'bias', name: 'Asymmetry / Tube Bias', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'pedal_overdrive_distortion',
    displayName: 'Guitar Overdrive / Distortion Pedal (Drive & Tone Stack)',
    badgeText: 'PEDAL',
    category: 'DynamicsAndNonLinear',
    blockType: 'WaveShaper',
    compatibleDeviceTypes: ['ANALOGUE_PEDAL', 'STUDIO_OUTBOARD', 'PREAMP', 'VIRTUAL_PLUGIN', 'DESKTOP_MODULE'],
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.00,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'drive', name: 'Drive / Gain', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'tone', name: 'Tone / EQ Filter', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'level', name: 'Output Level', type: 'Knob', steps: 1, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'analog_tape_saturation',
    displayName: 'Analog Tape Saturation & Magnetic Hysteresis',
    badgeText: 'TAPE',
    category: 'DynamicsAndNonLinear',
    blockType: 'WaveShaper',
    compatibleDeviceTypes: ['ANALOGUE_PEDAL', 'STUDIO_OUTBOARD', 'PREAMP', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'SineWave1kHz',
    burstDurationSec: 1.50,
    captureMode: 'FIXED_TIME',
    defaultControls: [
      { id: 'input_drive', name: 'Input Drive / Saturation', type: 'Knob', steps: 6, minPct: 0, maxPct: 100 },
      { id: 'tape_speed', name: 'Tape Speed (7.5 / 15 / 30 ips)', type: 'Switch', steps: 3, minPct: 0, maxPct: 100 }
    ]
  },
  {
    typologyId: 'compressor_limiter_gate',
    displayName: 'Compressor / Limiter / Gate Dynamics',
    badgeText: 'CMP',
    category: 'DynamicsAndNonLinear',
    blockType: 'TimeDynamic',
    compatibleDeviceTypes: ['ANALOGUE_PEDAL', 'STUDIO_OUTBOARD', 'PREAMP', 'AUTOMATED_MIDI_CC', 'MANUAL_SYNTH', 'ANALOGUE_SYNTH', 'DESKTOP_MODULE', 'VIRTUAL_PLUGIN'],
    stimulusType: 'AmplitudeRamp',
    burstDurationSec: 2.00,
    captureMode: 'ADAPTIVE_ENVELOPE',
    defaultControls: [
      { id: 'threshold', name: 'Compression Threshold', type: 'Knob', steps: 5, minPct: 0, maxPct: 100 },
      { id: 'ratio', name: 'Compression Ratio', type: 'Knob', steps: 4, minPct: 0, maxPct: 100 },
      { id: 'release', name: 'Release Recovery Time', type: 'Knob', steps: 3, minPct: 0, maxPct: 100 }
    ]
  }
];

// --- WORKSPACE & USER RECIPES PERSISTENCE ---
export const STORAGE_KEY_USER_RECIPES = '__ABDAudioLab_user_recipes__';

export function loadUserRecipesFromStorage() {
  try {
    if (typeof window === 'undefined' || !window.localStorage) return [];
    const raw = window.localStorage.getItem(STORAGE_KEY_USER_RECIPES);
    if (!raw) return [];
    const list = JSON.parse(raw);
    if (Array.isArray(list)) {
      list.forEach(recipe => {
        if (!recipe || !recipe.typologyId) return;
        recipe.isUserRecipe = true;
        // Sanitize legacy recipes that lacked targetId
        const dName = (recipe.displayName || recipe.recipeName || '').toLowerCase();
        if (!recipe.targetId) {
          if (dName.includes('juno')) {
            recipe.targetId = 'roland_juno106';
            recipe.compatibleDeviceTypes = ['AUTOMATED_MIDI_CC', 'ANALOGUE_SYNTH'];
          } else if (dName.includes('ds-1') || dName.includes('ds1')) {
            recipe.targetId = 'boss_ds1';
            recipe.compatibleDeviceTypes = ['ANALOGUE_PEDAL'];
          }
        }
        const existingIdx = MASTER_PRESETS.findIndex(p => p.typologyId === recipe.typologyId);
        if (existingIdx >= 0) {
          MASTER_PRESETS[existingIdx] = recipe;
        } else {
          MASTER_PRESETS.unshift(recipe);
        }
      });
      return list;
    }
  } catch (err) {
    console.warn('[stage3_data] Failed to load user recipes from localStorage:', err);
  }
  return [];
}

export function saveUserRecipe(recipe) {
  if (!recipe || !recipe.typologyId) return;
  recipe.isUserRecipe = true;
  const existingIdx = MASTER_PRESETS.findIndex(p => p.typologyId === recipe.typologyId);
  if (existingIdx >= 0) {
    MASTER_PRESETS[existingIdx] = recipe;
  } else {
    MASTER_PRESETS.unshift(recipe);
  }

  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      const userRecipes = MASTER_PRESETS.filter(p => p.isUserRecipe || p.isUserPreset || p.category === 'UserRecipes');
      window.localStorage.setItem(STORAGE_KEY_USER_RECIPES, JSON.stringify(userRecipes));
    }
  } catch (err) {
    console.warn('[stage3_data] Failed to save user recipe to localStorage:', err);
  }
}

export function registerWorkspaceRecipes(recipes) {
  if (!Array.isArray(recipes)) return;
  recipes.forEach(r => {
    if (!r) return;
    const typologyId = r.typologyId || r.recipeId || (r.displayName ? `recipe_${r.displayName.toLowerCase().replace(/[^a-z0-9]/g, '_')}` : `recipe_${Date.now()}`);
    const dName = (r.displayName || r.recipeName || typologyId).toLowerCase();
    let autoTargetId = r.targetId || '';
    let autoCompat = Array.isArray(r.compatibleDeviceTypes) && r.compatibleDeviceTypes.length > 0 ? r.compatibleDeviceTypes : ['ALL'];
    if (!autoTargetId) {
      if (dName.includes('juno')) {
        autoTargetId = 'roland_juno106';
        autoCompat = ['AUTOMATED_MIDI_CC', 'ANALOGUE_SYNTH'];
      } else if (dName.includes('ds-1') || dName.includes('ds1')) {
        autoTargetId = 'boss_ds1';
        autoCompat = ['ANALOGUE_PEDAL'];
      }
    }
    const normalized = {
      typologyId,
      displayName: r.displayName || r.recipeName || typologyId,
      badgeText: r.badgeText || 'USR',
      category: r.category || 'UserRecipes',
      isUserRecipe: true,
      stimulusType: r.stimulusType || 'LogFarinaSweep',
      burstDurationSec: typeof r.burstDurationSec === 'number' ? r.burstDurationSec : 1.0,
      captureMode: r.captureMode || 'FIXED_TIME',
      targetId: autoTargetId,
      targetName: r.targetName || '',
      compatibleDeviceTypes: autoCompat,
      defaultControls: Array.isArray(r.defaultControls) ? r.defaultControls : (Array.isArray(r.controls) ? r.controls : [])
    };
    saveUserRecipe(normalized);
  });
}

// Initialise user recipes from localStorage on boot
if (typeof window !== 'undefined' && window.localStorage) {
  loadUserRecipesFromStorage();
}

export const MAX_MATRIX_POINTS = 64;

/**
 * Calculates total factorial measurement states and estimated runtime
 */
export function calculatePlanEstimate(controls, burstDurationSec, isManual) {
  if (!controls || controls.length === 0) {
    return {
      states: 0,
      formula: '0 controls (blank)',
      isManual: !!isManual,
      totalSeconds: 0,
      durationStr: '0s',
      exceedsLimit: false,
      maxLimit: MAX_MATRIX_POINTS
    };
  }

  let states = 1;
  const parts = [];

  for (const c of controls) {
    const s = Math.max(1, parseInt(c.steps, 10) || 1);
    states *= s;
    parts.push(`${s} ${c.name || 'Param'}`);
  }

  const overheadPerPoint = isManual ? 2.5 : 0.05; // 2.5s for human knob turn, 50ms for digital automation
  const totalSeconds = states * (burstDurationSec + overheadPerPoint);

  return {
    states,
    formula: parts.join(' × '),
    isManual: !!isManual,
    totalSeconds,
    durationStr: formatDuration(totalSeconds),
    exceedsLimit: states > MAX_MATRIX_POINTS,
    maxLimit: MAX_MATRIX_POINTS
  };
}

function formatDuration(sec) {
  if (isNaN(sec) || !isFinite(sec) || sec <= 0) return '0s';
  const s = Math.round(sec);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const remS = s % 60;
  if (h > 0) {
    return `${h}h ${m}m ${remS}s`;
  }
  if (m > 0) {
    return `${m}m ${remS}s`;
  }
  return `${s}s`;
}

/**
 * Generates an array of individual Point items from the controls combination
 */
export function generateTestPoints(controls, totalPoints) {
  const points = [];
  const count = Math.min(64, Math.max(1, totalPoints || 1));

  for (let i = 0; i < count; i++) {
    const posPct = count === 1 ? 0.0 : ((i / (count - 1)) * 100);
    points.push({
      index: i + 1,
      total: count,
      posPct: parseFloat(posPct.toFixed(1)),
      status: 'QUEUED', // QUEUED | RUNNING | COMPLETED | INVALIDATED
      isSelected: false,
      telemetry: {
        peakDbfs: -120.0,
        rmsDbfs: -120.0,
        snrDb: 96.0
      }
    });
  }

  return points;
}


// --- CANONICAL HARDWARE TARGET RECIPES (SSOT MIRROR OF C++ CONTRACTS) ---
export const TARGET_RECIPES = {
  'boss_ds1': {
    targetId: 'boss_ds1',
    hardwareImage: 'models/boss-ds1.webp',
    hardwareName: 'BOSS DS-1 Distortion (Analogue Pedal)',
    recipeName: 'BOSS DS-1 Distortion (Analogue Pedal) (BOSS DS-1 Distortion (Analogue Pedal) Controls)',
    badgeText: 'DS1',
    stimulusType: 'SyncPulses3',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: true,
    controls: [
      { name: 'TONE Potentiometer', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'LEVEL Potentiometer', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'DIST Potentiometer', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'behringer_deepmind12': {
    targetId: 'behringer_deepmind12',
    hardwareImage: 'models/behringer-deepmind12.webp',
    hardwareName: 'Behringer DeepMind 12 / 12D',
    recipeName: 'Behringer DeepMind 12 / 12D (Analog 2-Pole / 4-Pole Resonant Low-Pass VCF)',
    badgeText: 'DM12',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Resonance (Q)', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Slope (2-Pole / 4-Pole)', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 },
      { name: 'VCF Bass Boost', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'behringer_deepmind6': {
    targetId: 'behringer_deepmind6',
    hardwareImage: 'models/behringer-deepmind6.webp',
    hardwareName: 'Behringer DeepMind 6',
    recipeName: 'Behringer DeepMind 6 (Analog Resonant Low-Pass VCF)',
    badgeText: 'DM6',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Resonance (Q)', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Slope (2-Pole / 4-Pole)', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 },
      { name: 'VCF Bass Boost', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'behringer_pro800': {
    targetId: 'behringer_pro800',
    hardwareImage: 'models/behringer-pro800.webp',
    hardwareName: 'Behringer PRO-800',
    recipeName: 'Behringer PRO-800 (Analog Polyphonic VCF)',
    badgeText: 'PRO',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'Filter Cutoff', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Resonance', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'Filter Envelope Amount', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'roland_juno106': {
    targetId: 'roland_juno106',
    hardwareImage: 'models/roland-juno-106.webp',
    hardwareName: 'Roland Juno-106',
    recipeName: 'Roland Juno-106 (IR3109 Resonant Low-Pass VCF)',
    badgeText: 'J106',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'HPF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Resonance', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 },
      { name: 'VCF Envelope Modulation', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'roland_juno60': {
    targetId: 'roland_juno60',
    hardwareImage: 'models/roland-juno-60.webp',
    hardwareName: 'Roland Juno-60',
    recipeName: 'Roland Juno-60 (Analogue Panel Sliders)',
    badgeText: 'J60',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: true,
    controls: [
      { name: 'HPF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Resonance', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'yamaha_dx7': {
    targetId: 'yamaha_dx7',
    hardwareImage: 'models/yamaha-dx7.webp',
    hardwareName: 'Yamaha DX7',
    recipeName: 'Yamaha DX7 (FM Operator Matrix & Feedback)',
    badgeText: 'DX7',
    stimulusType: 'SineWave1kHz',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'Operator 1 Output Level', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Operator 2 Coarse Ratio', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'Operator Feedback Level', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'korg_ms2000': {
    targetId: 'korg_ms2000',
    hardwareImage: 'models/korg-ms2000.webp',
    hardwareName: 'Korg MS2000 / MS2000R',
    recipeName: 'Korg MS2000 (DSP Modeled Multi-Mode Filter)',
    badgeText: 'MS2K',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'Filter Cutoff', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Resonance', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'Filter EG Int', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'roland_aira_modular': {
    targetId: 'roland_aira_modular',
    hardwareImage: 'models/roland-airamodular.webp',
    hardwareName: 'Roland AIRA Modular',
    recipeName: 'Roland AIRA Modular (Submodule CV Matrix)',
    badgeText: 'AIRA',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: true,
    controls: [
      { name: 'Cutoff / Color Knob', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Resonance / Width Knob', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'CV In Attenuator', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'behringer_deepmind12d': {
    targetId: 'behringer_deepmind12d',
    hardwareImage: 'models/behringer-deepmind12.webp',
    hardwareName: 'Behringer DeepMind 12D',
    recipeName: 'Behringer DeepMind 12D (Analog 2-Pole / 4-Pole Resonant Low-Pass VCF)',
    badgeText: 'DM12D',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Resonance (Q)', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Slope (2-Pole / 4-Pole)', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 },
      { name: 'VCF Bass Boost', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'roland_hs60': {
    targetId: 'roland_hs60',
    hardwareImage: 'models/roland-hs60.webp',
    hardwareName: 'Roland HS-60 Synth Plus',
    recipeName: 'Roland HS-60 (IR3109 Resonant Low-Pass VCF & BBD Chorus)',
    badgeText: 'HS60',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'HPF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Resonance', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 },
      { name: 'VCF Envelope Modulation', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'roland_juno6': {
    targetId: 'roland_juno6',
    hardwareImage: 'models/roland-juno-6.webp',
    hardwareName: 'Roland Juno-6',
    recipeName: 'Roland Juno-6 (Analogue Panel Sliders)',
    badgeText: 'J6',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: true,
    controls: [
      { name: 'HPF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'VCF Cutoff Frequency', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'VCF Resonance', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'korg_microkorg': {
    targetId: 'korg_microkorg',
    hardwareImage: 'models/korg-microkorg.webp',
    hardwareName: 'Korg microKORG',
    recipeName: 'Korg microKORG (DSP Modeled Multi-Mode Filter)',
    badgeText: 'mKORG',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'Filter Cutoff', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Resonance', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'Filter EG Int', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'casio_cz101': {
    targetId: 'casio_cz101',
    hardwareImage: 'models/casio-cz101.webp',
    hardwareName: 'Casio CZ-101 (Phase Distortion)',
    recipeName: 'Casio CZ-101 (DCW Phase Distortion Filter & Wave Shaper)',
    badgeText: 'CZ101',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'DCW Envelope Depth', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Waveform Select', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'DCA Level', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  },
  'casio_cz1000': {
    targetId: 'casio_cz1000',
    hardwareImage: 'models/casio-cz1000.webp',
    hardwareName: 'Casio CZ-1000',
    recipeName: 'Casio CZ-1000 (DCW Phase Distortion Filter & Wave Shaper)',
    badgeText: 'CZ1K',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual: false,
    controls: [
      { name: 'DCW Envelope Depth', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 8 },
      { name: 'Waveform Select', type: 'Knob', minPct: 0, maxPct: 100, defaultSteps: 4 },
      { name: 'DCA Level', type: 'Slider', minPct: 0, maxPct: 100, defaultSteps: 1 }
    ]
  }
};

export function hasStandardRecipe(target) {
  if (!target || target.isUnlisted) return false;
  return !!getRecipeForTarget(target);
}

export function getRecipeForTarget(target) {
  if (!target) return null;
  const tid = target.id || '';
  const normTid = tid.toLowerCase().replace(/-/g, '_');

  if (TARGET_RECIPES[tid]) return TARGET_RECIPES[tid];
  if (TARGET_RECIPES[normTid]) return TARGET_RECIPES[normTid];

  // Resolve through contract family SSOT
  const famId = getTargetFamily(target);
  if (famId && CONTRACT_FAMILIES[famId]) {
    for (const modelKey of CONTRACT_FAMILIES[famId].models) {
      const normKey = modelKey.toLowerCase().replace(/-/g, '_');
      if (TARGET_RECIPES[normKey]) {
        return {
          ...TARGET_RECIPES[normKey],
          hardwareName: target.name || target.shortName || TARGET_RECIPES[normKey].hardwareName,
          hardwareImage: target.image || TARGET_RECIPES[normKey].hardwareImage
        };
      }
    }
  }

  return null;
}

/**
 * Helper to determine if target is an external analogue audio processor
 * (pedal, preamp, studio outboard) where open thermal noise loopback metrology is meaningful.
 */
export function isAnalogueAudioDevice(target) {
  if (!target) return false;
  if (target.isPlugin || target.midi) return false;
  const devType = getTargetDeviceType(target);
  if (devType === 'ANALOGUE_PEDAL' || devType === 'PREAMP' || devType === 'STUDIO_OUTBOARD' || devType === 'MANUAL_EURORACK') {
    return true;
  }
  const cat = `${target.category || ''} ${target.name || ''} ${target.shortName || ''}`.toLowerCase();
  if (cat.includes('pedal') || cat.includes('distortion') || cat.includes('overdrive') || cat.includes('preamp') || cat.includes('outboard')) {
    return true;
  }
  return false;
}

/**
 * Creates the initial Test Suite Queue based on the active target.
 * Pre-flight noise floor and signal path sanity checks are handled by
 * Check Signal Path, so the measurement suite contains exclusively the target batteries.
 */
export function createDefaultSuite(target) {
  const queue = [];

  // If no target is selected yet, return empty queue
  if (!target) {
    return queue;
  }

  // 1. ACTIVE PRIMARY TEST (Target-Aware!)
  // Only pre-populate the queue if the target has a certified factory recipe in catalog
  const primaryConfig = createStandardTestConfig(target);
  if (primaryConfig) {
    const queueItem = buildQueueItemFromConfig(primaryConfig);
    queueItem.id = 'test_1_primary_measurement';
    queueItem.title = `1. ${primaryConfig.testName}`;
    queue.push(queueItem);
  }

  return queue;
}

export function getRecommendedPresetForTarget(target) {
  if (!target) return MASTER_PRESETS[0];
  const cat = `${target.category || ''} ${target.name || ''} ${target.shortName || ''} ${target.deviceType || ''}`.toLowerCase();
  
  if (cat.includes('pedal') || cat.includes('distortion') || cat.includes('overdrive') || cat.includes('fuzz') || cat.includes('drive')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'pedal_overdrive_distortion') || MASTER_PRESETS[0];
  }
  if (cat.includes('eq') || cat.includes('equalizer')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'eq_parametric_graphic') || MASTER_PRESETS[0];
  }
  if (cat.includes('compressor') || cat.includes('limiter') || cat.includes('dynamics') || cat.includes('gate')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'compressor_limiter_gate') || MASTER_PRESETS[0];
  }
  if (cat.includes('modular') || cat.includes('eurorack') || cat.includes('aira')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'aira_01_ladder_lpf') || MASTER_PRESETS[0];
  }
  if (cat.includes('vca') || cat.includes('attenu')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'vca_attenuverter') || MASTER_PRESETS[0];
  }
  if (cat.includes('tape') || cat.includes('saturat')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'analog_tape_saturation') || MASTER_PRESETS[0];
  }
  if (cat.includes('cz') || cat.includes('casio')) {
    return MASTER_PRESETS.find(p => p.typologyId === 'cz_dcw_waveshaping') || MASTER_PRESETS[0];
  }
  return MASTER_PRESETS.find(p => p.typologyId === 'vcf_ladder_ota_svf') || MASTER_PRESETS[0];
}

export function createTestConfigFromPreset(preset, target) {
  const isManual = target ? !!(target.deviceType === 'ANALOGUE_PEDAL' || target.deviceType === 'MANUAL_EURORACK' || target.deviceType === 'MANUAL_SYNTH') : false;
  const controls = (preset.defaultControls || []).map(c => ({
    name: c.name,
    type: c.type || 'Knob',
    steps: c.steps || 4,
    minPct: c.minPct || 0,
    maxPct: c.maxPct || 100
  }));

  return {
    id: `test_${Date.now()}`,
    testName: preset.displayName,
    typologyId: preset.typologyId,
    badgeText: preset.badgeText || 'TEST',
    stimulusType: preset.stimulusType || 'LogFarinaSweep',
    burstDurationSec: preset.burstDurationSec || 1.0,
    captureMode: preset.captureMode || 'FIXED_TIME',
    isManual,
    isStandard: true,
    hardwareImage: target ? target.image : null,
    controls
  };
}

export function buildQueueItemFromConfig(testConfig) {
  const estimate = calculatePlanEstimate(testConfig.controls, testConfig.burstDurationSec, testConfig.isManual);
  const points = generateTestPoints(testConfig.controls, estimate.states);

  return {
    id: testConfig.id || `test_${Date.now()}`,
    badgeText: testConfig.badgeText || 'TEST',
    badgeColor: 'var(--color-accent)',
    title: testConfig.testName || 'Measurement Profile',
    description: `${testConfig.stimulusType || 'LogFarinaSweep'} battery across ${estimate.states} physical parameter states (${estimate.durationStr})`,
    stimulusType: testConfig.stimulusType || 'LogFarinaSweep',
    burstDurationSec: typeof testConfig.burstDurationSec === 'number' ? testConfig.burstDurationSec : 1.0,
    captureMode: testConfig.captureMode || 'FIXED_TIME',
    isPinned: false,
    isSkipped: false,
    isExpanded: true,
    status: 'QUEUED',
    currentRunningPoint: 0,
    totalPoints: estimate.states,
    controls: testConfig.controls || [],
    points,
    isManual: !!testConfig.isManual,
    isStandard: typeof testConfig.isStandard === 'boolean' ? testConfig.isStandard : true,
    hardwareImage: testConfig.hardwareImage || null
  };
}

export function createStandardTestConfig(target) {
  const recipe = getRecipeForTarget(target);
  if (!recipe) return null;

  const controls = recipe.controls.map((c, idx) => ({
    name: c.name,
    type: c.type || 'Knob',
    steps: idx === 0 ? 8 : (idx === 1 ? 4 : 1),
    minPct: c.minPct || 0,
    maxPct: c.maxPct || 100
  }));

  return {
    id: `test_${Date.now()}`,
    testName: recipe.recipeName,
    typologyId: (target && target.category) || 'Standard',
    badgeText: recipe.badgeText || 'STD',
    stimulusType: recipe.stimulusType || 'LogFarinaSweep',
    burstDurationSec: recipe.burstDurationSec || 1.0,
    captureMode: recipe.captureMode || 'FIXED_TIME',
    isManual: !!recipe.isManual,
    isStandard: true,
    hardwareImage: (target && target.image) || recipe.hardwareImage || null,
    controls
  };
}

export function createCustomTestConfig(target) {
  const isManual = target ? !!(target.deviceType === 'ANALOGUE_PEDAL' || target.deviceType === 'MANUAL_EURORACK' || target.deviceType === 'MANUAL_SYNTH') : false;

  return {
    id: `test_${Date.now()}`,
    testName: 'Custom Profile',
    typologyId: 'Custom',
    badgeText: 'CST',
    stimulusType: 'LogFarinaSweep',
    burstDurationSec: 1.0,
    captureMode: 'FIXED_TIME',
    isManual,
    isStandard: false,
    hardwareImage: target ? target.image : null,
    controls: [] // Starts completely blank from scratch (0 parameters)
  };
}
