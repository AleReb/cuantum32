# LED RGB Addressable - Guía de Uso

## 📍 Configuración

### Hardware
- **LED**: WS2812B / NeoPixel (addressable RGB LED)
- **Pin**: GPIO 48 (ESP32 S3 Super Mini)
- **Cantidad**: 1 LED (configurable hasta múltiples LEDs)
- **Voltaje**: 5V (puede funcionar con 3.3V pero con colores menos brillantes)

### Conexión
```
LED RGB (WS2812B)
├── VCC  → 5V (o 3.3V)
├── GND  → GND
└── DIN  → GPIO 48
```

## 🎨 Códigos de Color

| Color | Estado | Significado |
|-------|--------|-------------|
| 🔵 **Azul** | Idle | Esperando próximo ciclo de lectura |
| 🟡 **Amarillo** | Reading | Leyendo datos de slaves |
| 🟢 **Verde** | Success | Todos los slaves respondieron OK |
| 🟠 **Naranja** | Warning | Algunos slaves no respondieron |
| 🔴 **Rojo** | Error | Ningún slave respondió |

## 🔄 Secuencia de Operación

```
1. AZUL (Idle)
   ↓
2. AMARILLO (Leyendo slaves...)
   ↓
3. VERDE/NARANJA/ROJO (según resultado)
   ↓
4. Espera UPDATE_INTERVAL_MS
   ↓
5. Volver a AZUL
```

## 📺 Indicadores en Pantalla OLED

La pantalla OLED ahora muestra el estado de cada slave en la esquina superior derecha:

```
OPINION ANALYSIS    Slaves: OK OK X OK
─────────────────────────────────────
Time: 12:45:32

A favor  : 42.3%
En contra: 35.8%
Dudando  : 21.9%
```

- **OK** = Slave respondió correctamente
- **X** = Slave no respondió o error

## ⚙️ Configuración en config.h

```cpp
// Habilitar/deshabilitar LED RGB
#define ENABLE_RGB_LED    true

// Pin del LED
#define RGB_LED_PIN       48

// Número de LEDs (si tienes una tira)
#define NUM_RGB_LEDS      1

// Personalizar colores (R, G, B: 0-255)
#define RGB_COLOR_IDLE      0, 0, 50      // Azul
#define RGB_COLOR_READING   255, 255, 0   // Amarillo
#define RGB_COLOR_SUCCESS   0, 255, 0     // Verde
#define RGB_COLOR_WARNING   255, 128, 0   // Naranja
#define RGB_COLOR_ERROR     255, 0, 0     // Rojo
```

## 🔧 Ajustes de Brillo

En `master.ino`, línea ~113:
```cpp
rgbLed.setBrightness(50);  // 0-255 (50 = ~20% brillo)
```

Valores recomendados:
- **25**: Muy tenue (ideal para uso nocturno)
- **50**: Tenue (recomendado, ahorra energía)
- **100**: Medio
- **255**: Máximo brillo (consume más corriente)

## 🐛 Troubleshooting

### LED no enciende
1. Verificar conexión VCC y GND
2. Verificar pin correcto (GPIO 48)
3. Verificar que `ENABLE_RGB_LED` está en `true`
4. Probar con ejemplo básico de NeoPixel

### Colores incorrectos
1. Verificar tipo de LED en código:
   ```cpp
   // Para WS2812B (más común)
   Adafruit_NeoPixel rgbLed(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
   
   // Si los colores están invertidos, probar:
   Adafruit_NeoPixel rgbLed(NUM_RGB_LEDS, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);
   ```

2. Ajustar valores RGB en `config.h`

### LED parpadea erráticamente
1. Reducir brillo: `rgbLed.setBrightness(25);`
2. Agregar capacitor 100µF entre VCC y GND del LED
3. Verificar fuente de alimentación estable

## 📊 Ejemplo de Uso

### Monitoreo Visual Rápido
Sin necesidad de abrir Serial Monitor, puedes saber el estado del sistema:

- **🔵 Parpadeando cada 1.5s** = Sistema funcionando normalmente
- **🟢 Constante** = Todos los slaves OK
- **🟠 Constante** = Algunos slaves fallando (revisar OLED para ver cuáles)
- **🔴 Constante** = Sistema sin comunicación con slaves

### Debug en Pantalla OLED
La pantalla muestra exactamente qué slaves están respondiendo:
```
Slaves: OK OK X OK
         ↑  ↑ ↑ ↑
         1  2 3 4
```
En este ejemplo, el Slave 3 (0x12) no está respondiendo.

## 🎯 Ventajas

1. **Feedback visual inmediato** sin necesidad de Serial Monitor
2. **Debug rápido** viendo qué slaves fallan en la OLED
3. **Sincronización visible** - el LED amarillo muestra cuándo se están leyendo datos
4. **Estado del sistema** de un vistazo

## 📝 Notas

- El LED consume ~20mA a brillo medio (50)
- Si usas múltiples LEDs (`NUM_RGB_LEDS > 1`), considera fuente externa de 5V
- El LED se actualiza en cada ciclo de lectura (cada `UPDATE_INTERVAL_MS`)

---

**LED RGB configurado y funcionando** ✅
