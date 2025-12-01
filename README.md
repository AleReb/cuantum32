# Sistema de Análisis de Opiniones Distribuido I2C

Sistema distribuido basado en Arduino que utiliza comunicación I2C para coordinar múltiples dispositivos esclavos que realizan análisis de opiniones (a favor/en contra/neutral). El maestro agrega los resultados y los muestra en una pantalla OLED con marcas de tiempo del RTC.

## 📋 Descripción

Este proyecto implementa una arquitectura maestro-esclavo donde:
- **1 Maestro (Master)**: Coordina la comunicación I2C, agrega resultados, y muestra datos en pantalla OLED con timestamps del RTC DS3231
- **4+ Esclavos (Slaves)**: Procesan datos de opiniones y responden a solicitudes del maestro

El sistema está diseñado para ser escalable, permitiendo agregar más esclavos según sea necesario.

## 🔧 Hardware Requerido

### Maestro (Master)
- 1x Arduino (Uno, Mega, Nano, ESP32, etc.)
- 1x Pantalla OLED 1.3" (SH1106, dirección I2C 0x3C)
- 1x Módulo RTC DS3231 (dirección I2C 0x68)
- Resistencias pull-up 4.7kΩ (2x para SDA y SCL)

### Cada Esclavo (Slave)
- 1x Arduino (Uno, Nano, ESP32, etc.)
- Conexión al bus I2C compartido

### Adicional
- Cables jumper
- Fuente de alimentación adecuada (si se usan múltiples dispositivos)
- Protoboard o PCB para conexiones

cd cuantum32
```

### 2. Configurar el Maestro
1. Abrir `master.ino` en Arduino IDE
2. Revisar `config.h` para ajustar configuraciones si es necesario
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

### Deshabilitar OLED o RTC
En `config.h`:
```cpp
#define ENABLE_OLED false  // Deshabilitar pantalla OLED
#define ENABLE_RTC false   // Deshabilitar RTC
```

### Ajustar Velocidad I2C
En `config.h`:
```cpp
#define I2C_CLOCK_SPEED 400000  // 400 kHz (modo rápido)
```

## 📊 Uso

### Monitor Serial
Abrir el monitor serial (115200 baud) para ver:
- Escaneo de dispositivos I2C al inicio
- Resultados agregados en tiempo real
- Mensajes de error o advertencias

### Pantalla OLED
La pantalla muestra:
- Título del sistema
- Timestamp actual (si RTC está habilitado)
- Porcentajes de opiniones:
  - A favor
  - En contra
  - Dudando (neutral)

### Ejemplo de Salida
```
======== CLUSTER ========
Time: 11:15:32
A favor  : 42.3 %
En contra: 34.8 %
Dudando  : 22.9 %
==========================
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

### RTC muestra hora incorrecta
- El RTC se ajusta automáticamente a la hora de compilación en el primer arranque
- Hasta 3 reintentos automáticos
- Registro de errores en monitor serial

## 🛠️ Desarrollo Futuro

Posibles mejoras:
- [ ] Implementar checksums para validación de datos
- [ ] Agregar comandos de configuración desde el maestro
- [ ] Almacenamiento de datos en SD card
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
