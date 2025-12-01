# Sistema de Análisis de Opiniones Distribuido I2C

Sistema distribuido basado en Arduino que utiliza comunicación I2C para coordinar múltiples dispositivos esclavos que realizan análisis de opiniones (a favor/en contra/neutral). El maestro agrega los resultados y los muestra en una pantalla OLED con marcas de tiempo del RTC.

## 📋 Descripción

Este proyecto implementa una arquitectura maestro-esclavo donde:
- **1 Maestro (Master)**: Coordina la comunicación I2C, agrega resultados, monitorea el ambiente (Temp/Hum), registra datos en SD y muestra información en pantalla OLED con timestamps del RTC DS3231.
- **4+ Esclavos (Slaves)**: Procesan datos de opiniones y responden a solicitudes del maestro.

El sistema está diseñado para ser escalable, permitiendo agregar más esclavos según sea necesario.

## 🔧 Hardware Requerido

### Maestro (Master)
- 1x ESP32-S3 Super Mini (o compatible)
- 1x Pantalla OLED 1.3" (SH1106, dirección I2C 0x3C)
- 1x Módulo RTC DS3231 (dirección I2C 0x68)
- 1x Sensor BME280 (Temp/Hum/Pres, dirección I2C 0x76)
- 1x Módulo MicroSD (SPI: CS=7, MOSI=6, MISO=5, SCLK=4)
- 1x LED RGB (NeoPixel/WS2812B en pin 48)
- Resistencias pull-up 4.7kΩ (2x para SDA y SCL)

### Cada Esclavo (Slave)
- 1x Arduino (Uno, Nano, ESP32, etc.)
- Conexión al bus I2C compartido

### Adicional
- Cables jumper
- Fuente de alimentación adecuada (si se usan múltiples dispositivos)
- Protoboard o PCB para conexiones

### 2. Configurar el Maestro
1. Abrir `master.ino` en Arduino IDE
2. Revisar `config.h` para ajustar configuraciones si es necesario (pines, direcciones, habilitar/deshabilitar módulos)
3. Conectar el Arduino maestro
4. Seleccionar placa y puerto correcto
5. Cargar el código

### 3. Configurar los Esclavos
Para **cada esclavo**:
1. Abrir `slave.ino` en Arduino IDE
2. **IMPORTANTE**: Cambiar la dirección I2C única:
   ```cpp
   #define SLAVE_ADDRESS 0x10  // Cambiar a 0x11, 0x12, 0x13, etc.
   ```
3. Conectar el Arduino esclavo
4. Cargar el código
5. Repetir para cada esclavo adicional

### 4. Conexiones de Hardware
Ver [WIRING.md](WIRING.md) para diagramas detallados de conexión.

**Conexiones básicas I2C:**
- Conectar **SDA** de todos los dispositivos juntos
- Conectar **SCL** de todos los dispositivos juntos
- Conectar **GND** común
- Agregar resistencias pull-up de 4.7kΩ en SDA y SCL al voltaje de alimentación (3.3V o 5V según dispositivos)

## ⚙️ Configuración Avanzada

### Agregar Más Esclavos
1. Editar `config.h` en el maestro:
   ```cpp
   const uint8_t SLAVE_ADDRESSES[] = {
     SLAVE_ADDR_1,
     SLAVE_ADDR_2,
     SLAVE_ADDR_3,
     SLAVE_ADDR_4,
     0x14,  // Nuevo esclavo 5
     0x15   // Nuevo esclavo 6
   };
   ```
2. Programar nuevos esclavos con direcciones únicas
3. Conectar al bus I2C

### Habilitar/Deshabilitar Módulos
En `config.h` puedes activar o desactivar componentes:
```cpp
#define ENABLE_OLED       true   // Pantalla OLED
#define ENABLE_RTC        true   // Reloj RTC
#define ENABLE_SD_LOGGING true   // Registro en SD
#define ENABLE_BME280     true   // Sensor ambiental
#define ENABLE_RGB_LED    true   // LED de estado
```

### Ajustar Velocidad I2C
En `config.h`:
```cpp
#define I2C_CLOCK_SPEED 100000  // 100 kHz (estándar) o 400000 (rápido)
```

## 📊 Uso

### Monitor Serial
Abrir el monitor serial (115200 baud) para ver:
- Escaneo de dispositivos I2C al inicio
- Resultados agregados en tiempo real
- Confirmación de escritura en SD (`>> Data saved to SD`)
- Mensajes de error o advertencias

### Pantalla OLED
La pantalla muestra:
- Título del sistema
- Timestamp actual
- Estado de conexión de esclavos (OK/X)
- Porcentajes de opiniones (Fav, Con, Neu)
- Datos ambientales (Temp, Hum)

### Registro en Tarjeta SD
El sistema crea/abre el archivo `datalog.csv` y registra una línea por ciclo con el formato:
`Timestamp,Favor,Contra,Neutral,Total,Temp,Humidity`

### Indicador LED RGB
El LED RGB indica el estado del sistema:
- **Azul**: En espera (Idle)
- **Amarillo**: Leyendo datos de esclavos
- **Verde**: Lectura exitosa (todos los esclavos respondieron)
- **Naranja**: Advertencia (algunos esclavos fallaron)
- **Rojo**: Error (ningún esclavo respondió)

### Ejemplo de Salida Serial
```
======== CLUSTER ========
Time: 11:15:32
A favor  : 42.3 %
En contra: 34.8 %
Dudando  : 22.9 %
==========================
>> Data saved to SD
```

## 🔍 Solución de Problemas

### No se detectan dispositivos I2C
- Verificar conexiones SDA/SCL
- Confirmar resistencias pull-up instaladas (4.7kΩ)
- Revisar alimentación de todos los dispositivos
- Usar sketch de escaneo I2C para detectar dispositivos

### OLED no muestra nada
- Verificar dirección I2C (0x3C o 0x3D)
- Confirmar librería correcta (SH1106 vs SSD1306)
- Revisar conexiones y alimentación

### Esclavos no responden
- Verificar direcciones únicas para cada esclavo
- Confirmar que los esclavos están alimentados
- Revisar que el código del esclavo se cargó correctamente
- Verificar timeout en `config.h` (aumentar si es necesario)

### Error en SD Card
- Verificar conexiones SPI (CS, MOSI, MISO, SCLK)
- Asegurar que la tarjeta está formateada en FAT32
- Revisar si el archivo `datalog.csv` está abierto en otro lugar

## 🛠️ Desarrollo Futuro

Posibles mejoras:
- [ ] Implementar checksums para validación de datos
- [ ] Agregar comandos de configuración desde el maestro
- [ ] Interfaz web para visualización remota
- [ ] Modo de bajo consumo para operación con batería

## 👤 Autor

**Alejandro Rebolledo**  
Email: arebolledo@udd.cl  
Fecha: 2025-12-01  

**Atribución Original**
El código original fue creado por **Vicente Lorca**; este proyecto es un derivado y el concepto inicial proviene de Vicente.

## 📄 Licencia

Este proyecto está licenciado bajo **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**.

Ver el archivo [LICENSE](LICENSE) para más detalles.

### Descargo de Responsabilidad

Este código se proporciona "tal cual", sin garantías de ningún tipo, expresas o implícitas. El autor no se hace responsable de ningún daño o pérdida que pueda resultar del uso de este código. Úselo bajo su propio riesgo.

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor:
1. Fork el proyecto
2. Crea una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

## 📞 Soporte

Para preguntas o problemas, por favor abre un issue en el repositorio de GitHub.

---

**¡Gracias por usar este proyecto!** ⭐ Si te resulta útil, considera darle una estrella al repositorio.
