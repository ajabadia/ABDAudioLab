/**
 * HardwareControlRenderer & OperatorCardsContainer
 * Faithful JavaScript/SVG mirror of C++ native:
 * - src/gui/HardwareControlRenderer.cpp
 * - src/gui/HardwareControlRenderer.h
 * - src/gui/OperatorCardsContainerComponent.cpp
 * - src/gui/OperatorCardsContainerComponent.h
 * 
 * Strict specifications:
 * - Zero emojis (clean SVGs, semantic badges and typography).
 * - Exact vector fidelity for Knob, Slider, JackPort, Button, and RotarySwitch.
 * - Dynamic layout: 
 *     1 control  -> Dual card view (large control + telemetry card)
 *     2 controls -> Side-by-side cards
 *     >=3 controls -> Horizontally scrollable micro-card grid + selected telemetry card
 */

/**
 * Normalizes a parameter step or control object into canonical shape
 */
export function normalizeParamStep(raw, fallbackIndex = 0) {
  const paramName = (raw.paramName || raw.name || `Parameter ${fallbackIndex + 1}`).trim();
  const rawType = (raw.controlType || raw.type || 'Knob').trim();
  
  // Standardize control types matching C++ HardwareControlRenderer
  let controlType = 'Knob';
  const lower = rawType.toLowerCase();
  if (lower.includes('jack') || lower.includes('port')) {
    controlType = 'JackPort';
  } else if (lower.includes('button') || lower.includes('push') || lower.includes('toggle')) {
    controlType = 'Button';
  } else if (lower.includes('switch') || lower.includes('selector')) {
    controlType = 'Switch';
  } else if (lower.includes('slider') || lower.includes('fader')) {
    controlType = 'Slider';
  } else {
    controlType = 'Knob';
  }

  let normalizedValue = typeof raw.normalizedValue === 'number' ? raw.normalizedValue : 0.5;
  if (typeof raw.posPct === 'number') {
    normalizedValue = raw.posPct / 100.0;
  }
  normalizedValue = Math.max(0.0, Math.min(1.0, normalizedValue));

  const minNormalized = typeof raw.minNormalized === 'number' 
    ? raw.minNormalized 
    : (typeof raw.minPct === 'number' ? raw.minPct / 100.0 : 0.0);

  const maxNormalized = typeof raw.maxNormalized === 'number' 
    ? raw.maxNormalized 
    : (typeof raw.maxPct === 'number' ? raw.maxPct / 100.0 : 1.0);

  return {
    paramName,
    controlType,
    normalizedValue,
    minNormalized: Math.max(0.0, Math.min(1.0, minNormalized)),
    maxNormalized: Math.max(minNormalized, Math.min(1.0, maxNormalized)),
    unit: raw.unit || '',
    steps: raw.steps || 0,
    totalSteps: raw.totalSteps || raw.steps || 1,
    stepIndex: typeof raw.stepIndex === 'number' ? raw.stepIndex : 0,
    protocol: raw.protocol || 'MANUAL',
    paramIndex: raw.paramIndex,
    cc: raw.cc,
    nrpn: raw.nrpn,
    sysex: raw.sysex
  };
}

/**
 * Draws a Knob matching HardwareControlRenderer::drawKnob
 */
export function renderKnobSvg(ps, width = 120, height = 120, isCompact = false) {
  const norm = ps.normalizedValue;
  const minNorm = ps.minNormalized;
  const maxNorm = ps.maxNormalized;

  // Enforce strictly 1:1 circular aspect ratio
  const dim = Math.min(width, height);
  const startAngle = -135;
  const endAngle = 135;
  const currentAngle = startAngle + norm * (endAngle - startAngle);
  const minAngle = startAngle + minNorm * (endAngle - startAngle);
  const maxAngle = startAngle + maxNorm * (endAngle - startAngle);

  const cx = dim / 2;
  const cy = dim / 2;
  const radius = dim * 0.38;
  const trackRadius = radius - (isCompact ? 4 : 6);
  const strokeW = isCompact ? 3.5 : 5.5;

  function polarToCartesian(centerX, centerY, r, angleInDegrees) {
    const angleInRadians = (angleInDegrees - 90) * Math.PI / 180.0;
    return {
      x: centerX + (r * Math.cos(angleInRadians)),
      y: centerY + (r * Math.sin(angleInRadians))
    };
  }

  function describeArc(x, y, r, startAng, endAng) {
    const start = polarToCartesian(x, y, r, endAng);
    const end = polarToCartesian(x, y, r, startAng);
    const largeArcFlag = (endAng - startAng) <= 180 ? '0' : '1';
    return [
      'M', start.x, start.y,
      'A', r, r, 0, largeArcFlag, 0, end.x, end.y
    ].join(' ');
  }

  const bgTrack = describeArc(cx, cy, trackRadius, startAngle, endAngle);
  const activeArc = describeArc(cx, cy, trackRadius, startAngle, Math.max(startAngle + 0.1, currentAngle));

  const showMinTick = minNorm > 0.01;
  const showMaxTick = maxNorm < 0.99;
  const minTickP1 = polarToCartesian(cx, cy, radius - 8, minAngle);
  const minTickP2 = polarToCartesian(cx, cy, radius + 3, minAngle);
  const maxTickP1 = polarToCartesian(cx, cy, radius - 8, maxAngle);
  const maxTickP2 = polarToCartesian(cx, cy, radius + 3, maxAngle);

  const needleP = polarToCartesian(cx, cy, radius - (isCompact ? 10 : 14), currentAngle);

  return `
    <svg width="${dim}" height="${dim}" viewBox="0 0 ${dim} ${dim}" preserveAspectRatio="xMidYMid meet" style="width:${dim}px; height:${dim}px; aspect-ratio:1/1; max-width:${dim}px; max-height:${dim}px; overflow:visible; display:block; margin:0 auto;">
      <defs>
        <radialGradient id="knobBodyGrad_${dim}" cx="38%" cy="38%" r="62%">
          <stop offset="0%" stop-color="#343d4c" />
          <stop offset="65%" stop-color="#181f28" />
          <stop offset="100%" stop-color="#0d1116" />
        </radialGradient>
      </defs>

      <!-- Background Circular Track -->
      <path d="${bgTrack}" fill="none" stroke="rgba(255,255,255,0.12)" stroke-width="${strokeW}" stroke-linecap="butt" />

      <!-- Active Value Arc (Accent Green) -->
      <path d="${activeArc}" fill="none" stroke="var(--color-accent)" stroke-width="${strokeW}" stroke-linecap="round" />

      <!-- Constrained Range Ticks -->
      ${showMinTick ? `
        <line x1="${minTickP1.x}" y1="${minTickP1.y}" x2="${minTickP2.x}" y2="${minTickP2.y}" stroke="var(--color-warning)" stroke-width="2" />
      ` : ''}
      ${showMaxTick ? `
        <line x1="${maxTickP1.x}" y1="${maxTickP1.y}" x2="${maxTickP2.x}" y2="${maxTickP2.y}" stroke="var(--color-warning)" stroke-width="2" />
      ` : ''}

      <!-- Center Knob Disc (Strictly circular circle) -->
      <circle cx="${cx}" cy="${cy}" r="${radius * 0.74}" fill="url(#knobBodyGrad_${dim})" stroke="var(--color-panel-border)" stroke-width="1.8" />

      <!-- Pointer Needle Line -->
      <line x1="${cx}" y1="${cy}" x2="${needleP.x}" y2="${needleP.y}" stroke="var(--color-accent)" stroke-width="${isCompact ? 3 : 3.5}" stroke-linecap="round" />

      <!-- Center Cap -->
      <circle cx="${cx}" cy="${cy}" r="${isCompact ? 4 : 5.5}" fill="var(--color-text-main)" />
    </svg>
  `;
}

/**
 * Draws a Vertical Slider matching HardwareControlRenderer::drawSlider (Tall format with scale ticks)
 */
export function renderSliderSvg(ps, width = 80, height = 180, isCompact = false) {
  const norm = ps.normalizedValue;
  const trackW = isCompact ? 8 : 10;
  const trackH = Math.max(70, height - (isCompact ? 36 : 46));
  const cx = width / 2;
  const trackX = cx - trackW / 2;
  const trackY = (height - trackH) / 2;

  const fillH = trackH * norm;
  const fillY = trackY + trackH - fillH;

  const handleW = isCompact ? 36 : 46;
  const handleH = isCompact ? 14 : 18;
  const handleY = trackY + trackH * (1.0 - norm) - (handleH / 2);
  const handleX = cx - handleW / 2;

  // Scale ticks along track (0%, 25%, 50%, 75%, 100%)
  const ticks = [0, 0.25, 0.5, 0.75, 1.0];
  const tickLines = ticks.map(t => {
    const ty = trackY + trackH * (1.0 - t);
    const isMajor = t === 0 || t === 0.5 || t === 1.0;
    const len = isMajor ? 8 : 4;
    return `
      <line x1="${trackX - len - 2}" y1="${ty}" x2="${trackX - 2}" y2="${ty}" stroke="rgba(255,255,255,${isMajor ? '0.35' : '0.15'})" stroke-width="${isMajor ? 1.5 : 1}" />
      <line x1="${trackX + trackW + 2}" y1="${ty}" x2="${trackX + trackW + len + 2}" y2="${ty}" stroke="rgba(255,255,255,${isMajor ? '0.35' : '0.15'})" stroke-width="${isMajor ? 1.5 : 1}" />
    `;
  }).join('');

  return `
    <svg width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" preserveAspectRatio="xMidYMid meet" style="width:${width}px; height:${height}px; max-width:${width}px; max-height:${height}px; overflow:visible; display:block; margin:0 auto;">
      <!-- Scale Ticks -->
      ${tickLines}

      <!-- Track Background Slot -->
      <rect x="${trackX}" y="${trackY}" width="${trackW}" height="${trackH}" rx="${trackW / 2}" fill="#0d1117" stroke="rgba(255,255,255,0.18)" stroke-width="1.2" />

      <!-- Active Level Fill (Accent) -->
      <rect x="${trackX}" y="${fillY}" width="${trackW}" height="${fillH}" rx="${trackW / 2}" fill="var(--color-accent)" opacity="0.85" />

      <!-- Metallic Fader Thumb -->
      <rect x="${handleX}" y="${handleY}" width="${handleW}" height="${handleH}" rx="3" fill="#222834" stroke="var(--color-panel-border)" stroke-width="1.4" />
      <!-- Center Indicator Line -->
      <line x1="${handleX + 4}" y1="${handleY + handleH / 2}" x2="${handleX + handleW - 4}" y2="${handleY + handleH / 2}" stroke="var(--color-accent)" stroke-width="2" stroke-linecap="round" />
    </svg>
  `;
}

/**
 * Draws a Jack Port matching HardwareControlRenderer::drawJackPort
 */
export function renderJackPortSvg(ps, width = 120, height = 120, isCompact = false) {
  const isPatched = ps.normalizedValue > 0.5;
  const cx = width / 2;
  const cy = height / 2;
  const outerR = Math.min(width, height) * (isCompact ? 0.32 : 0.36);
  const washerR = outerR * 0.65;
  const holeR = outerR * 0.35;
  const pinR = outerR * 0.22;

  return `
    <svg width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" style="overflow:visible; display:block; margin:0 auto;">
      <!-- Outer Metal Bezel -->
      <circle cx="${cx}" cy="${cy}" r="${outerR}" fill="#2a2e36" stroke="#4a5260" stroke-width="2" />
      
      <!-- Washer Hexagon / Ring -->
      <circle cx="${cx}" cy="${cy}" r="${washerR}" fill="#717a8a" stroke="#8d97a8" stroke-width="1" />

      <!-- Black Socket Hole -->
      <circle cx="${cx}" cy="${cy}" r="${holeR}" fill="#0a0c10" />

      <!-- Internal Contact Pin Indicator -->
      <circle cx="${cx}" cy="${cy}" r="${pinR}" fill="${isPatched ? 'var(--color-accent)' : 'var(--color-danger)'}" />
      ${isPatched ? `<circle cx="${cx}" cy="${cy}" r="${pinR * 1.5}" fill="none" stroke="var(--color-accent)" stroke-width="1" opacity="0.4" />` : ''}
    </svg>
  `;
}

/**
 * Draws a Stompbox / Synth Button matching HardwareControlRenderer::drawButton
 */
export function renderButtonSvg(ps, width = 120, height = 120, isCompact = false) {
  const isOn = ps.normalizedValue > 0.5;
  const size = Math.min(width, height) * (isCompact ? 0.52 : 0.60);
  const cx = width / 2;
  const cy = height / 2;
  const x = cx - size / 2;
  const y = cy - size / 2;
  const ledR = size * 0.18;

  return `
    <svg width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" style="overflow:visible; display:block; margin:0 auto;">
      <!-- Button Base -->
      <rect x="${x}" y="${y}" width="${size}" height="${size}" rx="8" fill="#1e222a" stroke="rgba(255,255,255,0.2)" stroke-width="1.5" />
      <rect x="${x + 3}" y="${y + 3}" width="${size - 6}" height="${size - 6}" rx="6" fill="#14171d" />

      <!-- Center Status LED -->
      <circle cx="${cx}" cy="${cy}" r="${ledR}" fill="${isOn ? 'var(--color-accent)' : 'var(--color-panel-border)'}" />
      ${isOn ? `
        <!-- LED Reflection & Glow -->
        <circle cx="${cx}" cy="${cy}" r="${ledR * 1.6}" fill="none" stroke="var(--color-accent)" stroke-width="1.5" opacity="0.4" />
        <circle cx="${cx - ledR * 0.3}" cy="${cy - ledR * 0.3}" r="${ledR * 0.35}" fill="rgba(255,255,255,0.7)" />
      ` : ''}
    </svg>
  `;
}

/**
 * Draws a Rotary Switch matching HardwareControlRenderer::drawSwitch
 */
export function renderSwitchSvg(ps, width = 120, height = 120, isCompact = false) {
  const norm = ps.normalizedValue;
  const numPositions = Math.max(2, Math.min(6, ps.steps || 4));
  const cx = width / 2;
  const cy = height / 2;
  const radius = Math.min(width, height) * (isCompact ? 0.34 : 0.38);

  const startAngle = -135;
  const endAngle = 135;
  const currentAngle = startAngle + norm * (endAngle - startAngle);

  function polarToCartesian(centerX, centerY, r, angleInDegrees) {
    const angleInRadians = (angleInDegrees - 90) * Math.PI / 180.0;
    return {
      x: centerX + (r * Math.cos(angleInRadians)),
      y: centerY + (r * Math.sin(angleInRadians))
    };
  }

  // Generate position dots
  let dotsSvg = '';
  for (let p = 0; p < numPositions; p++) {
    const posNorm = numPositions <= 1 ? 0.5 : (p / (numPositions - 1));
    const posAngle = startAngle + posNorm * (endAngle - startAngle);
    const dotPos = polarToCartesian(cx, cy, radius + (isCompact ? 4 : 6), posAngle);
    dotsSvg += `<circle cx="${dotPos.x}" cy="${dotPos.y}" r="${isCompact ? 2 : 2.5}" fill="rgba(255,255,255,0.4)" />`;
  }

  // Pointer needle to current position
  const needleP = polarToCartesian(cx, cy, radius - (isCompact ? 4 : 6), currentAngle);

  return `
    <svg width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" style="overflow:visible; display:block; margin:0 auto;">
      <!-- Body Circle -->
      <circle cx="${cx}" cy="${cy}" r="${radius}" fill="#161c24" stroke="rgba(255,255,255,0.18)" stroke-width="1.5" />

      <!-- Position Dots -->
      ${dotsSvg}

      <!-- Pointer Needle Line -->
      <line x1="${cx}" y1="${cy}" x2="${needleP.x}" y2="${needleP.y}" stroke="var(--color-accent)" stroke-width="${isCompact ? 2.5 : 3.5}" stroke-linecap="round" />

      <!-- Center Disc -->
      <circle cx="${cx}" cy="${cy}" r="${isCompact ? 5 : 7}" fill="#2a3340" stroke="rgba(255,255,255,0.3)" stroke-width="1" />
    </svg>
  `;
}

/**
 * Dispatches control SVG rendering based on ps.controlType
 */
export function renderControlVisual(ps, width = 120, height = 120, isCompact = false) {
  switch (ps.controlType) {
    case 'Slider':
      return renderSliderSvg(ps, width, height, isCompact);
    case 'JackPort':
      return renderJackPortSvg(ps, width, height, isCompact);
    case 'Button':
      return renderButtonSvg(ps, width, height, isCompact);
    case 'Switch':
      return renderSwitchSvg(ps, width, height, isCompact);
    case 'Knob':
    default:
      return renderKnobSvg(ps, width, height, isCompact);
  }
}

/**
 * Returns value badge text and color based on control type
 */
export function getTargetValueBadge(ps) {
  const norm = ps.normalizedValue;
  const pct = Math.round(norm * 100);

  switch (ps.controlType) {
    case 'JackPort':
      return {
        text: norm > 0.5 ? 'PATCHED' : 'UNPATCHED',
        color: norm > 0.5 ? 'var(--color-accent)' : 'var(--color-warning)'
      };
    case 'Button':
      return {
        text: norm > 0.5 ? 'ON' : 'OFF',
        color: norm > 0.5 ? 'var(--color-accent)' : 'var(--color-text-dim)'
      };
    case 'Switch': {
      const numPositions = Math.max(2, Math.min(6, ps.steps || 4));
      const posIdx = Math.round(norm * (numPositions - 1)) + 1;
      return {
        text: `POS ${posIdx}`,
        color: 'var(--color-accent)'
      };
    }
    case 'Slider':
    case 'Knob':
    default: {
      let text = `${pct}%`;
      if (ps.minNormalized > 0.01 || ps.maxNormalized < 0.99) {
        const minP = Math.round(ps.minNormalized * 100);
        const maxP = Math.round(ps.maxNormalized * 100);
        text += ` [${minP}%-${maxP}%]`;
      }
      return {
        text,
        color: 'var(--color-accent)'
      };
    }
  }
}

/**
 * Renders full Operator Cards Container matching OperatorCardsContainerComponent.cpp
 * @param {Array} rawSteps - array of parameter steps or control configs
 * @param {string} promptMessage - optional operator prompt text
 * @param {number} selectedIndex - currently selected index when >= 3 controls
 * @param {string} containerId - unique container id for DOM scoping
 */
export function renderOperatorCardsContainer(rawSteps, promptMessage = '', selectedIndex = 0, containerId = 'operatorCardsContainer') {
  if (!rawSteps || rawSteps.length === 0) {
    return `
      <div id="${containerId}" style="padding:16px; text-align:center; color:var(--color-text-dim); font-size:11px; font-style:italic;">
        No physical parameters defined for this measurement state.
      </div>
    `;
  }

  const steps = rawSteps.map((s, i) => normalizeParamStep(s, i));
  const selIdx = Math.max(0, Math.min(steps.length - 1, selectedIndex));
  const selPs = steps[selIdx];

  function getProtocolBadgeHtml(pStep) {
    let text = 'MANUAL';
    let color = 'var(--color-text-dim)';
    let bg = 'rgba(255,255,255,0.06)';
    let border = 'var(--color-panel-border)';

    const proto = (pStep.protocol || '').toUpperCase();
    if (proto.includes('VST')) {
      text = typeof pStep.paramIndex !== 'undefined' ? `VST #${pStep.paramIndex}` : 'VST PARAM';
      color = 'var(--color-accent)';
      bg = 'rgba(0, 230, 118, 0.12)';
      border = 'rgba(0, 230, 118, 0.35)';
    } else if (proto.includes('CC')) {
      text = typeof pStep.cc !== 'undefined' ? `CC #${pStep.cc}` : 'MIDI CC';
      color = '#64b5f6';
      bg = 'rgba(100, 181, 246, 0.12)';
      border = 'rgba(100, 181, 246, 0.35)';
    } else if (proto.includes('NRPN')) {
      text = typeof pStep.nrpn !== 'undefined' ? `NRPN #${pStep.nrpn}` : 'NRPN';
      color = '#ba68c8';
      bg = 'rgba(186, 104, 200, 0.12)';
      border = 'rgba(186, 104, 200, 0.35)';
    } else if (proto.includes('SYS')) {
      text = 'SysEx';
      color = '#ffb74d';
      bg = 'rgba(255, 183, 77, 0.12)';
      border = 'rgba(255, 183, 77, 0.35)';
    }
    return `<span style="padding:2px 7px; border-radius:4px; font-size:9.5px; font-weight:800; font-family:var(--font-mono, monospace); background:${bg}; color:${color}; border:1px solid ${border}; text-transform:uppercase; letter-spacing:0.4px;">${text}</span>`;
  }

  // 1 CONTROL: Dual Card View (Left: Control Card, Right: Telemetry & Spec Card)
  if (steps.length === 1) {
    const ps = steps[0];
    const badge = getTargetValueBadge(ps);
    const pct = Math.round(ps.normalizedValue * 100);
    const isSlider = ps.controlType === 'Slider';

    return `
      <div id="${containerId}" class="operator-cards-container dual-layout" style="display:flex; gap:16px; align-items:stretch; padding:12px 16px; width:100%; box-sizing:border-box;">
        <!-- Left: Control Presentation Card -->
        <div class="control-card-single" style="width:${isSlider ? '160px' : '180px'}; min-width:${isSlider ? '160px' : '180px'}; min-height:${isSlider ? '270px' : '220px'}; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:12px 10px; display:flex; flex-direction:column; align-items:center; justify-content:space-between; gap:6px;">
          <div style="font-size:11px; font-weight:800; color:var(--color-text-main); text-transform:uppercase; text-align:center; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; width:100%;">
            ${ps.paramName}
          </div>

          <div style="flex:1; display:flex; align-items:center; justify-content:center; width:100%; padding:4px 0;">
            ${isSlider ? renderControlVisual(ps, 80, 180, false) : renderControlVisual(ps, 100, 100, false)}
          </div>

          <!-- Parameter Details & Value Below Visual -->
          <div style="display:flex; flex-direction:column; align-items:center; gap:4px; width:100%; padding-top:2px;">
            ${getProtocolBadgeHtml(ps)}

            <div style="padding:2.5px 8px; border-radius:6px; background:rgba(0,0,0,0.38); border:1px solid var(--color-panel-border); font-size:11px; font-weight:800; color:${badge.color}; font-family:var(--font-mono, monospace); text-align:center; width:calc(100% - 6px); box-sizing:border-box;">
              ${pct}% <span style="font-size:9.5px; opacity:0.8; font-weight:500;">(${ps.normalizedValue.toFixed(3)})</span>
            </div>

            <div style="font-size:9.5px; font-weight:600; color:var(--color-text-dim); text-align:center;">
              Paso ${ps.stepIndex + 1} de ${ps.totalSteps}
            </div>
          </div>
        </div>

        <!-- Right: Parameter Telemetry & Specification Card -->
        <div class="telemetry-card" style="flex:1; background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:14px 18px; display:flex; flex-direction:column; justify-content:center; gap:8px;">
          <div style="font-size:10px; font-weight:800; color:var(--color-accent); text-transform:uppercase; letter-spacing:0.5px;">
            Parameter Telemetry &amp; Specification
          </div>

          <div style="font-size:14px; font-weight:800; color:var(--color-text-main); text-transform:uppercase;">
            ${ps.paramName} <span style="font-weight:600; color:var(--color-text-dim);">[${ps.controlType}]</span>
          </div>

          <div style="font-size:12px; color:var(--color-text-dim);">
            Target: <strong style="color:var(--color-text-main); font-family:var(--font-mono, monospace);">${pct}% (${ps.normalizedValue.toFixed(3)} norm)</strong>
            <span style="margin:0 8px; opacity:0.4;">|</span>
            Range: <span style="color:var(--color-text-main);">${Math.round(ps.minNormalized * 100)}% - ${Math.round(ps.maxNormalized * 100)}%</span>
          </div>

          ${promptMessage ? `
            <div style="font-size:11.5px; font-style:italic; color:var(--color-text-dim); margin-top:4px; line-height:1.4;">
              ${promptMessage}
            </div>
          ` : ''}
        </div>
      </div>
    `;
  }

  // > 1 CONTROLS: Multi-Control Grid View
  const selBadge = getTargetValueBadge(selPs);
  const selPct = Math.round(selPs.normalizedValue * 100);
  const hasSliders = steps.some(s => s.controlType === 'Slider');
  const cardW = hasSliders ? 140 : 150;
  const cardMinH = hasSliders ? 265 : 215;

  return `
    <div id="${containerId}" class="operator-cards-container multi-grid-layout" style="display:flex; flex-direction:column; gap:12px; padding:10px 14px; width:100%; box-sizing:border-box;">
      <!-- Row of Parameter Control Cards -->
      <div style="display:flex; gap:12px; overflow-x:auto; padding-bottom:6px; align-items:stretch;">
        ${steps.map((ps, i) => {
          const isSelected = i === selIdx;
          const badge = getTargetValueBadge(ps);
          const isSlider = ps.controlType === 'Slider';
          const pct = Math.round(ps.normalizedValue * 100);

          return `
            <div data-param-card-idx="${i}" role="button" tabindex="0" style="width:${cardW}px; min-width:${cardW}px; min-height:${cardMinH}px; background:${isSelected ? 'var(--color-accent-dim)' : 'var(--color-panel-surface-elevated)'}; border:1.5px solid ${isSelected ? 'var(--color-accent)' : 'var(--color-panel-border)'}; border-radius:var(--radius-md); padding:10px 8px; display:flex; flex-direction:column; align-items:center; justify-content:space-between; cursor:pointer; transition:border-color 0.15s, transform 0.1s; box-sizing:border-box;">
              <div style="font-size:10px; font-weight:800; color:${isSelected ? 'var(--color-accent)' : 'var(--color-text-main)'}; text-transform:uppercase; text-align:center; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; width:100%;">
                ${ps.paramName}
              </div>

              <div style="flex:1; display:flex; align-items:center; justify-content:center; width:100%; padding:4px 0;">
                ${isSlider ? renderControlVisual(ps, 76, 170, false) : renderControlVisual(ps, 82, 82, false)}
              </div>

              <!-- Parameter Details & Value Below Visual -->
              <div style="display:flex; flex-direction:column; align-items:center; gap:4px; width:100%; padding-top:2px;">
                ${getProtocolBadgeHtml(ps)}

                <div style="padding:2.5px 8px; border-radius:6px; background:rgba(0,0,0,0.38); border:1px solid var(--color-panel-border); font-size:11px; font-weight:800; color:${badge.color}; font-family:var(--font-mono, monospace); text-align:center; width:calc(100% - 4px); box-sizing:border-box;">
                  ${pct}% <span style="font-size:9.5px; opacity:0.8; font-weight:500;">(${ps.normalizedValue.toFixed(3)})</span>
                </div>

                <div style="font-size:9.5px; font-weight:600; color:var(--color-text-dim); text-align:center;">
                  Paso ${ps.stepIndex + 1} de ${ps.totalSteps}
                </div>
              </div>
            </div>
          `;
        }).join('')}
      </div>

      <!-- Selected Parameter Telemetry & Specification Card -->
      <div class="selected-param-telemetry" style="background:var(--color-panel-surface-elevated); border:1px solid var(--color-panel-border); border-radius:var(--radius-md); padding:10px 16px; display:flex; flex-direction:column; gap:4px;">
        <div style="font-size:10px; font-weight:800; color:var(--color-accent); text-transform:uppercase; letter-spacing:0.5px;">
          Selected Parameter Telemetry &amp; Specifications [${selIdx + 1}/${steps.length}]
        </div>

        <div style="font-size:13px; font-weight:800; color:var(--color-text-main); text-transform:uppercase;">
          ${selPs.paramName} <span style="font-weight:600; color:var(--color-text-dim);">[${selPs.controlType}]</span>
        </div>

        <div style="font-size:11.5px; color:var(--color-text-dim);">
          Target Value: <strong style="color:var(--color-text-main); font-family:var(--font-mono, monospace);">${selPct}% (${selPs.normalizedValue.toFixed(3)} norm)</strong>
          <span style="margin:0 8px; opacity:0.4;">|</span>
          Target Range: <span style="color:var(--color-text-main);">${Math.round(selPs.minNormalized * 100)}% - ${Math.round(selPs.maxNormalized * 100)}%</span>
        </div>

        ${promptMessage ? `
          <div style="font-size:11px; font-style:italic; color:var(--color-text-dim); margin-top:2px;">
            ${promptMessage}
          </div>
        ` : ''}
      </div>
    </div>
  `;
}
