# 🧠 Doom Simulator 2D(Tarea 2 — Sistemas Operativos UDP 2025)

**Autor:** *Mair Escobar Y Martín Quintana*  
**Curso:** Sistemas Operativos — Universidad Diego Portales  
**Profesor:** Yerko Ortiz
**Ayudante:** Diego Banda
**Fecha:** Noviembre 2025  

---

## 📋 Descripción General

Este proyecto implementa una **simulación concurrente** inspirada en el clásico videojuego **DOOM**, desarrollada en **C** usando **threads(`pthreads`)** y **monitores** (mutex + condition variables).  

La simulación se ejecuta sobre un **grid 2D**, donde héroes y monstruos se mueven, detectan y atacan de forma sincronizada.  
Se desarrollaron **dos versiones del programa**:

### ⚙️ Versiones disponibles
1. **Versión A — Un solo Héroe (`Doom2D.c`)**  
   - Implementa un único héroe (`Doom Slayer`) que sigue un camino predefinido.  
   - Monstruos actúan de manera concurrente, detectando y atacando al héroe.  
   - Ideal para entender el funcionamiento básico de los monitores y turnos sincronizados.

2. **Versión B — N Héroes (extensión experimental)**  
   - Extiende la simulación para múltiples héroes, cada uno ejecutado como un **thread independiente**.  
   - Cada héroe tiene su propio `path`, `hp` y `damage`.
   - Todos los héroes y monstruos comparten el mismo `monitor` y el mismo `mapa`.  
   - Presenta una sincronización más compleja, donde los turnos se reparten entre N héroes y M monstruos.

> Ambas versiones comparten la misma estructura base y pueden ser ejecutadas por separado.

---

## ⚙️ Estructuras Principales

### 🧍‍♂️ Héroe (`Hero`)
- `hp`: puntos de vida.  
- `damage`: daño por ataque.  
- `range`: rango de ataque.  
- `path`: secuencia de coordenadas que sigue hasta la meta.  
- `alive`, `fighting`: controlan si está vivo o en combate.  

### 👾 Monstruo (`Monster`)
- `hp`, `damage`, `range`, `vision_range`.  
- `alerted`: si detectó al héroe o fue alertado por otro monstruo.  
- `target_hero`: puntero al héroe actual que sigue.  
- `alive`: indica si el monstruo sigue activo.  

### 🧩 Monitor (`Monitor`)
- Controla el acceso concurrente al mapa.  
- Sincroniza los turnos de héroes y monstruos.  
- Variables principales:
  - `pthread_mutex_t mutex`
  - `pthread_cond_t hero_turn, monster_turn`
  - `turn`: indica de quién es el turno actual (`true` = héroes, `false` = monstruos).  
  - Contadores: `heroes_vivos`, `monsters_vivos`, `heroes_actuados`, `monsters_actuados`.

---

## ⚔️ Mecánica del Juego

### 🔹 Turnos
El sistema alterna entre turnos del **héroe/héroes** y **monstruos**:
1. **Turno del héroe:**  
   - Si hay monstruos en rango, ataca.  
   - Si no, avanza en su camino (`path`).  
   - Si llega a la meta, se marca como victoria.

2. **Turno de los monstruos:**  
   - Cada monstruo se ejecuta en su propio hilo.  
   - Si ve al héroe, lo marca como objetivo y alerta a otros.  
   - Si está en rango, lo ataca; si no, se acerca paso a paso.

3. Al terminar todos los monstruos, el turno vuelve a los héroes.

---

## 🧵 Concurrencia y Sincronización

- **Un hilo por héroe.**  
- **Un hilo por monstruo.**  
- Todos comparten el `Monitor` central, que controla el turno global.  

---

## 🗺️ Representación del Mapa

El mundo del juego se representa como una **matriz 2D (grid)**, donde cada celda contiene un símbolo que indica su estado actual.

| Símbolo | Significado |
|:--:|:--|
| `.` | Espacio vacío |
| `+` | Camino predefinido del héroe |
| `H` | Posición actual del héroe |
| `M` | Monstruo |
| `X` | Entidad muerta (opcional, puede representar héroe o monstruo derrotado) |

### 🧩 Ejemplo visual

```text
. . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . .
. . . . + + + + + . . . . . . . . . . . . . .
. . . . + H + . . . . . . . . . . . . . . . .
. . . . + + + . . . . . . . . M . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . .

```

Cada celda tiene un ancho fijo de impresión (por defecto 3 espacios) para mantener las filas alineadas, incluso cuando se usan símbolos de más de un carácter.
*Se puede ajustar el largo cambiando #define largo_celda 3*

## 📄 Entrada: Archivo de Configuración

El programa obtiene todos los parámetros de simulación desde un **archivo de texto plano** (`.txt`), que define las dimensiones del mapa, las estadísticas del héroe y los atributos de los monstruos.

Cada línea corresponde a una variable o grupo de datos específicos.  
El formato es **legible y editable**, lo que permite modificar fácilmente la simulación sin recompilar el código.

---

### 📘 Estructura general

| Clave | Descripción |
|:--|:--|
| `GRID_SIZE W H` | Tamaño del mapa: ancho (`W`) y alto (`H`). |
| `HERO_HP` | Vida inicial del héroe. |
| `HERO_ATTACK_DAMAGE` | Daño de ataque del héroe. |
| `HERO_ATTACK_RANGE` | Rango de ataque del héroe. |
| `HERO_START X Y` | Coordenadas iniciales del héroe. |
| `HERO_PATH (x,y)` | Secuencia de coordenadas que el héroe seguirá. |
| `MONSTER_COUNT` | Cantidad total de monstruos. |
| `MONSTER_<id>_HP` | Vida inicial del monstruo con ID `<id>`. |
| `MONSTER_<id>_ATTACK_DAMAGE` | Daño de ataque del monstruo. |
| `MONSTER_<id>_VISION_RANGE` | Rango de visión del monstruo. |
| `MONSTER_<id>_ATTACK_RANGE` | Rango de ataque del monstruo. |
| `MONSTER_<id>_COORDS X Y` | Coordenadas iniciales del monstruo. |

---

### 🧩 Ejemplo de archivo de configuración

```text
GRID_SIZE 30 20
HERO_HP 150
HERO_ATTACK_DAMAGE 20
HERO_ATTACK_RANGE 3
HERO_START 2 2
HERO_PATH (3,2) (4,2) (5,2) (5,3) (5,4) (6,4)

MONSTER_COUNT 3
MONSTER_1_HP 50
MONSTER_1_ATTACK_DAMAGE 10
MONSTER_1_VISION_RANGE 5
MONSTER_1_ATTACK_RANGE 1
MONSTER_1_COORDS 8 4

MONSTER_2_HP 50
MONSTER_2_ATTACK_DAMAGE 10
MONSTER_2_VISION_RANGE 5
MONSTER_2_ATTACK_RANGE 1
MONSTER_2_COORDS 15 10

MONSTER_3_HP 80
MONSTER_3_ATTACK_DAMAGE 15
MONSTER_3_VISION_RANGE 4
MONSTER_3_ATTACK_RANGE 2
MONSTER_3_COORDS 5 8
```
## ⚙️ Compilación y Ejecución

Esta sección explica cómo compilar y ejecutar el proyecto en sistemas **UNIX/Linux o macOS** usando `gcc` y la biblioteca `pthread`.

---

### 💻 Requisitos

Para compilar y ejecutar correctamente la simulación, necesitas:

- **Compilador:** GCC o Clang compatible con C11 o superior.  
- **Sistema operativo:** Linux, macOS o cualquier entorno UNIX.  
- **Biblioteca:** `pthread` (instalada por defecto en la mayoría de distribuciones).  


---

### 🧩 Compilar el proyecto

Desde la terminal, ejecuta:

```bash
gcc Doom2D.c -o Doom2D
./Doom2D ejemplo.txt

```

## ▶️ Ejecutar la Simulación

Una vez compilado el programa, puedes ejecutar la simulación desde la terminal.  
El comportamiento dependerá de la versión del código que estés usando.

---

### 🧩 Versión A — Un solo Héroe

Esta versión (`Doom2D.c`) simula un único héroe (Doom Slayer) recorriendo su ruta mientras los monstruos actúan concurrentemente.

```bash
./Doom2D ejemplo.txt

```
Si no especificas ningún archivo de configuración, el programa utiliza ejemplo.txt por defecto.

---

## 💡 Notas sobre el Archivo de Configuración

- El archivo puede incluir **saltos de línea o espacios adicionales**; el programa los ignora automáticamente gracias a las funciones de limpieza (`Quitar_Saltos`, `Quitar_Espacios`).

- Las coordenadas del héroe y los monstruos se expresan en **formato cartesiano `(x,y)`**, donde:
  - El eje **X** aumenta hacia la derecha.
  - El eje **Y** aumenta hacia arriba.
  - La posición `(0,0)` está en la **esquina inferior izquierda** del mapa.

- El héroe sigue el orden exacto de las coordenadas en su `HERO_PATH`.  
  Si el archivo tiene líneas extra o espacios, no afectan la simulación.

- Puedes crear múltiples configuraciones simplemente ingresar otro .txt.

---

## 🧩 Cierre del Proyecto

Este proyecto representa la integración práctica de **concurrencia, sincronización y diseño de software cooperativo** en lenguaje C.  
A través del uso de **threads(`pthreads`)**, **mutex**, y **variables de condición**, se logra coordinar correctamente la interacción entre múltiples entidades (héroes y monstruos) dentro de un entorno 2D compartido.

### 🎯 Objetivos logrados

- Implementación de **monitores** para coordinar hilos.  
- Control de turnos seguro entre héroes y monstruos.  
- Simulación de **detección, ataque y movimiento concurrente**.  
- Lectura dinámica desde archivos de configuración.  
- Manejo seguro de memoria.  
- Versión extendida con **N héroes concurrentes**.

---

### 📜 Créditos

**Autor:** *Martín Quintana y Mair Escobar*  
**Universidad:** Diego Portales  
**Curso:** Sistemas Operativos — 2025  
**Profesor:** Yerko Ortiz
**Ayudante:** Diego Banda

---




