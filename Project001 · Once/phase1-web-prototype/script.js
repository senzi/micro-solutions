const capMs = 3 * 3600_000; // 最多3小时
const HOUR = 3600_000;

let running = false, t0 = 0, raf = 0, lastLap = null;

const lcd = document.getElementById('lcd');
const mmEl = document.getElementById('mm');
const ssEl = document.getElementById('ss');
const d10El = document.getElementById('d10');
const status = document.getElementById('status');
const toggle = document.getElementById('toggle');
const chip = document.getElementById('chip');
const target = document.getElementById('target');
const score = document.getElementById('score');
const diff = document.getElementById('diff');
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
    // 小时灯
    lamps.forEach((lamp, i) => lamp.classList.toggle('on', h > i));
}

function evaluate(ms, targetSec) {
    if (!targetSec || !isFinite(targetSec) || targetSec <= 0) return null;
    const tgt = targetSec * 1000;
    const r = Math.abs(ms - tgt) / tgt;
    const sigma = 0.05;
    const val = Math.exp(-0.5 * (r / sigma) * (r / sigma));
    const sc = Math.round(100 * val);
    if (sc >= 90) lcd.animate([{ filter: 'brightness(1.0)' }, { filter: 'brightness(1.2)' }, { filter: 'brightness(1.0)' }], { duration: 420, easing: 'ease-out' });
    return sc;
}

function tick(ts) {
    if (!t0) t0 = ts;
    const elapsed = ts - t0;
    const total = Math.min(elapsed, capMs);
    render(total);
    if (elapsed >= capMs) { stopAt(total, true); return; }
    raf = requestAnimationFrame(tick);
}

function start() {
    running = true; t0 = performance.now();
    toggle.textContent = "STOP";
    lcd.classList.add('active');
    chip.setAttribute('aria-disabled', 'true');
    score.textContent = "";
    diff.textContent = "";
    cancelAnimationFrame(raf);
    raf = requestAnimationFrame(tick);
}

function stopAt(ms, hitCap = false) {
    running = false;
    cancelAnimationFrame(raf);
    lcd.classList.remove('active');
    toggle.textContent = "START";
    chip.removeAttribute('aria-disabled');

    const sc = evaluate(ms, parseFloat(target.value));
    if (sc !== null) {
        score.textContent = `score: ${sc}`;
    } else {
        score.textContent = "";
    }

    if (lastLap !== null) {
        const diffValue = (ms - lastLap) / 1000;
        const diffText = `${diffValue >= 0 ? '+' : ''}${diffValue.toFixed(2)}s`;
        diff.textContent = `diff: ${diffText}`;

        // Color coding for diff
        if (Math.abs(diffValue) < 0.005) { // 0.00 case
            diff.style.color = '#ffb300'; // gold
        } else if (diffValue < 0) {
            diff.style.color = '#4caf50'; // green
        } else {
            diff.style.color = '#f44336'; // red
        }
    } else {
        diff.textContent = "";
    }

    lastLap = ms;
}

toggle.addEventListener('click', () => {
    if (!running) { start(); }
    else {
        const now = performance.now();
        const ms = Math.min(now - t0, capMs);
        stopAt(ms);
    }
});

target.addEventListener('keydown', e => { if (e.key === 'Enter') target.blur(); });
target.addEventListener('blur', () => { const v = parseFloat(target.value); if (!(v > 0)) target.value = ""; });

render(0);
