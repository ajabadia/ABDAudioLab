/**
 * SoundID Reference Style Modal Dialog System for ABDAudioLab
 * 
 * Provides publication-grade, accessible, and beautifully styled alert & confirmation
 * dialogs matching the SoundID Reference metrological aesthetic.
 * 
 * Zero emojis, clean typography, rounded surfaces, smooth cubic-bezier transitions,
 * and high-contrast pill actions for both light and dark themes.
 */

const ICONS = {
  warning: `
    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="var(--color-warning)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10"></circle>
      <line x1="12" y1="8" x2="12" y2="12"></line>
      <line x1="12" y1="16" x2="12.01" y2="16"></line>
    </svg>
  `,
  error: `
    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="var(--color-danger)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10"></circle>
      <line x1="12" y1="8" x2="12" y2="12"></line>
      <line x1="12" y1="16" x2="12.01" y2="16"></line>
    </svg>
  `,
  success: `
    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="var(--color-accent)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10"></circle>
      <polyline points="16 9 10 15 7 12"></polyline>
    </svg>
  `,
  info: `
    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="var(--color-info)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10"></circle>
      <line x1="12" y1="16" x2="12" y2="12"></line>
      <line x1="12" y1="8" x2="12.01" y2="8"></line>
    </svg>
  `
};

/**
 * Universal Dialog Prompter
 * @param {Object} options
 * @param {string} options.title - Dialog heading
 * @param {string} options.message - Body text or explanation
 * @param {'warning'|'error'|'success'|'info'} [options.type='warning'] - Category/Icon
 * @param {string} [options.confirmText='Got it'] - Primary action label
 * @param {string} [options.cancelText] - Optional cancel button label
 * @param {string} [options.secondaryText] - Optional bottom-left action/link
 * @param {Function|string} [options.onSecondary] - Action or URL for secondary link
 * @param {boolean} [options.isConfirm=false] - If true, resolves boolean
 * @param {boolean} [options.danger=false] - Dangerous action styling
 * @returns {Promise<boolean>}
 */
export function showSoundIdModal(options = {}) {
  const {
    title = 'Notice',
    message = '',
    html = null,
    size = 'medium',
    type = 'warning',
    confirmText = options.isConfirm ? 'Confirm' : 'Got it',
    cancelText = options.isConfirm ? 'Cancel' : null,
    secondaryText = null,
    onSecondary = null,
    isConfirm = false,
    danger = false
  } = options;

  return new Promise((resolve) => {
    const overlay = document.createElement('div');
    overlay.className = 'soundid-modal-overlay';

    const iconSvg = ICONS[type] || ICONS.warning;
    const isLight = document.documentElement.getAttribute('data-theme') === 'audiolab-light' ||
                    document.body?.getAttribute('data-theme') === 'audiolab-light';

    const sizeClass = size === 'large' ? 'modal-large' : (size === 'xl' ? 'modal-xl' : '');
    const bodyContent = html ? html : formatMessage(message);

    overlay.innerHTML = `
      <div class="soundid-modal-card ${sizeClass}" role="dialog" aria-modal="true" aria-labelledby="soundidModalTitle" aria-describedby="soundidModalDesc">
        <div class="soundid-modal-icon-wrap soundid-icon-${type}">
          ${iconSvg}
        </div>
        <div class="soundid-modal-title" id="soundidModalTitle">${escapeHtml(title)}</div>
        <div class="soundid-modal-body" id="soundidModalDesc">${bodyContent}</div>
        <div class="soundid-modal-footer">
          <div class="soundid-modal-footer-left">
            ${secondaryText ? `
              <button type="button" class="soundid-modal-secondary-link" id="soundidSecondaryBtn">
                ${escapeHtml(secondaryText)}
              </button>
            ` : ''}
          </div>
          <div class="soundid-modal-footer-right">
            ${cancelText ? `
              <button type="button" class="soundid-modal-cancel-btn" id="soundidCancelBtn">
                ${escapeHtml(cancelText)}
              </button>
            ` : ''}
            <button type="button" class="soundid-modal-primary-btn ${danger ? 'danger-action' : ''}" id="soundidConfirmBtn">
              ${escapeHtml(confirmText)}
            </button>
          </div>
        </div>
      </div>
    `;

    document.body.appendChild(overlay);

    // Trigger smooth enter animation on next frame
    requestAnimationFrame(() => {
      overlay.classList.add('active');
    });

    const confirmBtn = overlay.querySelector('#soundidConfirmBtn');
    const cancelBtn = overlay.querySelector('#soundidCancelBtn');
    const secondaryBtn = overlay.querySelector('#soundidSecondaryBtn');

    if (confirmBtn) {
      setTimeout(() => confirmBtn.focus(), 60);
    }

    let isClosed = false;
    function close(result) {
      if (isClosed) return;
      isClosed = true;
      document.removeEventListener('keydown', handleKeydown);
      overlay.classList.remove('active');
      setTimeout(() => {
        if (overlay.parentNode) {
          overlay.parentNode.removeChild(overlay);
        }
        resolve(result);
      }, 200);
    }

    function handleKeydown(e) {
      if (e.key === 'Escape') {
        e.preventDefault();
        close(false);
      } else if (e.key === 'Enter' && !e.shiftKey) {
        if (document.activeElement !== cancelBtn && document.activeElement !== secondaryBtn) {
          e.preventDefault();
          close(true);
        }
      }
    }

    document.addEventListener('keydown', handleKeydown);

    if (confirmBtn) {
      confirmBtn.addEventListener('click', () => close(true));
    }

    if (cancelBtn) {
      cancelBtn.addEventListener('click', () => close(false));
    }

    if (secondaryBtn) {
      secondaryBtn.addEventListener('click', () => {
        if (typeof onSecondary === 'function') {
          onSecondary();
        } else if (typeof onSecondary === 'string' && onSecondary.startsWith('http')) {
          window.open(onSecondary, '_blank');
        }
      });
    }

    // Clicking the backdrop cancels if confirm, closes otherwise
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) {
        close(false);
      }
    });
  });
}

/**
 * Show a SoundID-style Informational/Warning/Error Alert Modal
 * @param {string|Object} messageOrOpts
 * @param {string} [title]
 * @param {'warning'|'error'|'success'|'info'} [type]
 * @returns {Promise<void>}
 */
export function showModalAlert(messageOrOpts, title = 'Notice', type = 'warning') {
  if (typeof messageOrOpts === 'object' && messageOrOpts !== null) {
    return showSoundIdModal({
      ...messageOrOpts,
      isConfirm: false
    });
  }
  return showSoundIdModal({
    title,
    message: String(messageOrOpts || ''),
    type,
    isConfirm: false
  });
}

/**
 * Show a SoundID-style Confirmation Modal (Accept / Cancel)
 * @param {string|Object} messageOrOpts
 * @param {string} [title='Confirmation']
 * @param {'warning'|'error'|'info'} [type='warning']
 * @returns {Promise<boolean>}
 */
export function showModalConfirm(messageOrOpts, title = 'Confirmation', type = 'warning') {
  if (typeof messageOrOpts === 'object' && messageOrOpts !== null) {
    return showSoundIdModal({
      ...messageOrOpts,
      isConfirm: true
    });
  }
  return showSoundIdModal({
    title,
    message: String(messageOrOpts || ''),
    type,
    isConfirm: true
  });
}

/**
 * Show a SoundID-style Prompt Modal with text input field
 * @param {Object} options
 * @param {string} options.title - Dialog heading
 * @param {string} options.message - Explanation or prompt instructions
 * @param {string} [options.defaultValue=''] - Pre-filled input value
 * @param {string} [options.placeholder=''] - Input placeholder
 * @param {string} [options.confirmText='Save'] - Confirm button label
 * @param {string} [options.cancelText='Cancel'] - Cancel button label
 * @param {'info'|'warning'|'success'} [options.type='info'] - Category/Icon
 * @returns {Promise<string|null>} Resolves with the trimmed string or null if cancelled
 */
export function showSoundIdPrompt(options = {}) {
  const {
    title = 'Enter Name',
    message = '',
    defaultValue = '',
    placeholder = 'Enter name...',
    confirmText = 'Save',
    cancelText = 'Cancel',
    type = 'info'
  } = options;

  return new Promise((resolve) => {
    const overlay = document.createElement('div');
    overlay.className = 'soundid-modal-overlay';

    const iconSvg = ICONS[type] || ICONS.info;

    overlay.innerHTML = `
      <div class="soundid-modal-card" role="dialog" aria-modal="true" aria-labelledby="soundidModalTitle">
        <div class="soundid-modal-icon-wrap soundid-icon-${type}">
          ${iconSvg}
        </div>
        <div class="soundid-modal-title" id="soundidModalTitle">${escapeHtml(title)}</div>
        <div class="soundid-modal-body">${formatMessage(message)}</div>
        <div style="width:100%; margin-top:14px;">
          <input type="text" id="soundidPromptInput" class="soundid-modal-input" value="${escapeHtml(defaultValue)}" placeholder="${escapeHtml(placeholder)}" spellcheck="false" autocomplete="off" />
        </div>
        <div class="soundid-modal-footer" style="margin-top:20px;">
          <div class="soundid-modal-footer-left"></div>
          <div class="soundid-modal-footer-right">
            <button type="button" class="soundid-modal-cancel-btn" id="soundidCancelBtn">
              ${escapeHtml(cancelText)}
            </button>
            <button type="button" class="soundid-modal-primary-btn" id="soundidConfirmBtn">
              ${escapeHtml(confirmText)}
            </button>
          </div>
        </div>
      </div>
    `;

    document.body.appendChild(overlay);

    requestAnimationFrame(() => {
      overlay.classList.add('active');
    });

    const input = overlay.querySelector('#soundidPromptInput');
    const confirmBtn = overlay.querySelector('#soundidConfirmBtn');
    const cancelBtn = overlay.querySelector('#soundidCancelBtn');

    if (input) {
      setTimeout(() => {
        input.focus();
        input.select();
      }, 70);
    }

    let isClosed = false;
    function close(result) {
      if (isClosed) return;
      isClosed = true;
      document.removeEventListener('keydown', handleKeydown);
      overlay.classList.remove('active');
      setTimeout(() => {
        if (overlay.parentNode) {
          overlay.parentNode.removeChild(overlay);
        }
        resolve(result);
      }, 200);
    }

    function handleKeydown(e) {
      if (e.key === 'Escape') {
        e.preventDefault();
        close(null);
      } else if (e.key === 'Enter') {
        e.preventDefault();
        const val = input ? input.value.trim() : '';
        close(val || defaultValue || null);
      }
    }

    document.addEventListener('keydown', handleKeydown);

    if (confirmBtn) {
      confirmBtn.addEventListener('click', () => {
        const val = input ? input.value.trim() : '';
        close(val || defaultValue || null);
      });
    }

    if (cancelBtn) {
      cancelBtn.addEventListener('click', () => close(null));
    }
  });
}

function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

function formatMessage(msg) {
  if (!msg) return '';
  // Support linebreaks and markdown-like backticks
  const escaped = escapeHtml(msg);
  return escaped.replace(/\n/g, '<br/>');
}

/**
 * Monkey-patches window.alert and window.confirm to gracefully route through SoundID modals
 */
export function patchBrowserDialogs() {
  if (typeof window === 'undefined') return;

  window.alert = (message) => {
    return showModalAlert(message, 'System Notice', 'warning');
  };

  // Note: synchronous code relying on confirm() should be migrated to `await showModalConfirm(...)`,
  // but if called synchronously, we log guidance and open the modal non-blocking.
  window._originalConfirm = window.confirm;
}
