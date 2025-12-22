# pc_client_maxcut_spyder.py
# Spyder-friendly: no argparse, optional interactive prompts, internal defaults.

import time
import sys
import numpy as np

try:
    import serial
except Exception as e:
    serial = None
    print("ERROR: pyserial is not available. Install it with: pip install pyserial")
    print("Details:", e)


# -------------------------
# USER SETTINGS (EDIT HERE)
# -------------------------
ASK_USER = True          # If Spyder console input is annoying, set False and edit defaults below.
SERIAL_PORT = "COM16"     # Example: "COM7" on Windows, "/dev/ttyUSB0" on Linux
BAUD_RATE = 115200

NUM_SLAVES = 4
BITS_PER_SLAVE = 4

# Hybrid sampler params sent to Master
PARAM_T = 0.20           # 0..1
PARAM_B = 5              # -127..127
PARAM_KP = 60            # 0..255 (coupling)
PARAM_M = 1              # mode

# Batch request
BATCH_K = 300            # number of emitted samples (ticks that are emitted)
BATCH_STRIDE = 1
BATCH_BURN = 20

# Problem instance (Max-Cut PoC)
GRAPH_TYPE = "ring"      # "ring" or "custom"
RING_WEIGHT = 1.0

# Runtime
READ_TIMEOUT_S = 25      # stop if no data for N seconds
PRINT_STATUS_LINES = True


# -------------------------
# HELPERS
# -------------------------
def clamp_int(x, lo, hi):
    return max(lo, min(hi, int(x)))

def clamp_float(x, lo, hi):
    return max(lo, min(hi, float(x)))

def bits_from_bmask(bmask: int, width: int = 4):
    return np.array([(bmask >> i) & 1 for i in range(width)], dtype=np.int8)

def maxcut_score(bits: np.ndarray, edges):
    score = 0.0
    for (i, j, w) in edges:
        if bits[i] != bits[j]:
            score += w
    return score

def make_ring_edges(n, w=1.0):
    return [(i, (i + 1) % n, float(w)) for i in range(n)]

def build_edges(n_bits):
    if GRAPH_TYPE == "ring":
        return make_ring_edges(n_bits, RING_WEIGHT)
    # Add custom graph here if needed
    # Example:
    # return [(0, 1, 1.0), (1, 2, 2.0), ...]
    return make_ring_edges(n_bits, RING_WEIGHT)

def parse_kv_from_batch_header(line: str):
    # line format: "@BATCH RUN=.. TICK0=.. K=.. ..."
    out = {}
    try:
        tokens = line.replace("@BATCH", "").strip().split()
        for t in tokens:
            if "=" in t:
                k, v = t.split("=", 1)
                out[k.strip()] = v.strip()
    except Exception:
        pass
    return out

def safe_input(prompt, default):
    try:
        s = input(f"{prompt} [{default}]: ").strip()
        if s == "":
            return default
        return s
    except Exception:
        # In some Spyder configs input() can be disabled
        return default


# -------------------------
# MAIN
# -------------------------
def main():
    global SERIAL_PORT, BAUD_RATE, NUM_SLAVES, BITS_PER_SLAVE
    global PARAM_T, PARAM_B, PARAM_KP, PARAM_M
    global BATCH_K, BATCH_STRIDE, BATCH_BURN

    if serial is None:
        return

    if ASK_USER:
        SERIAL_PORT = safe_input("Serial port", SERIAL_PORT)
        BAUD_RATE = int(safe_input("Baud rate", BAUD_RATE))
        NUM_SLAVES = int(safe_input("Number of slaves", NUM_SLAVES))
        BITS_PER_SLAVE = int(safe_input("Bits per slave (from bmask)", BITS_PER_SLAVE))

        PARAM_T = float(safe_input("T (0..1)", PARAM_T))
        PARAM_B = int(safe_input("Bias B (-127..127)", PARAM_B))
        PARAM_KP = int(safe_input("Coupling Kp (0..255)", PARAM_KP))
        PARAM_M = int(safe_input("Mode M (0..255)", PARAM_M))

        BATCH_K = int(safe_input("Batch K (emitted samples)", BATCH_K))
        BATCH_STRIDE = int(safe_input("Stride", BATCH_STRIDE))
        BATCH_BURN = int(safe_input("Burn-in ticks", BATCH_BURN))

    # Clamp to safe ranges
    PARAM_T = clamp_float(PARAM_T, 0.0, 1.0)
    PARAM_B = clamp_int(PARAM_B, -127, 127)
    PARAM_KP = clamp_int(PARAM_KP, 0, 255)
    PARAM_M = clamp_int(PARAM_M, 0, 255)

    BATCH_K = clamp_int(BATCH_K, 1, 2000)
    BATCH_STRIDE = clamp_int(BATCH_STRIDE, 1, 1000)
    BATCH_BURN = clamp_int(BATCH_BURN, 0, 5000)

    n_bits = NUM_SLAVES * BITS_PER_SLAVE
    edges = build_edges(n_bits)

    print("\n--- CONFIG ---")
    print("PORT:", SERIAL_PORT)
    print("SLAVES:", NUM_SLAVES, "BITS/SLAVE:", BITS_PER_SLAVE, "TOTAL BITS:", n_bits)
    print("PARAMS: T=%.2f B=%d Kp=%d M=%d" % (PARAM_T, PARAM_B, PARAM_KP, PARAM_M))
    print("BATCH: K=%d STRIDE=%d BURN=%d" % (BATCH_K, BATCH_STRIDE, BATCH_BURN))
    print("GRAPH:", GRAPH_TYPE, "EDGES:", len(edges))
    print("--------------\n")

    # Open serial
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(0.5)
        ser.reset_input_buffer()
    except Exception as e:
        print("ERROR: Could not open serial port.")
        print("Details:", e)
        print("Tip: close Arduino Serial Monitor / other apps using the port.")
        return

    def send(cmd: str):
        ser.write((cmd.strip() + "\n").encode("utf-8"))

    # Handshake and start
    send("@HELLO")
    time.sleep(0.05)
    # MasterV3 uses N for Noise, and @PARAM to broadcast everything
    send(f"@PARAM N={PARAM_T:.2f} B={PARAM_B} K={PARAM_KP} M={PARAM_M}")
    time.sleep(0.05)
    send(f"@GET K={BATCH_K} STRIDE={BATCH_STRIDE} BURN={BATCH_BURN}")

    best_score = -1e18
    best_bits = None
    best_tick = None

    last_rx_time = time.time()
    batch_meta = None

    # We collect per-tick lines (one per slave) then score when a tick is complete
    tick_buffer = {}  # tick -> dict(slaveIndex -> (bmask, loss, noise, seed))

    print("Listening... (Ctrl+C to stop)\n")

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            if line:
                last_rx_time = time.time()

                if line.startswith("@BATCH"):
                    batch_meta = parse_kv_from_batch_header(line)
                    print(f"[META] {batch_meta}")
                    continue

                if line.startswith("@DONE"):
                    print(f"[DONE] {line}")
                    break

                # Print everything else to see what's happening
                if not line.startswith("O,") and not line.startswith("@"):
                    print(f"[MSG] {line}")
                
                if line.startswith("O,"):
                    # O,<tick>,<slaveIndex>,<bmask>,<loss>,<noise>,<seed>
                    print(f"[DATA] {line}") # DEBUG: Verify data flow
                    parts = line.split(",")
                    if len(parts) < 7:
                        continue

                    try:
                        tick = int(parts[1])
                        sidx = int(parts[2])
                        bmask = int(parts[3])
                        loss = int(parts[4])
                        noise = float(parts[5])
                        seed = int(parts[6])
                    except Exception:
                        continue

                    if tick not in tick_buffer:
                        tick_buffer[tick] = {}

                    tick_buffer[tick][sidx] = (bmask, loss, noise, seed)

                    # UX: Progress indicator (dot every 10 lines received)
                    if tick % 10 == 0:
                        print(".", end="", flush=True)

                    if len(tick_buffer[tick]) >= NUM_SLAVES:
                        # build full vector ordered by slave index
                        vec = []
                        for s in range(NUM_SLAVES):
                            if s not in tick_buffer[tick]:
                                break
                            bm, *_rest = tick_buffer[tick][s]
                            vec.append(bits_from_bmask(bm, width=BITS_PER_SLAVE))
                        if len(vec) == NUM_SLAVES:
                            bits = np.concatenate(vec)
                            score = maxcut_score(bits, edges)
                            
                            # UX: Progress indicator
                            if tick % 10 == 0:
                                print(".", end="", flush=True)
                            if tick % 50 == 0:
                                print(f" [{tick}/{BATCH_K}]", end="", flush=True)

                            if score > best_score:
                                best_score = score
                                best_bits = bits.copy()
                                best_tick = tick
                                bitstr = "".join(str(int(x)) for x in best_bits.tolist())
                                # Newline before printing best to not mess up dots
                                print(f"\n[BEST] tick={best_tick} score={best_score:.2f} bits={bitstr}")
                                
                        # cleanup old ticks to keep memory bounded
                        # keep only last few ticks
                        if len(tick_buffer) > 50:
                            for old in sorted(tick_buffer.keys())[:-20]:
                                tick_buffer.pop(old, None)

            # Timeout handling
            if (time.time() - last_rx_time) > READ_TIMEOUT_S:
                print(f"TIMEOUT: No data received for {READ_TIMEOUT_S} seconds. Stopping.")
                break

    except KeyboardInterrupt:
        print("\nStopped by user (KeyboardInterrupt).")
    finally:
        try:
            ser.close()
        except Exception:
            pass

    print("\n--- RESULT ---")
    print("Best score:", best_score)
    if best_bits is not None:
        print("Best tick:", best_tick)
        print("Bits:", "".join(str(int(x)) for x in best_bits.tolist()))
    else:
        print("No valid samples scored.")
    print("--------------")


if __name__ == "__main__":
    main()
