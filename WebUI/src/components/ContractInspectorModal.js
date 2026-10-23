/**
 * ContractInspectorModal.js - ABDAudioLab
 * 
 * Interactive inspection, editing, photo gallery, and version CRUD manager for Hardware Profile Contracts (Schema 2.0).
 * Features:
 * - 100% English professional interface conforming to ABDAudioLab standards.
 * - Clear distinction between Official Factory Profiles (immutable) and Custom User Equipment.
 * - Working Drafts (in-place edits without history pollution) vs. Consolidated Releases (sealed versions).
 * - Target Photo Gallery & Cover Selector in Tab 1 (Artwork & Photos).
 * - MIDI Automation toggle inside Contract (Tab 1).
 * - Diff Engine with toggle ([ 👁 Diffs: ON/OFF ]) and individual field revert (↺).
 * - In-modal JSON Import and Export (.json).
 * - Pre-consolidation Schema 2.0 validation.
 * - Single fixed modal height (no jumping or resizing across tabs).
 */

import { showModalAlert, showModalConfirm, showSoundIdPrompt } from './SoundIdModal.js';

export const CANONICAL_DEVICE_TYPES = [
  'AUTOMATED_MIDI_CC',
  'AUTOMATED_SYSEX',
  'AUTOMATED_NRPN',
  'ANALOGUE_PEDAL',
  'MANUAL_EURORACK',
  'MANUAL_SYNTH',
  'MANUAL_PROMPT',
  'MOCK_DSP'
];

/**
 * Persistence layer for contract versions using localStorage.
 */
export class ContractVersionStore {
  static getStorageKey(baseId) {
    const cleanId = String(baseId || 'custom_device').toLowerCase().replace(/[^a-z0-9_]/g, '_');
    return `abdaudiolab_contract_versions_${cleanId}`;
  }

  static getVersions(baseId, factoryContract = null) {
    const key = this.getStorageKey(baseId);
    let userVersions = [];
    try {
      const stored = localStorage.getItem(key);
      if (stored) {
        userVersions = JSON.parse(stored);
      }
    } catch (e) {
      console.warn('[ContractVersionStore] Error loading versions from storage:', e);
    }

    const all = [];

    // 1. Factory Version (Certified baseline, only for real factory models)
    const isRealFactory = !!(factoryContract && !baseId.startsWith('custom_') && factoryContract.isFactory !== false);
    if (isRealFactory) {
      const fCopy = JSON.parse(JSON.stringify(factoryContract));
      all.push({
        id: `${baseId}_v1_0_0_factory`,
        baseProfileId: baseId,
        versionNumber: '1.0.0',
        name: `${fCopy.displayName || fCopy.name || 'Device'} (Factory Official)`,
        notes: 'Certified official factory contract.',
        author: 'ABDAudioLab Factory',
        createdAt: '2026-01-01T00:00:00Z',
        isFactory: true,
        isDraft: false,
        isConsolidated: true,
        contract: fCopy
      });
    }

    // 2. User Versions (Consolidated releases and working drafts)
    if (Array.isArray(userVersions)) {
      userVersions.forEach(v => all.push(v));
    }

    // 3. Fallback for custom devices with no stored versions yet
    if (all.length === 0) {
      const initContract = factoryContract ? JSON.parse(JSON.stringify(factoryContract)) : {};
      initContract.schemaVersion = "2.0";
      if (!initContract.displayName && !initContract.name) {
        initContract.displayName = 'Custom Equipment';
      }
      all.push({
        id: `${baseId}_v1_0_0_custom`,
        baseProfileId: baseId,
        versionNumber: '1.0.0',
        name: `${initContract.displayName || initContract.name || 'Custom Device'} (Initial Profile)`,
        notes: 'Custom hardware profile created by user.',
        author: 'User',
        createdAt: new Date().toISOString(),
        isFactory: false,
        isDraft: false,
        isConsolidated: true,
        isCustom: true,
        contract: initContract
      });
    }

    return all;
  }

  static getActiveDraft(baseId, factoryContract = null) {
    const versions = this.getVersions(baseId, factoryContract);
    return versions.find(v => v.isDraft) || null;
  }

  static saveUserVersion(baseId, newVersion) {
    const key = this.getStorageKey(baseId);
    let stored = [];
    try {
      const existing = localStorage.getItem(key);
      if (existing) stored = JSON.parse(existing);
    } catch (_) {}

    const idx = stored.findIndex(v => v.id === newVersion.id);
    if (idx >= 0) {
      stored[idx] = newVersion;
    } else {
      stored.push(newVersion);
    }

    try {
      localStorage.setItem(key, JSON.stringify(stored));
      return true;
    } catch (e) {
      console.error('[ContractVersionStore] Failed to save version:', e);
      return false;
    }
  }

  static deleteUserVersion(baseId, versionId) {
    const key = this.getStorageKey(baseId);
    let stored = [];
    try {
      const existing = localStorage.getItem(key);
      if (existing) stored = JSON.parse(existing);
    } catch (_) {}

    const filtered = stored.filter(v => v.id !== versionId);
    try {
      localStorage.setItem(key, JSON.stringify(filtered));
      return true;
    } catch (_) {
      return false;
    }
  }

  static resetToFactory(baseId) {
    const key = this.getStorageKey(baseId);
    try {
      localStorage.removeItem(key);
      return true;
    } catch (_) {
      return false;
    }
  }

  static getBaselineContract(baseId, factoryContract = null) {
    const isRealFactory = !!(factoryContract && !baseId.startsWith('custom_') && factoryContract.isFactory !== false);
    if (isRealFactory) {
      return JSON.parse(JSON.stringify(factoryContract));
    }
    const versions = this.getVersions(baseId, null);
    const firstConsolidated = versions.find(v => !v.isDraft && v.isConsolidated);
    if (firstConsolidated && firstConsolidated.contract) {
      return JSON.parse(JSON.stringify(firstConsolidated.contract));
    }
    return versions[0]?.contract ? JSON.parse(JSON.stringify(versions[0].contract)) : {};
  }
}

/**
 * Validate a hardware profile contract conforming to Schema 2.0
 */
export function validateContractSchema(contract) {
  const errors = [];
  if (!contract) {
    errors.push('Contract payload is empty or invalid.');
    return errors;
  }
  const name = contract.displayName || contract.name;
  if (!name || !String(name).trim()) {
    errors.push('Commercial Display Name (displayName) is required.');
  }
  if (!contract.id || !String(contract.id).trim()) {
    errors.push('Canonical Identifier (id) is required.');
  }
  if (!contract.deviceType || !CANONICAL_DEVICE_TYPES.includes(contract.deviceType)) {
    errors.push(`Invalid deviceType (${contract.deviceType}). Must be one of: ${CANONICAL_DEVICE_TYPES.join(', ')}`);
  }

  if (contract.lifecycle && Array.isArray(contract.lifecycle.preSessionSetup)) {
    contract.lifecycle.preSessionSetup.forEach((act, idx) => {
      if (act.method === 'MIDI_CC' || act.method === 'NRPN') {
        if (typeof act.channel === 'number' && (act.channel < 1 || act.channel > 16)) {
          errors.push(`Setup Action #${idx + 1}: MIDI Channel out of range 1-16 (current: ${act.channel}).`);
        }
        if (typeof act.cc === 'number' && (act.cc < 0 || act.cc > 127)) {
          errors.push(`Setup Action #${idx + 1}: Controller CC out of range 0-127 (current: ${act.cc}).`);
        }
      }
    });
  }

  if (Array.isArray(contract.controls)) {
    contract.controls.forEach((c, idx) => {
      if (!c.name || !String(c.name).trim()) {
        errors.push(`Control #${idx + 1}: Must have a valid name.`);
      }
      if (typeof c.defaultVal === 'number' && (c.defaultVal < 0 || c.defaultVal > 1)) {
        errors.push(`Control "${c.name || idx + 1}": Normalized default value must be between 0.0 and 1.0.`);
      }
    });
  }

  return errors;
}

/**
 * Convert any image file (PNG, JPEG, WebP, etc.) to an optimized WebP DataURL with bounded dimensions
 * and compute its SHA-256 hash for content-addressable storage (CAS) and automatic deduplication.
 * 
 * @param {File|Blob} file - Source image file
 * @param {number} [maxDimension=1600] - Maximum width or height constraint
 * @param {number} [quality=0.88] - WebP compression factor (0.0 to 1.0)
 * @returns {Promise<{dataUrl: string, canonicalFileName: string, sha256: string, shortHash: string, width: number, height: number, originalSize: number, optimizedSize: number}>}
 */
export async function optimizeImageToWebP(file, maxDimension = 1600, quality = 0.88) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error('Failed to read image file'));
    reader.onload = () => {
      const img = new Image();
      img.onerror = () => reject(new Error('Failed to decode image data'));
      img.onload = async () => {
        try {
          let w = img.naturalWidth || img.width;
          let h = img.naturalHeight || img.height;

          // Scale proportionally if either dimension exceeds maxDimension
          if (w > maxDimension || h > maxDimension) {
            const ratio = Math.min(maxDimension / w, maxDimension / h);
            w = Math.max(1, Math.round(w * ratio));
            h = Math.max(1, Math.round(h * ratio));
          }

          const canvas = document.createElement('canvas');
          canvas.width = w;
          canvas.height = h;
          const ctx = canvas.getContext('2d');
          ctx.drawImage(img, 0, 0, w, h);

          // Convert to WebP blob
          const blob = await new Promise((res) => {
            if (typeof canvas.toBlob === 'function') {
              canvas.toBlob((b) => {
                if (b) res(b);
                else res(canvas.toDataURL('image/webp', quality));
              }, 'image/webp', quality);
            } else {
              res(canvas.toDataURL('image/webp', quality));
            }
          });

          let dataUrl = '';
          let arrayBuffer = null;

          if (typeof blob === 'string') {
            dataUrl = blob;
            const parts = dataUrl.split(',');
            const byteString = atob(parts[1]);
            const ab = new ArrayBuffer(byteString.length);
            const ia = new Uint8Array(ab);
            for (let i = 0; i < byteString.length; i++) {
              ia[i] = byteString.charCodeAt(i);
            }
            arrayBuffer = ab;
          } else {
            arrayBuffer = await blob.arrayBuffer();
            dataUrl = await new Promise((resUrl) => {
              const r = new FileReader();
              r.onload = () => resUrl(r.result);
              r.readAsDataURL(blob);
            });
          }

          // Compute SHA-256 hash using native Web Crypto API
          const hashBuffer = await crypto.subtle.digest('SHA-256', arrayBuffer);
          const hashArray = Array.from(new Uint8Array(hashBuffer));
          const sha256Hex = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
          const shortHash = sha256Hex.slice(0, 16);
          const canonicalFileName = `img_${shortHash}.webp`;

          resolve({
            dataUrl,
            canonicalFileName,
            sha256: sha256Hex,
            shortHash,
            width: w,
            height: h,
            originalSize: file.size,
            optimizedSize: arrayBuffer.byteLength
          });
        } catch (err) {
          reject(err);
        }
      };
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

/**
 * Open the Contract Inspector and Version CRUD Modal
 */
export function openContractInspectorModal(options = {}) {
  const {
    target,
    factoryContract = null,
    onApply,
    onToast = (msg) => console.log(msg)
  } = options;

  if (!target) return;

  const baseId = target.id || 'custom_device';
  const isCustomTarget = !!(target.isCustom || baseId.startsWith('custom_') || !factoryContract);

  // Load versions
  let versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);

  // Default to active draft if one exists, otherwise the latest version
  const activeDraft = ContractVersionStore.getActiveDraft(baseId, isCustomTarget ? null : factoryContract);
  let activeVersionId = activeDraft ? activeDraft.id : versions[versions.length - 1].id;

  let activeTab = 'general'; // 'general' | 'cabling' | 'lifecycle' | 'controls' | 'json'
  let showDiffs = !isCustomTarget; // Disable diff markers by default on custom equipment to avoid false alarms

  // Working editable copy of the currently selected version's contract
  let currentContract = JSON.parse(JSON.stringify(getActiveVersion().contract));

  // Initialize gallery array if not present
  if (!Array.isArray(currentContract.gallery)) {
    currentContract.gallery = [];
    const mainImg = currentContract.modelImage || currentContract.image || target.image;
    if (mainImg) currentContract.gallery.push(mainImg);
  }

  // Immutable baseline copy for diff calculation
  const baselineContract = ContractVersionStore.getBaselineContract(baseId, isCustomTarget ? null : factoryContract);

  function getActiveVersion() {
    return versions.find(v => v.id === activeVersionId) || versions[0];
  }

  // --- DIFF COMPUTATION ENGINE ---
  function computeDiffs() {
    const diffs = {
      isModified: false,
      fields: {},
      summary: []
    };

    if (!baselineContract || !baselineContract.id) return diffs;

    const primitiveKeys = [
      { key: 'displayName', label: 'Commercial Name', fallback: 'name' },
      { key: 'deviceType', label: 'Device Typology' },
      { key: 'brand', label: 'Brand / Manufacturer', fallback: 'category' },
      { key: 'description', label: 'Description', fallback: 'details' },
      { key: 'routingNotes', label: 'Signal Routing' },
      { key: 'protocol', label: 'Control Protocol' },
      { key: 'midi', label: 'MIDI Enabled' }
    ];

    primitiveKeys.forEach(p => {
      const curVal = currentContract[p.key] !== undefined ? currentContract[p.key] : (p.fallback ? currentContract[p.fallback] : '');
      const baseVal = baselineContract[p.key] !== undefined ? baselineContract[p.key] : (p.fallback ? baselineContract[p.fallback] : '');
      if (String(curVal).trim() !== String(baseVal).trim()) {
        diffs.fields[p.key] = {
          current: curVal,
          original: baseVal,
          label: p.label
        };
        diffs.isModified = true;
        diffs.summary.push(`${p.label}: "${baseVal}" → "${curVal}"`);
      }
    });

    // Cabling Steps
    const curSteps = currentContract.cablingSteps || (currentContract.routingGuide && currentContract.routingGuide.steps) || [];
    const baseSteps = baselineContract.cablingSteps || (baselineContract.routingGuide && baselineContract.routingGuide.steps) || [];
    if (JSON.stringify(curSteps) !== JSON.stringify(baseSteps)) {
      diffs.fields['cablingSteps'] = {
        current: curSteps,
        original: baseSteps,
        label: 'Cabling Steps'
      };
      diffs.isModified = true;
      diffs.summary.push(`Physical Cabling: ${curSteps.length} steps (original: ${baseSteps.length})`);
    }

    // Lifecycle Actions
    const curSetup = currentContract.lifecycle?.preSessionSetup || [];
    const baseSetup = baselineContract.lifecycle?.preSessionSetup || [];
    if (JSON.stringify(curSetup) !== JSON.stringify(baseSetup)) {
      diffs.fields['lifecycle'] = {
        current: curSetup,
        original: baseSetup,
        label: 'Initialization Actions'
      };
      diffs.isModified = true;
      diffs.summary.push(`Ground State / Setup: ${curSetup.length} actions (original: ${baseSetup.length})`);
    }

    // Controls
    const curCtrls = currentContract.controls || [];
    const baseCtrls = baselineContract.controls || [];
    if (JSON.stringify(curCtrls) !== JSON.stringify(baseCtrls)) {
      diffs.fields['controls'] = {
        current: curCtrls,
        original: baseCtrls,
        label: 'Baseline Controls'
      };
      diffs.isModified = true;
      diffs.summary.push(`Baseline Controls: ${curCtrls.length} controls (original: ${baseCtrls.length})`);
    }

    return diffs;
  }

  function renderDiffMarker(fieldKey) {
    if (!showDiffs) return '';
    const diffs = computeDiffs();
    if (!diffs.fields[fieldKey]) return '';
    const orig = diffs.fields[fieldKey].original;
    const origDisplay = typeof orig === 'string' ? orig : JSON.stringify(orig);
    return `
      <span style="display:inline-flex; align-items:center; gap:4px; margin-left:6px;">
        <span class="badge badge-accent badge-xs" style="font-size:8.5px; font-weight:800; padding:1px 5px;" title="Baseline: ${escapeHtml(origDisplay)}">● Modified</span>
        <button type="button" class="btn btn-outline btn-xs btn-revert-field" data-revert-field="${fieldKey}" style="font-size:9.5px; padding:1px 5px; color:var(--color-text-dim);" title="Revert to baseline original">↺</button>
      </span>
    `;
  }

  // Modal Container
  const overlay = document.createElement('div');
  overlay.className = 'soundid-modal-overlay contract-inspector-overlay';
  overlay.setAttribute('role', 'dialog');
  overlay.setAttribute('aria-modal', 'true');

  function render() {
    const ver = getActiveVersion();
    const isFactory = !!ver.isFactory;
    const isDraft = !!ver.isDraft;
    const isConsolidated = !!ver.isConsolidated && !isFactory;
    const diffs = computeDiffs();

    // Determine badge status
    let badgeHtml = '';
    if (isFactory) {
      badgeHtml = '<span class="badge badge-success badge-sm" style="font-size:9.5px; font-weight:800;">FACTORY BASELINE (IMMUTABLE)</span>';
    } else if (isDraft) {
      badgeHtml = '<span class="badge badge-accent badge-sm" style="font-size:9.5px; font-weight:800; background:rgba(217, 119, 6, 0.2); color:#d97706; border-color:#d97706;">ACTIVE DRAFT</span>';
    } else if (isCustomTarget) {
      badgeHtml = '<span class="badge badge-accent badge-sm" style="font-size:9.5px; font-weight:800;">CUSTOM PROFILE</span>';
    } else {
      badgeHtml = '<span class="badge badge-accent badge-sm" style="font-size:9.5px; font-weight:800;">CONSOLIDATED RELEASE</span>';
    }

    overlay.innerHTML = `
      <div class="test-config-modal-card" style="width:940px; max-width:96vw; height:84vh; min-height:640px; max-height:88vh; display:flex; flex-direction:column;">
        
        <!-- MODAL HEADER -->
        <div class="test-config-modal-header" style="padding:10px 18px; border-bottom:1px solid var(--color-panel-border); background:var(--color-panel-surface);">
          <div style="display:flex; align-items:center; gap:10px; flex-wrap:wrap; min-width:0;">
            <div style="display:flex; align-items:center; gap:6px;">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--color-accent)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
                <polyline points="14 2 14 8 20 8"></polyline>
                <line x1="16" y1="13" x2="8" y2="13"></line>
                <line x1="16" y1="17" x2="8" y2="17"></line>
                <polyline points="10 9 9 9 8 9"></polyline>
              </svg>
              <span style="font-size:12px; font-weight:800; letter-spacing:0.6px; text-transform:uppercase; color:var(--color-text-main);">
                Contract Inspector &amp; Versions
              </span>
            </div>

            <!-- Version Selector Dropdown -->
            <div style="display:flex; align-items:center; gap:6px; margin-left:6px;">
              <select id="cboContractVersion" class="abd-select" style="height:28px; min-height:28px; font-size:11px; font-weight:700; padding:2px 8px; border-radius:6px; max-width:240px;">
                ${versions.map(v => `
                  <option value="${v.id}" ${v.id === activeVersionId ? 'selected' : ''}>
                    ${v.versionNumber} • ${v.name} ${v.isFactory ? '[Factory]' : (v.isDraft ? '[Draft]' : '[Consolidated]')}
                  </option>
                `).join('')}
              </select>
              ${badgeHtml}
            </div>

            <!-- Toggle Diff Button -->
            ${!isCustomTarget || versions.length > 1 ? `
              <button id="btnToggleDiffs" type="button" class="btn btn-outline btn-sm ${showDiffs ? 'active' : ''}" style="display:flex; align-items:center; gap:4px; font-size:10px; font-weight:700; padding:3px 8px; ${showDiffs ? 'background:var(--color-accent-dim); color:var(--color-accent); border-color:var(--color-accent);' : 'color:var(--color-text-dim);'}" title="Toggle diff highlighting against baseline">
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><circle cx="12" cy="12" r="3"/><path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"/></svg>
                <span>${showDiffs ? 'Diffs: ON' : 'Diffs: OFF'}</span>
                ${diffs.isModified && showDiffs ? `<span class="badge badge-accent badge-xs" style="font-size:8.5px; padding:0 4px;">${Object.keys(diffs.fields).length}</span>` : ''}
              </button>
            ` : ''}
          </div>

          <!-- Header Actions -->
          <div style="display:flex; align-items:center; gap:6px;">
            ${!isDraft ? `
              <button id="btnNewDraftFork" type="button" class="btn btn-outline btn-sm" style="display:flex; align-items:center; gap:5px; font-size:10.5px; font-weight:700; padding:4px 10px; color:var(--color-accent); border-color:var(--color-accent);" title="Create a new editable working draft from this version">
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
                <span>+ New Draft</span>
              </button>
            ` : ''}
            <button id="btnDuplicateAsNewTarget" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:600; padding:4px 8px;" title="Clone this contract as an independent hardware device in catalog">
              Duplicate Target...
            </button>
            <button id="btnModalClose" type="button" class="btn btn-outline btn-circle btn-sm" title="Close inspector">✕</button>
          </div>
        </div>

        <!-- TABS BAR -->
        <div style="display:flex; align-items:center; gap:4px; padding:6px 18px; background:var(--color-panel-bg); border-bottom:1px solid var(--color-panel-border); overflow-x:auto;">
          <button data-tab="general" class="contract-tab-btn ${activeTab === 'general' ? 'active' : ''}" style="font-size:11px; font-weight:700; padding:5px 12px; border-radius:12px; border:1px solid transparent; cursor:pointer; background:${activeTab === 'general' ? 'var(--color-accent-dim)' : 'transparent'}; color:${activeTab === 'general' ? 'var(--color-accent)' : 'var(--color-text-dim)'}; border-color:${activeTab === 'general' ? 'var(--color-accent)' : 'transparent'};">
            1. General &amp; Typology
          </button>
          <button data-tab="cabling" class="contract-tab-btn ${activeTab === 'cabling' ? 'active' : ''}" style="font-size:11px; font-weight:700; padding:5px 12px; border-radius:12px; border:1px solid transparent; cursor:pointer; background:${activeTab === 'cabling' ? 'var(--color-accent-dim)' : 'transparent'}; color:${activeTab === 'cabling' ? 'var(--color-accent)' : 'var(--color-text-dim)'}; border-color:${activeTab === 'cabling' ? 'var(--color-accent)' : 'transparent'};">
            2. Physical Cabling (${(currentContract.cablingSteps || []).length})
          </button>
          <button data-tab="lifecycle" class="contract-tab-btn ${activeTab === 'lifecycle' ? 'active' : ''}" style="font-size:11px; font-weight:700; padding:5px 12px; border-radius:12px; border:1px solid transparent; cursor:pointer; background:${activeTab === 'lifecycle' ? 'var(--color-accent-dim)' : 'transparent'}; color:${activeTab === 'lifecycle' ? 'var(--color-accent)' : 'var(--color-text-dim)'}; border-color:${activeTab === 'lifecycle' ? 'var(--color-accent)' : 'transparent'};">
            3. Ground State / Setup (${(currentContract.lifecycle?.preSessionSetup || []).length})
          </button>
          <button data-tab="controls" class="contract-tab-btn ${activeTab === 'controls' ? 'active' : ''}" style="font-size:11px; font-weight:700; padding:5px 12px; border-radius:12px; border:1px solid transparent; cursor:pointer; background:${activeTab === 'controls' ? 'var(--color-accent-dim)' : 'transparent'}; color:${activeTab === 'controls' ? 'var(--color-accent)' : 'var(--color-text-dim)'}; border-color:${activeTab === 'controls' ? 'var(--color-accent)' : 'transparent'};">
            4. Baseline Controls (${(currentContract.controls || []).length})
          </button>
          <button data-tab="json" class="contract-tab-btn ${activeTab === 'json' ? 'active' : ''}" style="font-size:11px; font-weight:700; padding:5px 12px; border-radius:12px; border:1px solid transparent; cursor:pointer; background:${activeTab === 'json' ? 'var(--color-accent-dim)' : 'transparent'}; color:${activeTab === 'json' ? 'var(--color-accent)' : 'var(--color-text-dim)'}; border-color:${activeTab === 'json' ? 'var(--color-accent)' : 'transparent'};">
            5. JSON Schema &amp; History
          </button>
        </div>

        <!-- SCROLLABLE BODY -->
        <div class="test-config-modal-body" style="flex:1; overflow-y:auto; padding:16px 20px; box-sizing:border-box;">
          ${isFactory ? `
            <div style="padding:8px 12px; background:rgba(0, 122, 77, 0.08); border:1px solid rgba(0, 122, 77, 0.25); border-radius:6px; font-size:11px; color:var(--color-text-main); margin-bottom:12px; display:flex; align-items:center; justify-content:space-between; gap:10px;">
              <span><strong>Official Factory Contract (Immutable):</strong> To customize parameters, physical cabling or setup routines, create a <strong>Working Draft</strong>. The certified master contract will never be overwritten.</span>
              <button id="btnBannerFork" type="button" class="btn btn-primary btn-sm" style="font-size:10px; font-weight:700; padding:3px 8px; flex-shrink:0;">+ Start Working Draft</button>
            </div>
          ` : (isDraft ? `
            <div style="padding:8px 12px; background:rgba(217, 119, 6, 0.08); border:1px solid rgba(217, 119, 6, 0.25); border-radius:6px; font-size:11px; color:var(--color-text-main); margin-bottom:12px; display:flex; align-items:center; justify-content:space-between; gap:10px;">
              <span><strong>Active Working Draft (${ver.versionNumber}):</strong> You can edit parameters, photos, cabling and setup actions in phases. Click <em>Save Draft</em> anytime, and click <strong>Consolidate Version</strong> when tests are complete.</span>
              <span class="badge badge-accent badge-sm" style="font-weight:700;">EDITABLE</span>
            </div>
          ` : (isCustomTarget ? `
            <div style="padding:8px 12px; background:rgba(59, 130, 246, 0.08); border:1px solid rgba(59, 130, 246, 0.25); border-radius:6px; font-size:11px; color:var(--color-text-main); margin-bottom:12px; display:flex; align-items:center; justify-content:space-between; gap:10px;">
              <span><strong>Custom Equipment Contract:</strong> This is your custom hardware profile (${ver.versionNumber}). Click <em>+ New Draft</em> to edit parameters or upload photos.</span>
              <button id="btnBannerForkCustom" type="button" class="btn btn-primary btn-sm" style="font-size:10px; font-weight:700; padding:3px 8px; flex-shrink:0;">+ Start Draft</button>
            </div>
          ` : `
            <div style="padding:8px 12px; background:rgba(59, 130, 246, 0.08); border:1px solid rgba(59, 130, 246, 0.25); border-radius:6px; font-size:11px; color:var(--color-text-main); margin-bottom:12px; display:flex; align-items:center; justify-content:space-between; gap:10px;">
              <span><strong>Consolidated Release (${ver.versionNumber}):</strong> This version is sealed for measurement traceability. Start a new working draft to continue customizing.</span>
              <button id="btnBannerForkConsolidated" type="button" class="btn btn-primary btn-sm" style="font-size:10px; font-weight:700; padding:3px 8px; flex-shrink:0;">+ New Draft from this</button>
            </div>
          `))}

          ${renderActiveTabBody(!isDraft)}
        </div>

        <!-- MODAL FOOTER -->
        <div class="test-config-modal-footer" style="padding:10px 18px; border-top:1px solid var(--color-panel-border); background:var(--color-panel-surface); display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
          <div style="display:flex; align-items:center; gap:6px;">
            <!-- In-Modal JSON Import / Upload -->
            <input type="file" id="fileImportJsonModal" accept=".json" style="display:none;" />
            <button id="btnImportJsonModal" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; display:flex; align-items:center; gap:4px;" title="Import external JSON profile contract">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
              <span>Load JSON...</span>
            </button>

            <!-- In-Modal JSON Export / Download -->
            <button id="btnExportJsonFile" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; display:flex; align-items:center; gap:4px;" title="Download active JSON contract for offline editing in any text editor">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
              <span>Download .json</span>
            </button>

            <!-- Discard Active Draft -->
            ${isDraft ? `
              <button id="btnDiscardDraft" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-danger); border-color:rgba(197,34,31,0.3);" title="Discard unsealed draft changes and revert to latest consolidated version">
                Discard Draft
              </button>
            ` : ''}

            <!-- Factory Reset (Purge all user versions for this device) -->
            ${!isCustomTarget && !isFactory ? `
              <button id="btnResetToFactory" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:600; color:var(--color-danger); border-color:rgba(197,34,31,0.25);" title="Purge user customizations and restore certified factory baseline">
                Reset to Factory
              </button>
            ` : ''}

            <!-- Custom Profile Delete (for purely custom equipment) -->
            ${isCustomTarget && versions.length > 0 ? `
              <button id="btnDeleteCustomProfile" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:600; color:var(--color-danger); border-color:rgba(197,34,31,0.25);" title="Delete this custom hardware profile from storage">
                Delete Profile
              </button>
            ` : ''}
          </div>

          <div style="display:flex; align-items:center; gap:6px;">
            <button id="btnCancelModal" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:600; padding:5px 12px;">
              Close
            </button>

            ${isDraft ? `
              <button id="btnSaveDraft" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:700; color:var(--color-accent); border-color:var(--color-accent); padding:5px 12px;" title="Save current draft snapshot without sealing version">
                Save Draft
              </button>

              <button id="btnConsolidateVersion" type="button" class="btn btn-outline btn-sm" style="font-size:11px; font-weight:800; background:rgba(0, 122, 77, 0.12); color:#007a4d; border-color:#007a4d; padding:5px 14px;" title="Seal and consolidate this draft into a permanent immutable version">
                ✓ Consolidate Version
              </button>
            ` : ''}

            <button id="btnApplyToSession" type="button" class="btn btn-primary btn-sm" style="font-size:11.5px; font-weight:800; padding:6px 16px; letter-spacing:0.3px;">
              Apply to Device &rarr;
            </button>
          </div>
        </div>

      </div>
    `;

    attachModalEvents();
  }

  function renderActiveTabBody(isReadOnly) {
    const disabledAttr = isReadOnly ? 'disabled' : '';

    if (activeTab === 'general') {
      const gallery = Array.isArray(currentContract.gallery) && currentContract.gallery.length > 0 
        ? currentContract.gallery 
        : [currentContract.modelImage || currentContract.image || 'models/desktop_black.webp'];
      const activeImg = currentContract.modelImage || currentContract.image || gallery[0];

      return `
        <div style="display:flex; flex-direction:column; gap:12px;">
          <!-- Identification Row -->
          <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
                <span>Commercial Display Name</span>
                ${renderDiffMarker('displayName')}
              </label>
              <input id="inpDisplayName" type="text" class="soundid-modal-input" value="${escapeHtml(currentContract.displayName || currentContract.name || '')}" ${disabledAttr} placeholder="e.g. BOSS DS-1 Distortion" />
            </div>
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:block; margin-bottom:4px; text-transform:uppercase;">Canonical Identifier (ID)</label>
              <input id="inpId" type="text" class="soundid-modal-input" value="${escapeHtml(currentContract.id || '')}" disabled placeholder="e.g. boss_ds1_distortion" style="opacity:0.75; font-family:var(--font-mono, monospace);" />
            </div>
          </div>

          <!-- Typology & Brand Row -->
          <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
                <span>Normative Typology (deviceType)</span>
                ${renderDiffMarker('deviceType')}
              </label>
              <select id="selDeviceType" class="abd-select" style="width:100%; font-size:11.5px;" ${disabledAttr}>
                ${CANONICAL_DEVICE_TYPES.map(dt => `
                  <option value="${dt}" ${(currentContract.deviceType === dt) ? 'selected' : ''}>${dt}</option>
                `).join('')}
              </select>
            </div>
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
                <span>Manufacturer / Brand</span>
                ${renderDiffMarker('brand')}
              </label>
              <input id="inpBrand" type="text" class="soundid-modal-input" value="${escapeHtml(currentContract.brand || currentContract.category || '')}" ${disabledAttr} placeholder="e.g. BOSS, Roland, Custom DIY" />
            </div>
          </div>

          <!-- Circuit Description -->
          <div>
            <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
              <span>Circuit &amp; Architecture Technical Description</span>
              ${renderDiffMarker('description')}
            </label>
            <textarea id="inpDescription" class="soundid-modal-input" rows="2" ${disabledAttr} placeholder="Description of analog filter topology, clipping diodes, discrete VCA...">${escapeHtml(currentContract.description || currentContract.details || '')}</textarea>
          </div>

          <!-- Routing & Protocol -->
          <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
                <span>Signal Routing Guide (Routing Notes)</span>
                ${renderDiffMarker('routingNotes')}
              </label>
              <input id="inpRoutingNotes" type="text" class="soundid-modal-input" value="${escapeHtml(currentContract.routingNotes || (currentContract.routingGuide && currentContract.routingGuide.notes) || '')}" ${disabledAttr} placeholder="e.g. DAC Out 1 -> Pedal In -> Out -> ADC In 1" />
            </div>
            <div>
              <label style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); display:flex; align-items:center; margin-bottom:4px; text-transform:uppercase;">
                <span>Automation Control Protocol</span>
                ${renderDiffMarker('protocol')}
              </label>
              <input id="inpProtocol" type="text" class="soundid-modal-input" value="${escapeHtml(currentContract.protocol || '')}" ${disabledAttr} placeholder="e.g. Manual Potentiometers (Drive / Tone / Level)" />
            </div>
          </div>

          <!-- MIDI Reception Automation Setting -->
          <div style="padding:8px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px; display:flex; align-items:center; justify-content:space-between;">
            <div>
              <span style="font-size:11px; font-weight:700; color:var(--color-text-main); display:block;">MIDI Control Automation Reception</span>
              <span style="font-size:9.5px; color:var(--color-text-dim);">Enable if this hardware receives automated parameter changes via MIDI CC, SysEx, or NRPN.</span>
            </div>
            <label style="display:flex; align-items:center; gap:6px; cursor:${isReadOnly ? 'default' : 'pointer'};">
              <input id="chkContractMidi" type="checkbox" ${currentContract.midi ? 'checked' : ''} ${disabledAttr} style="cursor:pointer;" />
              <span style="font-size:11px; font-weight:700; color:var(--color-text-main);">Target receives MIDI Automation</span>
            </label>
          </div>

          <!-- Device Artwork & Photo Gallery -->
          <div style="padding:10px 12px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px;">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px; flex-wrap:wrap; gap:8px;">
              <div>
                <span style="font-size:10.5px; font-weight:800; color:var(--color-text-main); text-transform:uppercase;">Device Artwork &amp; Photo Gallery</span>
                <span style="font-size:9.5px; color:var(--color-text-dim); display:block;">Click any photo to select as the primary device cover.</span>
              </div>
              <div style="display:flex; gap:6px; align-items:center;">
                <input type="file" id="fileUploadGalleryImage" multiple accept="image/png,image/jpeg,image/webp" style="display:none;" />
                <button id="btnUploadGalleryImage" type="button" class="btn btn-outline btn-sm" style="font-size:10px; font-weight:700; padding:4px 10px; color:var(--color-accent); border-color:var(--color-accent); display:flex; align-items:center; gap:5px;" title="Upload one or more photos to this hardware gallery">
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
                  <span>+ Add Images...</span>
                </button>
              </div>
            </div>

            <!-- Gallery Thumbnails Grid -->
            <div style="display:flex; gap:10px; flex-wrap:wrap; align-items:center;" id="galleryThumbnailsGrid">
              ${gallery.map((img, idx) => {
                const isSelected = (img === activeImg);
                return `
                  <div class="gallery-thumb-item" data-img-idx="${idx}" style="position:relative; width:88px; height:64px; background:var(--color-bg-base); border-radius:6px; border:2px solid ${isSelected ? 'var(--color-accent)' : 'var(--color-panel-border)'}; cursor:pointer; overflow:hidden; display:flex; align-items:center; justify-content:center; box-shadow:0 2px 6px rgba(0,0,0,0.25);" title="Set as primary cover photo">
                    <img src="${escapeHtml(img)}" alt="Target Photo" style="max-width:100%; max-height:100%; object-fit:contain;" />
                    ${isSelected ? `
                      <span class="badge badge-accent badge-xs" style="position:absolute; bottom:2px; left:2px; font-size:7.5px; font-weight:800; padding:0 3px;">✓ COVER</span>
                    ` : ''}
                    <button type="button" class="btn-delete-gallery-img" data-img-idx="${idx}" style="position:absolute; top:2px; right:2px; width:17px; height:17px; border-radius:50%; background:rgba(0,0,0,0.72); color:#fff; border:1px solid rgba(255,255,255,0.3); font-size:9.5px; line-height:1; display:flex; align-items:center; justify-content:center; cursor:pointer;" title="Remove this photo from gallery">✕</button>
                  </div>
                `;
              }).join('')}

              <!-- Visual slot to add more images directly inside the grid -->
              <div id="btnAddGalleryImageSlot" style="width:88px; height:64px; border:2px dashed var(--color-panel-border); border-radius:6px; display:flex; flex-direction:column; align-items:center; justify-content:center; cursor:pointer; color:var(--color-text-dim); gap:3px; background:rgba(255,255,255,0.02); transition:border-color 0.15s ease;" title="Upload one or more photos">
                <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
                <span style="font-size:9px; font-weight:700;">+ Add Image</span>
              </div>
            </div>
          </div>
        </div>
      `;
    }

    if (activeTab === 'cabling') {
      const steps = currentContract.cablingSteps || (currentContract.routingGuide && currentContract.routingGuide.steps) || [];
      return `
        <div style="display:flex; flex-direction:column; gap:10px;">
          <div style="display:flex; justify-content:space-between; align-items:center;">
            <div style="font-size:11px; color:var(--color-text-dim); display:flex; align-items:center;">
              <span>Step-by-step physical cabling checklist for the operator before test excitation.</span>
              ${renderDiffMarker('cablingSteps')}
            </div>
            ${!isReadOnly ? `
              <button id="btnAddCablingStep" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-accent); border-color:var(--color-accent); padding:3px 10px;">
                + Add Step
              </button>
            ` : ''}
          </div>

          <div style="display:flex; flex-direction:column; gap:8px;" id="cablingStepsListContainer">
            ${steps.length > 0 ? steps.map((s, idx) => `
              <div class="cabling-step-row" style="display:flex; align-items:center; gap:8px; padding:8px 10px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px;">
                <span class="badge badge-accent badge-sm" style="font-weight:800; width:22px; text-align:center; flex-shrink:0;">${idx + 1}</span>
                <input type="text" class="soundid-modal-input cabling-step-input" data-step-idx="${idx}" value="${escapeHtml(s)}" ${disabledAttr} style="flex:1; padding:6px 10px; font-size:11.5px;" />
                ${!isReadOnly ? `
                  <button type="button" class="btn btn-outline btn-sm btn-delete-cabling-step" data-step-idx="${idx}" style="color:var(--color-danger); border-color:rgba(197,34,31,0.3); padding:4px 8px;" title="Delete this step">✕</button>
                ` : ''}
              </div>
            `).join('') : `
              <div style="padding:16px; text-align:center; color:var(--color-text-dim); font-size:11px; background:var(--color-panel-surface-elevated); border:1px dashed var(--color-panel-border); border-radius:6px;">
                No physical cabling steps defined.
              </div>
            `}
          </div>
        </div>
      `;
    }

    if (activeTab === 'lifecycle') {
      const actions = currentContract.lifecycle?.preSessionSetup || [];
      return `
        <div style="display:flex; flex-direction:column; gap:10px;">
          <div style="display:flex; justify-content:space-between; align-items:center;">
            <div style="font-size:11px; color:var(--color-text-dim); display:flex; align-items:center;">
              <span>Idempotent initialization sequence executed automatically before measurement.</span>
              ${renderDiffMarker('lifecycle')}
            </div>
            ${!isReadOnly ? `
              <button id="btnAddLifecycleAction" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-accent); border-color:var(--color-accent); padding:3px 10px;">
                + Add Setup Action
              </button>
            ` : ''}
          </div>

          <div style="display:flex; flex-direction:column; gap:8px;" id="lifecycleActionsContainer">
            ${actions.length > 0 ? actions.map((act, idx) => `
              <div style="padding:10px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px; display:flex; flex-direction:column; gap:6px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                  <span style="font-size:10px; font-weight:800; color:var(--color-text-muted); text-transform:uppercase;">Action #${idx + 1}</span>
                  ${!isReadOnly ? `
                    <button type="button" class="btn btn-outline btn-sm btn-delete-lifecycle-action" data-action-idx="${idx}" style="color:var(--color-danger); border-color:rgba(197,34,31,0.3); padding:2px 6px; font-size:10px;">✕ Delete</button>
                  ` : ''}
                </div>
                <div style="display:grid; grid-template-columns:2fr 1fr; gap:8px;">
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">Action Description</label>
                    <input type="text" class="soundid-modal-input lf-desc" data-action-idx="${idx}" value="${escapeHtml(act.description || '')}" ${disabledAttr} placeholder="e.g. Reset All Controllers (MIDI CC 121)" style="font-size:11px; padding:5px 8px;" />
                  </div>
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">Dispatch Method</label>
                    <select class="abd-select lf-method" data-action-idx="${idx}" style="width:100%; font-size:11px; height:32px; min-height:32px;" ${disabledAttr}>
                      <option value="MIDI_CC" ${act.method === 'MIDI_CC' ? 'selected' : ''}>MIDI_CC (Control Change)</option>
                      <option value="SYSEX_RAW" ${act.method === 'SYSEX_RAW' ? 'selected' : ''}>SYSEX_RAW (System Exclusive)</option>
                      <option value="MANUAL_PROMPT" ${act.method === 'MANUAL_PROMPT' ? 'selected' : ''}>MANUAL_PROMPT (Operator Prompt)</option>
                      <option value="NRPN" ${act.method === 'NRPN' ? 'selected' : ''}>NRPN (14-bit)</option>
                    </select>
                  </div>
                </div>
                <div style="display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:8px;">
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">MIDI Ch (1-16)</label>
                    <input type="number" min="1" max="16" class="soundid-modal-input lf-channel" data-action-idx="${idx}" value="${act.channel || 1}" ${disabledAttr} style="font-size:11px; padding:5px 8px;" />
                  </div>
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">CC / Parameter</label>
                    <input type="number" min="-1" max="127" class="soundid-modal-input lf-cc" data-action-idx="${idx}" value="${typeof act.cc === 'number' ? act.cc : (act.controlNumber || -1)}" ${disabledAttr} style="font-size:11px; padding:5px 8px;" />
                  </div>
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">Value (0.0 - 1.0)</label>
                    <input type="number" step="0.01" min="0" max="1" class="soundid-modal-input lf-val" data-action-idx="${idx}" value="${typeof act.normalizedValue === 'number' ? act.normalizedValue : (typeof act.value === 'number' ? act.value : 0.5)}" ${disabledAttr} style="font-size:11px; padding:5px 8px;" />
                  </div>
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">Delay (ms)</label>
                    <input type="number" min="0" max="5000" class="soundid-modal-input lf-delay" data-action-idx="${idx}" value="${act.settlingDelayMs || 50}" ${disabledAttr} style="font-size:11px; padding:5px 8px;" />
                  </div>
                </div>
                ${(act.method === 'SYSEX_RAW' || act.sysexHexPayload) ? `
                  <div>
                    <label style="font-size:9.5px; font-weight:700; color:var(--color-text-dim);">SysEx Hex Payload</label>
                    <input type="text" class="soundid-modal-input lf-sysex" data-action-idx="${idx}" value="${escapeHtml(act.sysexHexPayload || '')}" ${disabledAttr} placeholder="F0 41 10 00 ... F7" style="font-family:var(--font-mono, monospace); font-size:10.5px; padding:5px 8px;" />
                  </div>
                ` : ''}
              </div>
            `).join('') : `
              <div style="padding:16px; text-align:center; color:var(--color-text-dim); font-size:11px; background:var(--color-panel-surface-elevated); border:1px dashed var(--color-panel-border); border-radius:6px;">
                No setup actions defined.
              </div>
            `}
          </div>
        </div>
      `;
    }

    if (activeTab === 'controls') {
      const ctrls = currentContract.controls || [];
      return `
        <div style="display:flex; flex-direction:column; gap:10px;">
          <div style="display:flex; justify-content:space-between; align-items:center;">
            <div style="font-size:11px; color:var(--color-text-dim); display:flex; align-items:center;">
              <span>Baseline potentiometers and faders evaluated in Stage 3 Card 3B for profiling.</span>
              ${renderDiffMarker('controls')}
            </div>
            ${!isReadOnly ? `
              <button id="btnAddControl" type="button" class="btn btn-outline btn-sm" style="font-size:10.5px; font-weight:700; color:var(--color-accent); border-color:var(--color-accent); padding:3px 10px;">
                + Add Control
              </button>
            ` : ''}
          </div>

          <div style="display:flex; flex-direction:column; gap:8px;" id="controlsContainer">
            ${ctrls.length > 0 ? ctrls.map((c, idx) => `
              <div style="padding:8px 10px; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px; display:grid; grid-template-columns:2fr 1fr 1fr 1fr auto; gap:8px; align-items:center;">
                <div>
                  <label style="font-size:9px; font-weight:700; color:var(--color-text-dim);">Name</label>
                  <input type="text" class="soundid-modal-input ctrl-name" data-ctrl-idx="${idx}" value="${escapeHtml(c.name || '')}" ${disabledAttr} placeholder="TONE, CUTOFF..." style="font-size:11px; padding:5px 8px;" />
                </div>
                <div>
                  <label style="font-size:9px; font-weight:700; color:var(--color-text-dim);">Type</label>
                  <select class="abd-select ctrl-type" data-ctrl-idx="${idx}" style="width:100%; font-size:11px; height:32px; min-height:32px;" ${disabledAttr}>
                    <option value="Knob" ${c.type === 'Knob' ? 'selected' : ''}>Knob (Rotary)</option>
                    <option value="Slider" ${c.type === 'Slider' ? 'selected' : ''}>Slider (Fader)</option>
                    <option value="Switch" ${c.type === 'Switch' ? 'selected' : ''}>Switch</option>
                  </select>
                </div>
                <div>
                  <label style="font-size:9px; font-weight:700; color:var(--color-text-dim);">Default (0.0-1.0)</label>
                  <input type="number" step="0.05" min="0" max="1" class="soundid-modal-input ctrl-default" data-ctrl-idx="${idx}" value="${typeof c.defaultVal === 'number' ? c.defaultVal : 0.5}" ${disabledAttr} style="font-size:11px; padding:5px 8px;" />
                </div>
                <div>
                  <label style="font-size:9px; font-weight:700; color:var(--color-text-dim);">Unit</label>
                  <input type="text" class="soundid-modal-input ctrl-unit" data-ctrl-idx="${idx}" value="${escapeHtml(c.unit || '%')}" ${disabledAttr} placeholder="%" style="font-size:11px; padding:5px 8px;" />
                </div>
                ${!isReadOnly ? `
                  <div style="padding-top:14px;">
                    <button type="button" class="btn btn-outline btn-sm btn-delete-control" data-ctrl-idx="${idx}" style="color:var(--color-danger); border-color:rgba(197,34,31,0.3); padding:4px 8px;" title="Delete control">✕</button>
                  </div>
                ` : '<div></div>'}
              </div>
            `).join('') : `
              <div style="padding:16px; text-align:center; color:var(--color-text-dim); font-size:11px; background:var(--color-panel-surface-elevated); border:1px dashed var(--color-panel-border); border-radius:6px;">
                No baseline controls declared.
              </div>
            `}
          </div>
        </div>
      `;
    }

    if (activeTab === 'json') {
      const historyEntries = currentContract.history || [];
      const diffs = computeDiffs();
      return `
        <div style="display:flex; flex-direction:column; gap:12px;">
          <!-- Diff Summary Card -->
          ${showDiffs && diffs.summary.length > 0 ? `
            <div style="background:var(--color-panel-surface-elevated); border:1px solid ${diffs.isModified ? 'var(--color-accent)' : 'var(--color-panel-border)'}; border-radius:6px; padding:10px 12px;">
              <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
                <span style="font-size:10.5px; font-weight:800; color:var(--color-text-main); text-transform:uppercase;">
                  Modifications vs Baseline (${baselineContract.displayName || baselineContract.name || 'Official'})
                </span>
                <span class="badge ${diffs.isModified ? 'badge-accent' : 'badge-success'} badge-xs">
                  ${diffs.isModified ? `${diffs.summary.length} CHANGE(S) DETECTED` : 'IDENTICAL TO BASELINE'}
                </span>
              </div>
              <ul style="margin:0; padding-left:18px; font-size:11px; color:var(--color-text-dim); display:flex; flex-direction:column; gap:3px;">
                ${diffs.summary.map(s => `<li>${escapeHtml(s)}</li>`).join('')}
              </ul>
            </div>
          ` : ''}

          <!-- History Section -->
          <div>
            <div style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); text-transform:uppercase; margin-bottom:6px;">Sealed Revision History</div>
            <div style="background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:6px; padding:8px; max-height:130px; overflow-y:auto;">
              ${historyEntries.length > 0 ? historyEntries.map(h => `
                <div style="padding:4px 0; border-bottom:1px solid rgba(255,255,255,0.05); font-size:11px; display:flex; justify-content:space-between; align-items:center;">
                  <div>
                    <strong style="color:var(--color-accent);">${h.version || 'v2.0'}</strong> • <span>${escapeHtml(h.notes || 'Unannotated revision')}</span>
                  </div>
                  <span style="font-size:9.5px; color:var(--color-text-dim);">${h.timestamp ? new Date(h.timestamp).toLocaleDateString() : ''}</span>
                </div>
              `).join('') : `
                <div style="font-size:10.5px; color:var(--color-text-dim); text-align:center; padding:6px;">No previous revision history.</div>
              `}
            </div>
          </div>

          <!-- Raw JSON Editor -->
          <div>
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
              <span style="font-size:10.5px; font-weight:700; color:var(--color-text-muted); text-transform:uppercase;">Raw JSON Schema 2.0 Editor</span>
              ${!isReadOnly ? `
                <button id="btnFormatJson" type="button" class="btn btn-outline btn-sm" style="font-size:10px; font-weight:700; padding:2px 8px;">Format JSON</button>
              ` : ''}
            </div>
            <textarea id="txtRawJson" class="soundid-modal-input" rows="11" ${disabledAttr} style="font-family:var(--font-mono, monospace); font-size:11px; line-height:1.35; white-space:pre;">${escapeHtml(JSON.stringify(currentContract, null, 2))}</textarea>
          </div>
        </div>
      `;
    }

    return '';
  }

  function syncFormToContract() {
    const curVer = getActiveVersion();
    if (!curVer.isDraft && !isCustomTarget) return;

    if (activeTab === 'general') {
      const disp = overlay.querySelector('#inpDisplayName');
      if (disp) currentContract.displayName = disp.value.trim();
      const devT = overlay.querySelector('#selDeviceType');
      if (devT) currentContract.deviceType = devT.value;
      const br = overlay.querySelector('#inpBrand');
      if (br) currentContract.brand = br.value.trim();
      const desc = overlay.querySelector('#inpDescription');
      if (desc) currentContract.description = desc.value.trim();
      const rn = overlay.querySelector('#inpRoutingNotes');
      if (rn) currentContract.routingNotes = rn.value.trim();
      const pr = overlay.querySelector('#inpProtocol');
      if (pr) currentContract.protocol = pr.value.trim();
      const chkM = overlay.querySelector('#chkContractMidi');
      if (chkM) currentContract.midi = chkM.checked;
    } else if (activeTab === 'cabling') {
      const inputs = overlay.querySelectorAll('.cabling-step-input');
      const updatedSteps = [];
      inputs.forEach(inp => {
        const val = inp.value.trim();
        if (val) updatedSteps.push(val);
      });
      currentContract.cablingSteps = updatedSteps;
    } else if (activeTab === 'lifecycle') {
      const rows = overlay.querySelectorAll('#lifecycleActionsContainer > div');
      const updatedActions = [];
      rows.forEach((row, idx) => {
        const desc = row.querySelector('.lf-desc')?.value.trim() || '';
        const method = row.querySelector('.lf-method')?.value || 'MIDI_CC';
        const ch = parseInt(row.querySelector('.lf-channel')?.value, 10) || 1;
        const cc = parseInt(row.querySelector('.lf-cc')?.value, 10);
        const val = parseFloat(row.querySelector('.lf-val')?.value);
        const delay = parseInt(row.querySelector('.lf-delay')?.value, 10) || 50;
        const sysex = row.querySelector('.lf-sysex')?.value.trim();

        const act = {
          description: desc,
          method: method,
          settlingDelayMs: delay
        };
        if (method === 'MIDI_CC') {
          act.channel = ch;
          act.cc = !isNaN(cc) ? cc : 74;
          act.normalizedValue = !isNaN(val) ? val : 0.5;
        } else if (method === 'NRPN') {
          act.channel = ch;
          act.nrpn = !isNaN(cc) ? cc : 0;
          act.normalizedValue = !isNaN(val) ? val : 0.5;
        } else if (method === 'SYSEX_RAW') {
          act.sysexHexPayload = sysex || '';
        } else if (method === 'MANUAL_PROMPT') {
          act.normalizedValue = !isNaN(val) ? val : 0.5;
        }
        updatedActions.push(act);
      });
      if (!currentContract.lifecycle) currentContract.lifecycle = {};
      currentContract.lifecycle.preSessionSetup = updatedActions;
    } else if (activeTab === 'controls') {
      const rows = overlay.querySelectorAll('#controlsContainer > div');
      const updatedCtrls = [];
      rows.forEach((row, idx) => {
        const name = row.querySelector('.ctrl-name')?.value.trim();
        if (!name) return;
        const type = row.querySelector('.ctrl-type')?.value || 'Knob';
        const defVal = parseFloat(row.querySelector('.ctrl-default')?.value);
        const unit = row.querySelector('.ctrl-unit')?.value.trim() || '%';
        updatedCtrls.push({
          name: name,
          type: type,
          defaultVal: !isNaN(defVal) ? defVal : 0.5,
          unit: unit
        });
      });
      currentContract.controls = updatedCtrls;
    } else if (activeTab === 'json') {
      const txt = overlay.querySelector('#txtRawJson');
      if (txt) {
        try {
          const parsed = JSON.parse(txt.value);
          currentContract = parsed;
        } catch (_) {}
      }
    }
  }

  function attachModalEvents() {
    // Tab switching
    overlay.querySelectorAll('.contract-tab-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        syncFormToContract();
        activeTab = e.currentTarget.getAttribute('data-tab');
        render();
      });
    });

    // Version selector dropdown
    const cbo = overlay.querySelector('#cboContractVersion');
    if (cbo) {
      cbo.addEventListener('change', (e) => {
        syncFormToContract();
        activeVersionId = e.target.value;
        const newVer = getActiveVersion();
        currentContract = JSON.parse(JSON.stringify(newVer.contract));
        render();
      });
    }

    // Toggle Diffs
    overlay.querySelector('#btnToggleDiffs')?.addEventListener('click', () => {
      showDiffs = !showDiffs;
      render();
    });

    // Revert individual field to baseline
    overlay.querySelectorAll('.btn-revert-field').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const fieldKey = e.currentTarget.getAttribute('data-revert-field');
        if (fieldKey && baselineContract[fieldKey] !== undefined) {
          syncFormToContract();
          currentContract[fieldKey] = JSON.parse(JSON.stringify(baselineContract[fieldKey]));
          onToast(`Field "${fieldKey}" reverted to baseline.`, 'info');
          render();
        }
      });
    });

    // Helper to ensure an editable draft is active before mutating photos or parameters
    const ensureEditableDraft = async (title = 'Start Working Draft', message = '') => {
      const curVer = getActiveVersion();
      if (curVer.isDraft) return true;

      const existingDraft = ContractVersionStore.getActiveDraft(baseId, isCustomTarget ? null : factoryContract);
      if (existingDraft) {
        activeVersionId = existingDraft.id;
        currentContract = JSON.parse(JSON.stringify(existingDraft.contract));
        render();
        return true;
      }

      const confirmed = await showModalConfirm({
        title,
        message: message || `The active version "${curVer.versionNumber}" is an immutable certified contract. To customize photos, cabling, or parameters, a working draft will be created.\n\nStart working draft now?`,
        type: 'info',
        confirmText: 'Create Draft',
        cancelText: 'Cancel'
      });

      if (!confirmed) return false;

      const lastVer = versions[versions.length - 1];
      const parts = (lastVer.versionNumber || '1.0.0').replace(/-draft$/, '').split('.').map(n => parseInt(n, 10) || 0);
      const nextMinor = `${parts[0] || 1}.${(parts[1] || 0) + 1}.0-draft`;

      const draftId = `${baseId}_v${nextMinor.replace(/[^a-z0-9]/gi, '_')}_draft_${Date.now()}`;
      const contractCopy = JSON.parse(JSON.stringify(currentContract));
      contractCopy.schemaVersion = "2.0";

      const newDraftObj = {
        id: draftId,
        baseProfileId: baseId,
        versionNumber: nextMinor,
        name: `Draft ${nextMinor.replace('-draft', '')}`,
        notes: 'Working draft for custom parameters and photos.',
        author: 'User',
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        isFactory: false,
        isDraft: true,
        isConsolidated: false,
        contract: contractCopy
      };

      ContractVersionStore.saveUserVersion(baseId, newDraftObj);
      versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);
      activeVersionId = draftId;
      currentContract = JSON.parse(JSON.stringify(contractCopy));
      onToast(`Working draft created: ${newDraftObj.name}`, 'success');
      render();
      return true;
    };

    // Gallery: Select image as cover
    overlay.querySelectorAll('.gallery-thumb-item').forEach(thumb => {
      thumb.addEventListener('click', async (e) => {
        if (e.target.classList.contains('btn-delete-gallery-img')) return;
        const idx = parseInt(thumb.getAttribute('data-img-idx'), 10);
        if (Array.isArray(currentContract.gallery) && currentContract.gallery[idx]) {
          const selectedImg = currentContract.gallery[idx];
          if (currentContract.modelImage === selectedImg && currentContract.image === selectedImg) return;

          const ok = await ensureEditableDraft(
            'Start Working Draft to Set Cover',
            `The active version is an immutable contract. To change the primary cover photo, a working draft will be created.\n\nStart working draft and set cover now?`
          );
          if (!ok) return;

          currentContract.modelImage = selectedImg;
          currentContract.image = selectedImg;
          onToast('Primary cover photo updated.', 'info');
          render();
        }
      });
    });

    // Gallery: Delete image (logical unlinking from contract, protecting system factory assets)
    overlay.querySelectorAll('.btn-delete-gallery-img').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        e.stopPropagation();
        const ok = await ensureEditableDraft(
          'Start Working Draft to Remove Photo',
          `The active version is an immutable contract. To remove photos from this profile, a working draft will be created.\n\nStart working draft now?`
        );
        if (!ok) return;

        const idx = parseInt(btn.getAttribute('data-img-idx'), 10);
        const targetImg = currentContract.gallery && currentContract.gallery[idx];
        if (!targetImg) return;

        const isFactoryAsset = typeof targetImg === 'string' && (targetImg.startsWith('models/') || targetImg.startsWith('interfaces/'));

        const confirmed = await showModalConfirm({
          title: 'Remove Photo from Gallery',
          message: isFactoryAsset
            ? 'This photo is a packaged factory system asset. It will be removed logically from this device profile gallery, while the original system file remains protected on disk.\n\nRemove from this profile?'
            : 'Remove this photo from the device gallery? It will be unlinked from the contract profile and JSON definition.',
          type: 'warning',
          confirmText: 'Remove Photo',
          cancelText: 'Cancel',
          danger: true
        });

        if (confirmed) {
          currentContract.gallery.splice(idx, 1);
          if (currentContract.modelImage === targetImg || currentContract.image === targetImg) {
            const nextCover = currentContract.gallery[0] || '';
            currentContract.modelImage = nextCover;
            currentContract.image = nextCover;
          }
          onToast('Photo removed from gallery.', 'info');
          render();
        }
      });
    });

    // Gallery: Upload new photo(s)
    const triggerAddImages = async () => {
      const ok = await ensureEditableDraft(
        'Start Working Draft to Add Photos',
        `The active version is an immutable contract. To add custom photos and customize this equipment profile, a working draft will be created.\n\nStart working draft and select photos now?`
      );
      if (!ok) return;

      const fileInput = overlay.querySelector('#fileUploadGalleryImage');
      if (fileInput) fileInput.click();
    };

    overlay.querySelector('#btnUploadGalleryImage')?.addEventListener('click', triggerAddImages);
    overlay.querySelector('#btnAddGalleryImageSlot')?.addEventListener('click', triggerAddImages);

    const fileUploadInput = overlay.querySelector('#fileUploadGalleryImage');
    if (fileUploadInput) {
      fileUploadInput.addEventListener('change', async (e) => {
        const files = Array.from(e.target.files || []);
        if (files.length === 0) return;
        if (!Array.isArray(currentContract.gallery)) currentContract.gallery = [];

        let addedCount = 0;
        let skippedCount = 0;

        for (const file of files) {
          try {
            const result = await optimizeImageToWebP(file);
            if (result && result.dataUrl) {
              // Deduplication by SHA-256 hash or data URL content
              const alreadyExists = currentContract.gallery.some(img => {
                if (typeof img === 'string') {
                  return img === result.dataUrl || img.includes(result.shortHash);
                }
                return img?.sha256 === result.sha256;
              });

              if (alreadyExists) {
                skippedCount++;
                continue;
              }

              currentContract.gallery.push(result.dataUrl);
              addedCount++;

              if (!currentContract.modelImage || !currentContract.image) {
                currentContract.modelImage = result.dataUrl;
                currentContract.image = result.dataUrl;
              }
            }
          } catch (err) {
            console.error('[Gallery] Failed to optimize image:', file.name, err);
          }
        }

        e.target.value = '';
        if (addedCount > 0) {
          const skippedMsg = skippedCount > 0 ? ` (${skippedCount} duplicates skipped by SHA-256)` : '';
          onToast(`${addedCount} photo(s) converted to WebP & added${skippedMsg}.`, 'success');
        } else if (skippedCount > 0) {
          onToast(`Selected photo(s) already exist in gallery (deduplicated by SHA-256).`, 'info');
        }
        render();
      });
    }

    // Close Modal Button
    overlay.querySelector('#btnModalClose')?.addEventListener('click', () => closeModal());
    overlay.querySelector('#btnCancelModal')?.addEventListener('click', () => closeModal());

    // --- NEW WORKING DRAFT HANDLER ---
    const newDraftHandler = async () => {
      syncFormToContract();
      const existingDraft = ContractVersionStore.getActiveDraft(baseId, isCustomTarget ? null : factoryContract);
      if (existingDraft) {
        showModalAlert({
          title: 'Draft Already in Progress',
          message: `An active working draft (${existingDraft.versionNumber}) is already in progress. Only one active draft is allowed per device. Switching to your active draft so you can continue editing.`,
          type: 'info'
        });
        activeVersionId = existingDraft.id;
        currentContract = JSON.parse(JSON.stringify(existingDraft.contract));
        render();
        return;
      }

      // Calculate next version tag
      const lastVer = versions[versions.length - 1];
      const parts = (lastVer.versionNumber || '1.0.0').replace(/-draft$/, '').split('.').map(n => parseInt(n, 10) || 0);
      const nextMinor = `${parts[0] || 1}.${(parts[1] || 0) + 1}.0-draft`;

      const draftName = await showSoundIdPrompt({
        title: 'Create Working Draft',
        message: `Create working draft "${nextMinor}". You will be able to customize parameters, cabling, and photos in phases before sealing.`,
        defaultValue: `Draft ${nextMinor.replace('-draft', '')}`,
        confirmText: 'Create Draft',
        cancelText: 'Cancel'
      });

      if (!draftName) return;

      const draftId = `${baseId}_v${nextMinor.replace(/[^a-z0-9]/gi, '_')}_draft_${Date.now()}`;
      const contractCopy = JSON.parse(JSON.stringify(currentContract));
      contractCopy.schemaVersion = "2.0";

      const newDraftObj = {
        id: draftId,
        baseProfileId: baseId,
        versionNumber: nextMinor,
        name: draftName.trim(),
        notes: 'Working draft in progress.',
        author: 'User',
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        isFactory: false,
        isDraft: true,
        isConsolidated: false,
        contract: contractCopy
      };

      ContractVersionStore.saveUserVersion(baseId, newDraftObj);
      versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);
      activeVersionId = draftId;
      currentContract = JSON.parse(JSON.stringify(contractCopy));
      onToast(`Working draft created: ${draftName}`, 'success');
      render();
    };

    overlay.querySelector('#btnNewDraftFork')?.addEventListener('click', newDraftHandler);
    overlay.querySelector('#btnBannerFork')?.addEventListener('click', newDraftHandler);
    overlay.querySelector('#btnBannerForkConsolidated')?.addEventListener('click', newDraftHandler);
    overlay.querySelector('#btnBannerForkCustom')?.addEventListener('click', newDraftHandler);

    // --- DUPLICATE AS NEW INDEPENDENT TARGET ---
    overlay.querySelector('#btnDuplicateAsNewTarget')?.addEventListener('click', async () => {
      syncFormToContract();
      const currentName = currentContract.displayName || currentContract.name || 'Custom Hardware';
      const newName = await showSoundIdPrompt({
        title: 'Duplicate as Independent Target',
        message: 'Enter the commercial name for the new independent device profile in your catalog:',
        defaultValue: `${currentName} (Custom)`,
        confirmText: 'Create Target',
        cancelText: 'Cancel'
      });

      if (!newName || !newName.trim()) return;

      const cleanNewId = 'custom_' + newName.toLowerCase().replace(/[^a-z0-9_]/g, '_').replace(/^_+|_+$/g, '');
      const newContract = JSON.parse(JSON.stringify(currentContract));
      newContract.id = cleanNewId;
      newContract.displayName = newName.trim();
      newContract.name = newName.trim();
      newContract.schemaVersion = "2.0";

      const initialVer = {
        id: `${cleanNewId}_v1_0_0_user`,
        baseProfileId: cleanNewId,
        versionNumber: '1.0.0',
        name: `${newName.trim()} (v1.0.0)`,
        notes: `Cloned from ${baseId}`,
        author: 'User',
        createdAt: new Date().toISOString(),
        isFactory: false,
        isDraft: false,
        isConsolidated: true,
        isCustom: true,
        contract: newContract
      };

      ContractVersionStore.saveUserVersion(cleanNewId, initialVer);
      onToast(`Cloned target created: ${newName.trim()}`, 'success');

      if (typeof onApply === 'function') {
        onApply(newContract);
      }
      closeModal();
    });

    // --- SAVE DRAFT IN PLACE ---
    overlay.querySelector('#btnSaveDraft')?.addEventListener('click', () => {
      const curVer = getActiveVersion();
      syncFormToContract();
      curVer.contract = JSON.parse(JSON.stringify(currentContract));
      curVer.updatedAt = new Date().toISOString();
      ContractVersionStore.saveUserVersion(baseId, curVer);
      onToast(`Draft saved successfully (${curVer.versionNumber}).`, 'success');
      render();
    });

    // --- CONSOLIDATE VERSION ---
    overlay.querySelector('#btnConsolidateVersion')?.addEventListener('click', async () => {
      const curVer = getActiveVersion();
      syncFormToContract();

      // Pre-consolidation Schema Validation
      const schemaErrors = validateContractSchema(currentContract);
      if (schemaErrors.length > 0) {
        showModalAlert({
          title: 'Contract Validation Errors (Schema 2.0)',
          message: 'Please resolve the following issues before consolidating:\n\n• ' + schemaErrors.join('\n• '),
          type: 'error'
        });
        return;
      }

      const cleanVerNumber = (curVer.versionNumber || '1.1.0').replace(/-draft$/, '');
      const notes = await showSoundIdPrompt({
        title: `Consolidate Version ${cleanVerNumber}`,
        message: 'Briefly describe your calibrations or improvements to seal into the permanent history log:',
        defaultValue: 'Verified level calibration and physical cabling routine.',
        confirmText: 'Seal & Consolidate',
        cancelText: 'Cancel'
      });

      if (notes === null) return;

      if (!Array.isArray(currentContract.history)) {
        currentContract.history = [];
      }
      currentContract.history.push({
        version: cleanVerNumber,
        timestamp: new Date().toISOString(),
        author: 'User',
        notes: notes.trim() || 'Consolidated release'
      });

      curVer.versionNumber = cleanVerNumber;
      curVer.name = curVer.name.replace(/\s*\(Draft\)/i, '').replace(/-draft/i, '') || `Version ${cleanVerNumber}`;
      curVer.notes = notes.trim();
      curVer.isDraft = false;
      curVer.isConsolidated = true;
      curVer.consolidatedAt = new Date().toISOString();
      curVer.contract = JSON.parse(JSON.stringify(currentContract));

      ContractVersionStore.saveUserVersion(baseId, curVer);
      versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);
      onToast(`Version ${cleanVerNumber} consolidated successfully!`, 'success');
      render();
    });

    // --- DISCARD ACTIVE DRAFT ---
    overlay.querySelector('#btnDiscardDraft')?.addEventListener('click', async () => {
      const curVer = getActiveVersion();
      if (!curVer.isDraft) return;

      const confirmed = await showModalConfirm({
        title: 'Discard Working Draft',
        message: `Are you sure you want to discard draft "${curVer.name}"? All unsealed changes will be lost.`,
        type: 'warning',
        confirmText: 'Discard Draft',
        cancelText: 'Keep Draft'
      });

      if (confirmed) {
        ContractVersionStore.deleteUserVersion(baseId, curVer.id);
        versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);
        activeVersionId = versions[versions.length - 1].id;
        currentContract = JSON.parse(JSON.stringify(getActiveVersion().contract));
        onToast('Draft discarded successfully.', 'info');
        render();
      }
    });

    // --- RESET ALL USER VERSIONS TO FACTORY ---
    overlay.querySelector('#btnResetToFactory')?.addEventListener('click', async () => {
      const confirmed = await showModalConfirm({
        title: 'Reset to Factory Baseline',
        message: 'Are you sure you want to remove all custom drafts and revisions for this device? It will be restored 100% to certified factory defaults.',
        type: 'warning',
        confirmText: 'Reset to Factory',
        cancelText: 'Cancel'
      });

      if (confirmed) {
        ContractVersionStore.resetToFactory(baseId);
        versions = ContractVersionStore.getVersions(baseId, factoryContract);
        activeVersionId = versions[0].id;
        currentContract = JSON.parse(JSON.stringify(getActiveVersion().contract));
        onToast('Restored to official factory contract.', 'info');
        render();
      }
    });

    // --- DELETE CUSTOM PROFILE ---
    overlay.querySelector('#btnDeleteCustomProfile')?.addEventListener('click', async () => {
      const confirmed = await showModalConfirm({
        title: 'Delete Custom Profile',
        message: 'Are you sure you want to delete this custom hardware profile and all its versions from storage?',
        type: 'warning',
        confirmText: 'Delete Profile',
        cancelText: 'Cancel'
      });

      if (confirmed) {
        ContractVersionStore.resetToFactory(baseId);
        onToast('Custom profile removed.', 'info');
        closeModal();
      }
    });

    // --- APPLY TO ACTIVE SESSION ---
    overlay.querySelector('#btnApplyToSession')?.addEventListener('click', () => {
      syncFormToContract();
      const curVer = getActiveVersion();
      if (curVer.isDraft || isCustomTarget) {
        curVer.contract = JSON.parse(JSON.stringify(currentContract));
        ContractVersionStore.saveUserVersion(baseId, curVer);
      }

      if (typeof onApply === 'function') {
        onApply(JSON.parse(JSON.stringify(currentContract)));
      }
      onToast(`Contract applied to active session (${curVer.versionNumber}).`, 'success');
      closeModal();
    });

    // --- EXPORT JSON FILE (OFFLINE EDITING) ---
    overlay.querySelector('#btnExportJsonFile')?.addEventListener('click', () => {
      syncFormToContract();
      const curVer = getActiveVersion();
      const exportCopy = JSON.parse(JSON.stringify(currentContract));
      exportCopy.schemaVersion = "2.0";
      exportCopy.profileVersion = curVer.versionNumber || "1.0.0";
      
      const jsonStr = JSON.stringify(exportCopy, null, 2);
      const blob = new Blob([jsonStr], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${baseId}_${curVer.versionNumber || 'contract'}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      onToast(`Contract exported: ${a.download}`, 'info');
    });

    // --- IN-MODAL JSON IMPORT ---
    const btnImportModal = overlay.querySelector('#btnImportJsonModal');
    const fileImportInput = overlay.querySelector('#fileImportJsonModal');
    if (btnImportModal && fileImportInput) {
      btnImportModal.addEventListener('click', () => fileImportInput.click());
      fileImportInput.addEventListener('change', (e) => {
        const file = e.target.files && e.target.files[0];
        if (!file) return;

        const reader = new FileReader();
        reader.onload = async (ev) => {
          try {
            const imported = JSON.parse(ev.target.result);
            const valErrors = validateContractSchema(imported);
            if (valErrors.length > 0) {
              showModalAlert({
                title: 'Invalid JSON Contract Schema',
                message: 'The imported file violates Schema 2.0:\n\n• ' + valErrors.join('\n• '),
                type: 'error'
              });
              return;
            }

            const curVer = getActiveVersion();
            if (curVer.isDraft) {
              const confirmReplace = await showModalConfirm({
                title: 'Import Contract into Active Draft',
                message: `Do you want to replace current draft (${curVer.versionNumber}) contents with data from "${file.name}"?`,
                type: 'info',
                confirmText: 'Replace in Draft',
                cancelText: 'Cancel'
              });
              if (confirmReplace) {
                currentContract = imported;
                curVer.contract = imported;
                ContractVersionStore.saveUserVersion(baseId, curVer);
                onToast('Contract loaded into active draft.', 'success');
                render();
              }
            } else {
              const newDraftId = `${baseId}_imported_${Date.now()}`;
              const importedDraft = {
                id: newDraftId,
                baseProfileId: baseId,
                versionNumber: `${imported.profileVersion || '1.1.0'}-draft`,
                name: `Imported Draft (${file.name})`,
                notes: `Loaded from external file ${file.name}`,
                author: 'User',
                createdAt: new Date().toISOString(),
                isFactory: false,
                isDraft: true,
                isConsolidated: false,
                contract: imported
              };
              ContractVersionStore.saveUserVersion(baseId, importedDraft);
              versions = ContractVersionStore.getVersions(baseId, isCustomTarget ? null : factoryContract);
              activeVersionId = newDraftId;
              currentContract = imported;
              onToast(`Contract loaded as working draft (${file.name}).`, 'success');
              render();
            }
          } catch (err) {
            showModalAlert({
              title: 'JSON Parse Error',
              message: 'Failed to read file:\n' + err.message,
              type: 'error'
            });
          }
        };
        reader.readAsText(file);
        fileImportInput.value = '';
      });
    }

    // Cabling operations
    overlay.querySelector('#btnAddCablingStep')?.addEventListener('click', () => {
      syncFormToContract();
      if (!Array.isArray(currentContract.cablingSteps)) currentContract.cablingSteps = [];
      currentContract.cablingSteps.push('New physical wiring verification step...');
      render();
    });

    overlay.querySelectorAll('.btn-delete-cabling-step').forEach(btn => {
      btn.addEventListener('click', (e) => {
        syncFormToContract();
        const idx = parseInt(e.currentTarget.getAttribute('data-step-idx'), 10);
        if (Array.isArray(currentContract.cablingSteps)) {
          currentContract.cablingSteps.splice(idx, 1);
          render();
        }
      });
    });

    // Lifecycle operations
    overlay.querySelector('#btnAddLifecycleAction')?.addEventListener('click', () => {
      syncFormToContract();
      if (!currentContract.lifecycle) currentContract.lifecycle = {};
      if (!Array.isArray(currentContract.lifecycle.preSessionSetup)) currentContract.lifecycle.preSessionSetup = [];
      currentContract.lifecycle.preSessionSetup.push({
        description: 'New baseline initialization action',
        method: 'MIDI_CC',
        channel: 1,
        cc: 74,
        normalizedValue: 0.5,
        settlingDelayMs: 50
      });
      render();
    });

    overlay.querySelectorAll('.btn-delete-lifecycle-action').forEach(btn => {
      btn.addEventListener('click', (e) => {
        syncFormToContract();
        const idx = parseInt(e.currentTarget.getAttribute('data-action-idx'), 10);
        if (currentContract.lifecycle?.preSessionSetup) {
          currentContract.lifecycle.preSessionSetup.splice(idx, 1);
          render();
        }
      });
    });

    // Control operations
    overlay.querySelector('#btnAddControl')?.addEventListener('click', () => {
      syncFormToContract();
      if (!Array.isArray(currentContract.controls)) currentContract.controls = [];
      currentContract.controls.push({
        name: 'New Control',
        type: 'Knob',
        defaultVal: 0.5,
        unit: '%'
      });
      render();
    });

    overlay.querySelectorAll('.btn-delete-control').forEach(btn => {
      btn.addEventListener('click', (e) => {
        syncFormToContract();
        const idx = parseInt(e.currentTarget.getAttribute('data-ctrl-idx'), 10);
        if (Array.isArray(currentContract.controls)) {
          currentContract.controls.splice(idx, 1);
          render();
        }
      });
    });

    // Format JSON
    overlay.querySelector('#btnFormatJson')?.addEventListener('click', () => {
      const txt = overlay.querySelector('#txtRawJson');
      if (txt) {
        try {
          const parsed = JSON.parse(txt.value);
          txt.value = JSON.stringify(parsed, null, 2);
          currentContract = parsed;
        } catch (err) {
          showModalAlert({
            title: 'JSON Syntax Error',
            message: 'Invalid JSON:\n' + err.message,
            type: 'error'
          });
        }
      }
    });

    // Backdrop click to cancel
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) {
        closeModal();
      }
    });
  }

  function closeModal() {
    overlay.classList.remove('active');
    setTimeout(() => {
      if (overlay.parentNode) {
        overlay.parentNode.removeChild(overlay);
      }
    }, 220);
  }

  // Mount to DOM and animate
  document.body.appendChild(overlay);
  render();
  requestAnimationFrame(() => {
    overlay.classList.add('active');
  });
}

function escapeHtml(str) {
  if (typeof str !== 'string') return String(str || '');
  return str.replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
}
