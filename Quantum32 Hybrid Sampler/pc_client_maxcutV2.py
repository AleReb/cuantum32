#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
===============================================================================
            QUANTUM32 HYBRID SAMPLER - CLIENTE MAX-CUT V2 (DEAR PYGUI)
===============================================================================

DESCRIPCIÓN:
    Cliente avanzado para Quantum32 Hybrid Sampler con dashboard nativo
    usando Dear PyGui. Incluye visualización en tiempo real, análisis
    de experimentos y control total del hardware.

CARACTERÍSTICAS:
    - Dashboard nativo con renderizado GPU.
    - Control de parámetros (T, B, Kp, M).
    - Botones de Inicio, Parada, Limpieza y Reinicio.
    - Exportación automática a CSV.

REQUERIMIENTOS:
    pip install pyserial numpy dearpygui

AUTOR: Alejandro Rebolledo (arebolledo@udd.cl)
LICENCIA: CC BY-NC 4.0
===============================================================================
"""

import time
import sys
import os
import math
from datetime import datetime
from glob import glob
import numpy as np

# Serial
try:
    import serial
    SERIAL_OK = True
except ImportError:
    SERIAL_OK = False

# Dear PyGui
try:
    import dearpygui.dearpygui as dpg
    DEARPYGUI_OK = True
except ImportError:
    DEARPYGUI_OK = False


# =============================================================================
# CONFIGURATION & CONSTANTS
# =============================================================================
DEFAULT_PORT = "COM16"
BAUD_RATE = 115200
NUM_SLAVES = 4
BITS_PER_SLAVE = 4

# Max-Cut Problem Edges (Ring Graph)
N_BITS = NUM_SLAVES * BITS_PER_SLAVE
EDGES = [(i, (i + 1) % N_BITS, 1.0) for i in range(N_BITS)]

# Default Params
PARAM_B = 5
PARAM_KP = 60
PARAM_M = 1
BATCH_STRIDE = 1
BATCH_BURN = 50  # Increased for better thermalization

# =============================================================================
# HELPER FUNCTIONS
# =============================================================================
def bits_from_bmask(bmask, width=4):
    return np.array([(bmask >> i) & 1 for i in range(width)], dtype=np.int8)

def maxcut_score(bits, edges):
    score = 0.0
    for (i, j, w) in edges:
        if bits[i] != bits[j]:
            score += w
    return score

def get_script_dir():
    return os.path.dirname(os.path.abspath(__file__))

def get_timestamp():
    return datetime.now().strftime("%Y%m%d_%H%M%S")

# =============================================================================
# DASHBOARD CLASS
# =============================================================================
class QuantumDashboard:
    def __init__(self):
        self.ser = None
        self.running = False
        
        # Data storage
        self.all_scores = []
        self.all_samples = []
        self.best_history_x = []
        self.best_history_y = []
        self.best_score = 0
        self.best_bits = None
        self.sample_count = 0
        self.tick_buffer = {}
        
        # UI State
        self.auto_start_requested = False
        self.anneal_active = False
        self.last_sent_t = -1.0
        self.batch_k = 300
        
        # Graph constants
        self.graph_center = (150, 150)
        self.graph_radius = 120
        self.node_radius = 12
        self.node_positions = []
        for i in range(N_BITS):
            angle = (2 * math.pi * i) / N_BITS - (math.pi / 2)
            x = self.graph_center[0] + self.graph_radius * math.cos(angle)
            y = self.graph_center[1] + self.graph_radius * math.sin(angle)
            self.node_positions.append((x, y))

    def setup_ui(self):
        dpg.create_context()
        dpg.create_viewport(title="Quantum32 Hybrid Sampler V2", width=1200, height=800)
        dpg.setup_dearpygui()

        # Custom Theme
        with dpg.theme() as global_theme:
            with dpg.theme_component(dpg.mvAll):
                dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (20, 20, 30))
                dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (30, 30, 45))
                dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, (50, 80, 150))
                dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 4)
                dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 8)
        dpg.bind_theme(global_theme)

        with dpg.window(label="Control Central", tag="PrimaryWindow"):
            # Header
            with dpg.group(horizontal=True):
                dpg.add_text("⚡ QUANTUM32 MONITOR V2", color=(0, 255, 255))
                dpg.add_spacer(width=20)
                dpg.add_text("Real-time Max-Cut Solver", color=(150, 150, 150))
            
            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Configuration Layout
            with dpg.group(horizontal=True):
                with dpg.child_window(width=300, height=220, border=True):
                    dpg.add_text("CONFIGURACIÓN DE HARDWARE", color=(255, 200, 100))
                    dpg.add_spacer(height=5)
                    dpg.add_input_text(label="Puerto Serial", tag="port_val", default_value=DEFAULT_PORT)
                    dpg.add_input_int(label="Muestras (K)", tag="k_val", default_value=300)
                    dpg.add_input_float(label="Ruido Fijo (T)", tag="t_val", default_value=0.20, format="%.2f")
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Simulated Annealing", tag="anneal_active", default_value=False)
                    with dpg.group(horizontal=True):
                        dpg.add_input_float(label="T Ini", tag="t_start", default_value=0.35, width=80, format="%.2f")
                        dpg.add_input_float(label="T Fin", tag="t_end", default_value=0.05, width=80, format="%.2f")
                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="CONECTAR / INICIAR", callback=self.on_start, width=150)
                        dpg.add_button(label="DETENER", callback=self.on_stop, width=80)

                with dpg.child_window(width=300, height=220, border=True):
                    dpg.add_text("GESTIÓN DE DATOS", color=(255, 200, 100))
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="🔄 REINICIAR MUESTRA", callback=self.on_restart, width=-1, height=40)
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="🗑️ LIMPIAR ESTADÍSTICAS", callback=self.on_clear, width=-1)
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="💾 GUARDAR ÚLTIMO CSV", callback=self.on_save, width=-1)

                with dpg.child_window(width=545, height=220, border=True):
                    dpg.add_text("MÉTRICAS EN TIEMPO REAL", color=(255, 200, 100))
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            dpg.add_text("MEJOR SCORE:")
                            dpg.add_text("0 / 16", tag="ui_best_score", color=(0, 255, 0))
                            dpg.bind_item_font(dpg.last_item(), self.get_large_font())
                        dpg.add_spacer(width=40)
                        with dpg.group():
                            dpg.add_text("EFICIENCIA:")
                            dpg.add_text("0.0%", tag="ui_efficiency", color=(255, 255, 0))
                            dpg.bind_item_font(dpg.last_item(), self.get_large_font())
                        dpg.add_spacer(width=40)
                        with dpg.group():
                            dpg.add_text("MUESTREO:")
                            dpg.add_text("0 / 300", tag="ui_progress_text")

            dpg.add_spacer(height=10)

            # Plots
            with dpg.group(horizontal=True):
                with dpg.plot(label="Evolución del Score", height=320, width=570):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="Tick", tag="x_axis_evo")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Score", tag="y_axis_evo")
                    dpg.add_line_series([], [], label="Mejor Encontrado", parent="y_axis_evo", tag="series_evo")
                    dpg.add_inf_line_series([N_BITS], label="Límite Máximo", parent="y_axis_evo", horizontal=True)
                
                with dpg.plot(label="Distribución de Soluciones", height=320, width=570):
                    dpg.add_plot_axis(dpg.mvXAxis, label="Score", tag="x_axis_hist")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Frecuencia", tag="y_axis_hist")
                    dpg.add_bar_series([], [], label="Docs", parent="y_axis_hist", tag="series_hist")

            dpg.add_spacer(height=10)
            
            # Bits monitor
            with dpg.group(horizontal=True):
                with dpg.child_window(width=300, height=310, border=True):
                    dpg.add_text("PARTICIÓN MAX-CUT", color=(255, 200, 100))
                    with dpg.drawlist(width=300, height=300, tag="graph_drawlist"):
                        # We will draw nodes here
                        pass
                
                with dpg.group():
                    with dpg.child_window(width=815, height=240, border=True):
                        dpg.add_text("MEJOR SOLUCIÓN ENCONTRADA (BITS):", color=(180, 180, 180))
                        dpg.add_text("----------------", tag="ui_bits", color=(0, 255, 255))
                        dpg.bind_item_font(dpg.last_item(), self.get_large_font())
                        dpg.add_spacer(height=20)
                        dpg.add_text("LEYENDA:", color=(150, 150, 150))
                        with dpg.group(horizontal=True):
                            with dpg.drawlist(width=20, height=20): 
                                dpg.draw_circle((10, 10), 8, color=(255, 100, 100), fill=(255, 100, 100))
                            dpg.add_text("Grupo A")
                            dpg.add_spacer(width=20)
                            with dpg.drawlist(width=20, height=20): 
                                dpg.draw_circle((10, 10), 8, color=(100, 255, 200), fill=(100, 255, 200))
                            dpg.add_text("Grupo B")

                    dpg.add_spacer(height=5)
                    # Progress bar and Status
                    dpg.add_progress_bar(tag="ui_progress_bar", width=815, height=25)
                    dpg.add_text("Estado: Desconectado", tag="ui_status", color=(150, 150, 150))

        dpg.show_viewport()
        dpg.set_primary_window("PrimaryWindow", True)

    def get_large_font(self):
        # Fallback if font loading fails, just returns None
        return None

    # --- Callbacks ---
    def on_start(self):
        if self.running: return
        
        port = dpg.get_value("port_val")
        k = dpg.get_value("k_val")
        t = dpg.get_value("t_val")
        
        try:
            self.ser = serial.Serial(port, BAUD_RATE, timeout=0.01)
            time.sleep(0.5)
            self.ser.reset_input_buffer()
            
            self.ser.write(b"@HELLO\n")
            time.sleep(0.05)
            
            # Use initial T from GUI
            t_start = dpg.get_value("t_start") if dpg.get_value("anneal_active") else dpg.get_value("t_val")
            self.last_sent_t = t_start
            
            self.ser.write(f"@PARAM N={t_start:.2f} B={PARAM_B} K={PARAM_KP} M={PARAM_M}\n".encode())
            time.sleep(0.05)
            self.ser.write(f"@GET K={k} STRIDE={BATCH_STRIDE} BURN={BATCH_BURN}\n".encode())
            
            self.running = True
            self.batch_k = k
            dpg.set_value("ui_status", f"Conectado a {port}. Recibiendo...")
        except Exception as e:
            dpg.set_value("ui_status", f"Error de conexión: {e}")

    def on_stop(self):
        self.running = False
        if self.ser:
            self.ser.close()
            self.ser = None
        dpg.set_value("ui_status", "Captura detenida.")

    def on_restart(self):
        """Reinicia la muestra actual limpiando datos y enviando comando de inicio."""
        self.on_stop()
        self.on_clear()
        time.sleep(0.2)
        self.on_start()
        dpg.set_value("ui_status", "Muestra reiniciada.")

    def on_clear(self):
        self.all_scores = []
        self.all_samples = []
        self.best_history_x = []
        self.best_history_y = []
        self.best_score = 0
        self.best_bits = None
        self.sample_count = 0
        self.tick_buffer = {}
        
        dpg.set_value("ui_best_score", "0 / 16")
        dpg.set_value("ui_efficiency", "0.0%")
        dpg.set_value("ui_progress_text", "0 / 300")
        dpg.set_value("ui_progress_bar", 0)
        dpg.set_value("ui_bits", "----------------")
        dpg.set_value("series_evo", [[], []])
        dpg.set_value("series_hist", [[], []])

    def on_save(self):
        if not self.all_samples: return
        ts = get_timestamp()
        path = os.path.join(get_script_dir(), f"maxcut_V2_{ts}.csv")
        with open(path, 'w') as f:
            f.write("tick,score,bits\n")
            for s in self.all_samples:
                f.write(f"{s['tick']},{s['score']},{s['bits']}\n")
        dpg.set_value("ui_status", f"Guardado en: {os.path.basename(path)}")

    # --- Main Loop Logic ---
    def update(self):
        if not self.running or not self.ser: return
        
        # Read incoming lines
        while self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            except: continue
            
            if line.startswith("@DONE"):
                self.running = False
                dpg.set_value("ui_status", "✓ Batch finalizado exitosamente.")
                break
                
            if line.startswith("O,"):
                parts = line.split(",")
                if len(parts) >= 7:
                    try:
                        tick = int(parts[1])
                        sidx = int(parts[2])
                        bmask = int(parts[3])
                        
                        if tick not in self.tick_buffer:
                            self.tick_buffer[tick] = {}
                        self.tick_buffer[tick][sidx] = bmask
                        
                        if len(self.tick_buffer[tick]) >= NUM_SLAVES:
                            # Full sample collected
                            vec = [bits_from_bmask(self.tick_buffer[tick][s]) 
                                  for s in range(NUM_SLAVES) if s in self.tick_buffer[tick]]
                            
                            if len(vec) == NUM_SLAVES:
                                bits = np.concatenate(vec)
                                score = maxcut_score(bits, EDGES)
                                bitstr = "".join(str(int(x)) for x in bits)
                                
                                self.all_scores.append(score)
                                self.sample_count += 1
                                self.all_samples.append({'tick': tick, 'score': score, 'bits': bitstr})
                                
                                if score > self.best_score:
                                    self.best_score = score
                                    self.best_bits = bitstr
                                    dpg.set_value("ui_best_score", f"{score:.0f} / {N_BITS}")
                                    dpg.set_value("ui_efficiency", f"{100*score/N_BITS:.1f}%")
                                    dpg.set_value("ui_bits", bitstr)
                                
                                self.best_history_x.append(tick)
                                self.best_history_y.append(self.best_score)
                                
                            # Cleanup old buffers
                            if len(self.tick_buffer) > 50:
                                for old in sorted(self.tick_buffer.keys())[:-20]:
                                    self.tick_buffer.pop(old, None)
                                    
                        # ANNEALING LOGIC
                        if dpg.get_value("anneal_active"):
                            t_start = dpg.get_value("t_start")
                            t_end = dpg.get_value("t_end")
                            progress = self.sample_count / self.batch_k if self.batch_k > 0 else 0
                            current_t = t_start + (t_end - t_start) * progress
                            
                            if abs(current_t - self.last_sent_t) >= 0.01:
                                self.ser.write(f"@PARAM N={current_t:.2f} B={PARAM_B} K={PARAM_KP} M={PARAM_M}\n".encode())
                                self.last_sent_t = current_t
                                # No print to avoid flooding, but we could update status
                    except: pass

        # Smooth UI update
        dpg.set_value("ui_progress_text", f"{self.sample_count} / {self.batch_k}")
        dpg.set_value("ui_progress_bar", self.sample_count / self.batch_k if self.batch_k > 0 else 0)
        
        if self.sample_count % 5 == 0 and self.sample_count > 0:
            dpg.set_value("series_evo", [self.best_history_x, self.best_history_y])
            dpg.fit_axis_data("x_axis_evo")
            dpg.fit_axis_data("y_axis_evo")
            
            hist, edges = np.histogram(self.all_scores, bins=min(10, len(set(self.all_scores)) or 1))
            centers = (edges[:-1] + edges[1:]) / 2
            dpg.set_value("series_hist", [centers.tolist(), hist.tolist()])
            dpg.fit_axis_data("x_axis_hist")
            dpg.fit_axis_data("y_axis_hist")
            
            self.draw_graph()

    def draw_graph(self):
        """Draws the partition graph in the DrawList."""
        if not dpg.does_item_exist("graph_drawlist"): return
        
        dpg.delete_item("graph_drawlist", children_only=True)
        
        # 1. Draw Edges
        for (i, j, w) in EDGES:
            p1 = self.node_positions[i]
            p2 = self.node_positions[j]
            # If nodes are in different groups, highlight edge
            is_cut = False
            if self.best_bits and self.best_bits[i] != self.best_bits[j]:
                is_cut = True
            
            color = (255, 255, 255, 100) if is_cut else (50, 50, 50, 100)
            thickness = 2 if is_cut else 1
            dpg.draw_line(p1, p2, color=color, thickness=thickness, parent="graph_drawlist")

        # 2. Draw Nodes
        for i, pos in enumerate(self.node_positions):
            val = int(self.best_bits[i]) if self.best_bits else 0
            # Colors: Group A (Reddish), Group B (Cyan/Greenish)
            color = (255, 100, 100) if val == 0 else (100, 255, 200)
            
            dpg.draw_circle(pos, self.node_radius, color=(255, 255, 255), fill=color, parent="graph_drawlist")
            dpg.draw_text((pos[0]-4, pos[1]-7), str(i), size=12, color=(0,0,0), parent="graph_drawlist")

    def run(self):
        self.setup_ui()
        while dpg.is_dearpygui_running():
            self.update()
            dpg.render_dearpygui_frame()
        dpg.destroy_context()

# =============================================================================
# MAIN ENTRY
# =============================================================================
if __name__ == "__main__":
    if not SERIAL_OK:
        print("Error: pyserial no detectado.")
        sys.exit(1)
    if not DEARPYGUI_OK:
        print("Error: dearpygui no detectado. Instala con: pip install dearpygui")
        sys.exit(1)

    app = QuantumDashboard()
    app.run()
