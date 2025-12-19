# Cuantum Life: Holographic Edition (v1.2)

**Cuantum Life** is a specialized implementation of Conway's Game of Life for the **Cuantum32** hardware, merging "Fast Life" optimization with the "MasterV2" holographic architecture.

## The Concept

This project implements a **Holographic Universe** metaphor:

*   **The Bulk (OLED Screen)**: Represents the emergent spacetime where the "Life" simulation occurs.
*   **The Boundary (I2C Slaves)**: Represents the "holographic screen" or "event horizon" surrounding the universe.

In standard Game of Life, the universe is closed and often dies (heat death) or freezes (stable loops). In **Cuantum Life**, the universe is **open**. The I2C Slaves act as entropy sources, injecting fluctuations (random cells) into the Bulk:
1.  **Reactive**: If a slave detects high noise (from real-world sensors), it immediately injects life.
2.  **Spontaneous**: Even in silence, each slave acts as a quantum fluctuation source, injecting life automatically every 2-8 seconds.

See [game_of_life_concept.md](game_of_life_concept.md) for a full theoretical explanation.

## Controls

*   **Button A (GPIO 2)**: **Reset / Big Bang**. Restarts the unknown universe with a new random seed.
*   **Button B (GPIO 1)**: **Scale Toggle**. Switches between:
    *   *Scale 1*: 128x64 resolution (1 pixel per cell). High detail, slower.
    *   *Scale 2*: 64x32 resolution (2x2 pixels per cell). Chunky, retro, easier to see.

## Feedback Systems

### 1. Visual Feedback (OLED)
*   **Active Slaves**: Small circular indicators appear in the 4 corners of the screen if the corresponding Slave (0-3) is connected and active.
*   **Interaction Flash**: When a Slave injects "life" (entropy) into the system, its sector of the screen momentarily flashes inverted colors.

### 2. Status LED (NeoPixel)
The RGB LED acts as a "Universe Health Monitor", changing color based on **Population Density**:
*   **Blue/Cyan**: Low density (Sparse, expanding universe).
*   **Purple**: Medium density (Stable growth).
*   **Red**: High density (Crowded).
*   **White (Bright)**: Overpopulation or "Injection Flash" (Energy burst from boundary).

### 3. Serial Monitor (Debug)
Connect at **115200 baud** to see real-time statistics (1Hz):
*   `Gen`: Current generation number.
*   `Pop`: Total population count.
*   `Slaves`: Connectivity status and Noise levels for each node (e.g., `[0:OK N=0.45]`).

## Hardware Configuration (Cuantum32)

*   **Master**: ESP32
*   **Display**: SH1106 OLED (0x3C)
*   **Storage**: SD Card (HSPI: CS=7, MOSI=6, MISO=4, CLK=5)
*   **Feedback**: NeoPixel (Pin 48)
*   **I2C Slaves**: Addresses 0x10, 0x11, 0x12, 0x13.

## License

Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0).
**Author**: Alejandro Rebolledo (arebolledo@udd.cl)
