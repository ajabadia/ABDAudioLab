import { StudioTopology } from '@components/studioTopology.js';
import { getAudioInterfaceImage } from './stage2_calibration.js';
import { showModalAlert } from '../components/SoundIdModal.js';

export function getThemeHardwareImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/audiohard_white.webp' : 'models/audiohard_black.webp';
}

export function getThemeVstImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/vst3_white.webp' : 'models/vst3_black.webp';
}

export function getThemeEurorackImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/eurorack2_white.webp' : 'models/eurorack2_black.webp';
}

export function getThemeDigitalSynthImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/digital_white.webp' : 'models/digital_black.webp';
}

export function getThemePedalImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/pedal_white.webp' : 'models/pedal_black.webp';
}

export function getThemeAnalogSynthImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/analog2_white.webp' : 'models/analog2_black.webp';
}

export function getThemeDrumMachineImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/drummachine_white.webp' : 'models/drummachine_black.webp';
}

export function getThemeDesktopModuleImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/desktop_white.webp' : 'models/desktop_black.webp';
}

export function getThemePreampImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/preamp_white.webp' : 'models/preamp_black.webp';
}

export function getThemeRackImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/rack_white.webp' : 'models/rack_black.webp';
}

export function getThemeCabinetImage() {
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  return isLight ? 'models/cabinet_white.webp' : 'models/cabinet_black.webp';
}

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

export function resolveThemeImage(imgPath) {
  if (!imgPath) return getThemePedalImage();
  if (imgPath.startsWith('data:') || imgPath.startsWith('blob:') || imgPath.startsWith('http:') || imgPath.startsWith('https:')) {
    return imgPath;
  }
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  if (isLight) {
    return imgPath.replace(/_black\.(webp|png|jpg)$/i, '_white.$1');
  } else {
    return imgPath.replace(/_white\.(webp|png|jpg)$/i, '_black.$1');
  }
}

export function resolveBrandLogo(logoPath) {
  if (!logoPath) return '';
  const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                  document.body?.getAttribute('data-theme') === 'audiolab-light';
  if (isLight) {
    // Modo claro: usa la versión en color (la actual)
    return logoPath.replace(/-white\.svg$/i, '.svg').replace(/-black\.svg$/i, '.svg');
  } else {
    // Modo oscuro: usa la versión en blanco monocromo
    return logoPath.replace(/-black\.svg$/i, '.svg').replace(/\.svg$/i, '-white.svg');
  }
}

export function getBlackBrandLogo(logoPath) {
  if (!logoPath) return '';
  return logoPath.replace(/-white\.svg$/i, '.svg').replace(/\.svg$/i, '-black.svg');
}

function buildTopologyData(targetInfo, audioDevice) {
  const devName = (audioDevice && audioDevice.deviceName) ? audioDevice.deviceName : 'Speakers (Realtek(R) Audio)';
  const sampleRate = (audioDevice && audioDevice.sampleRate) ? audioDevice.sampleRate : 48000;
  const bufSize = (audioDevice && audioDevice.bufferSize) ? audioDevice.bufferSize : 256;

  let audioDevImg = getAudioInterfaceImage(devName);
  let audioDevThemeClass = (audioDevImg.includes('auidomididev') || audioDevImg.includes('audiomididev') || audioDevImg.includes('nodevice')) ? 'theme-img-interface' : '';

  const devices = [
    {
      id: 'dev_0',
      name: devName,
      details: `${sampleRate} Hz • ${bufSize} spls (Audio Assigned)`,
      image: audioDevImg,
      themeClass: audioDevThemeClass,
      assigned: true,
      hasAudio: true,
      hasMidi: false,
      ports: [
        { id: 'audioOut_1', name: 'OUT 1 (Probe)', type: 'audioOut', connected: true },
        { id: 'audioIn_1', name: 'IN 1 (Return)', type: 'audioIn', connected: true }
      ]
    }
  ];

  const hasTarget = !!(targetInfo && targetInfo.name);

  const targetPorts = [
    { id: 'audioIn', name: 'Audio IN (Probe Stimulus Input)', type: 'audioIn', connected: true },
    { id: 'audioOut', name: 'Audio OUT (Synthesizer / Effect Output)', type: 'audioOut', connected: true },
    ...(targetInfo && targetInfo.midi ? [{ id: 'midiIn', name: 'MIDI IN (Automation Control)', type: 'midiIn', connected: true }] : [])
  ];

  const target = hasTarget ? {
    name: targetInfo.name,
    category: targetInfo.category,
    details: targetInfo.details || `Profile: ${targetInfo.name}`,
    image: targetInfo.image || 'models/placeholder-synth.svg',
    hasMidi: !!targetInfo.midi,
    isVirtual: !!(targetInfo.isPlugin || (targetInfo.category && targetInfo.category.toLowerCase().includes('plugin'))),
    ports: targetPorts
  } : {
    name: 'No Target Selected',
    category: 'Awaiting Hardware Selection',
    details: 'Select a profile from catalog or load a VST3 plugin',
    image: getThemeNoDeviceImage(),
    themeClass: 'theme-img-nodevice',
    hasMidi: false,
    isVirtual: false
  };

  const connections = [];
  if (hasTarget) {
    connections.push(
      { type: 'audioOut', from: 'dev_0', to: 'target', fromPort: 'audioOut_1', toPort: 'audioIn' },
      { type: 'audioIn', from: 'target', to: 'dev_0', fromPort: 'audioOut', toPort: 'audioIn_1' }
    );
  }

  const midiOutName = (audioDevice && Array.isArray(audioDevice.availableMidiOutputs) && audioDevice.availableMidiOutputs.length > 0)
    ? audioDevice.availableMidiOutputs[0]
    : 'Microsoft GS Wavetable Synth';

  if (hasTarget && target.hasMidi) {
    devices.push({
      id: 'dev_midi',
      name: midiOutName,
      details: 'MIDI Out Assigned',
      image: getThemeAudioInterfaceImage(),
      themeClass: 'theme-img-interface',
      assigned: true,
      hasAudio: false,
      hasMidi: true,
      ports: [
        { id: 'midiOut_1', name: 'MIDI Out', type: 'midiOut', connected: true }
      ]
    });
    connections.push({ type: 'midiOut', from: 'dev_midi', to: 'target', fromPort: 'midiOut_1', toPort: 'midiIn' });
  } else {
    devices.push({
      id: 'dev_midi',
      name: midiOutName,
      details: 'MIDI Idle (Not routed)',
      image: getThemeAudioInterfaceImage(),
      themeClass: 'theme-img-interface',
      assigned: false,
      hasAudio: false,
      hasMidi: true,
      ports: [
        { id: 'midiOut_1', name: 'MIDI Out', type: 'midiOut', connected: false }
      ]
    });
  }

  return {
    target,
    devices,
    connections
  };
}

// Catálogo de Marcas y Modelos de Fábrica con soporte de fotos y logos
const FACTORY_BRANDS = [
  {
    id: 'unlisted',
    name: 'Unlisted / Custom Hardware',
    isUnlisted: true
  },
  {
    id: 'Boss',
    name: 'BOSS',
    logo: 'brands/boss-logo.svg',
    models: [
      {
        id: 'boss_ds1',
        name: 'BOSS DS-1 Distortion',
        shortName: 'BOSS DS-1',
        category: 'Analogue Distortion Pedal',
        deviceType: 'ANALOGUE_PEDAL',
        image: 'models/boss-ds1.webp',
        midi: false,
        protocol: 'Manual Knobs (Tone / Level / Distortion)',
        routingNotes: 'DAC Out 1 -> DS-1 Input -> DS-1 Output -> ADC In 1',
        range: 'Instrument Line Level • Anti-parallel clipping diodes'
      }
    ]
  },
  {
    id: 'Roland',
    name: 'Roland',
    logo: 'brands/roland-logo.svg',
    models: [
      {
        id: 'roland_juno106',
        name: 'Roland Juno-106',
        shortName: 'Roland Juno-106',
        category: 'Analog Polyphonic Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/roland-juno-106.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx & CC (DCO, VCF, VCA, Chorus)',
        routingNotes: 'DAC Out 1 (L/R) -> Juno Stereo In/Out -> ADC In 1 (L/R)',
        range: 'IR3109 / 80017A VCF/VCA modules • Full MIDI CC implementation'
      },
      {
        id: 'roland_juno60',
        name: 'Roland Juno-60',
        shortName: 'Roland Juno-60',
        category: 'Analog Polyphonic Synthesizer',
        deviceType: 'MANUAL_SYNTH',
        image: 'models/roland-juno-60.webp',
        midi: false,
        protocol: 'Manual DCB / Panel Sliders',
        routingNotes: 'Mono/Stereo Audio Out -> ADC In 1/2',
        range: 'Discrete IR3109 VCF filters'
      },
      {
        id: 'roland_juno6',
        name: 'Roland Juno-6',
        shortName: 'Roland Juno-6',
        category: 'Analog Polyphonic Synthesizer',
        deviceType: 'MANUAL_SYNTH',
        image: 'models/roland-juno-6.webp',
        midi: false,
        protocol: 'Pure Analogue Panel Sliders',
        routingNotes: 'Mono/Stereo Audio Out -> ADC In 1/2',
        range: 'Discrete IR3109 VCF filters'
      },
      {
        id: 'roland_hs60',
        name: 'Roland HS-60 Synth Plus',
        shortName: 'Roland HS-60',
        category: 'Analog Polyphonic Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/roland-hs60.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx & CC',
        routingNotes: 'DAC Out 1 (L/R) -> HS-60 In/Out -> ADC In 1/2',
        range: 'Juno-106 internal engine with internal speakers'
      },
      {
        id: 'roland_aira_modular',
        name: 'Roland AIRA Modular (Torcido / Bitrazer / Demora / Scooper)',
        shortName: 'Roland AIRA Modular',
        category: 'Eurorack DSP Module',
        deviceType: 'MANUAL_EURORACK',
        image: 'models/roland-airamodular.webp',
        midi: true,
        protocol: 'Eurorack ±10V CV / USB Audio 24-bit 96kHz',
        routingNotes: 'DAC Out 1 -> Modular Audio In -> Module Out -> ADC In 1',
        range: 'Eurorack ±10V CV / 96kHz Audio • Reconfigurable DSP submodules'
      }
    ]
  },
  {
    id: 'Korg',
    name: 'Korg',
    logo: 'brands/korg-logo.svg',
    models: [
      {
        id: 'korg_ms2000',
        name: 'Korg MS2000 / MS2000R',
        shortName: 'Korg MS2000',
        category: 'Virtual Analog Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/korg-ms2000.webp',
        midi: true,
        protocol: 'Automated MIDI CC & NRPN',
        routingNotes: 'DAC Out 1 -> MS2000 Audio In -> Audio Out -> ADC In 1',
        range: 'Stereo Line Level • DSP modelled filters: LPF, BPF, HPF'
      },
      {
        id: 'korg_microkorg',
        name: 'Korg microKORG',
        shortName: 'Korg microKORG',
        category: 'Virtual Analog Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/korg-microkorg.webp',
        midi: true,
        protocol: 'Automated MIDI CC',
        routingNotes: 'DAC Out 1 -> microKORG Audio In -> Audio Out -> ADC In 1',
        range: 'MS2000 DSP Core • Full MIDI CC implementation'
      },
      {
        id: 'korg_prophecy',
        name: 'Korg Prophecy',
        shortName: 'Korg Prophecy',
        category: 'MOSS Monophonic Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/korg-prophecy.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx & CC',
        routingNotes: 'DAC Out 1 -> Audio In -> Audio Out -> ADC In 1',
        range: 'MOSS Physical Modeling Synthesis'
      }
    ]
  },
  {
    id: 'Behringer',
    name: 'Behringer',
    logo: 'brands/behringer-logo.svg',
    models: [
      {
        id: 'behringer_deepmind12',
        name: 'Behringer DeepMind 12 / 6',
        shortName: 'Behringer DeepMind 12',
        category: 'Analog Polyphonic Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/behringer-deepmind12.webp',
        midi: true,
        protocol: 'Automated MIDI CC Matrix',
        routingNotes: 'Balanced +4 dBu Line In -> Synth -> Audio Out -> ADC In 1/2',
        range: 'Balanced +4 dBu Line Level • Complete Mod Matrix CC Mapping'
      },
      {
        id: 'behringer_pro800',
        name: 'Behringer PRO-800',
        shortName: 'Behringer PRO-800',
        category: 'Desktop Analog Module',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/behringer-pro800.webp',
        midi: true,
        protocol: 'Automated MIDI CC & SysEx',
        routingNotes: 'Audio Out -> ADC In 1/2 • MIDI Out -> PRO-800 MIDI In',
        range: 'Curtis CEM3320 Clone Analogue VCF • Prophet-600 architecture'
      },
      {
        id: 'behringer_provs_mini',
        name: 'Behringer PRO-VS Mini',
        shortName: 'Behringer PRO-VS Mini',
        category: 'Hybrid Vector Synthesizer',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/behringer-provs-mini.webp',
        midi: true,
        protocol: 'Automated MIDI CC (Joystick & Filter)',
        routingNotes: 'Headphone/Line Out -> ADC In 1',
        range: 'Vector synthesis with analogue low-pass filter'
      },
      {
        id: 'behringer_jt4000',
        name: 'Behringer JT-4000 Micro',
        shortName: 'Behringer JT-4000',
        category: 'Portable Micro Synth',
        deviceType: 'AUTOMATED_MIDI_CC',
        image: 'models/behringer-jt4000.webp',
        midi: true,
        protocol: 'USB MIDI CC Automation',
        routingNotes: 'Headphone Out -> ADC In 1',
        range: 'Virtual analog supersaw engine with analogue VCF'
      },
      {
        id: 'behringer_modular_140',
        name: 'Behringer System 100 Modular 140',
        shortName: 'Behringer System 100 140',
        category: 'Eurorack Dual Envelope/LFO',
        deviceType: 'MANUAL_EURORACK',
        image: 'models/behringer-modular-140.webp',
        midi: false,
        protocol: 'Manual Potentiometers & Gate In (±10V)',
        routingNotes: 'Gate Pulse Out -> Gate In -> Env Out -> ADC In 1',
        range: 'Eurorack ±10V Modular Levels'
      }
    ]
  },
  {
    id: 'Casio',
    name: 'Casio',
    logo: 'brands/casio-logo.svg',
    models: [
      {
        id: 'casio_cz101',
        name: 'Casio CZ-101',
        shortName: 'Casio CZ-101',
        category: 'Phase Distortion Synthesizer',
        deviceType: 'AUTOMATED_SYSEX',
        image: 'models/casio-cz101.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx Parameter Spec',
        routingNotes: 'Line Out -> ADC In 1 • MIDI Out -> CZ-101 MIDI In',
        range: 'Phase Distortion (DCW) non-linear waveform shaper'
      },
      {
        id: 'casio_cz1000',
        name: 'Casio CZ-1000',
        shortName: 'Casio CZ-1000',
        category: 'Phase Distortion Synthesizer',
        deviceType: 'AUTOMATED_SYSEX',
        image: 'models/casio-cz1000.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx Parameter Spec',
        routingNotes: 'Line Out -> ADC In 1 • MIDI Out -> CZ-1000 MIDI In',
        range: 'Full-size Phase Distortion synthesizer engine'
      },
      {
        id: 'casio_cz5000',
        name: 'Casio CZ-5000',
        shortName: 'Casio CZ-5000',
        category: 'Phase Distortion Synthesizer',
        deviceType: 'AUTOMATED_SYSEX',
        image: 'models/casio-cz5000.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx & Built-in Sequencer',
        routingNotes: 'Stereo Line Out -> ADC In 1/2 • MIDI Out -> CZ-5000 MIDI In',
        range: 'Dual DCO Phase Distortion with onboard chorus'
      },
      {
        id: 'casio_cz1',
        name: 'Casio CZ-1',
        shortName: 'Casio CZ-1',
        category: 'Phase Distortion Synthesizer',
        deviceType: 'AUTOMATED_SYSEX',
        image: 'models/casio-cz1.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx (Velocity & Aftertouch)',
        routingNotes: 'Stereo Line Out -> ADC In 1/2 • MIDI Out -> CZ-1 MIDI In',
        range: 'Flagship Phase Distortion synthesizer'
      }
    ]
  },
  {
    id: 'Yamaha',
    name: 'Yamaha',
    logo: 'brands/yamaha-logo.svg',
    models: [
      {
        id: 'yamaha_dx7',
        name: 'Yamaha DX7 / DX7II',
        shortName: 'Yamaha DX7',
        category: 'FM Digital Synthesizer',
        deviceType: 'AUTOMATED_SYSEX',
        image: 'models/yamaha-dx7.webp',
        midi: true,
        protocol: 'Automated MIDI SysEx Bulk Dump & Parameter Change',
        routingNotes: 'DAC Out 1 -> Audio In (optional) • Line Out -> ADC In 1 • MIDI Out -> DX7 In',
        range: '6-Operator FM Synthesis • 32 Algorithms • 12-bit DAC non-linearities'
      }
    ]
  }
];

const UNLISTED_TYPOLOGIES = [
  {
    id: 'pedal',
    name: 'Guitar Pedal / Stompbox',
    deviceType: 'ANALOGUE_PEDAL',
    image: 'models/pedal_black.webp',
    details: 'Analogue distortion, overdrive, fuzz, compressor or chorus pedal'
  },
  {
    id: 'analog_synth',
    name: 'Analogue Synthesizer',
    deviceType: 'ANALOGUE_SYNTH',
    image: 'models/analog2_black.webp',
    details: 'Monophonic or polyphonic hardware synth with audio outputs'
  },
  {
    id: 'digital_synth',
    name: 'Digital Synthesizer',
    deviceType: 'DIGITAL_SYNTH',
    image: 'models/digital_black.webp',
    details: 'Digital, FM or Phase Distortion hardware keyboard synthesizer'
  },
  {
    id: 'modular',
    name: 'Eurorack Modular Module',
    deviceType: 'MANUAL_EURORACK',
    image: 'models/eurorack2_black.webp',
    details: 'Modular VCF, VCA, waveshaper or DSP Eurorack module (±10V)'
  },
  {
    id: 'rack',
    name: 'Studio Outboard / Rack (19")',
    deviceType: 'STUDIO_OUTBOARD',
    image: 'models/rack_black.webp',
    details: '19-inch rack compressor, parametric EQ, or channel strip'
  },
  {
    id: 'preamp',
    name: 'Microphone Preamp',
    deviceType: 'PREAMP',
    image: 'models/preamp_black.webp',
    details: 'Discrete transformer or tube microphone preamplifier'
  },
  {
    id: 'desktop',
    name: 'Desktop Audio Module',
    deviceType: 'DESKTOP_MODULE',
    image: 'models/desktop_black.webp',
    details: 'Desktop synthesizer, sound expander or tabletop unit'
  },
  {
    id: 'drum_machine',
    name: 'Drum Machine / Rhythm Unit',
    deviceType: 'DRUM_MACHINE',
    image: 'models/drummachine_black.webp',
    details: 'Analogue or sample-based drum synthesizer / groovebox'
  },
  {
    id: 'cabinet',
    name: 'Speaker Cabinet',
    deviceType: 'CABINET',
    image: 'models/cabinet_black.webp',
    details: 'Guitar/bass speaker cabinet, load box or acoustic monitor'
  }
];

export function getTargetImageByIdOrName(targetId, targetName) {
  if (!targetId && !targetName) return '';
  for (const brand of FACTORY_BRANDS) {
    if (brand.models) {
      const found = brand.models.find(m => m.id === targetId || m.name === targetName);
      if (found) return found.image;
    }
  }
  for (const typo of UNLISTED_TYPOLOGIES) {
    if (typo.id === targetId || typo.deviceType === targetId) {
      return resolveThemeImage(typo.image);
    }
  }
  return '';
}

export function renderStage1(container, state, onUpdate) {
  // Determine if target is currently locked (fossilized)
  let isTargetLocked = !!(state.selectedTarget && state.selectedTarget.isLocked);

  // Progressive selection state
  let selectedRootType = null; // 'PHYSICAL' | 'VST3'
  let selectedBrandId = null;  // null | 'unlisted' | 'Boss' | 'Roland' ...
  let selectedModelId = null;  // null | model ID
  let candidateTarget = state.selectedTarget || null;
  let currentTopologyInstance = null;

  // View modes (grid vs list) with local persistence
  let brandViewMode = 'grid';
  let modelViewMode = 'grid';
  let pluginViewMode = 'grid';
  try {
    brandViewMode = localStorage.getItem('abdaudiolab_brand_view_mode') || 'grid';
    modelViewMode = localStorage.getItem('abdaudiolab_model_view_mode') || 'grid';
    pluginViewMode = localStorage.getItem('abdaudiolab_vst_view_mode') || 'grid';
  } catch (_) {}

  function resolvePluginCover(p) {
    if (!p) return resolveThemeImage(getThemeVstImage());
    const isGeneric = (src) => !src || src.includes('generic-vst3') || src.includes('vst3_');

    // 1. Snapshot Data URL enviado directamente desde C++ en availablePlugins o en p
    if (p.snapshotUrl && !isGeneric(p.snapshotUrl)) return p.snapshotUrl;

    // 2. Si p tiene una imagen ya asignada que no es genérica
    if (p.image && !isGeneric(p.image)) return resolveThemeImage(p.image);
    if (p.customImage && !isGeneric(p.customImage)) return resolveThemeImage(p.customImage);

    // 3. Persistencia en localStorage
    try {
      const coversStr = localStorage.getItem('abdaudiolab_vst_covers');
      if (coversStr) {
        const covers = JSON.parse(coversStr);
        const k1 = p.name;
        const k2 = p.fileName;
        const k3 = p.fileOrIdentifier;
        const k4 = String(p.uniqueId || p.pluginId || '');
        if (k1 && covers[k1] && !isGeneric(covers[k1])) return covers[k1];
        if (k2 && covers[k2] && !isGeneric(covers[k2])) return covers[k2];
        if (k3 && covers[k3] && !isGeneric(covers[k3])) return covers[k3];
        if (k4 && covers[k4] && !isGeneric(covers[k4])) return covers[k4];
      }
    } catch (_) {}

    // 4. Si está en state.availablePlugins con snapshotUrl
    if (state.availablePlugins && Array.isArray(state.availablePlugins)) {
      const match = state.availablePlugins.find(item => 
        (item.name && p.name && item.name.toLowerCase() === p.name.toLowerCase()) ||
        (item.uniqueId && p.uniqueId && String(item.uniqueId) === String(p.uniqueId)) ||
        (item.fileOrIdentifier && p.fileOrIdentifier && item.fileOrIdentifier.toLowerCase() === p.fileOrIdentifier.toLowerCase())
      );
      if (match && match.snapshotUrl && !isGeneric(match.snapshotUrl)) return match.snapshotUrl;
    }

    // 5. Si es el candidateTarget o state.selectedTarget y tiene imagen asignada no genérica
    if (candidateTarget && candidateTarget.isPlugin && candidateTarget.image && !isGeneric(candidateTarget.image)) {
      return resolveThemeImage(candidateTarget.image);
    }
    if (state.selectedTarget && state.selectedTarget.isPlugin && state.selectedTarget.image && !isGeneric(state.selectedTarget.image)) {
      return resolveThemeImage(state.selectedTarget.image);
    }

    // 6. Fallback temático
    return resolveThemeImage(getThemeVstImage());
  }

  // Unlisted device customized fields
  let unlistedCustomName = 'Custom Hardware Device';
  let unlistedTypologyId = null;
  let unlistedHasMidi = false;
  let unlistedCustomImage = null;

  function notifyCandidateChanged(target) {
    if (typeof onUpdate === 'function') {
      onUpdate('targetCandidateChanged', target);
    }
  }

  function setCandidateTarget(target) {
    candidateTarget = target;
    if (candidateTarget) {
      candidateTarget.isLocked = false;
    }
    state.selectedTarget = candidateTarget;
    // Reset previously cached Stage 3 suiteQueue so the newly selected target gets a fresh suite
    window.__ABDAudioLab_suiteQueue__ = null;
    window.__ABDAudioLab_suiteTargetId__ = target ? target.id : null;
    window.__ABDAudioLab_activeTestIndex__ = 1;
    window.__ABDAudioLab_selectedObjective__ = null;
    notifyCandidateChanged(candidateTarget);
  }

  if (state.selectedTarget) {
    if (state.selectedTarget.isPlugin) {
      selectedRootType = 'VST3';
    } else {
      selectedRootType = 'PHYSICAL';
      const foundBrand = FACTORY_BRANDS.find(b => b.models && b.models.some(m => m.id === state.selectedTarget.id));
      if (foundBrand) {
        selectedBrandId = foundBrand.id;
        selectedModelId = state.selectedTarget.id;
      } else {
        selectedBrandId = 'unlisted';
        selectedModelId = state.selectedTarget.id;
        unlistedCustomName = state.selectedTarget.name || 'Custom Hardware Device';
        const foundTypo = UNLISTED_TYPOLOGIES.find(t => t.deviceType === state.selectedTarget.deviceType);
        if (foundTypo) unlistedTypologyId = foundTypo.id;
      }
    }
  }

  function render() {
    container.innerHTML = `
      <div class="stage-header" style="display:flex; justify-content:space-between; align-items:flex-start; flex-wrap:wrap; gap:12px; flex-shrink:0;">
        <div>
          <h2>Step 1: Target Selection &amp; Routing Topology</h2>
          <p>Select what you want to measure: physical hardware gear or virtual audio plugins.</p>
        </div>

        ${isTargetLocked ? `
          <div style="display:flex; align-items:center; gap:8px;">
            <span class="status-badge status-active" style="padding:4px 10px; font-size:10px; font-weight:800; letter-spacing:0.5px; text-transform:uppercase;">
              LOCKED TARGET
            </span>
            <button id="btnUnlockTarget" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-warning); border-color:rgba(255,177,66,0.4);">
              Change Target...
            </button>
          </div>
        ` : ''}
      </div>

      <div class="panel-row stage1-grid">
        
        <!-- LEFT COLUMN: Progressive Target Selector or Locked Card -->
        <div class="module stage1-left-module" style="display:flex; flex-direction:column; gap:14px; min-width:0;">
          
          ${isTargetLocked ? renderLockedFossilView() : renderSelectionFlowView()}

        </div>

        <!-- RIGHT COLUMN: Interactive Studio Wiring Topology Graph -->
        <div class="module stage1-right-module" style="display:flex; flex-direction:column; min-width:0; height:100%; flex:1;">
          <div class="module-header" style="display:flex; justify-content:space-between; align-items:center;">
            <span>Interactive Studio Wiring Topology</span>
            <span style="font-size:10px; font-weight:600; color:var(--color-text-dim);">Live I/O Routing</span>
          </div>

          <div id="studioTopologyContainer" style="flex:1; display:flex; flex-direction:column; min-height:480px; height:100%; width:100%; position:relative;"></div>
        </div>

      </div>
    `;

    // Attach Event Listeners FIRST so UI is always fully interactive
    attachEvents();

    // Render Topology Graph safely
    try {
      const topoMount = container.querySelector('#studioTopologyContainer');
      if (topoMount) {
        const topoData = buildTopologyData(isTargetLocked ? state.selectedTarget : candidateTarget, state.audioDevice);
        if (currentTopologyInstance) {
          currentTopologyInstance.destroy();
          currentTopologyInstance = null;
        }
        currentTopologyInstance = new StudioTopology(topoMount, topoData);
      }
    } catch (err) {
      console.warn('[Stage1] StudioTopology render warning:', err);
    }
  }

  // --- RENDER: LOCKED FOSSIL VIEW ---
  function renderLockedFossilView() {
    const target = state.selectedTarget || candidateTarget;
    return `
      <div class="locked-target-container" style="flex:1; display:flex; flex-direction:column; justify-content:space-between; height:100%; box-sizing:border-box;">
        <div style="display:flex; justify-content:space-between; align-items:flex-start;">
          <div>
            <div style="font-size:9.5px; font-weight:700; color:var(--color-accent); text-transform:uppercase; letter-spacing:0.8px;">
              ACTIVE TARGET (SEALED)
            </div>
            <div style="font-size:18px; font-weight:800; color:var(--color-text-main); margin-top:2px;">
              ${target.name || 'Hardware Target'}
            </div>
            <div style="font-size:11px; color:var(--color-text-dim); margin-top:2px;">
              ${target.category || 'Audio Device'} • ${target.shortName || target.name}
            </div>
          </div>

          <span class="status-badge status-active" style="padding:4px 10px; font-size:10px; font-weight:800; border-radius:4px;">
            CERTIFIED
          </span>
        </div>

        <div class="fossil-specs-card">
          <div class="fossil-thumb-card">
            <img src="${target.isPlugin ? resolvePluginCover(target) : resolveThemeImage(target.image)}" alt="${target.name}" style="max-width:100%; max-height:100%; object-fit:contain;" />
          </div>

          <div style="display:flex; flex-direction:column; gap:4px; font-size:11px; flex:1;">
            <div><strong style="color:var(--color-text-dim);">Device Type:</strong> <span style="color:var(--color-accent); font-weight:700;">${target.deviceType || (target.isPlugin ? 'VIRTUAL_PLUGIN' : 'ANALOGUE_HARDWARE')}</span></div>
            <div><strong style="color:var(--color-text-dim);">Control Protocol:</strong> <span style="color:var(--color-text-main);">${target.protocol || (target.isPlugin ? (target.midi ? 'VST3 Parameter & MIDI Automation' : 'VST3 Parameter Automation') : (target.midi ? 'MIDI CC / SysEx Automation' : 'Manual Potentiometer Adjustment'))}</span></div>
            <div><strong style="color:var(--color-text-dim);">Audio Routing:</strong> <span style="color:var(--color-text-main);">${target.routingNotes || (target.isPlugin ? 'DAW / Test Host Internal Bus (Zero Latency Direct I/O)' : 'Probe Out 1 -> Audio In 1')}</span></div>
            <div><strong style="color:var(--color-text-dim);">Session State:</strong> <span style="color:var(--color-text-dim);">Inmutable • Ready to Save (.abdsess)</span></div>
          </div>
        </div>

        <!-- Profile Contract Bar (Locked) -->
        <div style="padding:10px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
          <div>
            <div style="font-size:10.5px; font-weight:700; color:var(--color-text-main);">Equipment Profile Contract (.json)</div>
            <div style="font-size:9.5px; color:var(--color-text-dim);">Declarative parameter specification &amp; I/O routing configuration</div>
          </div>
          <div style="display:flex; gap:6px; align-items:center; flex-wrap:wrap;">
            <input type="file" id="fileLockedContract" accept=".json" style="display:none;" />
            <button id="btnImportLockedContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10.5px; font-weight:700; padding:4px 10px;" title="Import a JSON hardware or plugin profile contract">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
              <span>Import Contract (.json)</span>
            </button>
            <button id="btnExportLockedContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10.5px; font-weight:700; padding:4px 10px;" title="Generate and download a sample JSON contract template">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
              <span>Generate Sample (.json)</span>
            </button>
            <button id="btnDownloadSpecLocked" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10.5px; font-weight:700; padding:4px 10px;" title="Download technical specifications and JSON templates (.md)">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
              <span>Spec (.md)</span>
            </button>
          </div>
        </div>

        ${target.isPlugin ? `
          <!-- VST3 Locked Controls: GUI, Image Import, Snapshot & Audition -->
          <div style="padding:12px 14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); display:flex; flex-direction:column; gap:8px;">
            <div style="font-size:10px; font-weight:700; color:var(--color-accent); text-transform:uppercase; letter-spacing:0.6px; display:flex; justify-content:space-between; align-items:center;">
              <span>VST3 PLUGIN HOST ACTIONS</span>
              <span class="badge badge-success badge-xs">${state.isPluginWindowOpen ? 'GUI WINDOW OPEN' : 'PLUGIN ACTIVE'}</span>
            </div>
            <div style="display:flex; gap:8px; flex-wrap:wrap;">
              <button id="btnTogglePluginGuiLocked" type="button" class="btn btn-primary btn-sm" style="flex:1; min-width:130px; font-size:11px; font-weight:700; display:flex; align-items:center; justify-content:center; gap:6px;">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="9" y1="21" x2="9" y2="9"/></svg>
                <span>${state.isPluginWindowOpen ? 'Close VST GUI' : 'Open VST GUI'}</span>
              </button>

              <input type="file" id="fileVstCoverLocked" accept="image/png,image/jpeg,image/webp" style="display:none;" />
              <button id="btnImportVstCoverLocked" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:700; display:flex; align-items:center; gap:5px;" title="Import screenshot or cover photo for this plugin">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>
                <span>Import Cover Image...</span>
              </button>

              <button id="btnCaptureVstImageLocked" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:700; display:flex; align-items:center; gap:5px;" title="Capture snapshot from VST window">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"></path><circle cx="12" cy="13" r="4"></circle></svg>
                <span>Capture Snapshot</span>
              </button>
            </div>

            <div style="display:flex; justify-content:space-between; align-items:center; padding-top:6px; border-top:1px solid var(--color-panel-border);">
              <span style="font-size:10.5px; color:var(--color-text-dim);">Audition sound via MIDI keyboard:</span>
              <button id="btnOpenVirtualKeyboardLocked" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-accent); border-color:var(--color-accent);">
                🎹 Open MIDI Keyboard (Ctrl+K)
              </button>
            </div>
          </div>
        ` : ''}

        <div style="display:flex; gap:10px; align-items:center;">
          <button id="btnSaveSession" type="button" class="btn btn-outline" style="display:flex; align-items:center; justify-content:center; gap:8px; padding:9px 16px; font-weight:700; font-size:11.5px; border-radius:6px; flex:1;">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path>
              <polyline points="17 21 17 13 7 13 7 21"></polyline>
              <polyline points="7 3 7 8 15 8"></polyline>
            </svg>
            <span>Save Session (.abdsess)</span>
          </button>

          <button id="btnSaveSessionAs" type="button" class="btn btn-outline" style="padding:9px 14px; font-weight:700; font-size:11.5px; border-radius:6px;">
            Save As...
          </button>
        </div>

        <div style="padding:10px 12px; background:rgba(0, 230, 118, 0.08); border:1px solid rgba(0, 230, 118, 0.2); border-radius:6px; font-size:11px; line-height:1.4; color:var(--color-text-main);">
          The target device is locked into the session context. To measure a different physical device or VST3, start a new session from the File menu.
        </div>
      </div>
    `;
  }

  // --- RENDER: PROGRESSIVE SELECTION FLOW ---
  function renderSelectionFlowView() {
    return `
      <!-- LEVEL 1: Root Choice (Physical vs VST3) -->
      ${!selectedRootType ? `
        <div style="flex:1; display:flex; flex-direction:column; min-height:0;">
          <div style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px; margin-bottom:8px; flex-shrink:0;">
            1. Choose What You Want to Measure
          </div>

          <div class="target-root-grid">
            <div id="choicePhysical" class="target-type-choice">
              <div class="target-type-thumb">
                <img src="${getThemeHardwareImage()}" alt="Physical Hardware" class="target-choice-img theme-img-hard" />
              </div>
              <div>
                <div style="font-weight:800; font-size:13px; color:var(--color-text-main);">PHYSICAL HARDWARE</div>
                <div style="font-size:10.5px; color:var(--color-text-dim); margin-top:4px; line-height:1.35;">
                  Analogue pedals, synthesizers, Eurorack modules, outboard gear &amp; unlisted devices.
                </div>
              </div>
            </div>

            <div id="choiceVst3" class="target-type-choice">
              <div class="target-type-thumb">
                <img src="${getThemeVstImage()}" alt="VST3 Plugin" class="target-choice-img theme-img-vst" />
              </div>
              <div>
                <div style="font-weight:800; font-size:13px; color:var(--color-text-main);">VST3 PLUGIN</div>
                <div style="font-size:10.5px; color:var(--color-text-dim); margin-top:4px; line-height:1.35;">
                  Virtual studio instruments and audio effect plugins installed on this computer.
                </div>
              </div>
            </div>
          </div>
        </div>
      ` : `
        <!-- LEVEL 1 RETRACTED: Becomes a Single-Line Title Header -->
        <div class="target-type-header-row">
          <div class="target-type-header-left">
            <img src="${selectedRootType === 'PHYSICAL' ? getThemeHardwareImage() : getThemeVstImage()}" alt="${selectedRootType}" class="target-type-header-icon ${selectedRootType === 'PHYSICAL' ? 'theme-img-hard' : 'theme-img-vst'}" />
            <div class="target-type-header-info">
              <div class="target-type-header-badge">TARGET TYPE</div>
              <div class="target-type-header-title">
                ${selectedRootType === 'PHYSICAL' ? 'PHYSICAL HARDWARE' : 'VST3 PLUGIN'}
              </div>
            </div>
          </div>
          <button id="btnChangeTargetType" type="button" class="btn btn-sm btn-outline target-change-btn" title="Change Target Type">
            <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;">
              <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
              <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
            </svg>
            <span>Change Type</span>
          </button>
        </div>
      `}

      <!-- LEVEL 2: Physical Hardware Brand Selection -->
      ${selectedRootType === 'PHYSICAL' ? `
        ${!selectedBrandId ? `
          <div style="display:flex; flex-direction:column; gap:8px; margin-top:12px; flex:1; min-height:0;">
            <div style="display:flex; justify-content:space-between; align-items:center; flex-shrink:0;">
              <div style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">
                2. Select Manufacturer / Brand
              </div>
              <div style="display:flex; align-items:center; gap:8px;">
                <button id="btnStage1AutoDetectMidi" type="button" class="btn btn-sm btn-outline" style="border-color:var(--color-accent); color:var(--color-accent); font-size:10.5px; padding:3px 10px; font-weight:700; display:flex; align-items:center; gap:4px;" title="Query connected MIDI/USB ports via ABDShared SysEx Identity Inquiry to auto-detect hardware">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                    <polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon>
                  </svg>
                  Auto-Detect MIDI Device
                </button>
                <div class="view-mode-toggle-group">
                  <button id="btnViewBrandGrid" type="button" class="view-mode-btn ${brandViewMode === 'grid' ? 'active' : ''}" title="Vista Rejilla (Grid View)">
                    <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                      <rect x="3" y="3" width="7" height="7"></rect>
                      <rect x="14" y="3" width="7" height="7"></rect>
                      <rect x="14" y="14" width="7" height="7"></rect>
                      <rect x="3" y="14" width="7" height="7"></rect>
                    </svg>
                  </button>
                  <button id="btnViewBrandList" type="button" class="view-mode-btn ${brandViewMode === 'list' ? 'active' : ''}" title="Vista Lista (List View)">
                    <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                      <line x1="8" y1="6" x2="21" y2="6"></line>
                      <line x1="8" y1="12" x2="21" y2="12"></line>
                      <line x1="8" y1="18" x2="21" y2="18"></line>
                      <line x1="3" y1="6" x2="3.01" y2="6"></line>
                      <line x1="3" y1="12" x2="3.01" y2="12"></line>
                      <line x1="3" y1="18" x2="3.01" y2="18"></line>
                    </svg>
                  </button>
                </div>
              </div>
            </div>

            <div class="brands-scroll-container">
              ${brandViewMode === 'grid' ? `
                <div class="brand-cards-grid">
                  <!-- 1st Option: Unlisted / Custom Hardware -->
                  <div data-brand="unlisted" class="brand-card-item brand-unlisted-card" title="Unlisted / Custom Hardware">
                    <div style="font-size:24px; font-weight:800; color:var(--color-info); line-height:1;">+</div>
                    <div style="font-size:11px; font-weight:700; color:var(--color-text-main); margin-top:4px;">Unlisted Device</div>
                    <span class="brand-tooltip">Custom / DIY Hardware</span>
                  </div>

                  <!-- Factory Brands: Logo ONLY, name in popup tooltip -->
                  ${FACTORY_BRANDS.filter(b => !b.isUnlisted).map(b => `
                    <div data-brand="${b.id}" class="brand-card-item" title="${b.name}">
                      <img src="${resolveBrandLogo(b.logo)}" alt="${b.name}" class="brand-logo-img" />
                      <span class="brand-tooltip">${b.name}</span>
                    </div>
                  `).join('')}
                </div>
              ` : `
                <div class="brand-list-view">
                  <!-- 1st Option: Unlisted / Custom Hardware -->
                  <div data-brand="unlisted" class="brand-list-item brand-unlisted-list-item" title="Unlisted / Custom Hardware">
                    <div class="brand-list-logo-box brand-unlisted-logo-box">
                      <span style="font-size:20px; font-weight:800; color:var(--color-info); line-height:1;">+</span>
                    </div>
                    <div class="brand-list-info">
                      <div class="brand-list-name">Unlisted / Custom Hardware</div>
                      <div class="brand-list-meta">Define custom device parameters or import contract profile</div>
                    </div>
                    <div class="brand-list-action">
                      <span class="brand-list-count">Custom</span>
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="brand-list-arrow"><polyline points="9 18 15 12 9 6"></polyline></svg>
                    </div>
                  </div>

                  ${FACTORY_BRANDS.filter(b => !b.isUnlisted).map(b => `
                    <div data-brand="${b.id}" class="brand-list-item" title="${b.name}">
                      <div class="brand-list-logo-box">
                        <img src="${resolveBrandLogo(b.logo)}" alt="${b.name}" class="brand-list-logo-img" />
                      </div>
                      <div class="brand-list-info">
                        <div class="brand-list-name">${b.name}</div>
                        <div class="brand-list-meta">${(b.models && b.models.length) ? `${b.models.length} model${b.models.length > 1 ? 's' : ''} available` : 'Hardware catalog'}</div>
                      </div>
                      <div class="brand-list-action">
                        <span class="brand-list-count">${b.models ? `${b.models.length} models` : ''}</span>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="brand-list-arrow"><polyline points="9 18 15 12 9 6"></polyline></svg>
                      </div>
                    </div>
                  `).join('')}
                </div>
              `}
            </div>
          </div>
        ` : `
          <!-- LEVEL 2 RETRACTED: Subtitle Row 2 (Brand) -->
          <div class="target-type-header-row" style="margin-top:8px;">
            <div class="target-type-header-left">
              ${(() => {
                const b = FACTORY_BRANDS.find(br => br.id === selectedBrandId);
                if (b && b.logo) {
                  return `<img src="${resolveBrandLogo(b.logo)}" alt="${b.name}" class="target-type-header-icon brand-logo-icon" />`;
                }
                return `<span style="font-size:16px; font-weight:800; color:var(--color-info); width:28px; text-align:center;">+</span>`;
              })()}
              <div class="target-type-header-info">
                <div class="target-type-header-badge">MANUFACTURER</div>
                <div class="target-type-header-title">
                  ${(() => {
                    const b = FACTORY_BRANDS.find(br => br.id === selectedBrandId);
                    return b ? b.name : 'Unlisted / Custom Hardware';
                  })()}
                </div>
              </div>
            </div>
            <button id="btnChangeBrand" type="button" class="btn btn-sm btn-outline target-change-btn" title="Change Manufacturer / Brand">
              <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;">
                <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
                <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
              </svg>
              <span>Change Brand</span>
            </button>
          </div>

          <!-- LEVEL 3: Model Selection or Unlisted Configurator -->
          ${!selectedModelId ? `
            <div style="margin-top:10px; flex:1; min-height:0; display:flex; flex-direction:column;">
              ${selectedBrandId === 'unlisted' ? renderUnlistedConfigurator() : renderFactoryModelsGrid()}
            </div>
          ` : `
            <!-- LEVEL 3 RETRACTED: Subtitle Row 3 (Model) -->
            <div class="target-type-header-row" style="margin-top:8px;">
              <div class="target-type-header-left">
                <img src="${resolveThemeImage(candidateTarget && candidateTarget.image)}" alt="${candidateTarget ? candidateTarget.name : 'Model'}" class="target-type-header-icon model-thumb-icon" />
                <div class="target-type-header-info">
                  <div class="target-type-header-badge">HARDWARE MODEL</div>
                  <div class="target-type-header-title">
                    ${candidateTarget ? candidateTarget.name : 'Selected Model'}
                  </div>
                </div>
              </div>
              <button id="btnChangeModel" type="button" class="btn btn-sm btn-outline target-change-btn" title="Change Hardware Model">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;">
                  <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
                  <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
                </svg>
                <span>Change Model</span>
              </button>
            </div>
          `}
        `}
      ` : ''}

      <!-- LEVEL 2B: VST3 Plugins List -->
      ${selectedRootType === 'VST3' ? renderVst3Browser() : ''}

      <!-- LEVEL 4: Candidate Target Preview & Lock Button -->
      ${(selectedRootType === 'VST3' || (selectedRootType === 'PHYSICAL' && selectedModelId)) && candidateTarget ? `
        <div class="stage1-confirm-card" style="display:flex; flex-direction:column; gap:10px; margin-top:auto; padding:14px; background:var(--color-panel-surface); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); box-sizing:border-box;">
          <div style="display:flex; justify-content:space-between; align-items:center;">
            <div style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.5px;">
              Ready to Confirm Target
            </div>
            <span style="font-size:9.5px; font-weight:700; color:var(--color-accent); background:rgba(0,230,118,0.1); padding:2px 8px; border-radius:4px;">
              ${candidateTarget.deviceType || (candidateTarget.isPlugin ? 'VIRTUAL_PLUGIN' : 'CUSTOM')}
            </span>
          </div>

          <div class="fossil-specs-card" style="padding:10px;">
            <div class="fossil-thumb-card" style="width:68px; height:52px;">
              <img src="${candidateTarget.isPlugin ? resolvePluginCover(candidateTarget) : resolveThemeImage(candidateTarget.image)}" alt="${candidateTarget.name}" style="max-width:100%; max-height:100%; object-fit:contain;" />
            </div>
            <div style="flex:1;">
              <div style="font-size:13.5px; font-weight:800; color:var(--color-text-main);">${candidateTarget.name}</div>
              <div style="font-size:10.5px; color:var(--color-text-dim); margin-top:2px;">${candidateTarget.category || 'Target Device'} • ${candidateTarget.midi ? 'MIDI Enabled' : 'Manual Audio Only'}</div>
            </div>
          </div>

          <!-- Profile Contract Action Bar (Candidate) -->
          <div style="display:flex; justify-content:space-between; align-items:center; padding:8px 10px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-sm); gap:8px; flex-wrap:wrap;">
            <div>
              <div style="font-size:10px; font-weight:700; color:var(--color-text-main);">Equipment Profile Contract (.json)</div>
              <div style="font-size:9px; color:var(--color-text-dim);">Load declarative equipment contract or export sample JSON template</div>
            </div>
            <div style="display:flex; gap:6px; align-items:center; flex-wrap:wrap;">
              <input type="file" id="fileCandidateContract" accept=".json" style="display:none;" />
              <button id="btnImportCandidateContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:9.5px; font-weight:700; padding:3px 8px;" title="Import a JSON hardware or plugin profile contract">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
                <span>Import Contract (.json)</span>
              </button>
              <button id="btnExportCandidateContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:9.5px; font-weight:700; padding:3px 8px;" title="Generate and download a sample JSON contract template">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
                <span>Generate Sample (.json)</span>
              </button>
              <button id="btnSpecCandidateContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:9.5px; font-weight:700; padding:3px 8px;" title="Download technical specifications and JSON templates (.md)">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
                <span>Spec (.md)</span>
              </button>
            </div>
          </div>

          <button id="btnConfirmAndLock" type="button" class="btn btn-confirm-lock-glow" style="width:100%; padding:13px; font-weight:800; font-size:12.5px; text-transform:uppercase; letter-spacing:0.8px;">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="display:inline-block; vertical-align:middle; margin-right:4px;"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>
            Confirm &amp; Lock Target (Seals Session) &rarr;
          </button>
        </div>
      ` : ''}
    `;
  }

  // --- RENDER: UNLISTED DEVICE CONFIGURATOR ---
  function renderUnlistedConfigurator() {
    return `
      <div style="display:flex; flex-direction:column; gap:12px; padding:14px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); margin-top:4px;">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
          <div>
            <span style="font-size:10px; font-weight:700; color:var(--color-info); text-transform:uppercase; letter-spacing:0.5px;">
              Custom Hardware Setup
            </span>
            <div style="font-size:9.5px; color:var(--color-text-dim);">Define manual device parameters or import contract profile</div>
          </div>
        </div>

        <!-- TYPOLOGY SELECTION / RETRACTED ROW -->
        ${!unlistedTypologyId ? `
          <div>
            <label style="font-size:10px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:8px; text-transform:uppercase;">
              Hardware Typology
            </label>
            <div class="hardware-typology-grid">
              ${UNLISTED_TYPOLOGIES.map(t => {
                const isModular = (t.id === 'modular');
                const imgSrc = isModular ? getThemeEurorackImage() : resolveThemeImage(t.image);
                return `
                <div data-typology="${t.id}" class="model-item-card" style="padding:10px; cursor:pointer;">
                  <div class="model-thumb-box" style="height:60px;">
                    <img src="${imgSrc}" alt="${t.name}" class="model-thumb-img ${isModular ? 'theme-img-eurorack' : ''}" />
                  </div>
                  <div style="font-size:10.5px; font-weight:700; color:var(--color-text-main); margin-top:4px; line-height:1.2;">${t.name}</div>
                </div>
              `;}).join('')}
            </div>
          </div>

          <!-- Contract Profile Row (Available when typology is not yet selected) -->
          <div style="padding-top:4px; border-top:1px solid rgba(255,255,255,0.06);">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
              <label style="font-size:10px; font-weight:700; color:var(--color-text-muted); text-transform:uppercase;">Hardware Profile Contract</label>
              <div style="display:flex; gap:6px; align-items:center; flex-wrap:wrap;">
                <input type="file" id="fileUnlistedContract" accept=".json" style="display:none;" />
                <button id="btnImportContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;" title="Import a JSON hardware profile contract">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
                  <span>Import Contract (.json)</span>
                </button>
                <button id="btnExportUnlistedContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;" title="Generate and download a sample JSON contract template">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
                  <span>Generate Sample (.json)</span>
                </button>
                <button id="btnDownloadContractSpec" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;" title="Download technical specifications and JSON templates (.md)">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
                  <span>Get Spec &amp; Templates (.md)</span>
                </button>
              </div>
            </div>
          </div>
        ` : `
          <!-- LEVEL 3 RETRACTED: Hardware Typology Header Row (Takes whole row with less height) -->
          <div class="target-type-header-row" style="margin-top:2px;">
            <div class="target-type-header-left">
              ${(() => {
                const t = UNLISTED_TYPOLOGIES.find(item => item.id === unlistedTypologyId) || UNLISTED_TYPOLOGIES[0];
                const isModular = (t.id === 'modular');
                const imgSrc = isModular ? getThemeEurorackImage() : resolveThemeImage(t.image);
                return `
                  <div style="width:38px; height:28px; display:flex; align-items:center; justify-content:center; flex-shrink:0;">
                    <img src="${imgSrc}" alt="${t.name}" style="max-width:100%; max-height:100%; object-fit:contain;" class="${isModular ? 'theme-img-eurorack' : ''}" />
                  </div>
                  <div class="target-type-header-info">
                    <div class="target-type-header-badge">HARDWARE TYPOLOGY</div>
                    <div class="target-type-header-title">
                      ${t.name}
                    </div>
                  </div>
                `;
              })()}
            </div>
            <button id="btnChangeTypology" type="button" class="btn btn-sm btn-outline target-change-btn" title="Change Hardware Typology">
              <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;">
                <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
                <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
              </svg>
              <span>Change Typology</span>
            </button>
          </div>

          <!-- Configuration parameters for the chosen typology -->
          <div>
            <label style="font-size:10px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase;">Device Name</label>
            <input id="txtUnlistedName" type="text" class="btn btn-sm" style="width:100%; text-align:left; background:var(--color-panel-surface-elevated); color:var(--color-text-main); font-size:11.5px; padding:8px 10px;" value="${unlistedCustomName}" placeholder="e.g. My Custom DIY Overdrive Pedal" />
          </div>

          <!-- Hardware Profile Contract (JSON) directly above Custom Device Photo -->
          <div>
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
              <label style="font-size:10px; font-weight:700; color:var(--color-text-muted); text-transform:uppercase;">Hardware Profile Contract</label>
              <div style="display:flex; gap:6px; align-items:center;">
                <input type="file" id="fileUnlistedContract" accept=".json" style="display:none;" />
                <button id="btnImportContract" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;" title="Import a JSON hardware profile contract">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
                  <span>Import Contract (.json)</span>
                </button>
                <button id="btnDownloadContractSpec" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;" title="Download technical specifications and JSON templates (.md)">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
                  <span>Get Spec &amp; Templates (.md)</span>
                </button>
              </div>
            </div>
          </div>

          <!-- Custom Device Photo / Image -->
          <div>
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
              <label style="font-size:10px; font-weight:700; color:var(--color-text-muted); text-transform:uppercase;">Custom Device Photo / Image</label>
              <div style="display:flex; gap:6px;">
                <input type="file" id="fileUnlistedImage" accept="image/png,image/jpeg,image/webp" style="display:none;" />
                <button id="btnPickUnlistedImage" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10px; font-weight:700; padding:4px 10px;">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>
                  <span>${unlistedCustomImage ? 'Change Image...' : 'Load Device Image...'}</span>
                </button>
                ${unlistedCustomImage ? `
                  <button id="btnResetUnlistedImage" type="button" class="btn btn-outline btn-sm" style="font-size:10px; font-weight:700; padding:4px 8px; color:var(--color-danger);">
                    Reset
                  </button>
                ` : ''}
              </div>
            </div>
            ${unlistedCustomImage ? `
              <div style="display:flex; align-items:center; gap:12px; padding:8px; background:var(--color-panel-surface-elevated); border-radius:6px; border:1px solid var(--color-panel-border);">
                <div style="width:70px; height:50px; background:var(--color-bg-base); border-radius:4px; display:flex; align-items:center; justify-content:center; overflow:hidden;">
                  <img src="${unlistedCustomImage}" alt="Custom Hardware Preview" style="max-width:100%; max-height:100%; object-fit:contain;" />
                </div>
                <div style="font-size:11px; color:var(--color-accent); font-weight:700;">Custom hardware image active</div>
              </div>
            ` : ''}
          </div>

          <div style="display:flex; align-items:center; justify-content:space-between; padding-top:6px; border-top:1px solid rgba(255,255,255,0.06);">
            <span style="font-size:10.5px; font-weight:600; color:var(--color-text-muted);">MIDI Control Automation</span>
            <label style="display:flex; align-items:center; gap:6px; cursor:pointer; font-size:11px; font-weight:700; color:var(--color-text-main);">
              <input id="chkUnlistedMidi" type="checkbox" ${unlistedHasMidi ? 'checked' : ''} style="cursor:pointer;" />
              <span>Target receives MIDI CC</span>
            </label>
          </div>

          <button id="btnConfirmUnlistedModel" type="button" class="btn btn-primary" style="padding:10px 18px; font-weight:700; font-size:11.5px; border-radius:6px; width:100%; margin-top:6px;">
            Select This Hardware &rarr;
          </button>
        `}
      </div>
    `;
  }

  // --- RENDER: FACTORY MODELS GRID ---
  function renderFactoryModelsGrid() {
    const brand = FACTORY_BRANDS.find(b => b.id === selectedBrandId);
    if (!brand || !brand.models) return '';

    return `
      <div style="display:flex; flex-direction:column; gap:8px; flex:1; min-height:0;">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-shrink:0;">
          <div style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">
            3. Select ${brand.name} Model
          </div>
          <div class="view-mode-toggle-group">
            <button id="btnViewModelGrid" type="button" class="view-mode-btn ${modelViewMode === 'grid' ? 'active' : ''}" title="Vista Rejilla (Grid View)">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                <rect x="3" y="3" width="7" height="7"></rect>
                <rect x="14" y="3" width="7" height="7"></rect>
                <rect x="14" y="14" width="7" height="7"></rect>
                <rect x="3" y="14" width="7" height="7"></rect>
              </svg>
            </button>
            <button id="btnViewModelList" type="button" class="view-mode-btn ${modelViewMode === 'list' ? 'active' : ''}" title="Vista Lista (List View)">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                <line x1="8" y1="6" x2="21" y2="6"></line>
                <line x1="8" y1="12" x2="21" y2="12"></line>
                <line x1="8" y1="18" x2="21" y2="18"></line>
                <line x1="3" y1="6" x2="3.01" y2="6"></line>
                <line x1="3" y1="12" x2="3.01" y2="12"></line>
                <line x1="3" y1="18" x2="3.01" y2="18"></line>
              </svg>
            </button>
          </div>
        </div>

        <div class="models-scroll-container">
          ${modelViewMode === 'grid' ? `
            <div class="model-cards-grid">
              ${brand.models.map(m => `
                <div data-model-id="${m.id}" class="model-item-card">
                  <div class="model-thumb-box">
                    <img src="${m.image}" alt="${m.name}" class="model-thumb-img" />
                  </div>
                  <div style="font-size:11.5px; font-weight:700; color:var(--color-text-main); line-height:1.25;">
                    ${m.name}
                  </div>
                  <div style="font-size:9.5px; color:var(--color-text-dim);">
                    ${m.deviceType || 'HARDWARE'}
                  </div>
                </div>
              `).join('')}
            </div>
          ` : `
            <div class="model-list-view">
              ${brand.models.map(m => `
                <div data-model-id="${m.id}" class="model-list-item">
                  <div class="model-list-thumb">
                    <img src="${m.image}" alt="${m.name}" class="model-list-img" />
                  </div>
                  <div class="model-list-info">
                    <div class="model-list-name">${m.name}</div>
                    <div class="model-list-meta">
                      <span>${m.category || 'Hardware Model'}</span>
                      ${m.protocol ? ` • <span>${m.protocol}</span>` : ''}
                    </div>
                  </div>
                  <div class="model-list-action">
                    <span class="model-list-badge">${m.deviceType || 'HARDWARE'}</span>
                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="model-list-arrow"><polyline points="9 18 15 12 9 6"></polyline></svg>
                  </div>
                </div>
              `).join('')}
            </div>
          `}
        </div>
      </div>
    `;
  }

  // --- RENDER: VST3 BROWSER ---
  function renderVst3Browser() {
    const plugins = state.availablePlugins || [];

    // Si ya hay un plugin seleccionado como candidateTarget, mostramos la ficha compacta retraída idéntica al hardware
    if (candidateTarget && candidateTarget.isPlugin) {
      const cover = resolvePluginCover(candidateTarget);
      return `
        <!-- LEVEL 2 RETRACTED: Subtitle Row 2 (VST3 Plugin) -->
        <div class="target-type-header-row" style="margin-top:8px;">
          <div class="target-type-header-left">
            <img src="${cover}" alt="${candidateTarget.name}" class="target-type-header-icon model-thumb-icon" />
            <div class="target-type-header-info">
              <div class="target-type-header-badge">VST3 PLUGIN</div>
              <div class="target-type-header-title">
                ${candidateTarget.name}
              </div>
            </div>
          </div>
          <button id="btnChangeVst" type="button" class="btn btn-sm btn-outline target-change-btn" title="Change VST Plugin">
            <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;">
              <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
              <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
            </svg>
            <span>Change VST</span>
          </button>
        </div>
      `;
    }

    // Paso 2: Selector de catálogo VST3 (cuando ningún VST está seleccionado)
    return `
      <div style="display:flex; flex-direction:column; gap:8px; margin-top:12px; flex:1; min-height:0;">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-shrink:0;">
          <div style="font-size:10px; font-weight:700; color:var(--color-text-dim); text-transform:uppercase; letter-spacing:0.6px;">
            2. Select VST3 Plugin
          </div>
          <div style="display:flex; align-items:center; gap:8px;">
            <button id="btnRescanPlugins" type="button" class="btn btn-sm btn-outline" style="border-color:var(--color-accent); color:var(--color-accent); font-size:10.5px; padding:3px 10px; font-weight:700; display:flex; align-items:center; gap:5px;" title="Scan VST3 directories for plugins">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M21.5 2v6h-6M2.5 22v-6h6M2 11.5a10 10 0 0 1 18.8-4.3M22 12.5a10 10 0 0 1-18.8 4.2"/></svg>
              <span>Scan Folders...</span>
            </button>
            <div class="view-mode-toggle-group">
              <button id="btnViewPluginGrid" type="button" class="view-mode-btn ${pluginViewMode === 'grid' ? 'active' : ''}" title="Vista Rejilla (Grid View)">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                  <rect x="3" y="3" width="7" height="7"></rect>
                  <rect x="14" y="3" width="7" height="7"></rect>
                  <rect x="14" y="14" width="7" height="7"></rect>
                  <rect x="3" y="14" width="7" height="7"></rect>
                </svg>
              </button>
              <button id="btnViewPluginList" type="button" class="view-mode-btn ${pluginViewMode === 'list' ? 'active' : ''}" title="Vista Lista (List View)">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                  <line x1="8" y1="6" x2="21" y2="6"></line>
                  <line x1="8" y1="12" x2="21" y2="12"></line>
                  <line x1="8" y1="18" x2="21" y2="18"></line>
                  <line x1="3" y1="6" x2="3.01" y2="6"></line>
                  <line x1="3" y1="12" x2="3.01" y2="12"></line>
                  <line x1="3" y1="18" x2="3.01" y2="18"></line>
                </svg>
              </button>
            </div>
          </div>
        </div>

        ${plugins.length > 0 ? `
          <div class="models-scroll-container">
            ${pluginViewMode === 'grid' ? `
              <div class="model-cards-grid">
                ${plugins.map((p, idx) => {
                  const cover = resolvePluginCover(p);
                  return `
                    <div data-plugin-idx="${idx}" class="model-item-card vst-plugin-card" style="cursor:pointer;" title="Click para cargar ${p.name}">
                      <div class="model-thumb-box" style="height:76px; display:flex; align-items:center; justify-content:center; overflow:hidden; border-radius:6px; background:rgba(0,0,0,0.12);">
                        <img src="${cover}" alt="${p.name}" class="model-thumb-img" style="max-width:100%; max-height:100%; object-fit:contain;" />
                      </div>
                      <div style="font-size:11.5px; font-weight:700; color:var(--color-text-main); line-height:1.25; margin-top:4px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">
                        ${p.name}
                      </div>
                      <div style="font-size:9.5px; color:var(--color-text-dim); display:flex; justify-content:space-between; align-items:center; width:100%; margin-top:2px;">
                        <span style="white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${p.manufacturerName || 'VST3'}</span>
                        <span class="badge ${p.isInstrument ? 'badge-info' : 'badge-neutral'} badge-xs">${p.isInstrument ? 'SYNTH' : 'EFFECT'}</span>
                      </div>
                    </div>
                  `;
                }).join('')}
              </div>
            ` : `
              <div class="model-list-view">
                ${plugins.map((p, idx) => {
                  const cover = resolvePluginCover(p);
                  return `
                    <div data-plugin-idx="${idx}" class="model-list-item vst-plugin-card" style="cursor:pointer;" title="Click para cargar ${p.name}">
                      <div class="model-list-thumb" style="width:58px; height:42px; display:flex; align-items:center; justify-content:center; overflow:hidden; border-radius:4px; background:rgba(0,0,0,0.12); flex-shrink:0;">
                        <img src="${cover}" alt="${p.name}" class="model-list-img" style="max-width:100%; max-height:100%; object-fit:contain;" />
                      </div>
                      <div class="model-list-info" style="min-width:0; flex:1;">
                        <div class="model-list-name" style="white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${p.name}</div>
                        <div class="model-list-meta" style="white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">
                          <span>${p.manufacturerName || 'VST3 Developer'}</span> • <span>${p.pluginFormatName || 'VST3'}</span>
                        </div>
                      </div>
                      <div class="model-list-action" style="flex-shrink:0;">
                        <span class="model-list-badge">${p.isInstrument ? 'SYNTH / MIDI' : 'EFFECT'}</span>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="model-list-arrow"><polyline points="9 18 15 12 9 6"></polyline></svg>
                      </div>
                    </div>
                  `;
                }).join('')}
              </div>
            `}
          </div>
        ` : `
          <div style="padding:16px; background:var(--color-panel-surface-elevated); border:1px dashed var(--color-panel-border); border-radius:var(--radius-md); text-align:center; display:flex; flex-direction:column; align-items:center; gap:8px;">
            <div style="font-size:11.5px; font-weight:600; color:var(--color-text-muted);">
              No plugins found in the local catalog.
            </div>
            <div style="font-size:10.5px; color:var(--color-text-dim); max-width:340px; line-height:1.4;">
              Click below to scan standard directories (e.g. C:\\Program Files\\Common Files\\VST3) and index your installed VST3 plugins.
            </div>
            <button id="btnScanFirstPlugins" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:700; margin-top:4px;">
              Scan Installed VST3 Plugins...
            </button>
          </div>
        `}

        <!-- LOAD MANUAL / UNLISTED FILE FROM DISK -->
        <div style="margin-top:4px; padding:10px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
          <div>
            <div style="font-size:11px; font-weight:700; color:var(--color-text-main);">Load Loose Plugin from Disk</div>
            <div style="font-size:10px; color:var(--color-text-dim);">Load an unlisted or portable .vst3 file or bundle from any folder</div>
          </div>
          <button id="btnBrowseVst3" type="button" class="btn btn-primary btn-sm" style="padding:6px 14px; font-weight:700; font-size:11px; border-radius:5px;">
            Browse .vst3 File...
          </button>
        </div>
      </div>
    `;
  }

  // --- ATTACH DOM EVENTS ---
  function attachEvents() {
    // Locked view actions
    const btnUnlock = container.querySelector('#btnUnlockTarget');
    if (btnUnlock) {
      btnUnlock.addEventListener('click', () => {
        isTargetLocked = false;
        if (state.selectedTarget) {
          state.selectedTarget.isLocked = false;
        }
        if (candidateTarget) {
          candidateTarget.isLocked = false;
        }
        notifyCandidateChanged(candidateTarget);
        render();
      });
    }

    // Helper for 2-phase morphing animation:
    // Phase 1: Fade out unselected siblings
    // Phase 2: Morph selected card across full width while narrowing in height
    function morphAndAdvance(clickedEl, siblingSelector, callback) {
      if (!clickedEl) {
        callback();
        return;
      }
      container.querySelectorAll(siblingSelector).forEach(sibling => {
        if (sibling !== clickedEl) {
          sibling.classList.add('morph-fade-out');
        }
      });

      setTimeout(() => {
        clickedEl.classList.add('morph-expand-header');
        setTimeout(() => {
          callback();
        }, 280);
      }, 200);
    }

    // Root Choice Cards (Level 1)
    const choicePhys = container.querySelector('#choicePhysical');
    if (choicePhys) {
      choicePhys.addEventListener('click', () => {
        morphAndAdvance(choicePhys, '.target-type-choice', () => {
          selectedRootType = 'PHYSICAL';
          selectedBrandId = null;
          selectedModelId = null;
          setCandidateTarget(null);
          render();
        });
      });
    }

    const choiceVst = container.querySelector('#choiceVst3');
    if (choiceVst) {
      choiceVst.addEventListener('click', () => {
        morphAndAdvance(choiceVst, '.target-type-choice', () => {
          selectedRootType = 'VST3';
          selectedBrandId = null;
          selectedModelId = null;
          if (state.activePlugin) {
            const cover = resolvePluginCover(state.activePlugin);
            setCandidateTarget({
              id: state.activePlugin.pluginId || 'vst3_plugin',
              name: state.activePlugin.name || state.activePlugin.fileName,
              shortName: state.activePlugin.name || state.activePlugin.fileName,
              category: 'Virtual Audio Plugin',
              deviceType: 'VIRTUAL_PLUGIN',
              image: cover,
              customImage: cover,
              midi: !!state.activePlugin.acceptsMidi,
              isPlugin: true
            });
          } else {
            setCandidateTarget(null);
          }
          render();
        });
      });
    }

    const btnChangeRoot = container.querySelector('#btnChangeTargetType');
    if (btnChangeRoot) {
      btnChangeRoot.addEventListener('click', () => {
        selectedRootType = null;
        selectedBrandId = null;
        selectedModelId = null;
        setCandidateTarget(null);
        render();
      });
    }

    // Auto-Detect MIDI Device via ABDShared HardwareMidiDetector
    const btnAutoDetectMidi = container.querySelector('#btnStage1AutoDetectMidi');
    if (btnAutoDetectMidi) {
      btnAutoDetectMidi.addEventListener('click', (e) => {
        e.stopPropagation();
        if (typeof state.autoDetectHardware === 'function') {
          state.autoDetectHardware();
        }
      });
    }

    // View Mode Switchers (Grid vs List)
    const btnViewBrandGrid = container.querySelector('#btnViewBrandGrid');
    const btnViewBrandList = container.querySelector('#btnViewBrandList');
    if (btnViewBrandGrid) {
      btnViewBrandGrid.addEventListener('click', (e) => {
        e.stopPropagation();
        brandViewMode = 'grid';
        try { localStorage.setItem('abdaudiolab_brand_view_mode', 'grid'); } catch (_) {}
        render();
      });
    }
    if (btnViewBrandList) {
      btnViewBrandList.addEventListener('click', (e) => {
        e.stopPropagation();
        brandViewMode = 'list';
        try { localStorage.setItem('abdaudiolab_brand_view_mode', 'list'); } catch (_) {}
        render();
      });
    }

    const btnViewModelGrid = container.querySelector('#btnViewModelGrid');
    const btnViewModelList = container.querySelector('#btnViewModelList');
    if (btnViewModelGrid) {
      btnViewModelGrid.addEventListener('click', (e) => {
        e.stopPropagation();
        modelViewMode = 'grid';
        try { localStorage.setItem('abdaudiolab_model_view_mode', 'grid'); } catch (_) {}
        render();
      });
    }
    if (btnViewModelList) {
      btnViewModelList.addEventListener('click', (e) => {
        e.stopPropagation();
        modelViewMode = 'list';
        try { localStorage.setItem('abdaudiolab_model_view_mode', 'list'); } catch (_) {}
        render();
      });
    }

    // Brand Cards & List Items (Level 2)
    container.querySelectorAll('[data-brand]').forEach(card => {
      card.addEventListener('click', (e) => {
        const bId = e.currentTarget.dataset.brand;
        morphAndAdvance(e.currentTarget, '.brand-card-item, .brand-list-item', () => {
          selectedBrandId = bId;
          selectedModelId = null;
          unlistedTypologyId = null;
          if (bId === 'unlisted') {
            syncCandidateFromUnlisted();
          } else {
            setCandidateTarget(null);
          }
          render();
        });
      });
    });

    const btnChangeBrand = container.querySelector('#btnChangeBrand');
    if (btnChangeBrand) {
      btnChangeBrand.addEventListener('click', () => {
        selectedBrandId = null;
        selectedModelId = null;
        setCandidateTarget(null);
        unlistedTypologyId = null;
        render();
      });
    }

    // Model Cards & List Items (Level 3)
    container.querySelectorAll('[data-model-id]').forEach(card => {
      card.addEventListener('click', (e) => {
        const mId = e.currentTarget.dataset.modelId;
        const brand = FACTORY_BRANDS.find(b => b.id === selectedBrandId);
        const m = brand && brand.models && brand.models.find(mod => mod.id === mId);
        if (m) {
          morphAndAdvance(e.currentTarget, '.model-item-card, .model-list-item', () => {
            selectedModelId = m.id;
            setCandidateTarget(m);
            render();
          });
        }
      });
    });

    const btnConfirmUnlisted = container.querySelector('#btnConfirmUnlistedModel');
    if (btnConfirmUnlisted) {
      btnConfirmUnlisted.addEventListener('click', () => {
        syncCandidateFromUnlisted();
        selectedModelId = candidateTarget.id;
        render();
      });
    }

    const btnChangeModel = container.querySelector('#btnChangeModel');
    if (btnChangeModel) {
      btnChangeModel.addEventListener('click', () => {
        selectedModelId = null;
        setCandidateTarget(null);
        render();
      });
    }

    // Unlisted Configurator Inputs
    const txtName = container.querySelector('#txtUnlistedName');
    if (txtName) {
      txtName.addEventListener('input', (e) => {
        unlistedCustomName = e.target.value;
        syncCandidateFromUnlisted();
      });
    }

    const chkMidi = container.querySelector('#chkUnlistedMidi');
    if (chkMidi) {
      chkMidi.addEventListener('change', (e) => {
        unlistedHasMidi = e.target.checked;
        syncCandidateFromUnlisted();
      });
    }

    container.querySelectorAll('[data-typology]').forEach(card => {
      card.addEventListener('click', (e) => {
        const typoId = e.currentTarget.dataset.typology;
        const typo = UNLISTED_TYPOLOGIES.find(t => t.id === typoId);
        if (typo) {
          morphAndAdvance(e.currentTarget, '.hardware-typology-grid .model-item-card', () => {
            unlistedTypologyId = typo.id;
            if (!unlistedCustomName || unlistedCustomName === 'Custom Hardware Device' || unlistedCustomName.startsWith('Custom ')) {
              unlistedCustomName = `Custom ${typo.name}`;
            }
            unlistedHasMidi = (typo.deviceType === 'AUTOMATED_MIDI_CC' || typo.id.includes('synth'));
            syncCandidateFromUnlisted();
            render();
          });
        }
      });
    });

    const btnChangeTypology = container.querySelector('#btnChangeTypology');
    if (btnChangeTypology) {
      btnChangeTypology.addEventListener('click', () => {
        unlistedTypologyId = null;
        setCandidateTarget(null);
        render();
      });
    }

    // Unlisted Image Picker & Reset
    const btnPickImg = container.querySelector('#btnPickUnlistedImage');
    const fileImg = container.querySelector('#fileUnlistedImage');
    if (btnPickImg && fileImg) {
      btnPickImg.addEventListener('click', () => fileImg.click());
      fileImg.addEventListener('change', (e) => {
        const file = e.target.files && e.target.files[0];
        if (file) {
          const reader = new FileReader();
          reader.onload = (loadEvt) => {
            unlistedCustomImage = loadEvt.target.result;
            syncCandidateFromUnlisted();
            render();
          };
          reader.readAsDataURL(file);
        }
      });
    }

    const btnResetImg = container.querySelector('#btnResetUnlistedImage');
    if (btnResetImg) {
      btnResetImg.addEventListener('click', () => {
        unlistedCustomImage = null;
        syncCandidateFromUnlisted();
        render();
      });
    }

    // Contract Profile Handlers (Unlisted, Candidate, and Locked views)
    function setupContractImportButton(btnId, fileInputId) {
      const btn = container.querySelector(btnId);
      const fileInput = container.querySelector(fileInputId);
      if (btn && fileInput) {
        btn.addEventListener('click', () => fileInput.click());
        fileInput.addEventListener('change', (e) => {
          const file = e.target.files && e.target.files[0];
          if (file) {
            const reader = new FileReader();
            reader.onload = (loadEvt) => {
              try {
                const contract = JSON.parse(loadEvt.target.result);
                applyImportedContract(contract);
              } catch (err) {
                showModalAlert({
                  title: 'Error de Perfil de Equipo (.json)',
                  message: 'No se pudo procesar el archivo JSON de perfil de equipo:\n' + err.message,
                  type: 'error',
                  confirmText: 'Entendido'
                });
              }
            };
            reader.readAsText(file);
          }
        });
      }
    }

    setupContractImportButton('#btnImportContract', '#fileUnlistedContract');
    setupContractImportButton('#btnImportCandidateContract', '#fileCandidateContract');
    setupContractImportButton('#btnImportLockedContract', '#fileLockedContract');

    // Export Sample JSON Contract buttons
    const btnExportUnlisted = container.querySelector('#btnExportUnlistedContract');
    if (btnExportUnlisted) {
      btnExportUnlisted.addEventListener('click', () => {
        exportSampleContractJson(candidateTarget || state.selectedTarget);
      });
    }

    const btnExportCandidate = container.querySelector('#btnExportCandidateContract');
    if (btnExportCandidate) {
      btnExportCandidate.addEventListener('click', () => {
        exportSampleContractJson(candidateTarget || state.selectedTarget);
      });
    }

    const btnExportLocked = container.querySelector('#btnExportLockedContract');
    if (btnExportLocked) {
      btnExportLocked.addEventListener('click', () => {
        exportSampleContractJson(state.selectedTarget || candidateTarget);
      });
    }

    // Spec & Template Guides (.md)
    container.querySelectorAll('#btnDownloadContractSpec, #btnSpecCandidateContract, #btnDownloadSpecLocked').forEach(btn => {
      btn.addEventListener('click', () => {
        downloadContractSpecificationGuide();
      });
    });

    // Save Session Actions (Sealed State)
    const btnSave = container.querySelector('#btnSaveSession');
    if (btnSave) {
      btnSave.addEventListener('click', () => {
        if (typeof state.saveSession === 'function') {
          state.saveSession();
        } else if (typeof onUpdate === 'function') {
          onUpdate('saveSession');
        }
      });
    }

    const btnSaveAs = container.querySelector('#btnSaveSessionAs');
    if (btnSaveAs) {
      btnSaveAs.addEventListener('click', () => {
        if (typeof state.saveSessionAs === 'function') {
          state.saveSessionAs();
        } else if (typeof onUpdate === 'function') {
          onUpdate('saveSessionAs');
        }
      });
    }

    // Confirm & Lock Button
    const btnLock = container.querySelector('#btnConfirmAndLock');
    if (btnLock) {
      btnLock.addEventListener('click', () => {
        if (!candidateTarget) return;
        if (candidateTarget.isPlugin) {
          const cover = resolvePluginCover(candidateTarget);
          if (cover && !cover.includes('generic-vst3') && !cover.includes('vst3_')) {
            candidateTarget.image = cover;
            candidateTarget.customImage = cover;
          }
        }
        candidateTarget.isLocked = true;
        isTargetLocked = true;
        state.selectedTarget = candidateTarget;

        window.__ABDAudioLab_suiteQueue__ = null;
        window.__ABDAudioLab_activeTestIndex__ = 0;

        if (typeof onUpdate === 'function') {
          onUpdate('targetSelected', candidateTarget);
        }
        render();
      });
    }

    // VST3 View Mode Toggle (Grid vs List)
    const btnPluginGrid = container.querySelector('#btnViewPluginGrid');
    if (btnPluginGrid) {
      btnPluginGrid.addEventListener('click', () => {
        pluginViewMode = 'grid';
        try { localStorage.setItem('abdaudiolab_vst_view_mode', 'grid'); } catch (_) {}
        render();
      });
    }

    const btnPluginList = container.querySelector('#btnViewPluginList');
    if (btnPluginList) {
      btnPluginList.addEventListener('click', () => {
        pluginViewMode = 'list';
        try { localStorage.setItem('abdaudiolab_vst_view_mode', 'list'); } catch (_) {}
        render();
      });
    }

    // VST3 Catalog Selection
    container.querySelectorAll('.vst-plugin-card').forEach(card => {
      card.addEventListener('click', (e) => {
        e.stopPropagation();
        const idx = parseInt(card.getAttribute('data-plugin-idx'), 10);
        const plugins = state.availablePlugins || [];
        const p = plugins[idx];
        if (p) {
          morphAndAdvance(card, '.model-item-card, .model-list-item', () => {
            const cover = resolvePluginCover(p);
            const plugTarget = {
              id: p.uniqueId ? String(p.uniqueId) : (p.fileOrIdentifier || p.name),
              name: p.name,
              shortName: p.name,
              category: p.category || (p.isInstrument ? 'Virtual Instrument' : 'Virtual Effect'),
              deviceType: 'VIRTUAL_PLUGIN',
              image: cover,
              customImage: cover,
              midi: !!p.isInstrument,
              isPlugin: true,
              pluginData: p
            };
            setCandidateTarget(plugTarget);

            if (typeof state.loadPlugin === 'function') {
              state.loadPlugin(p);
            }
            render();
          });
        }
      });
    });

    const btnRescan = container.querySelector('#btnRescanPlugins');
    if (btnRescan) {
      btnRescan.addEventListener('click', () => {
        if (typeof state.openPluginScan === 'function') {
          state.openPluginScan();
        }
      });
    }

    const btnScanFirst = container.querySelector('#btnScanFirstPlugins');
    if (btnScanFirst) {
      btnScanFirst.addEventListener('click', () => {
        if (typeof state.openPluginScan === 'function') {
          state.openPluginScan();
        }
      });
    }

    // VST3 buttons (Selection view)
    const btnBrowse = container.querySelector('#btnBrowseVst3');
    if (btnBrowse) {
      btnBrowse.addEventListener('click', () => {
        if (typeof state.loadPluginFromFile === 'function') {
          state.loadPluginFromFile();
        }
      });
    }

    const btnChangeVst = container.querySelector('#btnChangeVst') || container.querySelector('#btnUnloadVst3');
    if (btnChangeVst) {
      btnChangeVst.addEventListener('click', () => {
        setCandidateTarget(null);
        if (state.selectedTarget && state.selectedTarget.isPlugin) {
          state.selectedTarget = null;
        }
        state.activePlugin = null;
        if (typeof state.unloadPlugin === 'function') {
          state.unloadPlugin();
        }
        render();
      });
    }


    // VST3 Locked Controls
    const btnGuiLocked = container.querySelector('#btnTogglePluginGuiLocked');
    if (btnGuiLocked) {
      btnGuiLocked.addEventListener('click', () => {
        if (state.isPluginWindowOpen && typeof state.closePluginGui === 'function') {
          state.closePluginGui();
        } else if (typeof state.openPluginGui === 'function') {
          state.openPluginGui();
        }
      });
    }

    const btnCaptureVstLocked = container.querySelector('#btnCaptureVstImageLocked');
    if (btnCaptureVstLocked) {
      btnCaptureVstLocked.addEventListener('click', () => {
        if (typeof state.captureVstImage === 'function') {
          state.captureVstImage();
        }
      });
    }

    const btnImportCoverLocked = container.querySelector('#btnImportVstCoverLocked');
    const fileCoverLocked = container.querySelector('#fileVstCoverLocked');
    if (btnImportCoverLocked && fileCoverLocked) {
      btnImportCoverLocked.addEventListener('click', () => fileCoverLocked.click());
      fileCoverLocked.addEventListener('change', (e) => {
        const file = e.target.files && e.target.files[0];
        if (file) {
          const reader = new FileReader();
          reader.onload = (ev) => {
            const dataUrl = ev.target.result;
            if (state.selectedTarget) {
              state.selectedTarget.image = dataUrl;
              state.selectedTarget.customImage = dataUrl;
            }
            if (candidateTarget) {
              candidateTarget.image = dataUrl;
              candidateTarget.customImage = dataUrl;
            }
            notifyCandidateChanged(state.selectedTarget || candidateTarget);
            render();
          };
          reader.readAsDataURL(file);
        }
      });
    }

    const btnKbdLocked = container.querySelector('#btnOpenVirtualKeyboardLocked');
    if (btnKbdLocked) {
      btnKbdLocked.addEventListener('click', () => {
        if (typeof state.toggleVirtualKeyboard === 'function') {
          state.toggleVirtualKeyboard(true);
        } else if (window.app && typeof window.app.toggleVirtualKeyboard === 'function') {
          window.app.toggleVirtualKeyboard(true);
        }
      });
    }
  }

  function syncCandidateFromUnlisted() {
    const typo = UNLISTED_TYPOLOGIES.find(t => t.id === unlistedTypologyId) || UNLISTED_TYPOLOGIES[0];
    candidateTarget = {
      id: `custom_${unlistedTypologyId || 'device'}`,
      name: unlistedCustomName || typo.name,
      shortName: unlistedCustomName || typo.name,
      category: typo.name,
      deviceType: typo.deviceType,
      image: unlistedCustomImage || (typo.id === 'modular' ? getThemeEurorackImage() : resolveThemeImage(typo.image)),
      midi: unlistedHasMidi,
      protocol: unlistedHasMidi ? 'MIDI CC Channel 1 Automation' : 'Manual Potentiometer / Physical Sweep',
      routingNotes: 'DAC Out 1 -> Audio In -> Output -> ADC In 1',
      details: typo.details
    };
    state.selectedTarget = candidateTarget;
    notifyCandidateChanged(candidateTarget);
  }

  function downloadContractSpecificationGuide() {
    const mdContent = `# ABDAudioLab - Hardware Profile Specification & Contract Guide (v2.0)

## Overview
ABDAudioLab uses declarative JSON contracts conforming to JSON Schema 2.0 (hardware_profile.schema.json) to automatically configure measurement stimuli, I/O routing, and parameter sweeps.

### File Locations
- Custom Contracts: <WorkingDir>/Hardware/<DeviceName>.json
- Custom Photos: <WorkingDir>/Hardware/Images/<DeviceName>.png

---

## Contract Fields

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| id | string | Yes | Unique identifier (e.g. "my_tube_preamp") |
| name | string | Yes | Full commercial display name (e.g. "Custom Tube Mic Preamp") |
| shortName | string | Yes | Compact display name for telemetry headers (e.g. "Tube Preamp") |
| category | string | Yes | Hardware Category / Manufacturer |
| deviceType | string | Yes | One of: ANALOGUE_PEDAL, SYNTHESIZER_KEYBOARD, SYNTHESIZER_MODULE, EURORACK_MODULAR, AUDIO_RACK, MIC_PREAMP, LOOPBACK_UNIT |
| image | string | No | Path to photo relative to assets or data folder |
| midi | boolean | Yes | Whether target accepts MIDI automation |
| protocol | string | No | Control protocol description |
| routingNotes | string | No | DAC / ADC connection guide |
| details | string | No | Technical description of hardware architecture |

---

## JSON Templates

### 1. Analogue Guitar Pedal (Stompbox)
\`\`\`json
{
  "id": "my_custom_overdrive",
  "name": "DIY Vintage Overdrive Pedal",
  "shortName": "DIY OD-1",
  "category": "Guitar Pedal",
  "deviceType": "ANALOGUE_PEDAL",
  "image": "images/custom_od.png",
  "midi": false,
  "protocol": "Manual Knobs (Drive, Tone, Level)",
  "routingNotes": "DAC Out 1 -> Pedal Input -> Pedal Output -> ADC In 1",
  "details": "JFET op-amp asymmetrical clipping overdrive circuit"
}
\`\`\`

### 2. Hardware Synthesizer (MIDI CC Enabled)
\`\`\`json
{
  "id": "my_analogue_mono_synth",
  "name": "Custom Monophonic Analogue Synth",
  "shortName": "MonoSynth",
  "category": "Synthesizer",
  "deviceType": "SYNTHESIZER_MODULE",
  "image": "images/custom_synth.png",
  "midi": true,
  "protocol": "MIDI CC Automation (Cutoff: CC74, Res: CC71)",
  "routingNotes": "MIDI Out 1 -> Synth MIDI In; Synth Audio Out -> ADC In 1",
  "details": "Dual VCO, 24dB ladder filter, discrete analogue signal path"
}
\`\`\`

### 3. Outboard Studio Mic Preamp / Compressor
\`\`\`json
{
  "id": "my_studio_vca_compressor",
  "name": "Stereo VCA Bus Compressor",
  "shortName": "Bus Comp",
  "category": "Outboard Studio Gear",
  "deviceType": "AUDIO_RACK",
  "image": "images/bus_comp.png",
  "midi": false,
  "protocol": "Manual Stepped Controls (Threshold, Ratio, Attack, Release)",
  "routingNotes": "DAC Out 1/2 -> Line In L/R -> Line Out L/R -> ADC In 1/2",
  "details": "True RMS detection discrete VCA bus compressor"
}
\`\`\`

### 4. Audio Interface Loopback Calibration Target
\`\`\`json
{
  "id": "interface_direct_loopback",
  "name": "Audio Interface Direct Loopback Cable",
  "shortName": "Loopback Cable",
  "category": "Calibration",
  "deviceType": "LOOPBACK_UNIT",
  "image": "images/generic-desktop-module.webp",
  "midi": false,
  "protocol": "Direct Physical TRS Balanced Cable",
  "routingNotes": "DAC Output 1 -> TRS Cable -> ADC Input 1",
  "details": "Zero-load unity-gain calibration reference loop"
}
\`\`\`
`;

    const blob = new Blob([mdContent], { type: 'text/markdown;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'ABDAudioLab_Hardware_Contract_Spec_Templates.md';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function exportSampleContractJson(target) {
    const isPlug = target && target.isPlugin;
    const sample = {
      "$schema": "https://abdsynths.com/schemas/hardware_profile.schema.json",
      "version": "2.0.0",
      "id": (target && target.id) ? String(target.id).toLowerCase().replace(/[^a-z0-9_]/g, '_') : "custom_device_profile",
      "name": (target && target.name) ? target.name : "Custom Audio Target",
      "shortName": (target && target.shortName) ? target.shortName : (target && target.name) || "Device",
      "category": (target && target.category) ? target.category : (isPlug ? "Virtual Plugin" : "Hardware Target"),
      "deviceType": (target && target.deviceType) ? target.deviceType : (isPlug ? "VIRTUAL_PLUGIN" : "SYNTHESIZER_MODULE"),
      "midi": target ? !!target.midi : true,
      "protocol": (target && target.protocol) ? target.protocol : (isPlug ? "VST3 Parameter Automation" : "MIDI CC Automation"),
      "routingNotes": (target && target.routingNotes) ? target.routingNotes : (isPlug ? "DAW / Test Host Internal Bus (Zero Latency Direct I/O)" : "Probe Out 1 -> Audio In 1; Target Audio Out -> Return In 1"),
      "details": (target && target.details) ? target.details : "Declarative measurement contract for profiling and parameter tracking",
      "parameters": (target && Array.isArray(target.parameters) && target.parameters.length > 0) ? target.parameters : [
        {
          "id": "param_cutoff",
          "name": "Filter Cutoff",
          "type": "CONTINUOUS",
          "cc": 74,
          "min": 0,
          "max": 127,
          "default": 64,
          "unit": "Hz"
        },
        {
          "id": "param_resonance",
          "name": "Resonance",
          "type": "CONTINUOUS",
          "cc": 71,
          "min": 0,
          "max": 127,
          "default": 0,
          "unit": "%"
        }
      ]
    };

    const jsonStr = JSON.stringify(sample, null, 2);
    const blob = new Blob([jsonStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    const baseName = (target && target.name) ? target.name.toLowerCase().replace(/[^a-z0-9_]/g, '_') : 'sample_device';
    a.download = `${baseName}_profile.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function applyImportedContract(contract) {
    if (!contract) return;
    const targetObj = candidateTarget || state.selectedTarget || {};
    if (contract.name) {
      targetObj.name = contract.name;
      targetObj.shortName = contract.shortName || contract.name;
      unlistedCustomName = contract.name;
    }
    if (contract.id) targetObj.id = contract.id;
    if (contract.category) targetObj.category = contract.category;
    if (contract.deviceType) {
      targetObj.deviceType = contract.deviceType;
      const foundTypo = UNLISTED_TYPOLOGIES.find(t => t.deviceType === contract.deviceType);
      if (foundTypo) unlistedTypologyId = foundTypo.id;
    }
    if (typeof contract.midi !== 'undefined') {
      targetObj.midi = !!contract.midi;
      unlistedHasMidi = !!contract.midi;
    }
    if (contract.protocol) targetObj.protocol = contract.protocol;
    if (contract.routingNotes) targetObj.routingNotes = contract.routingNotes;
    if (contract.details) targetObj.details = contract.details;
    if (contract.parameters && Array.isArray(contract.parameters)) {
      targetObj.parameters = contract.parameters;
    } else if (contract.functions && Array.isArray(contract.functions)) {
      const extracted = [];
      contract.functions.forEach(fn => {
        if (fn.controls && Array.isArray(fn.controls)) {
          fn.controls.forEach(c => {
            extracted.push({
              name: c.name,
              type: c.type || 'Knob',
              controlMethod: c.controlMethod || 'MIDI_CC',
              cc: c.cc,
              nrpn: c.nrpn,
              sysex: c.sysex || c.sysexTemplate,
              minPct: typeof c.minVal === 'number' ? c.minVal * 100 : 0,
              maxPct: typeof c.maxVal === 'number' ? c.maxVal * 100 : 100
            });
          });
        }
      });
      if (extracted.length > 0) {
        targetObj.parameters = extracted;
      }
    }
    if (contract.image) {
      targetObj.image = contract.image;
      targetObj.customImage = contract.image;
      unlistedCustomImage = contract.image;
    }

    if (!candidateTarget && !state.selectedTarget) {
      candidateTarget = targetObj;
    }
    if (candidateTarget) {
      Object.assign(candidateTarget, targetObj);
      notifyCandidateChanged(candidateTarget);
    }
    if (state.selectedTarget) {
      Object.assign(state.selectedTarget, targetObj);
      notifyCandidateChanged(state.selectedTarget);
    }
    if (typeof state.showToast === 'function') {
      state.showToast(`Perfil importado: ${targetObj.name}`);
    }
    render();
  }

  // Initial render
  if (selectedRootType === 'PHYSICAL' && selectedBrandId === 'unlisted' && !candidateTarget && unlistedTypologyId) {
    syncCandidateFromUnlisted();
  } else if (candidateTarget) {
    notifyCandidateChanged(candidateTarget);
  }
  render();
}
