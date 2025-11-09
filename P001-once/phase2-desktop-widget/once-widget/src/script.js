const capMs = 3 * 3600_000; // 最长 3 小时
const maxSeconds = capMs / 1000;

let running = false;
let t0 = 0;
let raf = 0;
let lastLap = null;

const root = document.documentElement;
const wrap = document.querySelector('.wrap');
const device = document.querySelector('.device');

const lcd = document.getElementById('lcd');
const mmEl = document.getElementById('mm');
const ssEl = document.getElementById('ss');
const d10El = document.getElementById('d10');
const toggle = document.getElementById('toggle');
const score = document.getElementById('score');
const diff = document.getElementById('diff');
const targetInput = document.getElementById('targetInput');
const lamps = [document.getElementById('lamp1'), document.getElementById('lamp2'), document.getElementById('lamp3')];

const pad = (n) => String(n).padStart(2, '0');

function render(ms) {
    const s = Math.floor(ms / 1000);
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    const d10 = Math.floor((ms % 1000) / 100);
    mmEl.textContent = pad(m);
    ssEl.textContent = pad(sec);
    d10El.textContent = d10;
    lamps.forEach((lamp, i) => lamp.classList.toggle('on', h > i));
}

function evaluate(ms, targetSec) {
    if (!targetSec || !isFinite(targetSec) || targetSec <= 0) return null;
    const tgt = targetSec * 1000;
    const r = Math.abs(ms - tgt) / tgt;
    const sigma = 0.05;
    const val = Math.exp(-0.5 * (r / sigma) * (r / sigma));
    const sc = Math.round(100 * val);
    if (sc >= 90) {
        lcd.animate(
            [{ filter: 'brightness(1.0)' }, { filter: 'brightness(1.2)' }, { filter: 'brightness(1.0)' }],
            { duration: 420, easing: 'ease-out' }
        );
    }
    return sc;
}

function tick(ts) {
    if (!t0) t0 = ts;
    const elapsed = ts - t0;
    const total = Math.min(elapsed, capMs);
    render(total);
    if (elapsed >= capMs) {
        stopAt(total);
        return;
    }
    raf = requestAnimationFrame(tick);
}

function setTargetEditable(editable) {
    if (!targetInput) return;
    targetInput.disabled = !editable;
    if (!editable) targetInput.blur();
}

function start() {
    running = true;
    t0 = performance.now();
    formatTargetInput();
    setTargetEditable(false);
    toggle.textContent = 'STOP';
    lcd.classList.add('active');
    score.textContent = '';
    diff.textContent = '';
    diff.style.color = '';
    cancelAnimationFrame(raf);
    raf = requestAnimationFrame(tick);
}

function stopAt(ms) {
    running = false;
    cancelAnimationFrame(raf);
    lcd.classList.remove('active');
    toggle.textContent = 'START';
    setTargetEditable(true);

    const sc = evaluate(ms, parseFloat(targetInput?.value || ''));
    score.textContent = sc !== null ? `score: ${sc}` : '';

    if (lastLap !== null) {
        const diffValue = (ms - lastLap) / 1000;
        const diffText = `${diffValue >= 0 ? '+' : ''}${diffValue.toFixed(2)}s`;
        diff.textContent = `diff: ${diffText}`;

        if (Math.abs(diffValue) < 0.005) {
            diff.style.color = '#ffb300';
        } else if (diffValue < 0) {
            diff.style.color = '#4caf50';
        } else {
            diff.style.color = '#f44336';
        }
    } else {
        diff.textContent = '';
        diff.style.color = '';
    }

    lastLap = ms;
}

function toggleTimer() {
    if (!running) {
        start();
        return;
    }
    const now = performance.now();
    const ms = Math.min(now - t0, capMs);
    stopAt(ms);
}

if (toggle) {
    toggle.addEventListener('click', toggleTimer);
}

function sanitizeTargetValue(raw) {
    if (!raw) return '';
    let sanitized = raw.replace(/[^\d.]/g, '');
    const firstDot = sanitized.indexOf('.');
    if (firstDot !== -1) {
        const before = sanitized.slice(0, firstDot + 1);
        const after = sanitized.slice(firstDot + 1).replace(/\./g, '');
        sanitized = before + after;
    }
    if (sanitized.startsWith('.')) {
        sanitized = `0${sanitized}`;
    }
    if (sanitized.includes('.')) {
        const [intPart, decimalPart = ''] = sanitized.split('.');
        sanitized = `${intPart || '0'}.${decimalPart.slice(0, 3)}`;
    } else {
        sanitized = sanitized.replace(/^0+(?=\d)/, '') || sanitized;
    }
    return sanitized.slice(0, 8);
}

function formatTargetInput() {
    if (!targetInput || !targetInput.value) return;
    const numeric = parseFloat(targetInput.value);
    if (!Number.isFinite(numeric)) {
        targetInput.value = '';
        return;
    }
    const clamped = Math.min(numeric, maxSeconds);
    const formatted = clamped % 1 === 0 ? clamped.toString() : clamped.toFixed(2);
    targetInput.value = formatted.replace(/\.?0+$/, '');
}

if (targetInput) {
    targetInput.addEventListener('input', (event) => {
        const sanitized = sanitizeTargetValue(event.target.value);
        if (sanitized !== event.target.value) {
            event.target.value = sanitized;
        }
    });

    targetInput.addEventListener('blur', formatTargetInput);
    targetInput.addEventListener('focus', () => {
        targetInput.select();
    });
}

document.addEventListener('keydown', (event) => {
    if (event.repeat) return;
    const isSpace = event.code === 'Space';
    const isEnter = event.code === 'Enter';
    if (!isSpace && !isEnter) return;

    if (event.target === toggle) {
        // 交给按钮自身的键盘交互
        return;
    }

    event.preventDefault();
    if (event.target === targetInput) {
        formatTargetInput();
    }
    toggleTimer();
});

let baseWidth = null;
let baseHeight = null;

function captureBaseDimensions() {
    if (!device || (baseWidth && baseHeight)) return;
    const rect = device.getBoundingClientRect();
    baseWidth = rect.width;
    baseHeight = rect.height;
    root.style.setProperty('--base-width', `${baseWidth}px`);
    root.style.setProperty('--base-height', `${baseHeight}px`);
}

function updateScale() {
    captureBaseDimensions();
    if (!baseWidth || !baseHeight) return;

    let availableWidth = window.innerWidth;
    let availableHeight = window.innerHeight;

    if (wrap) {
        const styles = getComputedStyle(wrap);
        const horizontalPadding = parseFloat(styles.paddingLeft) + parseFloat(styles.paddingRight);
        const verticalPadding = parseFloat(styles.paddingTop) + parseFloat(styles.paddingBottom);
        availableWidth = Math.max(120, availableWidth - horizontalPadding);
        availableHeight = Math.max(120, availableHeight - verticalPadding);
    }

    const scale = Math.min(availableWidth / baseWidth, availableHeight / baseHeight);
    const safeScale = Math.max(0.3, Math.min(scale, 3));
    root.style.setProperty('--app-scale', safeScale.toFixed(4));
}

window.addEventListener('resize', updateScale);

render(0);
captureBaseDimensions();
updateScale();
