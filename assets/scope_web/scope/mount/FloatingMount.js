/**
 * ABDScope Floating Modal Mount
 * =============================
 * Renders the scope inside a draggable, resizable, floating desktop modal with close button,
 * supporting configurable Multi-Lane layout up to `maxLanes` (default 1).
 */

import { LaneController } from './LaneController.js';

export class FloatingMount {
  constructor(options = {}) {
    this.options = options;
    this.maxLanes = Math.max(1, parseInt(options.maxLanes || 1, 10));
    this.layout = String(this._parseLaneCount(options.layout || 1));
    this.isOpen = false;
    this.isDestroyed = false;

    this.onModeSelect = options.onModeSelect || (() => {});
    this.onResize = options.onResize || (() => {});
    this.onClose = options.onClose || (() => {});
    this.onFreezeToggle = options.onFreezeToggle || (() => {});
    this.onSnapshot = options.onSnapshot || (() => {});
    this.onTapChange = options.onTapChange || (() => {});
    this.onLayoutChange = options.onLayoutChange || (() => {});

    this.lanes = [];
    this.widget = null;
    this.bodyElement = null;
    this.lanesContainer = null;
    this.headerElement = null;
    this.layoutButtons = [];
    this.resizeObserver = null;

    this._createDOM();
    this._initLanes();
    this._initDragging();
    this._initResizeObserver();
  }

  _parseLaneCount(layout) {
    if (layout === 'dual') return 2;
    if (layout === 'triple') return 3;
    if (layout === 'single') return 1;
    const n = parseInt(layout, 10);
    return isNaN(n) ? 1 : Math.max(1, Math.min(n, this.maxLanes));
  }

  _createDOM() {
    this.widget = document.createElement('div');
    this.widget.className = 'abd-scope-root abd-scope-floating';
    this.widget.style.display = 'none';

    const title = this.options.title || 'OSCILLOSCOPE & SPECTRUM';

    let layoutButtonsHtml = '';
    if (this.maxLanes > 1) {
      for (let i = 1; i <= this.maxLanes; ++i) {
        const isActive = this._parseLaneCount(this.layout) === i;
        layoutButtonsHtml += `<button class="abd-scope-layout-btn ${isActive ? 'active' : ''}" data-layout="${i}">${i}</button>`;
      }
    } else {
      layoutButtonsHtml = `<button class="abd-scope-layout-btn active" data-layout="1" disabled title="Single lane configured">1</button>`;
    }

    this.widget.innerHTML = `
      <div class="abd-scope-header abd-scope-drag-handle">
        <div class="abd-scope-title">
          <span class="abd-scope-indicator"></span>
          <span class="abd-scope-title-text">${title}</span>
        </div>
        <div class="abd-scope-controls">
          <div class="abd-scope-layout-group" title="Split View Lanes (Max: ${this.maxLanes})">
            ${layoutButtonsHtml}
          </div>
          ${this.options.showFreeze ? '<button class="abd-scope-freeze-btn" id="scope-freeze-btn">FREEZE</button>' : ''}
          ${this.options.showSnapshot !== false ? `
            <button class="abd-scope-snapshot-btn" id="scope-snapshot-btn" title="Capture PNG Snapshot">
              <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/>
                <circle cx="12" cy="13" r="4"/>
              </svg>
            </button>
          ` : ''}
          <button class="abd-scope-close-btn" title="Close Scope" aria-label="Close">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
              <line x1="18" y1="6" x2="6" y2="18"/>
              <line x1="6" y1="6" x2="18" y2="18"/>
            </svg>
          </button>
        </div>
      </div>
      <div class="abd-scope-body">
        <div class="abd-scope-lanes-container"></div>
      </div>
    `;

    const targetContainer = this.options.containerId
      ? document.getElementById(this.options.containerId) || document.body
      : document.body;

    targetContainer.appendChild(this.widget);
    this.headerElement = this.widget.querySelector('.abd-scope-header');
    this.bodyElement = this.widget.querySelector('.abd-scope-body');
    this.lanesContainer = this.widget.querySelector('.abd-scope-lanes-container');
    this.layoutButtons = Array.from(this.widget.querySelectorAll('.abd-scope-layout-btn'));

    // Controls
    this.widget.querySelector('.abd-scope-close-btn').onclick = () => this.close();

    this.headerElement.addEventListener('click', (e) => {
      const layoutBtn = e.target.closest('.abd-scope-layout-btn');
      if (layoutBtn && !layoutBtn.disabled) {
        const l = layoutBtn.dataset.layout;
        this.setLayout(l);
        this.onLayoutChange(l);
        return;
      }

      if (e.target.closest('#scope-freeze-btn')) {
        const btn = e.target.closest('#scope-freeze-btn');
        const isFrozen = btn.classList.toggle('active');
        btn.textContent = isFrozen ? 'RESUME' : 'FREEZE';
        this.onFreezeToggle(isFrozen);
        return;
      }
      if (e.target.closest('#scope-snapshot-btn')) {
        this.onSnapshot();
      }
    });
  }

  _initLanes() {
    this._rebuildLanes();
  }

  _rebuildLanes() {
    // Preserve existing lane configurations
    const previousStates = this.lanes.map(lane => ({
      mode: lane.getActiveMode(),
      tapId: lane.getActiveTap(),
      requestedColSpan: lane.requestedColSpan,
      isFrozen: lane.isFrozen
    }));

    this.lanes.forEach(l => l.destroy());
    this.lanes = [];
    if (this.lanesContainer) {
      this.lanesContainer.innerHTML = '';
    }

    const numLanes = this._parseLaneCount(this.layout);
    const enabledModes = this.options.enabledModes || ['oscilloscope', 'spectrum', 'lissajous', 'phase', 'spectrogram'];
    const availableTaps = this.options.availableTaps || [{ id: 'master', name: 'Master Out' }];

    const selectedModes = [];
    const selectedTaps = [];

    for (let i = 0; i < numLanes; ++i) {
      const prevState = previousStates[i];

      let initMode;
      if (prevState) {
        initMode = prevState.mode;
      } else {
        initMode = this._pickNextAvailable(enabledModes, selectedModes) || (this.options.defaultMode || enabledModes[0]);
      }
      selectedModes.push(initMode);

      let initTap;
      if (prevState) {
        initTap = prevState.tapId;
      } else {
        initTap = this._pickNextAvailable(availableTaps, selectedTaps) || (availableTaps[0]?.id ?? 'master');
      }
      selectedTaps.push(initTap);

      let initColSpan = prevState ? prevState.requestedColSpan : (['lissajous', 'phase'].includes(initMode) ? 1 : 2);
      let initFrozen = prevState ? prevState.isFrozen : false;

      const lane = new LaneController({
        index: i,
        initialMode: initMode,
        colSpan: initColSpan,
        enabledModes,
        availableTaps,
        defaultTap: initTap,
        onModeChange: (laneIdx, mode) => {
          this._updateGridLanes();
          this.onModeSelect(mode, laneIdx);
        },
        onColSpanChange: () => {
          this._updateGridLanes();
        },
        onTapChange: (laneIdx, tapId) => {
          this.onTapChange(tapId, laneIdx);
        }
      });

      if (initFrozen) lane.freeze(true);

      this.lanes.push(lane);
      this.lanesContainer.appendChild(lane.rootElement);
    }

    this._updateGridLanes();
    this._triggerResize();
  }

  _pickNextAvailable(candidates, existingList) {
    if (!candidates || candidates.length === 0) return null;

    // 1. First unused candidate
    for (const item of candidates) {
      const id = typeof item === 'string' ? item : item.id;
      if (!existingList.includes(id)) {
        return id;
      }
    }

    // 2. Fallback: least repeated candidate
    const counts = new Map();
    candidates.forEach(c => {
      const id = typeof c === 'string' ? c : c.id;
      counts.set(id, 0);
    });
    existingList.forEach(id => {
      if (counts.has(id)) {
        counts.set(id, counts.get(id) + 1);
      }
    });

    let minCount = Infinity;
    let bestChoice = typeof candidates[0] === 'string' ? candidates[0] : candidates[0].id;

    for (const item of candidates) {
      const id = typeof item === 'string' ? item : item.id;
      const count = counts.get(id) || 0;
      if (count < minCount) {
        minCount = count;
        bestChoice = id;
      }
    }

    return bestChoice;
  }

  _updateGridLanes() {
    let pendingHalfLane = null;

    for (let i = 0; i < this.lanes.length; ++i) {
      const lane = this.lanes[i];
      if (lane.requestedColSpan === 1) {
        if (pendingHalfLane === null) {
          pendingHalfLane = lane;
        } else {
          pendingHalfLane.setEffectiveColSpan(1);
          lane.setEffectiveColSpan(1);
          pendingHalfLane = null;
        }
      } else {
        if (pendingHalfLane !== null) {
          pendingHalfLane.setEffectiveColSpan(2);
          pendingHalfLane = null;
        }
        lane.setEffectiveColSpan(2);
      }
    }

    if (pendingHalfLane !== null) {
      pendingHalfLane.setEffectiveColSpan(2);
    }
  }

  setLayout(layout) {
    const parsed = this._parseLaneCount(layout);
    if (this._parseLaneCount(this.layout) === parsed && this.layout === String(parsed)) return;
    this.layout = String(parsed);

    this.layoutButtons.forEach(btn => {
      btn.classList.toggle('active', btn.dataset.layout === String(parsed));
    });
    this._rebuildLanes();
  }

  get canvas() {
    return this.lanes[0]?.canvasElement ?? null;
  }

  setActiveTab(modeName) {
    if (this.lanes[0]) {
      this.lanes[0].setMode(modeName);
    }
  }

  setActiveTap(tapId) {
    if (this.lanes[0]) {
      this.lanes[0].setActiveTap(tapId);
    }
  }

  _initDragging() {
    const handle = this.widget.querySelector('.abd-scope-drag-handle');
    if (!handle) return;

    let startX = 0, startY = 0;

    const onPointerMove = (e) => {
      const dx = e.clientX - startX;
      const dy = e.clientY - startY;
      startX = e.clientX;
      startY = e.clientY;
      this.widget.style.top = `${this.widget.offsetTop + dy}px`;
      this.widget.style.left = `${this.widget.offsetLeft + dx}px`;
      this.widget.style.right = 'auto';
    };

    const onPointerUp = () => {
      document.removeEventListener('pointermove', onPointerMove);
      document.removeEventListener('pointerup', onPointerUp);
    };

    handle.addEventListener('pointerdown', (e) => {
      if (e.target.closest('button') || e.target.closest('select')) return;
      e.preventDefault();
      startX = e.clientX;
      startY = e.clientY;
      document.addEventListener('pointermove', onPointerMove);
      document.addEventListener('pointerup', onPointerUp);
    });
  }

  _initResizeObserver() {
    if (typeof ResizeObserver === 'undefined') return;

    this.resizeObserver = new ResizeObserver((entries) => {
      if (this.isDestroyed || !this.isOpen || !entries[0]) return;
      const rect = entries[0].contentRect;
      if (rect.width > 0 && rect.height > 0) {
        this._dispatchResize(rect.width, rect.height);
      }
    });

    const bodyElem = this.widget.querySelector('.abd-scope-body') || this.widget;
    this.resizeObserver.observe(bodyElem);
  }

  _triggerResize() {
    if (!this.isOpen || !this.bodyElement) return;
    const rect = this.bodyElement.getBoundingClientRect();
    if (rect.width > 0 && rect.height > 0) {
      this._dispatchResize(rect.width, rect.height);
    }
  }

  _dispatchResize(totalW, totalH) {
    const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;
    const numLanes = this.lanes.length;
    if (numLanes === 0) return;

    const minLaneH = this.options.minLaneHeight || 130;
    const computedH = Math.floor(totalH / numLanes);
    const laneH = Math.max(minLaneH, computedH);

    this.lanes.forEach(lane => {
      lane.resize(totalW, laneH, dpr);
    });

    this.onResize(totalW, totalH, dpr);
  }

  open() {
    if (this.isDestroyed) return;
    this.isOpen = true;
    this.widget.style.display = 'flex';
    this.widget.classList.add('active');

    const rect = this.widget.querySelector('.abd-scope-body').getBoundingClientRect();
    this._dispatchResize(rect.width || 480, rect.height || 260);
  }

  close() {
    this.isOpen = false;
    if (this.widget) {
      this.widget.style.display = 'none';
      this.widget.classList.remove('active');
    }
    this.onClose();
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }

  freeze(isFrozen) {
    this.lanes.forEach(l => l.freeze(isFrozen));
  }

  render(dataFrame, options = {}) {
    if (this.isDestroyed || !this.isOpen || !dataFrame) return;

    this.lanes.forEach(lane => {
      let laneFrame = dataFrame;
      if (dataFrame.taps && dataFrame.taps[lane.activeTap]) {
        laneFrame = dataFrame.taps[lane.activeTap];
      }
      lane.render(laneFrame, options);
    });
  }

  destroy() {
    this.isDestroyed = true;
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
      this.resizeObserver = null;
    }
    this.close();
    this.lanes.forEach(l => l.destroy());
    this.lanes = [];
    if (this.widget && this.widget.parentNode) {
      this.widget.parentNode.removeChild(this.widget);
    }
    this.widget = null;
  }
}
