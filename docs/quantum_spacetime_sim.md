# Simulación de sistemas espacio-temporales con la arquitectura I2C

Este documento conecta la plataforma distribuida maestro–esclavos de este
repositorio con ideas modernas de simulación cuántica y la intuición holográfica
propuesta por Juan Maldacena. El objetivo es reutilizar los nodos I2C como
**bloques de procesamiento** que emulen la evolución de un sistema espacio-
tiempo discretizado, manteniendo el maestro como orquestador y agregador de
observables.

## Mapa entre la arquitectura actual y la simulación cuántica

- **Maestro como capa “bulk”**: el nodo maestro ya agrega y temporaliza datos de
  los esclavos, mostrando resultados y registrándolos en SD
  (`master/master.ino`). Esto es análogo a un observador en el volumen (bulk)
  que recopila observables globales.
- **Esclavos como “boundary blocks”**: cada esclavo procesa muestras
  independientes y entrega un paquete compacto por I2C
  (`cuantums/cuantums.ino`). En una analogía AdS/CFT, estos bloques serían
  grados de libertad de la teoría de campos en el borde que codifican la
  información del volumen.
- **Ciclos de sondeo**: el maestro ya implementa intervalos de actualización y
  reintentos de comunicación, lo que puede reinterpretarse como pasos de
  evolución discreta de un hamiltoniano efectivo.
- **Estado del bus como grafo**: las direcciones I2C y el escaneo del bus pueden
  verse como nodos y aristas de un grafo de interacción. Extender el arreglo de
  direcciones permite aumentar la “resolución” espacial del grafo.

## Cómo reutilizar los nodos como bloques de simulación

1. **Codificar variables físicas**: en lugar de conteos de opiniones,
   reinterpretar `favor/contra/neutral` como amplitudes, campos discretizados o
   contadores de eventos (p. ej., ocupación de sitios, número de qubits
   excitados). Para mayor fidelidad, cambiar el formato de payload a valores de
   punto flotante o enteros fijos escalados.
2. **Programar reglas locales**: en cada esclavo, reemplazar `simulateOpinion()`
   por la actualización de un modelo físico (Ising, autómatas celulares
   cuánticos aproximados, circuitos variacionales). El maestro solo requiere
   los resultados agregados por ciclo, por lo que la carga principal reside en
   los nodos.
3. **Sincronizar pasos**: usar el `UPDATE_INTERVAL_MS` como paso de tiempo
   discreto global. Si se necesitan dependencias entre nodos, incluir en el
   payload identificadores de vecino o fases compartidas y que el maestro
   redistribuya parámetros comunes al siguiente ciclo.
4. **Registrar observables**: aprovechar el logger en SD y el display OLED para
   monitorear magnitudes agregadas (energía, magnetización, entropía estimada).
   Mantener la misma ruta de datos simplifica comparar corridas.
5. **Escalabilidad**: añadir más direcciones I2C en `config.h` y programar
   esclavos adicionales para aumentar la granularidad espacial. Verificar el
   ruido y la disipación del bus antes de exceder longitudes físicas de cable.

## Inspiración desde Maldacena y la correspondencia AdS/CFT

- **Principio holográfico aplicado a hardware**: la idea central de Maldacena es
  que una teoría gravitacional en el bulk es dual a una teoría de campos sin
  gravedad en el borde. En esta plataforma, los esclavos (borde) codifican el
  estado global que el maestro (bulk) observa. Ajustar reglas locales que
  preserven información (checksums, corrección de errores) refuerza la analogía
  con códigos holográficos.
- **Geometría emergente**: variar las reglas de acoplamiento entre esclavos (p.
  ej., coeficientes de interacción incluidos en el payload) permite explorar
  cómo métricas efectivas emergen de correlaciones, inspirándose en trabajos de
  redes tensoriales y MERA como modelos de espacio-tiempo emergente.
- **Evaporación de agujeros negros y transferencia de información**: usando el
  canal SD/serie se pueden estudiar balances de entropía y flujos de
  información, similar a los experimentos numéricos que verifican el
  comportamiento unitario propuesto en la paradoja de la información.

## Papers y recursos clave para diseñar la simulación

- J. Maldacena, “The Large N Limit of Superconformal Field Theories and
  Supergravity,” *Adv. Theor. Math. Phys.* 2 (1998) 231–252. Punto de partida de
  la correspondencia AdS/CFT.
- G. ’t Hooft y L. Susskind, trabajos sobre el **principio holográfico**, base
  conceptual de la dualidad.
- B. Swingle, “Entanglement Renormalization and Holography,” *Phys. Rev. D* 86,
  065007 (2012). Relaciona redes tensoriales con geometría emergente.
- F. Pastawski, B. Yoshida, D. Harlow, J. Preskill, “Holographic quantum
  error-correcting codes: Toy models for the bulk/boundary correspondence,”
  *JHEP* 06 (2015) 149. Ilustra cómo la corrección de errores implementa el
  principio holográfico.
- P. Hayden, J. Preskill, “Black holes as mirrors,” *JHEP* 09 (2007) 120.
  Referencia clásica sobre recuperación de información.
- S. Lloyd et al., “Quantum algorithms for simulating gauge theories,” y M.
  Biamonte et al., “Quantum Machine Learning,” como guías para mapear modelos
  físicos a circuitos cuánticos o simuladores híbridos.

## Pasos sugeridos para una primera implementación

1. Definir una magnitud física simple (p. ej., magnetización Ising 1D) y mapearla
   a los tres contadores de cada esclavo.
2. Ajustar el payload a números enteros escalados y documentar el factor de
   escala.
3. Usar el maestro como recolector y visualizador de observables; mantener un
   script externo que procese `datalog.csv` para graficar correlaciones.
4. Iterar agregando más nodos para mejorar la resolución espacial y probar reglas
   de acoplamiento dependientes de vecinos.

> Esta aproximación no convierte el hardware en un computador cuántico, pero
> permite prototipar algoritmos de inspiración holográfica y modelos de dinámica
> espacio-temporal distribuida con hardware accesible.
