# Sistema de Análisis de Opiniones Distribuido I2C

## Changelog / Recent Updates (Quantum32 V3)
- **2025-12-22**:
  - **Fixed Blocking**: Master V3 now updates the OLED display continuously, even during high-speed Python batched sampling.
  - **Fixed Slave LED**: Slaves now explicitly use **Pin 7** for the status LED and give visual feedback on every I2C RX/TX event.
  - **Environment**: Fixed local Python environment initialization.


Sistema distribuido basado en Arduino que utiliza comunicación I2C para coordinar múltiples dispositivos esclavos que realizan análisis de opiniones (a favor/en contra/neutral). El maestro agrega los resultados y los muestra en una pantalla OLED con marcas de tiempo del RTC.

## 📋 Descripción


Este proyecto implementa una **simulación pedagógica de holografía (Toy Holography)** inspirada en el principio holográfico y la correspondencia AdS/CFT. Utiliza una arquitectura maestro-esclavo donde:

- **Maestro (Bulk)**: Representa el volumen ("bulk") del espacio-tiempo. Reconstruye información lógica a partir de las correlaciones de los esclavos, monitorea el ambiente y visualiza la "reconstrucción holográfica".
- **Esclavos (Boundary)**: Representan el borde ("boundary") del sistema. Generan observables locales con ruido y pérdida simulada que el maestro utiliza para inferir el estado del volumen.

El sistema ilustra cómo propiedades, corrección de errores y geometrías emergentes pueden surgir de interacciones distribuidas simples sobre un bus I2C.

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

### Pantalla OLED (MasterV2)
La pantalla ahora visualiza la dinámica de la simulación en tiempo real:
- **Header**: `T:####` (Contador de Ticks de simulación).
- **Status Row**: Estado de cada esclavo (Cuadro lleno = OK, Vacío = Missing).
- **Bulk Reconstruction**: Barra de progreso que muestra qué porcentaje de la lógica del "bulk" ha sido recuperada exitosamente desde el borde.
  - El porcentaje se muestra centrado con contraste dinámico para evitar solapamiento.
- **Boundary Map (2x2)**: Visualización matricial del ruido en cada esclavo (cuadros más llenos = menos ruido/mejor señal).
- **History Graph**: Un gráfico histórico a la derecha que muestra la evolución de la `Reconstruction Ratio` en los últimos 40 ticks.
- **Footer**: Máscaras de bits (Boundary/Bulk) y datos ambientales.

### Registro en Tarjeta SD
El sistema registra en `datalog.csv`:
`timestamp, tick, active_slaves, boundary_mask, bulk_mask, recon_ratio, loss_sum, noise_avg, temp, hum`

### Disclaimer de "Veracidad Científica"
> [!IMPORTANT]
> **Nota sobre la precisión científica**:
> Este código es un **"modelo de juguete" (toy model)** con fines educativos y de prototipado.
> *   **No es** una simulación cuántica real (no hay entrelazamiento cuántico real ni hamiltonianos complejos).
> *   La "Reconstrucción del Bulk" usa una lógica de máscaras de bits simplificada para *ilustrar* conceptos de corrección de errores cuánticos, pero no implementa un código corrección de errores (QECC) riguroso como el código de HaPPY o Shor.
> *   Su objetivo es proveer una intuición física de cómo información local en el borde puede redundar en información robusta en el volumen, usando hardware clásico accesible.

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
- [*] Implementar cuantum life! revisa la carpeta cuantum_life para ver el código del juego de la vida. 

## 👤 Autor

**Alejandro Rebolledo**  
Email: arebolledo@udd.cl  
Fecha: 2025-12-01  

**Atribución Original**
El código original fue creado por **Vicente Lorca** [@vlorcap](https://github.com/vlorcap) ; este proyecto es un derivado y el concepto inicial proviene de Vicente.

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
