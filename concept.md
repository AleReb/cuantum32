# Simulación holográfica borde–bulk en arquitectura maestro–esclavos (Toy Model)

## 1. Motivación

La correspondencia AdS/CFT, propuesta por Juan Maldacena, plantea que ciertos sistemas de gravedad cuántica definidos en un volumen (bulk) son equivalentes a teorías cuánticas de campos sin gravedad definidas en un borde de menor dimensión. Esta idea concreta el principio holográfico: la información contenida en un volumen puede estar completamente codificada en una superficie.

El objetivo de este proyecto no es implementar gravedad cuántica real ni computación cuántica completa, sino construir un **modelo pedagógico y experimental** que permita explorar esta idea central: *el bulk no se almacena explícitamente, sino que se reconstruye a partir del estado del borde*.

La arquitectura maestro–esclavos basada en I2C de este repositorio ofrece una oportunidad adecuada para implementar un **toy model holográfico**, donde los nodos esclavos representan grados de libertad del borde y el nodo maestro actúa como orquestador temporal y reconstructor del bulk discreto.

---

## 2. Marco teórico

### 2.1 Correspondencia AdS/CFT

Juan Maldacena demostró que, en el límite de gran número de colores (large N), ciertas teorías gauge conformes en el borde son duales a teorías de supergravedad en un espacio Anti‑de Sitter de mayor dimensión. Esta dualidad no es una aproximación, sino una equivalencia completa entre descripciones aparentemente distintas.

Referencia:
- J. Maldacena, *The Large N Limit of Superconformal Field Theories and Supergravity*, Adv. Theor. Math. Phys. 2 (1998) 231–252.

### 2.2 Principio holográfico

El principio holográfico, formulado por Gerard ’t Hooft y Leonard Susskind, surge originalmente del estudio de la entropía de agujeros negros y propone que el número de grados de libertad físicos escala con el área y no con el volumen.

Referencia:
- G. ’t Hooft, *Dimensional Reduction in Quantum Gravity*.
- L. Susskind, *The World as a Hologram*.

### 2.3 Códigos holográficos y redes tensoriales

Trabajos posteriores mostraron que la correspondencia holográfica puede entenderse como un **código cuántico de corrección de errores**, donde los grados de libertad del bulk están redundante y no localmente codificados en el borde. Modelos discretos como MERA y el código HaPPY hacen explícita esta idea.

Referencia:
- F. Pastawski et al., *Holographic quantum error‑correcting codes: Toy models for the bulk/boundary correspondence*, JHEP 06 (2015) 149.

---

## 3. Idea central de la simulación

La simulación propuesta implementa explícitamente la intuición holográfica:

- **El borde es dinámico y local**: cada nodo esclavo simula un conjunto reducido de grados de libertad locales.
- **El bulk es reconstruido**: el nodo maestro no contiene directamente el estado del bulk, sino que lo infiere a partir de los datos del borde.

Esto refleja la idea fundamental de Maldacena: el bulk no es una estructura independiente, sino otra descripción equivalente de la información del borde.

---

## 4. Mapeo arquitectura ↔ holografía

| Arquitectura física | Interpretación holográfica |
|---------------------|----------------------------|
| Nodo esclavo I2C    | Segmento del borde (boundary degrees of freedom) |
| Variables locales   | Campos discretizados / qudits del borde |
| Tick global         | Evolución temporal discreta |
| Nodo maestro        | Reconstrucción del bulk / observador global |
| SD / Serial / OLED  | Observables macroscópicos |

---

## 5. Enfoque implementado: Toy holográfico explícito (HaPPY‑like)

### 5.1 Borde (esclavos)

Cada esclavo:
- Mantiene un pequeño estado local discreto (bits, enteros escalados o variables tipo estabilizador).
- Aplica reglas locales de actualización en cada tick.
- Reporta observables y síndromes al maestro.

No se manejan amplitudes complejas ni estados cuánticos completos; se utilizan **versiones estabilizadoras o análogas clásicas**, suficientes para capturar la estructura de codificación holográfica.

### 5.2 Bulk (maestro)

El maestro:
- Sincroniza el avance temporal del sistema.
- Recibe información del borde.
- Ejecuta algoritmos de **reconstrucción discreta**, inspirados en:
  - greedy reconstruction,
  - entanglement wedge reconstruction.

El resultado es un bulk discreto inferido, representado como un grafo, árbol o estructura jerárquica.

---

## 6. Qué hace a esta simulación “holográfica”

- El bulk **no existe como memoria centralizada**.
- El mismo estado del borde puede permitir reconstruir múltiples regiones del bulk.
- La pérdida de nodos del borde no implica pérdida inmediata del bulk, reflejando la idea de corrección de errores holográfica.

Esto reproduce, a nivel conceptual, la propiedad clave de la correspondencia AdS/CFT.

---

## 7. Limitaciones prácticas

- No se simulan estados cuánticos completos ni dinámica gravitacional real.
- La reconstrucción del bulk es aproximada y discreta.
- El enfoque prioriza claridad conceptual y pedagogía sobre fidelidad física extrema.

Estas limitaciones son deliberadas y coherentes con el uso de microcontroladores.

---

## 8. Objetivo pedagógico

Este proyecto permite:
- Explorar la idea de espacio‑tiempo emergente desde el borde.
- Visualizar conceptos abstractos de holografía de forma tangible.
- Servir como plataforma educativa para cursos y prototipos experimentales.

---

## 9. Próximos pasos

1. Definir formalmente el protocolo de comunicación borde ↔ maestro.
2. Implementar estados locales y reglas de actualización en esclavos.
3. Programar la reconstrucción del bulk en el maestro.
4. Analizar y visualizar resultados mediante logs y gráficos externos.

Este documento constituye la base conceptual para las siguientes etapas de desarrollo del código.

