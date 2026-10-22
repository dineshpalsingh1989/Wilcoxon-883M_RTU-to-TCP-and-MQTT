#pragma once

// Small local canvas renderer for the dashboard's line and peak plots.
static const char LOCAL_CHARTS_JS[] PROGMEM = R"js(
class Chart {
    static register() {}
    constructor(ctx, config) {
        this.ctx = ctx;
        this.data = config.data;
        this.options = config.options || {};
        ctx.canvas.setAttribute('role', 'img');
        this.observer = new ResizeObserver(() => this.update());
        this.observer.observe(ctx.canvas.parentElement);
        this.update();
    }
    update() {
        const ctx = this.ctx, canvas = ctx.canvas;
        const width = Math.max(220, canvas.parentElement.clientWidth - 24);
        const height = 240, ratio = window.devicePixelRatio || 1;
        canvas.style.width = '100%'; canvas.style.height = height + 'px';
        canvas.width = width * ratio; canvas.height = height * ratio;
        ctx.scale(ratio, ratio);
        const left = 62, top = 30, right = width - 18, bottom = height - 42;
        const datasets = this.data.datasets || [], labels = this.data.labels || [];
        const point = (value, i) => typeof value === 'object' && value !== null
            ? value : {x: typeof labels[i] === 'number' ? labels[i] : i, y: value};
        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
        for (const dataset of datasets) (dataset.data || []).forEach((value, i) => {
            const p = point(value, i);
            if (!Number.isFinite(p.x) || !Number.isFinite(p.y)) return;
            minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x);
            minY = Math.min(minY, p.y); maxY = Math.max(maxY, p.y);
        });
        const hasData = Number.isFinite(minX);
        if (!hasData) { minX = 0; maxX = 1; minY = 0; maxY = 1; }
        if (Number.isFinite(this.options.scales?.y?.min)) minY = this.options.scales.y.min;
        if (Number.isFinite(this.options.scales?.y?.max)) maxY = this.options.scales.y.max;
        if (maxX === minX) maxX = minX + 1;
        if (maxY === minY) maxY = minY + 1;
        const x = value => left + (value - minX) / (maxX - minX) * (right - left);
        const y = value => bottom - (value - minY) / (maxY - minY) * (bottom - top);
        ctx.font = '12px system-ui'; ctx.fillStyle = '#536171';
        ctx.strokeStyle = '#dce3ea'; ctx.lineWidth = 1;
        for (let i = 0; i <= 4; i++) {
            const value = minY + (maxY - minY) * i / 4;
            ctx.beginPath(); ctx.moveTo(left, y(value)); ctx.lineTo(right, y(value)); ctx.stroke();
            ctx.textAlign = 'right'; ctx.fillText(value.toFixed(3), left - 6, y(value) + 4);
        }
        ctx.textAlign = 'left'; ctx.fillText(String(labels[0] ?? minX), left, bottom + 18);
        ctx.textAlign = 'right'; ctx.fillText(String(labels[labels.length - 1] ?? maxX), right, bottom + 18);
        ctx.textAlign = 'center';
        const title = datasets[0]?.label || this.options.scales?.y?.title?.text || '';
        canvas.setAttribute('aria-label', title + (hasData ? '' : ': no captured data'));
        ctx.fillText(title, width / 2, 17);
        ctx.fillText(this.options.scales?.x?.title?.text || '', width / 2, height - 4);
        if (!hasData) {
            ctx.fillText(this.options.plugins?.customCanvasMessage?.text || 'No captured data yet', width / 2, height / 2);
            return;
        }
        ctx.save(); ctx.beginPath(); ctx.rect(left, top, right-left, bottom-top); ctx.clip();
        for (const dataset of datasets) {
            ctx.strokeStyle = dataset.borderColor || '#198754';
            ctx.fillStyle = dataset.backgroundColor || ctx.strokeStyle;
            ctx.lineWidth = dataset.borderWidth || 1.5;
            const values = dataset.data || [];
            if (dataset.showLine === false) {
                values.forEach((value, i) => {
                    const p = point(value, i);
                    if (!Number.isFinite(p.x) || !Number.isFinite(p.y)) return;
                    ctx.beginPath(); ctx.arc(x(p.x), y(p.y), 4, 0, Math.PI * 2); ctx.fill();
                });
                continue;
            }
            // Keep each pixel bucket's extrema so narrow vibration peaks remain visible.
            const step = Math.max(1, Math.ceil(values.length / (right - left)));
            ctx.beginPath(); let started = false;
            for (let i = 0; i < values.length; i += step) {
                let low, high;
                for (let j = i; j < Math.min(i + step, values.length); j++) {
                    const p = point(values[j], j);
                    if (!Number.isFinite(p.x) || !Number.isFinite(p.y)) continue;
                    if (!low || p.y < low.y) low = p;
                    if (!high || p.y > high.y) high = p;
                }
                if (!low) continue;
                const points = low.x <= high.x ? [low, high] : [high, low];
                for (const p of points) {
                    if (!started) { ctx.moveTo(x(p.x), y(p.y)); started = true; }
                    else ctx.lineTo(x(p.x), y(p.y));
                }
            }
            ctx.stroke();
        }
        ctx.restore();
    }
}
)js";
