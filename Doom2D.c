#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // strlen
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>// usleep 
#define largo_celda 3 
/* 
=================INFO==================

Ultimos cambios:
    - Juego para un Heroe
    - Sin errores,,, quisas
Ultima edicion: 2/11/2025, 21:45

*/

// -Funciones auxiliares==================

void Quitar_Saltos(char *s){
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')){
        s[--n] = '\0';
    }   
}

void Quitar_Espacios(char *s){
    if (!s) return;
    char *inicio = s;
    while (*inicio && isspace((unsigned char)*inicio)){
        inicio++;
    } 
    if (inicio != s){
        memmove(s, inicio, strlen(inicio) + 1);
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])){
        s[--n] = '\0';
    }
}

static inline void saltar_espacios(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static size_t count_tuples(const char *s) {
    const char *p = s;
    size_t c = 0;
    int x, y, used;

    for (;;) {
        saltar_espacios(&p);
        if (sscanf(p, " ( %d , %d ) %n", &x, &y, &used) != 2) break;
        p += used;
        c++;
    }
    return c;
}

// ==================Estructuras==================

int num_heros = 0;
int num_monsters = 0;

typedef struct {
    int x, y;
} Coord;

static struct { 
    int grid_h, grid_w; 
} Grid; 

typedef struct {
    int id, hp, damage, range;
    Coord current_coords;
    Coord *path;
    int path_length;
    bool alive, fighting; //new
} Hero; 

typedef struct { 
    int id, hp, damage, range, vision_range;
    Coord current_coords;
    bool alive, alerted;
    Hero *target_hero; // Puntero al heroe cuando esta alerta
    //podria poner un array de indices de ids de monstruos a los q se alerto
} Monster; 

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t hero_turn;      
    pthread_cond_t monster_turn;   
    // Estado compartido protegido por el monitor

    char ***mapa;  // Ajusta el tamaño según Grid

    int grid_h;
    int grid_w;

    bool game_over; // Para cuando todos los heroes lleguen a la meta o todos mueran

    // Necesario para saber a cuantos de ellos les 
    // toca actuar antes de cambiar de turno
    int heroes_vivos;
    int monsters_vivos;
    int heroes_esperando;
    int monsters_esperando;

    int heroes_actuados; // por ahora no se usan
    int monsters_actuados;

    bool turn;  // true = turno de heroes, false = turno de monstruos
    
} Monitor;

Monitor monitor;

void monitor_init(Monitor *monitor, int grid_h, int grid_w) {
    pthread_mutex_init(&monitor->mutex, NULL);
    pthread_cond_init(&monitor->hero_turn, NULL);
    pthread_cond_init(&monitor->monster_turn, NULL);

    // Nesesario para despues crear el mapa
    monitor->grid_h = grid_h;
    monitor->grid_w = grid_w;
    
    // Inicializar el mapa con '.'
    monitor->mapa = malloc(grid_h * sizeof *monitor->mapa);          // filas (char **)
    for (int i = 0; i < grid_h; i++) {
      monitor->mapa[i] = malloc(grid_w * sizeof *monitor->mapa[i]); // columnas (char *)
        for (int j = 0; j < grid_w; j++) {
            monitor->mapa[i][j] = ".";
        }
    }

    monitor->game_over = false;
    monitor->heroes_vivos = num_heros;
    monitor->monsters_vivos = num_monsters;
    monitor->heroes_esperando = 0;
    monitor->monsters_esperando = 0;

    monitor->heroes_actuados = 0;
    monitor->monsters_actuados = 0;

    monitor->turn = true; // Empieza el turno de los heroes
}

Hero Doom_Slayer = {  //Unico heroe: Doom SlayerDoom_Slayer -_-,    ULTIMO CAMBIO
    .id = 0,
    .hp = 0,
    .damage = 0,
    .range = 0,
    .current_coords = {0, 0},
    .path = NULL,
    .path_length = 0,
    .alive = true   
};

Monster *monsters = NULL; // Array de monstruos

// ==================Funciones para leer datos del txt==================

void LeerConfig(FILE *archivo, Hero *hero){
    rewind(archivo); // NECESARIO
    char linea[256];
    while (fgets(linea, sizeof(linea), archivo)){
        Quitar_Espacios(linea);
        Quitar_Saltos(linea);
        if (linea[0] == '\0') continue;

        if (strncmp(linea, "GRID_SIZE", 9) == 0){ // Solo esta parte es para el juego, el resto es para leer los datos del heroe
            
            int h = 0, w = 0; //height y width

            if(sscanf(linea + 9, "%d %d\n", &w, &h ) == 2 && h > 0 && w > 0){
                Grid.grid_h = h; 
                Grid.grid_w = w; 
                printf("El mapa tendra un tamaño de: %d x %d\n", Grid.grid_h, Grid.grid_w);
            }
            else{
                printf("Error en el formato del mapa");
            }
            
        }
        else if (strncmp(linea, "HERO_HP", 7) == 0){ // Despues quitar para hacer una funcion q lea varios heroes
            
            int hp = 0; 

            if(sscanf(linea + 7, "%d", &hp) == 1 && hp > 0){
                hero->hp = hp; 
                printf("La vida de Heroe es: %d\n", hero->hp);
            }
            else{
                printf("Error en el formato de hp\n");
            }
            
        }        
        else if (strncmp(linea, "HERO_ATTACK_DAMAGE", 18) == 0){
            
            int dmg = 0;

            if(sscanf(linea + 18, "%d", &dmg) == 1 && dmg > 0){
                hero->damage = dmg; 

                printf("El daño de ataque del Heroe es de: %d\n", hero->damage);
            }
            else{
                printf("Error en el formato del HERO_ATTACK_DAMAGE");
            }
            
        }
        else if (strncmp(linea, "HERO_ATTACK_RANGE", 17) == 0){
            
            int range = 0;

            if(sscanf(linea + 17, "%d", &range) == 1 && range > 0){
                hero->range = range;

                printf("El rango de ataque del Heroe es de: %d\n", hero->range);
            }
            else{
                printf("Error en el formato del HERO_ATTACK_RANGE");
            }
            
        }
        else if (strncmp(linea, "HERO_START", 10) == 0){
            
            int x = 0, y = 0;

            if(sscanf(linea + 10, "%d %d", &x, &y) == 2 && x >= 0 && y >= 0){
                hero->current_coords.x = x; 
                hero->current_coords.y = y; 

                printf("El spawn point del Heroe es: (%d, %d)\n", hero->current_coords.x, hero->current_coords.y);
            }
            else{
                printf("Error en el formato del HERO_START");
            }
            
        }
        // Guardar path completo del hero 
        else if (strncmp(linea, "HERO_PATH", 9) == 0){

        }
            
            
    }
}

void LeerHeroPath(FILE *archivo, Hero *heroe) {
    rewind(archivo); // NECESARIO
    char linea[256];
    if (!heroe || !archivo) return;
    
    // Buscar la línea que empieza con HERO_PATH
    while (fgets(linea, sizeof(linea), archivo)) {
        Quitar_Espacios(linea);
        Quitar_Saltos(linea);
        if (linea[0] == '\0') continue;

        if (strncmp(linea, "HERO_PATH", 9) == 0) {
            // Encontramos HERO_PATH, ahora acumular todas las coordenadas
            int capacidad = 100;  // Capacidad inicial
            heroe->path = malloc(sizeof(Coord) * capacidad);
            heroe->path_length = 0;
            
            if (!heroe->path) {
                printf("Error al asignar memoria para el path del heroe\n");
                return;
            }
            
            const char *p = linea + 9; 
            int x, y;
            
            while ((p = strchr(p, '(')) != NULL) {
                if (sscanf(p, "(%d,%d)", &x, &y) == 2) { // Saca los datos en el formato (x,y)
                    // Expandir el array si es necesario
                    if (heroe->path_length >= capacidad) {
                        capacidad *= 2;
                        Coord *temp = realloc(heroe->path, sizeof(Coord) * capacidad);
                        if (!temp) {
                            printf("Error al expandir memoria del path\n");
                            free(heroe->path);
                            heroe->path = NULL;
                            return;
                        }
                        heroe->path = temp;
                    }
                    
                    heroe->path[heroe->path_length].x = x;
                    heroe->path[heroe->path_length].y = y;
                    heroe->path_length++;
                }
                p++;
            }
            
            // Continuar leyendo las siguientes líneas mientras empiecen con '('
            long posicion_actual = ftell(archivo);
            while (fgets(linea, sizeof(linea), archivo)) {
                Quitar_Espacios(linea);
                Quitar_Saltos(linea);
                
                // Si la línea no empieza con '(', terminamos
                if (linea[0] != '(') {
                    // Retroceder el puntero del archivo para no perder esta línea
                    fseek(archivo, posicion_actual, SEEK_SET);
                    break;
                }
                
                // Procesar esta línea de coordenadas
                p = linea;
                while ((p = strchr(p, '(')) != NULL) {
                    if (sscanf(p, "(%d,%d)", &x, &y) == 2) {
                        // Expandir el array si es necesario
                        if (heroe->path_length >= capacidad) {
                            capacidad *= 2;
                            Coord *temp = realloc(heroe->path, sizeof(Coord) * capacidad);
                            if (!temp) {
                                printf("Error al expandir memoria del path\n");
                                free(heroe->path);
                                heroe->path = NULL;
                                return;
                            }
                            heroe->path = temp;
                        }
                        
                        heroe->path[heroe->path_length].x = x;
                        heroe->path[heroe->path_length].y = y;
                        heroe->path_length++;
                    }
                    p++;
                }
                
                posicion_actual = ftell(archivo);
            }
            
            // Ajustar el tamaño final del array
            if (heroe->path_length > 0) {
                Coord *temp = realloc(heroe->path, sizeof(Coord) * heroe->path_length);
                if (temp) {
                    heroe->path = temp;
                }
            }
            
            printf("Path del heroe cargado con %d coordenadas:\n", heroe->path_length);
            for (int j = 0; j < heroe->path_length; j++) {
                printf("(%d, %d) ", heroe->path[j].x, heroe->path[j].y);
                if ((j + 1) % 10 == 0) printf("\n");  // Salto cada 10 coordenadas
            }
            printf("\n");
            
            return;  // Salir después de procesar HERO_PATH
        }
    }
    
    printf("No se encontró HERO_PATH en el archivo\n");
}

void LeerMonstersConfig(FILE *archivo, Monster **monsters) { // Usar cmo base para los varios heroes
    rewind(archivo); // NECESARIO
    char linea[256];
    
    // Primero buscar MONSTER_COUNT
    while (fgets(linea, sizeof(linea), archivo)) {
        Quitar_Espacios(linea);
        Quitar_Saltos(linea);
        
        if (strncmp(linea, "MONSTER_COUNT", 13) == 0) {
            int count = 0;
            if (sscanf(linea + 13, "%d", &count) == 1 && count > 0) {
                num_monsters = count;
                *monsters = malloc(sizeof(Monster) * num_monsters);
                if (!*monsters) {
                    printf("Error al asignar memoria para los monstruos\n");
                    return;
                }
                printf("Número de monstruos: %d\n", num_monsters);
                break;
            } else {
                printf("Error en el formato de MONSTER_COUNT\n");
                return;
            }
        }
    }
    
    if (num_monsters == 0 || !*monsters) {
        printf("No se encontró MONSTER_COUNT o es inválido\n");
        return;
    }
    
    // Atributos de cada Monstro

    int monsters_leidos = 0;
    int monster_id = -1;
    Monster temp_monster = {0};
    
    while (fgets(linea, sizeof(linea), archivo) && monsters_leidos < num_monsters) {
        Quitar_Espacios(linea);
        Quitar_Saltos(linea);
        
        if (linea[0] == '\0') continue;
        
        int id_temp, valor;
        int x, y;
        
        // Intentar leer cada tipo de atributo
        if (sscanf(linea, "MONSTER_%d_HP %d", &id_temp, &valor) == 2) { // EJ: MONSTER_1_HP 100, toma el 1 como id y el 100 como hp
            monster_id = id_temp;
            temp_monster.id = monster_id;
            temp_monster.hp = valor;
            printf("  Monster %d - HP: %d\n", monster_id, valor);
        }
        else if (sscanf(linea, "MONSTER_%d_ATTACK_DAMAGE %d", &id_temp, &valor) == 2) {
            temp_monster.damage = valor;
            printf("  Monster %d - Damage: %d\n", id_temp, valor);
        }
        else if (sscanf(linea, "MONSTER_%d_VISION_RANGE %d", &id_temp, &valor) == 2) {
            temp_monster.vision_range = valor;
            printf("  Monster %d - Vision: %d\n", id_temp, valor);
        }
        else if (sscanf(linea, "MONSTER_%d_ATTACK_RANGE %d", &id_temp, &valor) == 2) {
            temp_monster.range = valor;
            printf("  Monster %d - Range: %d\n", id_temp, valor);
        }
        else if (sscanf(linea, "MONSTER_%d_COORDS %d %d", &id_temp, &x, &y) == 3) {
            temp_monster.current_coords.x = x;
            temp_monster.current_coords.y = y;
            temp_monster.alive = true;
            printf("  Monster %d - Coords: (%d, %d)\n", id_temp, x, y);
            
            // Guardar atributos
 
            (*monsters)[monsters_leidos] = temp_monster;
            (*monsters)[monsters_leidos].alive = true;
            (*monsters)[monsters_leidos].alerted = false;
            printf("Monstruo %d guardado completamente\n", monster_id);
            monsters_leidos++;
                
            // Resetear temp
            temp_monster = (Monster){0};

        }
    }

    printf("\n=== Resumen de monstruos cargados===\n");
    for (int i = 0; i < monsters_leidos; i++) {
        printf("Monstruo %d: HP=%d, Nivel de Dañino=%d, Rango🦎🤠ATQ=%d, Vision=%d, Coords=(%d,%d), Vivicidad=%d, Alerticidad=%d\n",
               (*monsters)[i].id,
               (*monsters)[i].hp,
               (*monsters)[i].damage,
               (*monsters)[i].range,
               (*monsters)[i].vision_range,
               (*monsters)[i].current_coords.x,
               (*monsters)[i].current_coords.y,
               (*monsters)[i].alive,
               (*monsters)[i].alerted);
    }
}


// ==================Funciones del juego==================

// Funcion para transformar coordenadas cartesianas a indices de matriz
int obtenerFila(int y, int filas) {
    return filas - 1 - y;  // Invertir el eje Y
}

int obtenerColumna(int x) {
    return x;  // El eje X se mantiene igual
}

void PrintMapa(Monitor *monitor) {
    for (int i = 0; i < Grid.grid_h; i++) {
        for (int j = 0; j < Grid.grid_w; j++) {
            printf("%-*s", largo_celda, monitor->mapa[i][j]);  // alineación fija
        }
        putchar('\n');
    }
}

void CrearMapa(Hero *hero, Monster *monsters, Monitor *monitor) {
    for (int i = 0; i < Grid.grid_h; i++) {
        for (int j = 0; j < Grid.grid_w; j++) {
            monitor->mapa[i][j] = ".";
        }
    }

    for (int i = 0; i < hero->path_length; i++) {
        int py = obtenerFila(hero->path[i].y, Grid.grid_h);
        int px = obtenerColumna(hero->path[i].x);
        if (py >= 0 && py < Grid.grid_h && px >= 0 && px < Grid.grid_w) {
            if (strcmp(monitor->mapa[py][px], ".") == 0) {
                monitor->mapa[py][px] = "+";
            }
        }
    }

    int hy = obtenerFila(hero->current_coords.y, Grid.grid_h);
    int hx = obtenerColumna(hero->current_coords.x);
    if (hy >= 0 && hy < Grid.grid_h && hx >= 0 && hx < Grid.grid_w) {
        monitor->mapa[hy][hx] = "H";
    }

    for (int i = 0; i < num_monsters; i++) {
        int my = obtenerFila(monsters[i].current_coords.y, Grid.grid_h);
        int mx = obtenerColumna(monsters[i].current_coords.x);
        if (my >= 0 && my < Grid.grid_h && mx >= 0 && mx < Grid.grid_w) {
            monitor->mapa[my][mx] = "M";  // <- solo literal, NO strdup
        }
    }

}

// ==================Funciones de Heroes y Monstruos==================

void AlertarMonstruos(Monster *monster) { // <- indicando moster para despues tomar sus coordenadas y sacar el radio en el q alerta
    printf("(🗣️ ​🗣️ ​🗣️ ​) El Monstruo%d esta alertando en un radio de %d casillas\n", monster->id, monster->vision_range);

    int x_og_mon = monster->current_coords.x;
    int y_og_mon = monster->current_coords.y;

    for (int i = 0; i < num_monsters; i++){
        if(monsters[i].alive == false || (monsters[i].id ==monster->id || monsters[i].alerted)){
            continue;
        }
        else{
            int x_mon = monsters[i].current_coords.x;
            int y_mon = monsters[i].current_coords.y;

            int diff_x = abs(x_og_mon - x_mon);
            int diff_y = abs(y_og_mon - y_mon);

            if (diff_x <= monster->vision_range && diff_y <= monster->vision_range) {
                monsters[i].alerted = true;
                monsters[i].target_hero = monster->target_hero;
                printf("( ❗ ) El Monstruo%d fue alertado por el Monstruo%d sobre el Heroe%d\n", monsters[i].id, monster->id, monster->target_hero->id);
            }
        }
    }
} 

// Podria cambiar la funcion haciendo q se rellene un array con indices de monstruos alertados, para despues cuando el heroe muera poder hacer q dejen de seguirlo 
void MonstruoAtaca(Monster *monster){ // ataca o se acerca al heroe
    int x_mon = monster->current_coords.x;
    int y_mon = monster->current_coords.y;

    int x_hero = monster->target_hero->current_coords.x;
    int y_hero = monster->target_hero->current_coords.y;

    int diff_x = abs(x_mon - x_hero);
    int diff_y = abs(y_mon - y_hero);

    if (diff_x <= monster->range && diff_y <= monster->range && monster->target_hero->alive && monster->alive && monster->target_hero->hp > 0) {
        printf("( ⚔️ ) El Monstruo%d ataca al Hero%d\n", monster->id, monster->target_hero->id);
        monster->target_hero->hp = monster->target_hero->hp - monster->damage;
        if (monster->target_hero->hp <= 0)
        {
            monster->target_hero->hp = 0;
            monster->target_hero->alive = false;
            printf("( ❌ ) El Monstruo%d ha derrotado al Hero%d\n", monster->id, monster->target_hero->id);
            int hero_y_mapa = obtenerFila(monster->target_hero->current_coords.y, Grid.grid_h);
            int hero_x_mapa = obtenerColumna(monster->target_hero->current_coords.x);
            monitor.mapa[hero_y_mapa][hero_x_mapa] = ".";

            monitor.heroes_vivos--;
        }
        else{
            printf("El Hero%d quedo con %d HP\n", monster->target_hero->id, monster->target_hero->hp);
        }
        
    }
    else {
        printf("El Monstruo%d debe acercarse al heroe para atacar\n", monster->id);
        int dir_x = 0, dir_y = 0;
        
        if (x_mon < x_hero) dir_x = 1;       
        else if (x_mon > x_hero) dir_x = -1; 
        
        if (y_mon < y_hero) dir_y = 1;       
        else if (y_mon > y_hero) dir_y = -1; 
        
        // Intentar movimiento original
        int new_x = x_mon, new_y = y_mon;
        bool movimiento_exitoso = false;
        
        if (abs(x_mon - x_hero) >= abs(y_mon - y_hero)) {
            new_x = x_mon + dir_x;
        } else {
            new_y = y_mon + dir_y;
        }
        
        // Verificar
        if (new_x >= 0 && new_x < Grid.grid_w && new_y >= 0 && new_y < Grid.grid_h) {
            bool posicion_libre = true;
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].current_coords.x == new_x && monsters[i].current_coords.y == new_y && 
                    monsters[i].id != monster->id && monsters[i].alive) {
                    posicion_libre = false;
                    break;
                }
            }
            
            if (posicion_libre) {
                movimiento_exitoso = true;
            }
        }
        
        // Movimiento alternativo
        if (!movimiento_exitoso) {
            new_x = x_mon;
            new_y = y_mon;

            if (abs(x_mon - x_hero) >= abs(y_mon - y_hero)) {
                new_y = y_mon + dir_y;
            } else {
                new_x = x_mon + dir_x;
            }
            
            // Verificar movimiento alternativo
            if (new_x >= 0 && new_x < Grid.grid_w && new_y >= 0 && new_y < Grid.grid_h) {
                bool posicion_libre = true;
                for (int i = 0; i < num_monsters; i++) {
                    if (monsters[i].current_coords.x == new_x && monsters[i].current_coords.y == new_y && 
                        monsters[i].id != monster->id && monsters[i].alive) {
                        posicion_libre = false;
                        break;
                    }
                }
                
                if (posicion_libre) {
                    movimiento_exitoso = true;
                }
            }
        }
        
        // Aplicar movimiento si se encontró una casilla libre
        if (movimiento_exitoso) {
            int old_y_mapa = obtenerFila(y_mon, Grid.grid_h);
            int old_x_mapa = obtenerColumna(x_mon);
            monitor.mapa[old_y_mapa][old_x_mapa] = ".";
            
            monster->current_coords.x = new_x;
            monster->current_coords.y = new_y;
            
            int new_y_mapa = obtenerFila(new_y, Grid.grid_h);
            int new_x_mapa = obtenerColumna(new_x);
            char temp_name[10];
            sprintf(temp_name, "M%d", monster->id);
            monitor.mapa[new_y_mapa][new_x_mapa] = strdup(temp_name);

            printf("( ➡️  ​) El Monstruo%d se movio a (%d, %d)\n", monster->id, new_x, new_y);
            PrintMapa(&monitor);
        } else {
            printf("El Monstruo%d no puede moverse\n", monster->id);
        }
    }
}

void MounstroEnRangoAtaque(Hero *hero){
    int hero_x = hero->current_coords.x;
    int hero_y = hero->current_coords.y;
    // Recorrer el mapa en busca de monstruos en rango de ataque y ver si estan en el radio de ataque del heroe
    for (int i = 0; i < num_monsters; i++) { // Ver todos los monstruos y despues filtrar por los q estan vivos
        // Distancia entre el heroe y el monstruo
        int mon_x = monsters[i].current_coords.x;
        int mon_y = monsters[i].current_coords.y;

        int diff_x = abs(hero_x - mon_x); // Cambiado pq antes era a partir de la dist de manhattan
        int diff_y = abs(hero_y - mon_y);

        if (diff_x <= hero->range && diff_y <= hero->range && monsters[i].alive) {
            printf("( ⚔️ ) El Heroe%d esta atacando al Monstruo%d (Ha dejado de avanzar)\n",hero->id, monsters[i].id);
            hero->fighting = true;

            // Atacar al monstruo
            monsters[i].hp = monsters[i].hp - hero->damage;
            if(monsters[i].hp <= 0) {
                monsters[i].alive = false;
                printf("( ❌ ) Monstruo%d ha sido derrotado por el Heroe%d\n", monsters[i].id, hero->id);
                
                // Borrar del mapa
                int mon_y_mapa = obtenerFila(monsters[i].current_coords.y, Grid.grid_h);
                int mon_x_mapa = obtenerColumna(monsters[i].current_coords.x);
                monitor.mapa[mon_y_mapa][mon_x_mapa] = ".";

                monitor.monsters_vivos--;
            } else {
                printf("Monstruo%d quedo con %d HP.\n", monsters[i].id, monsters[i].hp);
            }

            return; // Para evitar q pelee con mas de un Monstruo en un solo turno... NEW
        }
        else{
            hero->fighting = false;
        }

    }
}

void HeroEnRangoVision(Monster *monster){
    int x_monster = monster->current_coords.x;
    int y_monster = monster->current_coords.y;
    
    int x_hero = Doom_Slayer.current_coords.x;
    int y_hero = Doom_Slayer.current_coords.y;

    int diff_x = abs(x_monster - x_hero); // Cambiado pq antes era a partir de la dist de manhattan
    int diff_y = abs(y_monster - y_hero);

    if (diff_x <= monster->vision_range && diff_y <= monster->vision_range && Doom_Slayer.alive && monster->alerted == false) {  
        monster->alerted = true; // nose si ta bien la logica del alerted   
        monster->target_hero = &Doom_Slayer; // Apuntar al heroe detectado
        printf("( ❗ ​) Monstruo%d detecto al Heroe%d\n", monster->id, monster->target_hero->id);
        AlertarMonstruos(monster);
    }
    else if (monster->alerted == true && monster->target_hero != NULL && monster->alive)
    {
        printf("El Monstruo%d esta visualizando al Heroe%d\n", monster->id, monster->target_hero->id);
        MonstruoAtaca(monster);
    }
    else{
        monster->target_hero = NULL;
    }
}

// ==================Funciones de los threads==================

void* hero_routine(void* arg) {
    Hero* hero = (Hero*)arg;
    printf("Heroe%d inicia en (%d, %d) con %d de vida.\n",hero->id, hero->current_coords.x, hero->current_coords.y, hero->hp);

    for (int i = 0; i < hero->path_length; i++) {
        pthread_mutex_lock(&monitor.mutex);

        while(monitor.turn == false && !monitor.game_over) { // Esperar turno de heroes
            monitor.heroes_esperando++;
            pthread_cond_wait(&monitor.hero_turn, &monitor.mutex);
            monitor.heroes_esperando--;
        }

        if(monitor.game_over) {
            pthread_mutex_unlock(&monitor.mutex);
            printf("Heroe%d detecta que el juego ha terminado.\n", hero->id);
            break;
        }

        if (!hero->alive) { // Cambiar logica despues para mas heroes (heroes_vivos == 0) {
            printf("( ❌ ) El Heroe%d ha muerto, ya no quiero jugar the game\n", hero->id);
            monitor.game_over = true;
            monitor.turn = false;

            pthread_mutex_unlock(&monitor.mutex);

            pthread_cond_broadcast(&monitor.monster_turn); 
            pthread_cond_broadcast(&monitor.hero_turn);
            break;
        }

        MounstroEnRangoAtaque(hero); // Siempre poner antes q el movimiento

        if(!hero->fighting) { // mover
            monitor.mapa[obtenerFila(hero->current_coords.y, Grid.grid_h)][obtenerColumna(hero->current_coords.x)] = "."; // Limpiar la posicion anterior

            hero->current_coords.x = hero->path[i].x;
            hero->current_coords.y = hero->path[i].y;

            monitor.mapa[obtenerFila(hero->current_coords.y, Grid.grid_h)][obtenerColumna(hero->current_coords.x)] = "H"; // Actualizar la nueva posicion
        
            printf("( ➡️  ​) Heroe%d se movio a (%d, %d)\n",hero->id, hero->current_coords.x, hero->current_coords.y);
        }
        else{
            i--; // No avanza en el path si esta peleando
            hero->fighting = false; // Resetear pelea para el siguiente turno
        }

        monitor.turn = false; // Cambiar turno a monstruos

        if(i == hero->path_length - 1) { // El héroe ha llegado al final
            monitor.game_over = true; 
            monitor.heroes_vivos--;
            printf("Heroe%d llego al final de su ruta YIPI!!! 🎉 ​🎉 ​🎉 ​\n", hero->id);
            PrintMapa(&monitor);
            printf("Estadisticas finales del Heroe%d:\n HP: %d\n",hero->id, hero->hp);

            pthread_cond_broadcast(&monitor.monster_turn);
            pthread_cond_broadcast(&monitor.hero_turn);

            pthread_mutex_unlock(&monitor.mutex);
            continue;
        }


        PrintMapa(&monitor);
        printf("\nHP: %d\n", hero->hp);
        printf("\n========= El Heroe ya actuo, turno de los Monstruos =========\n\n");
        pthread_mutex_unlock(&monitor.mutex);

        pthread_cond_broadcast(&monitor.monster_turn); // Notificar a los monstruos
        
        // Pausa para simular el movimiento
        usleep(20000);// 0.02 segundos
    }

    printf("**********DEBUG: Heroe%d termina su THREAD**********\n", hero->id); //borrar pq no esta dentro del mutex y se puede bugear
    pthread_exit(NULL);
}

void* monster_routine(void* arg) {
    Monster* monster = (Monster*)arg;
    Hero temp_hero; // Heroe temporal para funciones
    while (monster->alive && !monitor.game_over) //yo
    {
        pthread_mutex_lock(&monitor.mutex);

        while(monitor.turn == true && !monitor.game_over) { // Esperar turno de monstruos
            monitor.monsters_esperando++;
            pthread_cond_wait(&monitor.monster_turn, &monitor.mutex);
            monitor.monsters_esperando--;
        }

        if(monitor.game_over) {
            pthread_mutex_unlock(&monitor.mutex);
            break;
        }

        if(!monster->alive) {
            monitor.monsters_actuados++;

            if(monitor.monsters_actuados == monitor.monsters_vivos) {
                monitor.turn = true;
                monitor.monsters_actuados = 0;
                printf("\n========= Todos los Monstruos actuaron, turno del Heroe =========\n\n");
                pthread_cond_broadcast(&monitor.hero_turn);
            }
            
            pthread_mutex_unlock(&monitor.mutex);
            usleep(20000);
            continue;
        }


        // Lógica del monstruo: moverse o atacar
        // Hacer q el monstro se acerque hacia el heroe si lo ve en su vision range, hacer funcion despues 
        HeroEnRangoVision(monster);

        if(monster->target_hero != NULL) { // Verificar rango de vision y alertar a los demas con una funcion despues
            
            //printf("(Monstruo%d vio al Heroe%d, monstruo quiere ver sangre)\n", monster->id, monster->target_hero->id);
        }
        else {
            printf("( 💤 ) Monstruo%d esta procrastinando en su turno para actuar, asi que no hace nada.\n", monster->id);
        }
        
        monitor.monsters_actuados++;

        if(monitor.monsters_actuados == monitor.monsters_vivos) { //Una vez todos los monstruos tienen su turno
            monitor.turn = true;  // Cambiar turno a héroes
            monitor.monsters_actuados = 0;  // Resetear contador
            printf("\n========= Todos los Monstruos actuaron, turno del Heroe =========\n\n");
            pthread_cond_broadcast(&monitor.hero_turn);  // Avisar a TODOS los héroes
        }

        pthread_mutex_unlock(&monitor.mutex);

        usleep(20000); // 0.02 segundos
    }
    

    printf("**********DEBUG: Monstruo%d termina su THREAD**********\n", monster->id);
    pthread_exit(NULL);
}


int main(int argc, char **argv) { // LISTO NO TOCAR GRRR (solo agregado argv)
    // Si no se pasa argumento, usa "ejemplo.txt" por defecto
    const char *nombre_archivo = (argc >= 2) ? argv[1] : "ejemplo.txt";

    FILE *archivo = fopen(nombre_archivo, "r");
    if(1){ // Configuracion de heroe y monstruos
        if (archivo == NULL) {
            perror("No se pudo abrir el archivo");
            return 1; // Terminar el programa con error
        }

        LeerConfig(archivo, &Doom_Slayer);
        LeerHeroPath(archivo, &Doom_Slayer);
        LeerMonstersConfig(archivo, &monsters);
    }
    
    // Inicializar el monitor
    monitor_init(&monitor, Grid.grid_h, Grid.grid_w);
    CrearMapa(&Doom_Slayer, monsters, &monitor); // Despues de leer los datos

    printf("----------------------------FIN DE LEER DATOS----------------------------\n\n\n");

    // Creacion de threads
    pthread_t hero_thread;
    pthread_t monster_threads[num_monsters];

    pthread_create(&hero_thread, NULL, hero_routine, &Doom_Slayer);

    for (int i = 0; i < num_monsters; i++) {
        pthread_create(&monster_threads[i], NULL, monster_routine, &monsters[i]);
    }

    // Esperar a que los threads terminen
    pthread_join(hero_thread, NULL);
    for (int i = 0; i < num_monsters; i++) {
        pthread_join(monster_threads[i], NULL);
    }

    // Liberar memoria
    free(Doom_Slayer.path);
    free(monsters);
    fclose(archivo);
    return 0;
}