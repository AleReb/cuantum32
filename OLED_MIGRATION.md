# Cambio de Librería OLED: Adafruit → U8g2

## ✅ Cambios Completados

Se migró exitosamente la librería de control de la pantalla OLED de **Adafruit_GFX/SH1106** a **U8g2** por olikraus.

## 📝 Archivos Modificados

### 1. [config.h](file:///c:/Users/Ale/Dropbox/clases_universidad/cuantum32/config.h)
**Cambios**:
- Eliminadas definiciones de `OLED_WIDTH`, `OLED_HEIGHT`, `OLED_RESET`
- Reemplazados defines `USE_SH1106` / `USE_SSD1306` por `USE_SH1106_128X64` / `USE_SSD1306_128X64`
- Actualizada documentación para reflejar constructores U8g2

### 2. [master.ino](file:///c:/Users/Ale/Dropbox/clases_universidad/cuantum32/master.ino)
**Cambios**:
- **Includes**: Reemplazado `Adafruit_GFX.h` y `Adafruit_SH1106.h` por `U8g2lib.h`
- **Constructor**: Cambiado de `Adafruit_SH1106 display(OLED_RESET)` a `U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE)`
- **Inicialización**: Simplificado de `display.begin(OLED_ADDRESS)` a `display.begin()`
- **API de dibujo**: Migrado completamente a U8g2:
  - `clearDisplay()` → `clearBuffer()`
  - `display()` → `sendBuffer()`
  - `setTextSize()`, `setTextColor()`, `setCursor()`, `print()` → `setFont()`, `drawStr()`, `sprintf()`
  - `drawLine()` → `drawLine()` (mismo nombre, diferente sintaxis)

### 3. [README.md](file:///c:/Users/Ale/Dropbox/clases_universidad/cuantum32/README.md)
**Cambios**:
- Actualizada sección de librerías requeridas
- Cambiadas instrucciones de instalación para U8g2
- Removidas referencias a Adafruit_GFX y Adafruit_SH1106

## 🔄 Comparación de APIs

| Función | Adafruit | U8g2 |
|---------|----------|------|
| Inicializar | `display.begin(address)` | `display.begin()` |
| Limpiar buffer | `display.clearDisplay()` | `display.clearBuffer()` |
| Enviar a pantalla | `display.display()` | `display.sendBuffer()` |
| Configurar fuente | `display.setTextSize(1)` | `display.setFont(u8g2_font_6x10_tr)` |
| Dibujar texto | `display.print("texto")` | `display.drawStr(x, y, "texto")` |
| Línea horizontal | `display.drawLine(x1,y1,x2,y2,color)` | `display.drawLine(x1,y1,x2,y2)` |

## ✨ Ventajas de U8g2

1. **Mejor rendimiento**: Más rápido y eficiente en memoria
2. **Soporte nativo SH1106**: Mejor compatibilidad con pantallas de 1.3"
3. **Más fuentes**: Gran variedad de fuentes integradas
4. **API más limpia**: Funciones más directas y predecibles
5. **Amplio soporte**: Compatible con múltiples controladores OLED

## 📦 Librería Requerida

**Nombre**: U8g2  
**Autor**: olikraus  
**Instalación**: Arduino IDE → Library Manager → Buscar "U8g2"

## 🎨 Fuentes Utilizadas

- `u8g2_font_ncenB08_tr` - Título (negrita, 8pt)
- `u8g2_font_6x10_tr` - Texto normal (6x10 pixels)

## 🔧 Configuración Actual

```cpp
// En config.h
#define USE_SH1106_128X64  // Para OLED 1.3" SH1106

// Constructor en master.ino
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
```

## ✅ Verificación

- ✅ Código compila sin errores
- ✅ API U8g2 implementada correctamente
- ✅ Documentación actualizada
- ✅ Configuración centralizada en config.h
- ✅ Soporte para SH1106 y SSD1306

---

**Migración completada exitosamente** 🎉
