/**
 * QUANTUM32 HYBRID SAMPLER - WEB PORTAL V1.0
 * Logic & Controller
 */

class QuantumPortal {
    constructor() {
        // Serial State
        this.port = null;
        this.reader = null;
        this.writer = null;
        this.reading = false;
        this.decoder = new TextDecoder();
        this.buffer = "";

        // Data Storage
        this.allScores = [];
        this.allSamples = [];
        this.bestScore = 0;
        this.bestBits = null;
        this.sampleCount = 0;
        this.tickBuffer = {}; // { tick: { sidx: bmask } }
        this.historyX = [];
        this.historyY = [];

        // Config
        this.NUM_SLAVES = 4;
        this.N_BITS = 16;
        this.EDGES = this.generateRingEdges(16);
        this.BATCH_K = 300;
        this.lastSentT = -1.0;

        // Charts & Visuals
        this.chartEvo = null;
        this.chartHist = null;
        this.canvas = document.getElementById('canvasGraph');
        this.ctx = this.canvas.getContext('2d');
        this.nodePositions = this.calculateNodePositions(16, 200, 200, 150);

        // UI Binding
        this.initUI();
    }

    generateRingEdges(n) {
        const edges = [];
        for (let i = 0; i < n; i++) {
            edges.push([i, (i + 1) % n]);
        }
        return edges;
    }

    calculateNodePositions(n, cx, cy, r) {
        const pos = [];
        for (let i = 0; i < n; i++) {
            const angle = (2 * Math.PI * i) / n - (Math.PI / 2);
            pos.push({
                x: cx + r * Math.cos(angle),
                y: cy + r * Math.sin(angle)
            });
        }
        return pos;
    }

    initUI() {
        // Elements
        this.btnConnect = document.getElementById('btnConnect');
        this.btnStart = document.getElementById('btnStart');
        this.btnStop = document.getElementById('btnStop');
        this.btnClear = document.getElementById('btnClear');
        this.btnSave = document.getElementById('btnSave');
        this.annealCheck = document.getElementById('annealActive');
        this.annealSection = document.getElementById('annealSection');

        // Events
        this.btnConnect.addEventListener('click', () => this.toggleSerial());
        this.btnStart.addEventListener('click', () => this.startBatch());
        this.btnStop.addEventListener('click', () => this.stopBatch());
        this.btnClear.addEventListener('click', () => this.clearData());
        this.btnSave.addEventListener('click', () => this.exportCSV());
        this.annealCheck.addEventListener('change', (e) => {
            this.annealSection.style.display = e.target.checked ? 'block' : 'none';
        });

        this.initCharts();
        this.drawGraph();
    }

    async toggleSerial() {
        if (this.port) {
            await this.disconnect();
        } else {
            await this.connect();
        }
    }

    async connect() {
        try {
            this.port = await navigator.serial.requestPort();
            await this.port.open({ baudRate: 115200 });

            this.btnConnect.innerHTML = '<i class="fas fa-unlink"></i> DESCONECTAR';
            document.getElementById('statusIndicator').className = 'status-badge status-connected';
            document.getElementById('statusIndicator').innerText = 'CONECTADO';
            this.btnStart.disabled = false;
            this.log("Puerto abierto. Baud: 115200");

            this.writer = this.port.writable.getWriter();
            this.readLoop();

            // Send HELLO
            this.sendLine("@HELLO");
        } catch (err) {
            this.log("Error de conexión: " + err.message);
            this.port = null;
        }
    }

    async disconnect() {
        this.reading = false;
        if (this.reader) await this.reader.cancel();
        if (this.writer) this.writer.releaseLock();
        if (this.port) await this.port.close();

        this.port = null;
        this.btnConnect.innerHTML = '<i class="fas fa-link"></i> CONECTAR SERIAL';
        document.getElementById('statusIndicator').className = 'status-badge status-disconnected';
        document.getElementById('statusIndicator').innerText = 'DESCONECTADO';
        this.btnStart.disabled = true;
        this.btnStop.disabled = true;
        this.log("Puerto cerrado.");
    }

    async readLoop() {
        this.reading = true;
        while (this.port && this.port.readable && this.reading) {
            this.reader = this.port.readable.getReader();
            try {
                while (true) {
                    const { value, done } = await this.reader.read();
                    if (done) break;
                    this.processData(this.decoder.decode(value));
                }
            } catch (err) {
                console.error(err);
            } finally {
                this.reader.releaseLock();
            }
        }
    }

    processData(chunk) {
        this.buffer += chunk;
        let lines = this.buffer.split("\n");
        this.buffer = lines.pop(); // Keep last incomplete line

        for (let line of lines) {
            line = line.trim();
            if (!line) continue;

            if (line.startsWith("@DONE")) {
                this.stopBatch(true);
                this.log("Batch finalizado.");
            } else if (line.startsWith("O,")) {
                this.handleObs(line);
            }
        }
    }

    handleObs(line) {
        const p = line.split(",");
        if (p.length < 4) return;

        const tick = parseInt(p[1]);
        const sidx = parseInt(p[2]); // Slave Index (0-3)
        const bmask = parseInt(p[3]); // 4-bit mask

        if (!this.tickBuffer[tick]) this.tickBuffer[tick] = {};
        this.tickBuffer[tick][sidx] = bmask;

        // If we have all 4 slaves for this tick
        if (Object.keys(this.tickBuffer[tick]).length === this.NUM_SLAVES) {
            this.finalizeSample(tick);
        }

        // Cleanup old ticks
        if (Object.keys(this.tickBuffer).length > 50) {
            const keys = Object.keys(this.tickBuffer).sort((a, b) => a - b);
            delete this.tickBuffer[keys[0]];
        }
    }

    finalizeSample(tick) {
        const raw = this.tickBuffer[tick];
        let bits = [];
        for (let i = 0; i < this.NUM_SLAVES; i++) {
            const mask = raw[i] || 0;
            for (let b = 0; b < 4; b++) {
                bits.push((mask >> b) & 1);
            }
        }

        const score = this.calculateMaxCut(bits);
        const bitstr = bits.join("");

        this.sampleCount++;
        this.allScores.push(score);
        this.allSamples.push({ tick, score, bitstr });

        if (score > this.bestScore) {
            this.bestScore = score;
            this.bestBits = bits;
            this.updateBestUI();
        }

        this.historyX.push(this.sampleCount);
        this.historyY.push(this.bestScore);

        this.updateExperimentUI();

        // Simulated Annealing
        if (this.annealCheck.checked) {
            this.handleAnnealing();
        }
    }

    calculateMaxCut(bits) {
        let score = 0;
        for (const [i, j] of this.EDGES) {
            if (bits[i] !== bits[j]) score++;
        }
        return score;
    }

    handleAnnealing() {
        const tStart = parseFloat(document.getElementById('annealTStart').value);
        const tEnd = parseFloat(document.getElementById('annealTEnd').value);
        const progress = Math.min(1, this.sampleCount / this.BATCH_K);
        const currentT = tStart + (tEnd - tStart) * progress;

        if (Math.abs(currentT - this.lastSentT) >= 0.01) {
            const b = document.getElementById('paramB').value;
            const kp = document.getElementById('paramKp').value;
            const m = document.getElementById('paramM').value;
            this.sendLine(`@PARAM N=${currentT.toFixed(2)} B=${b} K=${kp} M=${m}`);
            this.lastSentT = currentT;
        }
    }

    async sendLine(text) {
        if (!this.writer) return;
        const data = new TextEncoder().encode(text + "\n");
        await this.writer.write(data);
    }

    startBatch() {
        const k = parseInt(document.getElementById('paramK').value);
        const t = document.getElementById('paramT').value;
        const b = document.getElementById('paramB').value;
        const kp = document.getElementById('paramKp').value;
        const m = document.getElementById('paramM').value;

        this.BATCH_K = k;
        this.lastSentT = this.annealCheck.checked ? parseFloat(document.getElementById('annealTStart').value) : parseFloat(t);

        this.sendLine(`@PARAM N=${this.lastSentT.toFixed(2)} B=${b} K=${kp} M=${m}`);
        this.sendLine(`@GET K=${k} STRIDE=1 BURN=10`);

        this.btnStart.disabled = true;
        this.btnStop.disabled = false;
        this.log("Batch iniciado: K=" + k);
    }

    stopBatch(auto = false) {
        this.btnStart.disabled = false;
        this.btnStop.disabled = true;
        if (!auto) this.sendLine("@STOP");
    }

    clearData() {
        this.allScores = [];
        this.allSamples = [];
        this.bestScore = 0;
        this.bestBits = null;
        this.sampleCount = 0;
        this.tickBuffer = {};
        this.historyX = [];
        this.historyY = [];

        this.updateBestUI();
        this.updateExperimentUI();
        this.drawGraph();
        this.initCharts();
    }

    updateBestUI() {
        document.getElementById('valBestScore').innerText = `${this.bestScore} / ${this.N_BITS}`;
        document.getElementById('valEfficiency').innerText = `${((this.bestScore / this.N_BITS) * 100).toFixed(1)}%`;

        const grid = document.getElementById('bitsDisplay');
        grid.innerHTML = "";
        if (this.bestBits) {
            this.bestBits.forEach((b, i) => {
                const node = document.createElement('div');
                node.className = `bit-node active-${b}`;
                node.innerText = b;
                grid.appendChild(node);
            });
        }
        this.drawGraph();
    }

    updateExperimentUI() {
        document.getElementById('valProgress').innerText = `${this.sampleCount} / ${this.BATCH_K}`;
        const pct = Math.min(100, (this.sampleCount / this.BATCH_K) * 100);
        document.getElementById('progressBar').style.width = pct + "%";

        if (this.sampleCount % 5 === 0) {
            this.updateCharts();
        }
    }

    initCharts() {
        const ctxEvo = document.getElementById('chartEvo').getContext('2d');
        const ctxHist = document.getElementById('chartHist').getContext('2d');

        if (this.chartEvo) this.chartEvo.destroy();
        if (this.chartHist) this.chartHist.destroy();

        const commonOptions = {
            responsive: true,
            animation: false,
            scales: {
                x: { ticks: { color: "#94a3b8" }, grid: { color: "rgba(255,255,255,0.05)" } },
                y: { ticks: { color: "#94a3b8" }, grid: { color: "rgba(255,255,255,0.05)" } }
            },
            plugins: { legend: { display: false } }
        };

        this.chartEvo = new Chart(ctxEvo, {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'Mejor Score', data: [], borderColor: '#5080f0', borderWidth: 2, pointRadius: 0, fill: true, backgroundColor: 'rgba(80,128,240,0.1)' }] },
            options: commonOptions
        });

        this.chartHist = new Chart(ctxHist, {
            type: 'bar',
            data: { labels: [], datasets: [{ label: 'Frecuencia', data: [], backgroundColor: '#2dd4bf' }] },
            options: commonOptions
        });
    }

    updateCharts() {
        // Evolution
        this.chartEvo.data.labels = this.historyX;
        this.chartEvo.data.datasets[0].data = this.historyY;
        this.chartEvo.update();

        // Histogram
        const hist = {};
        this.allScores.forEach(s => hist[s] = (hist[s] || 0) + 1);
        const labels = Object.keys(hist).sort((a, b) => a - b);
        const data = labels.map(l => hist[l]);

        this.chartHist.data.labels = labels;
        this.chartHist.data.datasets[0].data = data;
        this.chartHist.update();
    }

    drawGraph() {
        const ctx = this.ctx;
        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        // Draw Edges
        ctx.lineWidth = 2;
        for (const [i, j] of this.EDGES) {
            const p1 = this.nodePositions[i];
            const p2 = this.nodePositions[j];
            const isCut = this.bestBits && (this.bestBits[i] !== this.bestBits[j]);

            ctx.strokeStyle = isCut ? "rgba(255, 255, 255, 0.4)" : "rgba(255, 255, 255, 0.05)";
            ctx.beginPath();
            ctx.moveTo(p1.x, p1.y);
            ctx.lineTo(p2.x, p2.y);
            ctx.stroke();
        }

        // Draw Nodes
        this.nodePositions.forEach((p, i) => {
            const val = this.bestBits ? this.bestBits[i] : 0;
            const color = (val === 0) ? "#ef4444" : "#2dd4bf";

            ctx.fillStyle = color;
            ctx.beginPath();
            ctx.arc(p.x, p.y, 10, 0, Math.PI * 2);
            ctx.fill();

            ctx.strokeStyle = "white";
            ctx.lineWidth = 1;
            ctx.stroke();

            // Index label
            ctx.fillStyle = "white";
            ctx.font = "10px sans-serif";
            ctx.textAlign = "center";
            ctx.fillText(i, p.x, p.y + 22);
        });
    }

    exportCSV() {
        if (this.allSamples.length === 0) return;
        let csv = "tick,score,bits\n";
        this.allSamples.forEach(s => {
            csv += `${s.tick},${s.score},${s.bitstr}\n`;
        });

        const blob = new Blob([csv], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `maxcut_web_${new Date().getTime()}.csv`;
        a.click();
        URL.revokeObjectURL(url);
    }

    log(msg) {
        document.getElementById('logDisplay').innerHTML = `<i class="fas fa-info-circle"></i> ${msg}`;
        console.log("[Portal]", msg);
    }
}

// Start App
window.addEventListener('DOMContentLoaded', () => {
    window.portal = new QuantumPortal();
});
