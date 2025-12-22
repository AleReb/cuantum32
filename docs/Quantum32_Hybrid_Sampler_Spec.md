# Quantum32 Hybrid Sampler Spec
**Filename:** `docs/Quantum32_Hybrid_Sampler_Spec.md`  
**Project name:** **Quantum32 Hybrid Sampler (QHS)**  
**Status:** Implementable specification (Master/Slave **V3** derived from **V2**)  
**Primary goal:** Turn Quantum32 from a purely pedagogical “toy holography” demo into a **hardware-in-the-loop stochastic sampler** that a **PC/notebook** can use for **combinatorial optimization** (e.g., Max-Cut) and as a stepping stone toward **RBM negative-phase sampling**.

---

## 1. Scope and non-goals

### 1.1 In-scope
- Keep the existing **Master–Slave I2C** architecture.
- Use **V2 code style** (non-blocking, no fatal aborts) as the baseline.
- Implement **bidirectional** control:
  - **PC → Master → Slaves:** parameters and run control.
  - **Slaves → Master → PC:** **observables only** (no full state streaming for PoC).
- Provide a clean data contract so a **second AI** can work on the repo without guessing.

### 1.2 Explicit non-goals (for the PoC)
- No “all-training-on-ESP32”.
- No large state vectors over I2C.
- No guarantee of equilibrium or detailed-balance sampling.
- No claims of “quantum”; this is **stochastic classical sampling**.

---

## 2. Engineering constraints (must-haves)

These are repo-level rules for **all new code** (Master V3 and Slave V3):

1) **No `while(1){}`** to indicate fatal errors.  
2) **No `return;` inside `setup()`** to abort boot.  
3) Always continue initialization; use:
   - **global boolean flags** per subsystem  
   - **Serial diagnostics** in `loop()`  
   - periodic **retry** using `millis()` scheduling  
4) No heavy work inside I2C callbacks (`onReceive`, `onRequest`).

---

## 3. Repository baseline

### 3.1 Relevant folders (current repo)
- `masterV2/masterV2.ino` — non-blocking master baseline
- `cuantumsV2/cuantumsV2.ino` — non-blocking slave baseline
- `master/config.h` — useful pin definitions and feature flags (even if V2 currently uses defaults)
- `docs/` — conceptual and visualization documents
- `WIRING.md` — wiring notes

### 3.2 Current V2 data protocol (as implemented)
**Slave receives (from Master)**
- Tick command:  
  `T,<tick>,<mode>,<noise>\n`  
  Parsed in slave V2 via `parseTickCommand(...)`.

**Slave responds (to Master on I2C request)**
- Observable response:  
  `O,<tick>,<bmask>,<loss>,<noise>,<seed>\n`

**Master parses (from Slave)**
- `sscanf(buf, "%c,%lu,%u,%u,%f,%lu", &prefix, &tick, &bmask, &loss, &noise, &seed)`

This spec keeps those **as the compatible baseline**.

---

## 4. Target architecture (Hybrid Sampler)

### 4.1 Roles
**Hardware**
- Runs **p-bit-like dynamics** on each slave.
- Emits **observables** (compact summaries) that represent sampled configurations.
- Applies parameters from PC (via Master).

**PC/Notebook**
- Defines an objective (e.g., Max-Cut on a graph).
- Requests batches, evaluates scores, and updates parameters (annealing schedule or learned control).
- Logs and plots metrics (mixing/diversity) to validate PoC.

### 4.2 Minimal hybrid loop
1) PC connects to Master over USB Serial.
2) PC sets parameters (T, bias, coupling, mode).
3) Master pushes params to all slaves over I2C.
4) PC requests a batch of **K** samples; Master orchestrates ticks and collects observables.
5) PC computes:
   - Max-Cut score (or other objective)
   - diversity and autocorrelation metrics
6) PC updates parameters and repeats.

---

## 5. Data model: Observables

### 5.1 Per-slave observable record
A single record corresponds to “what one slave produced for a given tick”.

| Field | Type | Meaning | Notes |
|---|---:|---|---|
| `tick` | uint32 | global tick id | set by Master |
| `slave_id` | uint8 | index of slave in Master list | added by Master when streaming to PC |
| `bmask` | uint16 | compact boundary configuration | treated as “observable microstate” |
| `loss` | uint16 | proxy loss / mismatch counter | pedagogy or objective proxy |
| `noise` | float (2–3 dp) | effective temperature | should match parameter T |
| `seed` | uint32 | PRNG state / nonce | helps debug non-determinism |

**Why `bmask` counts as “observable”:** it is not the full internal state; it is a compact, lossy view that is enough for PoC optimization and sampler validation.

### 5.2 Batch stream format to PC
To keep parsing robust, Master MUST prefix machine-readable lines with `@` (control) and use `O,` lines strictly for data.

Example:
```
@BATCH RUN=12 T=0.20 B=10 Kp=40 M=1 K=100 STRIDE=1 BURN=20
O,1234,0,9,2,0.20,30123456
O,1234,1,3,0,0.20,99123001
...
@DONE RUN=12 LASTTICK=1333
```

---

## 6. Parameter model (PC → hardware)

### 6.1 Parameters to control (PoC)
| Name | Symbol | Type | Range | Meaning |
|---|---|---|---|---|
| Noise / Temperature | `T` | float | 0.00–1.00 | flip intensity / randomness |
| Bias | `B` | int8 | -127..127 | pushes configurations toward 0/1 patterns |
| Coupling | `Kp` | uint8 | 0..255 | introduces correlation / smoothness |
| Mode | `M` | uint8 | 0..255 | selects dynamics variant |

**Notes**
- V2 already supports `T` and `M` (via the tick command).
- V3 introduces `B` and `Kp` and moves parameters into a dedicated command (recommended).

### 6.2 Annealing schedules (PC side)
Two supported patterns:
1) **Manual schedule:** PC sweeps T from high → low.
2) **Adaptive schedule:** PC updates T/B/Kp based on score improvements.

---

## 7. Communication protocols

### 7.1 I2C: Master ↔ Slaves
**Constraints**
- Typical I2C read buffer is small; V2 uses **≤32 bytes** reads.
- Keep message lengths under 32 bytes for PoC. Avoid chunking in V3 unless needed.

#### 7.1.1 V3 I2C commands
To keep compatibility, do not break V2; add one new command type.

1) **SET_PARAMS** (new in V3)  
   `P,<T>,<B>,<Kp>,<M>\n`  
   - Sent infrequently (only when parameters change).
   - Slaves store parameters; do not compute heavy updates inside callback.

2) **TICK** (existing V2)  
   Keep V2 tick for synchronization:
   - Option A (compatible): `T,<tick>,<M>,<T>\n`
   - Option B (cleaner): `T,<tick>\n` and rely on last `P` for params.

**Recommendation:** Use **Option B** in V3 (short, robust) once `P` exists; keep Option A as fallback during migration.

#### 7.1.2 Slave response (unchanged)
`O,<tick>,<bmask>,<loss>,<noise>,<seed>\n`

### 7.2 Serial: PC ↔ Master
**Transport:** USB Serial @ 115200 (default)

#### 7.2.1 PC → Master commands
- `@HELLO`
- `@SET T=0.20 B=10 Kp=40 M=1`
- `@GET K=200 STRIDE=1 BURN=20`
- `@STOP`

#### 7.2.2 Master → PC responses
- `@HELLO OK FW=MasterV3 SLAVES=4`
- `@ACK ...` / `@ERR ...`
- `@STATUS ...` (periodic health)
- `@BATCH ...`, data lines `O,...`, `@DONE ...`

---

## 8. Non-blocking state machines

### 8.1 Master V3 state machine
- `BOOT`
- `INIT` (set flags; never abort)
- `IDLE` (wait for PC commands)
- `APPLY_PARAMS` (push `P` to all slaves with retry counters)
- `SAMPLING`
  - schedule ticks
  - request slave responses
  - stream `O,...` lines to PC
- `DEGRADED` (optional) if I2C is down; still respond to PC with errors and status

**Non-blocking rule:** each `loop()` iteration must do a small bounded amount of work (e.g., poll one slave per loop).

### 8.2 Slave V3 state machine
- `BOOT`
- `READY`
- `RX_PENDING` (flag set by `onReceive`)
- `DYNAMICS_STEP` (time-sliced updates)
- `BUILD_TX` (prepare `O,...` response in a buffer)
- `REQUEST` (I2C request event reads the prepared buffer only)

---

## 9. Failure policy and observability

### 9.1 Master global flags (minimum)
- `bool i2cOK`
- `bool pcLinkOK` (recent Serial command seen)
- `bool slaveOK[NUM_SLAVES]` (timeout-based, e.g., lastRxMs)
- optional peripheral flags if enabled later: `sdOK`, `oledOK`, `rtcOK`, `bmeOK`, `rgbOK`

### 9.2 Retry policy
- I2C init retry every 2–5 s if bus failed.
- Per-slave command retry counters for `P` and `T` (do not stall the whole system if one slave fails).
- If a slave is missing, keep sampling with remaining slaves.

### 9.3 Logging
- Serial logs for humans:
  - `[INIT] ...`, `[I2C] ...`, `[WARN] ...`
- Serial stream for PC:
  - `@...` control + `O,...` data

SD logging is optional for PoC; if enabled, log **batches** rather than every tick to reduce overhead.

---

## 10. PoC acceptance criteria

The PoC is “approved” if the following can be shown with plots/logs:

### 10.1 Operational robustness
- Runs for **≥30 minutes** without lockups, while:
  - one slave is unplugged/replugged, and/or
  - PC disconnects/reconnects Serial.

### 10.2 Sampler behavior vs temperature
For at least 3 T values (e.g., 0.05, 0.20, 0.50), using K≥200 samples per condition:
- **Diversity increases with T**
  - `unique(bmask)/K` increases as T increases
- **Autocorrelation decreases with T**
  - correlation time (lag where autocorr < 0.2) decreases as T increases

### 10.3 Optimization utility (Max-Cut demo)
Using a small fixed graph (ring or random sparse graph):
- Best Max-Cut score found at low T is **≥** best score at high T baseline.
- A basic annealing schedule T high → low improves best score over fixed T.

---

## 11. Implementation plan (recommended PR sequence)

### PR-1: Serial protocol on Master (no sampling changes)
- Add `@HELLO`, `@SET`, `@GET`, `@STOP`
- Add `@STATUS` periodic line
- Keep V2 I2C tick/response unchanged

### PR-2: Add `P` command (SET_PARAMS) end-to-end
- Master: send `P,<T>,<B>,<Kp>,<M>`
- Slave: parse `P` in loop (non-blocking)
- Keep `T` as synchronization tick only

### PR-3: Sampling driver + batching
- Master: orchestrate burn-in, stride, K batch size
- Stream `@BATCH`, `O,...`, `@DONE`

### PR-4: PC reference client + metrics
- Python:
  - connect, set params, request batches
  - compute diversity + autocorrelation
  - compute Max-Cut objective from bmask mapping
  - export CSV

### PR-5: Documentation + reproducible demo
- Update `docs/` with:
  - wiring assumptions
  - run commands
  - expected output examples
  - plots for PoC criteria

---

## 12. Files to add to the repository

Recommended additions (keep current folders intact):
- `docs/Quantum32_Hybrid_Sampler_Spec.md` (this file)
- `masterV3/` (new)  
  - `masterV3.ino`
- `cuantumsV3/` (new)  
  - `cuantumsV3.ino`
- `pc/` (new)
  - `pc_client_maxcut.py`
  - `metrics.py`
  - `sweep_params.py` (optional)

---

## 13. Reference: mapping bmask to optimization variables

For PoC Max-Cut, treat bits of each slave’s `bmask` as binary variables.

Example:
- `NUM_SLAVES = 4`
- use lower 4 bits of each `bmask` → total **16** variables
- concatenate as: `x = [b0..b3 from slave0, b0..b3 from slave1, ...]`

Max-Cut score:
- For each edge (i, j) with weight w:
  - add w if `x[i] != x[j]`

This is intentionally simple and makes the system immediately useful for “annealing-as-a-service”.

---

## 14. Notes for future extension (RBM / negative phase)

Once the PoC is stable:
- interpret the hardware sampler as a **negative-phase sampler**
- PC computes gradient updates for an RBM using:
  - positive phase from data
  - negative phase from hardware samples (bmask-derived variables)
- Start with tiny RBM (e.g., 8 visible + 8 hidden) and map couplings into local rules gradually.

No RBM claims are part of the PoC; this is a roadmap.

---
