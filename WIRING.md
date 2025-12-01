# Guía de Conexiones - Sistema I2C Multi-Slave

Esta guía detalla las conexiones de hardware para el sistema de análisis de opiniones distribuido.

## 📌 Conceptos Básicos I2C

El bus I2C (Inter-Integrated Circuit) es un protocolo de comunicación serial que utiliza solo 2 cables:
- **SDA** (Serial Data): Línea de datos bidireccional
- **SCL** (Serial Clock): Línea de reloj generada por el maestro

### Características Importantes
- Todos los dispositivos comparten las mismas líneas SDA y SCL
- Cada dispositivo tiene una dirección única (7 bits)
- Se requieren resistencias pull-up en ambas líneas
- Máximo de 127 dispositivos en un bus (teóricamente)

## 🔌 Pines I2C por Placa Arduino

| Placa Arduino | SDA Pin | SCL Pin |
|---------------|---------|---------|
| Uno / Nano | A4 | A5 |
| Mega 2560 | 20 | 21 |
| Leonardo | 2 | 3 |
| ESP32 | 21 | 22 |
| ESP8266 | GPIO4 (D2) | GPIO5 (D1) |

> [!NOTE]
> Algunos microcontroladores permiten configurar pines I2C alternativos. Consulta la documentación de tu placa.

## 🔧 Esquema de Conexión General

```
                    +5V (o 3.3V)
                     |
                     |
        4.7kΩ        |        4.7kΩ
    ┌────────────────┼────────────────┐
    |                |                |
    |                |                |
   SDA              GND              SCL
    |                                 |
    |─────────────────────────────────|
    |                                 |
    |    ┌──────────┐                 |
    ├────┤  MASTER  ├─────────────────┤
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    ├────┤ SLAVE 1  ├─────────────────┤
    |    │ (0x10)   │                 |
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    ├────┤ SLAVE 2  ├─────────────────┤
    |    │ (0x11)   │                 |
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    ├────┤ SLAVE 3  ├─────────────────┤
    |    │ (0x12)   │                 |
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    ├────┤ SLAVE 4  ├─────────────────┤
    |    │ (0x13)   │                 |
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    ├────┤   OLED   ├─────────────────┤
    |    │ (0x3C)   │                 |
    |    └──────────┘                 |
    |                                 |
    |    ┌──────────┐                 |
    └────┤   RTC    ├─────────────────┘
         │ (0x68)   │
         └──────────┘
```

## 📋 Conexiones Detalladas

### Maestro (Master Arduino)

#### Conexiones I2C
| Pin Master | Conectar a |
|------------|------------|
| SDA | Bus SDA común (con pull-up 4.7kΩ a VCC) |
| SCL | Bus SCL común (con pull-up 4.7kΩ a VCC) |
| GND | GND común de todos los dispositivos |
| 5V o 3.3V | Alimentación (compartida si es posible) |

#### OLED Display (1.3" SH1106)
| Pin OLED | Conectar a |
|----------|------------|
| VCC | 3.3V o 5V (según modelo) |
| GND | GND común |
| SDA | Bus SDA |
| SCL | Bus SCL |

#### RTC DS3231
| Pin RTC | Conectar a |
|---------|------------|
| VCC | 5V (o 3.3V según modelo) |
| GND | GND común |
| SDA | Bus SDA |
| SCL | Bus SCL |

### Esclavos (Slave Arduinos)

Cada esclavo se conecta de manera idéntica:

| Pin Slave | Conectar a |
|-----------|------------|
| SDA | Bus SDA común |
| SCL | Bus SCL común |
| GND | GND común |
| 5V o 3.3V | Alimentación |

> [!IMPORTANT]
> **Cada esclavo debe programarse con una dirección I2C única** (0x10, 0x11, 0x12, 0x13, etc.)

## ⚡ Resistencias Pull-Up

### ¿Por qué son necesarias?
Las líneas I2C son de "drenaje abierto", lo que significa que los dispositivos solo pueden llevar la línea a GND. Las resistencias pull-up son necesarias para llevar las líneas a nivel alto (VCC).

### Valores Recomendados
- **4.7kΩ**: Valor estándar para la mayoría de aplicaciones
- **2.2kΩ**: Para buses largos o alta velocidad
- **10kΩ**: Para buses cortos con pocos dispositivos

### Ubicación
Instalar **una sola vez** en el bus:
```
VCC (5V o 3.3V)
 |
 ├── 4.7kΩ ──┬── SDA (a todos los dispositivos)
 |           |
 └── 4.7kΩ ──┴── SCL (a todos los dispositivos)
```

> [!WARNING]
> **No duplicar pull-ups**: Algunos módulos (como OLED o RTC) pueden incluir resistencias pull-up integradas. Si tienes problemas de comunicación, verifica con un multímetro.

## 🔋 Consideraciones de Alimentación

### Voltaje
- **5V**: Arduino Uno, Mega, Nano (5V)
- **3.3V**: ESP32, ESP8266, Arduino Due

> [!CAUTION]
> **Mezclar voltajes**: Si mezclas dispositivos de 3.3V y 5V, usa un convertidor de nivel lógico (level shifter) para proteger los dispositivos de 3.3V.

### Consumo de Corriente
Estimación aproximada:
- Arduino Uno/Nano: ~50mA
- OLED Display: ~20mA
- RTC DS3231: ~1mA
- **Total para 1 master + 4 slaves**: ~300mA

Recomendación: Usar fuente de alimentación de al menos **1A** para estabilidad.

### Distribución de Alimentación
Para múltiples dispositivos:
1. **Opción 1**: Alimentar todos desde una fuente externa común
2. **Opción 2**: Alimentar el master desde USB y los slaves desde fuente externa (conectar GND común)

## 📏 Longitud de Cables

| Longitud | Velocidad I2C | Notas |
|----------|---------------|-------|
| < 1 metro | 400 kHz | Sin problemas |
| 1-3 metros | 100 kHz | Reducir velocidad |
| > 3 metros | < 100 kHz | Usar cables apantallados |

> [!TIP]
> Para cables largos, reduce la velocidad I2C en `config.h`:
> ```cpp
> #define I2C_CLOCK_SPEED 50000  // 50 kHz para cables largos
> ```

## 🧪 Verificación de Conexiones

### 1. Verificar Continuidad
Con un multímetro en modo continuidad:
- Verificar que todos los SDA están conectados
- Verificar que todos los SCL están conectados
- Verificar GND común

### 2. Verificar Pull-Ups
Con multímetro en modo resistencia:
- Entre SDA y VCC: debe leer ~4.7kΩ
- Entre SCL y VCC: debe leer ~4.7kΩ

### 3. Escaneo I2C
Cargar el siguiente sketch en el master para escanear dispositivos:

```cpp
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("Escaneando bus I2C...");
  
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Dispositivo en 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Escaneo completo");
}

void loop() {}
```

Deberías ver:
- `0x3C` o `0x3D` - OLED
- `0x68` - RTC
- `0x10`, `0x11`, `0x12`, `0x13` - Slaves

## 🛠️ Solución de Problemas de Conexión

### No se detectan dispositivos
1. ✅ Verificar alimentación de todos los dispositivos
2. ✅ Confirmar resistencias pull-up instaladas
3. ✅ Revisar continuidad de SDA y SCL
4. ✅ Verificar GND común conectado

### Comunicación intermitente
1. ✅ Reducir velocidad I2C a 100 kHz o menos
2. ✅ Acortar cables
3. ✅ Verificar valor de pull-ups (probar 2.2kΩ)
4. ✅ Revisar fuente de alimentación (debe ser estable)

### Conflictos de dirección
1. ✅ Usar sketch de escaneo para identificar direcciones
2. ✅ Verificar que cada slave tiene dirección única
3. ✅ Confirmar que no hay duplicados

## 📸 Ejemplo de Montaje en Protoboard

Para un montaje limpio:
1. Colocar las resistencias pull-up cerca del master
2. Usar cables de colores:
   - **Rojo**: VCC/5V
   - **Negro**: GND
   - **Amarillo**: SDA
   - **Verde**: SCL
3. Mantener cables cortos y organizados
4. Etiquetar cada slave con su dirección I2C

---

**¿Necesitas ayuda?** Abre un issue en el repositorio de GitHub con fotos de tu montaje.
