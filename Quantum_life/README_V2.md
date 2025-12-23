# Quantum Life V2.1 — Holographic Sampler Integration

Este es el firmware avanzado (**V2.1**) para el Master del Juego de la Vida, diseñado para integrarse con los esclavos **quantumV3**.

## 🚀 Novedades de la V2.1

### 1. Resolución 1:1 (Alta Densidad)
Se ha habilitado la resolución completa de **128x64** píxeles (1 píxel por célula). Esto permite observar patrones mucho más complejos y detallados en la pantalla OLED.

### 2. Smart Loop Break (Detección de Bucles)
El sistema ahora detecta automáticamente si el juego se queda "congelado" o entra en un ciclo infinito mediante un buffer de hashes 32-bit (FNV-1a).
- **Alerta**: Si se detecta un bucle, aparece un mensaje en pantalla y el NeoPixel parpadea en **ROJO** durante 10 segundos.
- **Salida Holográfica**: Si hay esclavos conectados, su entropía se usa para "romper" el bucle inyectando patrones. Si no hay esclavos, el sistema entra en un "Estado Cuántico Puro" (congelado) hasta que el usuario reinicie.

### 3. Gravedad Holográfica
Se ha implementado una fuerza de atracción virtual. Los cuadrantes donde hay esclavos activos generan una "gravedad" que sesga las reglas del juego, facilitando que las células vivas se agrupen cerca del hardware conectado.

### 4. Optimización Bit-Parallel (High FPS)
Se ha implementado un motor de cálculo paralelo (Bit-Parallel GoL) que permite procesar columnas completas de 64 bits de forma simultánea. Esto garantiza una ejecución fluida incluso en la resolución completa de 128x64.

### 6. Entropía Ambiental (BME280)
El sistema ahora vincula la inyección de vida al clima local:
- **Temperatura Base**: 10% de inyección a 21°C.
- **Escalado**: Sube o baja ±0.5% por cada grado de diferencia.
- **Suelo de Seguridad**: Si hay esclavos, la inyección nunca baja del **1%**. Si no hay, queda en **0%**.
- **Throttling**: Se mantiene la pausa si la población supera el 20%.

### 7. Robustez I2C (Hot-swap v4 - Hybrid Engine)
Se han adoptado técnicas del motor industrial "Hybrid Sampler" para garantizar estabilidad total:
- **Parser Dual**: El sistema ahora reconoce tanto el protocolo **Largo** como el **Compacto**, siendo más resistente a datos ruidosos.
- **Polling Escalonado (Breather delay)**: Se ha añadido un retardo de **4ms** entre las peticiones a cada esclavo. Esto permite que la capacitancia del bus se estabilice tras el ruido de inserción.
- **Timeouts de 200ms**: Tiempo extendido para permitir que esclavos con mucha carga de trabajo respondan sin bloquear al Master.
- **Ping de Pre-vuelo**: Validación de presencia antes de solicitar datos para evitar cuelgues de hardware.

## 🛠️ Guía de Uso

1. **Botón A**: 
   - **Click Corto**: Reiniciar juego (o despertar de estado congelado).
   - **Mantener (>2s)**: Guardar **SVG** en la tarjeta SD.
2. **Botón B**: Alternar Resolución (128x64 <-> 64x32).
3. **Monitor OLED**: 
   - Verás `GEN` (Generación), `POP` (Población), `T` (Ruido) y `Kp` (Acoplamiento).
   - Indicador visual del escudo cuando está activo.

---
> [!NOTE]
> El sistema requiere al menos un esclavo quantumV3 para procesar la salida de bucles complejos de forma automática. De lo contrario, el juego se detendrá al detectar un estado estático para evitar desperdicio de ciclos.

## Descargo de Responsabilidad
Este software se ofrece 'tal cual', sin garantías de ningún tipo. El usuario lo utiliza bajo su propio riesgo.
