# Hybrid Sampler Spec (Quantum32)

**Repository:** Quantum32 (ESP32-S3 master + multiple I2C slaves)  
**Goal:** Extend the existing I2C master/slave architecture to support a *hybrid training loop* where:
- **Hardware (ESP32 + I2C slaves)** acts as a **non-deterministic sampler** (state generator).
- **PC/Notebook** performs **parameter updates / gradient computation** and requests new samples from hardware.

This document is designed to be explicit enough for another AI (or developer) to implement work in this repo without needing extra context.

---

## 1. Current repository layout (as shipped in the ZIP)

Root (high-level):
- `master/`  
  - `master.ino` (I2C Master v1: aggregates slave results)  
  - `config.h` (feature flags, addresses, pins)
- `masterV2/`  
  - `masterV2.ino` (Master v2 variant)
- `cuantums/`  
  - `cuantums.ino` (Slave v1: returns opinion counts)
- `cuantumsV2/`  
  - `cuantumsV2.ino` (Slave v2: “Non-Blocking” boundary node)
- `docs/`  
  - `quantum_spacetime_sim.md` (conceptual simulation doc)  
  - `visualization_guide.md` (visual interpretation guide)  
  - PDFs (background material)
- `WIRING.md`, `I2C_FIXES.md`, `OLED_MIGRATION.md`, `RGB_LED_GUIDE.md`, `concept.md`, `README.md`
- CAD + Eagle files: `modelo_3d_cuantum32/`, `Cuantum32/`, etc.

**Important:** The existing system already implements:
- Master polling slaves via I2C
- Slaves responding with a compact payload (v1 uses a CSV-like string)
- Optional peripherals controlled by flags (OLED, RTC, SD, BME280, RGB LED)

This spec **extends**; it does not rewrite the architecture.

---

## 2. Terminology

- **Master:** ESP32-S3 device coordinating the network; connected to PC via USB Serial.
- **Slave / Boundary Node:** I2C node returning a payload on request.
- **Sample:** A discrete system state returned to the PC, suitable for “negative phase” sampling (or at minimum, a structured observable derived from that state).
- **Parameters:** Control variables sent from PC → Master → Slaves (e.g., temperature/noise, bias, coupling scalar, or other model parameters).
- **PoC:** Proof-of-concept milestone, minimal but defendable.

---

## 3. Non-negotiable engineering constraints (repo style)

These constraints must be preserved across all new code:

1) **No `while(1){}` infinite blocks** for “fatal error” handling.  
2) **No `return;` from `setup()`** to abort initialization.  
3) Always continue with remaining initializations, using:
- **global boolean flags** for each subsystem  
- **Serial diagnostic messages** in `loop()`  
- optional **retries** (non-blocking; time-based backoff)

This applies to **Master and Slaves**.

---

## 4. What must be defined before writing code

### 4.1. Select baseline firmware versions
Decide which files are the active baseline:
- Master: `master/master.ino` + `master/config.h` **or** `masterV2/masterV2.ino`
- Slave: `cuantumsV2/cuantumsV2.ino` (preferred due to “non-blocking” approach) or `cuantums/cuantums.ino`

**Default recommendation for PoC:** Master v1 + Slave v2 (least disruption; preserves config flags).

### 4.2. Define “sample” for PoC (minimal viable)
Choose one of these formats:

**Option A — Observables only (fastest):**
- Keep current (favor/contra/neutral) and add at least 1–2 extra observables (e.g., local “energy” proxy, magnetization-like scalar).
- Pros: minimal refactor.
- Cons: not a full state sampler.

**Option B — Full discrete state (recommended):**
- Each slave returns a bitset/spin vector chunk (e.g., 8–32 bits).
- Master concatenates into N-bit state and forwards to PC.
- Pros: true sampler; enables mixing/diversity metrics properly.
- Cons: requires defining a compact binary payload.

**PoC target:** Option B with N = 32 or 64 bits.

### 4.3. Define parameter set (PC → hardware)
Minimal parameter set (PoC):
- `T` (temperature/noise strength) : uint8 or fixed-point
- `B` (global bias) : int8
- `K` (global coupling scalar) : uint8 or fixed-point

Optional later:
- per-node bias vector
- per-edge coupling list or sparse adjacency

### 4.4. Define update cadence
- `burn_in_steps` (discarded steps after parameter change)
- `stride` (steps between returned samples)
- `K_samples` (batch size returned per request)

---

## 5. System overview (hybrid loop)

### 5.1. Responsibilities

**Hardware (Master + Slaves)**
- Maintain discrete system dynamics.
- Accept parameters and apply them.
- Generate samples (states/observables) upon request.
- Provide status/health information.

**PC/Notebook**
- Request samples, collect batches.
- Compute metrics:
  - autocorrelation / correlation time (mixing)
  - diversity (unique states, Hamming distance)
  - entropy proxy
- Optionally compute parameter updates (training loop).

### 5.2. High-level sequence

1) PC connects via Serial to Master.
2) PC sends `SET_PARAMS`.
3) Master distributes params to all Slaves via I2C.
4) PC sends `REQUEST_SAMPLES(K)`.
5) Master triggers sampling and returns K samples (or aggregated batch).
6) PC computes metrics and decides next params; loop repeats.

---

## 6. Communication specifications

### 6.1. PC ↔ Master transport
- Transport: USB Serial (115200 default)
- Protocol: framed messages (recommended), not raw CSV logs.

**Must define:**
- framing (start byte/marker, length, type, payload, checksum)
- timeouts and retries
- version field for forward compatibility

### 6.2. Master ↔ Slave transport
- Transport: I2C
- Current master code requests up to 32 bytes (`Wire.requestFrom(address, 32)` in v1), so design payloads accordingly.
- If payload > 32 bytes, define chunking (page index + total pages).

**Payload type must be explicit:**
- `SAMPLE_STATE` (binary bitset recommended)
- `STATUS` (health + counters)
- `ACK` for parameter updates (optional but recommended)

---

## 7. Message formats (normative)

### 7.1. PC ↔ Master (Serial)

Define message types (minimum set):
- `HELLO` (PC → Master): request protocol/version/status
- `STATUS` (Master → PC): flags + counts
- `SET_PARAMS` (PC → Master): {T, B, K, burn_in, stride}
- `GET_SAMPLES` (PC → Master): {K_samples}
- `SAMPLES` (Master → PC): batch payload

**Batch payload requirements:**
- include `run_id` / `seq`
- include `timestamp_ms` (from millis) for ordering
- include `params_hash` so PC can confirm which params produced the batch

### 7.2. Master ↔ Slave (I2C)

Define I2C commands (1-byte opcodes):
- `0x01 SET_PARAMS`
- `0x02 GET_SAMPLE`
- `0x03 GET_STATUS`

Define payload layout for `SET_PARAMS` (fixed-size):
- Byte0: opcode
- Byte1: T
- Byte2: B (two’s complement)
- Byte3: K
- Byte4–5: burn_in_steps (uint16)
- Byte6–7: stride (uint16)

Define payload layout for `GET_SAMPLE` response (example for 32-bit state):
- Byte0: sample_id (uint8 rolling)
- Byte1–4: state_bits (uint32, little-endian)
- Byte5: observables (optional flags)
- Byte6: checksum (optional)
- Remaining bytes unused/reserved

If N > 32, use chunking:
- Byte0: sample_id
- Byte1: chunk_index
- Byte2: chunk_count
- Byte3–(3+chunk_len-1): chunk_bits
- Last byte: checksum (optional)

---

## 8. State machines (implementation guidance)

### 8.1. Master state machine
- `BOOT`
- `INIT_PERIPHERALS` (sets flags; never aborts)
- `IDLE`
- `APPLY_PARAMS` (distribute to slaves; track per-slave success)
- `SAMPLING` (collect K samples; store in buffer; non-blocking loop)
- `STREAM_TO_PC` (serialize and send batch; non-blocking)

### 8.2. Slave state machine
- `BOOT`
- `READY`
- `RUN_DYNAMICS` (continuous background updates, time-sliced)
- `ON_SET_PARAMS` (apply; reset burn-in counters)
- `ON_GET_SAMPLE` (return latest sampled state or generate on-demand)

**Key requirement:** sampling must not block I2C callbacks for long time.  
Use cached/latest state or minimal work during request callback.

---

## 9. Reliability & observability

### 9.1. Global flags (mandatory)
Master must track:
- `i2c_ok`
- `serial_ok` (link active / last command time)
- `sd_ok`
- `oled_ok`
- `rtc_ok`
- `bme_ok`
- `rgb_ok`
- `slave_ok[i]` (per slave)

Slaves must track:
- `i2c_ok`
- `params_ok`
- `dynamics_ok`
- counters: `set_params_count`, `sample_count`, `i2c_errors`

### 9.2. Retry policy
- No blocking loops.
- Use `millis()` scheduling with backoff:
  - e.g., retry I2C init every 2–5 seconds if failed
  - retry SD init periodically if missing card

### 9.3. Logging policy
- Master logs to Serial periodically:
  - flags summary
  - per-slave last seen
  - sample throughput
- Optional SD logging:
  - `run_id, seq, params, sample_count, error_counts`

---

## 10. Validation metrics (PC-side) to accept PoC

Minimum metrics to compute from returned states:

1) **Diversity**
- unique states ratio
- mean Hamming distance between consecutive samples

2) **Mixing**
- autocorrelation vs lag for a few bits
- estimate correlation time (lag where autocorr < threshold)

3) **Stability**
- distribution of basic observables (e.g., magnetization proxy) stable under same params
- predictable change when T changes

**PoC acceptance criteria (pragmatic):**
- With fixed params, sampler produces > X unique states in K samples
- Autocorrelation decays (not flat at 1)
- Increasing T increases diversity/entropy proxy

---

## 11. Implementation milestones (recommended PR sequence)

**PR-1: Protocol skeleton**
- Add Serial framing + command parser on Master
- Implement `HELLO/STATUS` end-to-end
- No sampling changes yet

**PR-2: Param distribution**
- Implement `SET_PARAMS` PC→Master, Master→Slaves
- Add per-slave ACK/status

**PR-3: Sample payload**
- Implement `GET_SAMPLE` I2C opcode + binary response
- Master concatenates into N-bit state and sends `SAMPLES`

**PR-4: PC reference client**
- Python scripts:
  - connect, set params, request batch
  - compute metrics and export plots

**PR-5: Documentation + demo**
- Update `docs/` with diagrams, run instructions, expected outputs

---

## 12. Files to add (proposed)

Inside repo:
- `docs/Hybrid_Sampler_Spec.md` (this file)
- `pc/` (or `tools/pc/`) for Python client:
  - `client.py`
  - `metrics.py`
  - `plot.py`
- Optional: `protocol/` with a single source of truth:
  - `protocol.md` (opcode tables, frame format)
  - `protocol.h` (shared constants for firmware)

---

## 13. Run instructions (baseline)

1) Flash Master firmware (baseline + protocol skeleton).
2) Flash N slave nodes (unique I2C addresses).
3) Verify on Serial:
- peripherals flags
- slave discovery / polling
4) Run PC client:
- `HELLO`
- `SET_PARAMS`
- `GET_SAMPLES`
5) Confirm metrics output.

---

## 14. Notes for contributors (and other AIs)

- Do not change existing peripheral wiring assumptions unless also updating `WIRING.md`.
- Keep payload sizes compatible with I2C constraints; prefer binary over CSV for sampling.
- Avoid heavy computation in I2C callbacks.
- All new features must degrade gracefully when a module is missing.
- Keep “educational visualization” features (OLED/RGB) optional and independent of sampling path.

