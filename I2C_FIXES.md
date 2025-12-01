# Corrección de Problemas I2C - Cuantums

## ✅ Problemas Identificados y Corregidos

### Problema 1: Doble inicialización de Wire en Slave
**Error**: `Wire.begin()` llamado dos veces con parámetros diferentes
```cpp
// ❌ ANTES (líneas 55 y 66)
Wire.begin(SDAPIN, SCLPIN);  // Primera llamada
...
Wire.begin(SLAVE_ADDRESS);   // Segunda llamada - conflicto!
```

**Solución**: Una sola inicialización con todos los parámetros
```cpp
// ✅ AHORA
Wire.begin(SLAVE_ADDRESS, SDAPIN, SCLPIN);  // ESP32 format
```

### Problema 2: Protocolo I2C incompatible
**Error**: Slave enviaba buffer binario, pero el patrón funcional usa String

**Código que funcionaba** (sensor SO₂/TVOC):
```cpp
String payload = String(sensor1.pVgas, 3) + "," + String(vocConcentration) + "\n";
Wire.write(payload.c_str());
```

**Solución aplicada**:
```cpp
// Slave envía CSV string
String payload = String(favorCount) + "," + String(contraCount) + "," + String(neutralCount) + "\n";
Wire.write(payload.c_str());

// Master lee y parsea CSV
String response = "";
while (Wire.available()) {
  char c = Wire.read();
  if (c == '\n') break;
  response += c;
}
// Parse: "123,456,789"
```

### Problema 3: delay() en ISR
**Error**: `delay(10)` dentro de `requestEvent()` puede causar problemas
```cpp
// ❌ ANTES
void requestEvent() {
  Wire.write(buffer, 6);
  digitalWrite(LED_PIN, HIGH);
  delay(10);  // ¡NO hacer delay() en ISR!
  digitalWrite(LED_PIN, LOW);
}
```

**Solución**: Manejo no bloqueante del LED
```cpp
// ✅ AHORA
void requestEvent() {
  Wire.write(payload.c_str());
  digitalWrite(LED_PIN, HIGH);
  // LED se apaga en loop() principal
}

void loop() {
  if (digitalRead(LED_PIN) == HIGH && (millis() - ledOffTime > 10)) {
    digitalWrite(LED_PIN, LOW);
  }
}
```

## 📝 Cambios Realizados

### cuantums/cuantums.ino (Slave)
1. ✅ Corregida inicialización I2C para ESP32
2. ✅ Cambiado protocolo de buffer binario a String CSV
3. ✅ Eliminado `delay()` de `requestEvent()`
4. ✅ Agregado manejo no bloqueante de LED

### master/master.ino (Master)
1. ✅ Actualizado `requestDataFromSlave()` para leer String CSV
2. ✅ Agregado parsing de formato "favor,contra,neutral\n"
3. ✅ Aumentado buffer de lectura a 32 bytes
4. ✅ Agregado logging de respuestas recibidas

## 🔧 Configuración Actual

### Slave (cuantums.ino)
```cpp
#define SLAVE_ADDRESS 0x10
#define SDAPIN 5
#define SCLPIN 6
#define LED_PIN 8
```

### Master (master.ino)
```cpp
// En config.h
const uint8_t SLAVE_ADDRESSES[] = {0x10, 0x11, 0x12, 0x13};
```

## 🧪 Pruebas Recomendadas

1. **Probar con 1 slave primero**
   - Cargar `cuantums.ino` con `SLAVE_ADDRESS 0x10`
   - Verificar en Serial Monitor del slave que se inicializa
   - Verificar en Serial Monitor del master que detecta 0x10

2. **Verificar comunicación**
   - Master debe mostrar: `Received from 0x10: 123,456,789`
   - Slave debe mostrar: `Data sent to master: 123, 456, 789`

3. **Agregar más slaves**
   - Cambiar `SLAVE_ADDRESS` a 0x11, 0x12, 0x13
   - Cargar en cada Arduino
   - Verificar que master agrega correctamente

## ⚠️ Notas Importantes

### Para ESP32
- Los pines I2C por defecto en ESP32 S3 Super Mini son:
  - SDA = Pin 8
  - SCL = Pin 9
- Si usas otros pines, ajusta `SDAPIN` y `SCLPIN`

### Para Arduino Uno/Nano
- Cambiar inicialización a:
```cpp
Wire.begin(SLAVE_ADDRESS);  // Sin especificar pines
```

### Resistencias Pull-up
- Asegúrate de tener 4.7kΩ en SDA y SCL
- Sin pull-ups, la comunicación I2C fallará

## 🐛 Debugging

Si sigue sin funcionar:

1. **Verificar escaneo I2C**
   ```
   Device found at 0x10 (Slave Device)
   ```

2. **Verificar Serial del Slave**
   ```
   === I2C Slave - Opinion Analysis Worker ===
   I2C Address: 0x10
   Slave ready and waiting for master requests...
   ```

3. **Verificar Serial del Master**
   ```
   Received from 0x10: 102,89,65
   ```

4. **Si no hay comunicación**
   - Verificar conexiones SDA/SCL
   - Verificar GND común
   - Verificar pull-ups
   - Probar con I2C scanner

---

**Cambios aplicados exitosamente** ✅
