// =============================================================================
//  PROYECTO 1A — VIDEOJUEGO XOP (Versión Preliminar)
//  Curso: Estructuras de Datos IC2001
//  Descripción: Réplica preliminar del bullet hell XOP usando Allegro 5.
//               Implementa nave del jugador, enemigos, 6 armas secundarias,
//               sistema de monedas (DAL Coins), estadísticas básicas y
//               múltiples modos/dificultades. Esta versión es un prototipo
//               funcional sobre el cual se seguirá construyendo.
// =============================================================================

// Suprime advertencias de funciones "inseguras" de MSVC (fopen, etc.)
#define _CRT_SECURE_NO_WARNINGS

// ── Allegro : módulos necesarios ────────────────────────────────────────────
#include <allegro5/allegro.h>            // Núcleo: display, timer, cola de eventos
#include <allegro5/allegro_primitives.h> // Formas geométricas (círculos, rectángulos)
#include <allegro5/allegro_font.h>       // Renderizado de texto con fuente built-in
#include <allegro5/allegro_ttf.h>        // Soporte para fuentes TrueType (.ttf)

// ── Biblioteca estándar ───────────────────────────────────────────────────────
#include <vector>      // std::vector — usado para listas de balas, enemigos, etc.
                       // NOTA: en la versión final estas colecciones serán
                       // reemplazadas por listas enlazadas manuales con new/delete
#include <algorithm>   // std::max, std::min, std::remove_if
#include <cstdlib>     // rand(), srand()
#include <ctime>       // time() — semilla del generador aleatorio
#include <cstdio>      // fopen, fprintf, fclose — persistencia de estadísticas
#include <cmath>       // fabs() — valor absoluto en colisiones

// =============================================================================
//  CONSTANTES GLOBALES
// =============================================================================

const int WIDTH = 800;   // Ancho de la ventana en píxeles
const int HEIGHT = 600;   // Alto  de la ventana en píxeles
const float FPS = 60;    // Frames (ticks de lógica) por segundo

// =============================================================================
//  ENUMERACIONES — definen los estados y tipos del juego
// =============================================================================

// Estados posibles de la máquina de estados del juego.
// El game loop usa este valor para decidir qué lógica y qué pantalla ejecutar.
enum EstadoJuego {
    MENU,       // Pantalla de inicio: selección de dificultad, modo y arma
    JUGANDO,    // Gameplay activo: lógica de movimiento, disparo y colisiones
    GAME_OVER,  // El jugador perdió todas las vidas; se muestran opciones
    STATS       // Pantalla informativa: indica al usuario dónde ver las estadísticas
};

// Niveles de dificultad escalonados (0 = más fácil, 7 = más difícil).
// Cada valor indexa la tabla configs[] para obtener spawn rate, velocidad
// de enemigos y velocidad de balas enemigas.
enum Dificultad {
    DIF_EASIEST,    // 0 — Balas lentas, spawn escaso
    DIF_EASY,       // 1
    DIF_NORMAL,     // 2 — Punto de equilibrio
    DIF_HARD,       // 3
    DIF_VERY_HARD,  // 4
    DIF_EXTREME,    // 5
    DIF_NIGHTMARE,  // 6
    DIF_INSANE      // 7 — Experiencia máxima bullet hell
};

// Modos de juego que modifican el comportamiento global de la simulación.
enum ModoJuego {
    ORIGINAL,   // Velocidad y spawn estándar
    SLUDGE,     // Factor de tiempo 0.5× — cámara lenta
    MANIAC,     // Factor de tiempo 1.5× — acción frenética
    MASSACRE    // Spawneo doble de enemigos por tick
};

// Armas secundarias disponibles para el jugador.
// El jugador selecciona con teclas 0-6 y dispara con ESPACIO.
enum Arma {
    ARMA_NORMAL,  // 0 — Disparo simple hacia arriba
    BOTS,         // 1 — Cuatro drones en offset fijo que disparan en conjunto
    VELOCITY,     // 2 — Rayo instantáneo en línea recta (elimina enemigos alineados)
    SPREAD,       // 3 — Disparo en arco: 5 balas en abanico
    MISSILE,      // 4 — Proyectil con homing básico hacia el primer enemigo de la lista
    IMPLOSION,    // 5 — Campo de gravedad: atrae enemigos cercanos hacia el jugador
    PLASMA        // 6 — Proyectil rápido con efecto visual de destello verde
};

// =============================================================================
//  STRUCTS — representación de entidades del juego
// =============================================================================

// Bala disparada por el jugador.
// 'hit' evita contar fallos dobles en balas que ya impactaron.
// 'tipo' define el color de renderizado (0=normal, 1=spread, 2=plasma, 3=bots).
struct Bala {
    float x, y, vel;
    bool hit;
    int tipo;
    Bala* siguiente; 
};

// Nave del jugador. 'vel' es la velocidad de desplazamiento lateral.
struct Jugador { float x, y, vel; int vidas; };

// Enemigo básico. Se desplaza verticalmente hacia abajo a velocidad 'vel'.
struct Enemigo {
    float x, y, vel;
    Enemigo* siguiente;
};
// Moneda DAL Coin soltada al destruir un enemigo.
// tipo 0 = moneda azul (+5 pts), tipo 1 = moneda morada (+10 pts).
struct Moneda {
    float x, y;
    int tipo;
    Moneda* siguiente;
};
// Drone del arma BOTS. 'ox' y 'oy' son offsets relativos a la posición del jugador.
struct Bot {
    float ox, oy;
    Bot* siguiente;
};
// Misil del arma MISSILE. Se desplaza hacia arriba con homing rudimentario.
struct Missile {
    float x, y;
    Missile* siguiente;
};
// =============================================================================
//  VARIABLES GLOBALES DE ESTADÍSTICAS
//  NOTA: en la versión final estas variables alimentarán un BST dinámico
//  con persistencia incremental en disco (fflush tras cada escritura).
// =============================================================================

int disparos = 0;  // Total de veces que el jugador presionó ESPACIO
int aciertos = 0;  // Balas que impactaron un enemigo
int fallos = 0;  // Balas que salieron de pantalla sin impactar
int score = 0;  // Puntuación acumulada en la partida actual

// =============================================================================
//  guardarStats()
//  Persiste las estadísticas de la partida actual al archivo "stats.txt"
//  en modo append (no sobreescribe partidas anteriores).
//  NOTA: en la versión final se reemplazará por serialización del BST
//  con escritura incremental + fflush() para resistencia ante caídas abruptas.
// =============================================================================
void guardarStats() {
    FILE* file = fopen("stats.txt", "a");  // "a" = append — preserva historial
    if (file) {
        // Formato por línea: disparos aciertos fallos score
        fprintf(file, "%d %d %d %d\n", disparos, aciertos, fallos, score);
        fclose(file);
    }
}

// =============================================================================
//  CONFIGURACIÓN DE DIFICULTAD
//  Cada índice corresponde a un valor del enum Dificultad (0-7).
//  spawnRate: cada cuántos ticks aparece un enemigo (menor = más rápido)
//  velEnemigo: velocidad vertical de bajada del enemigo (px/tick)
//  velBala:    velocidad vertical de las balas enemigas (px/tick)
// =============================================================================
struct ConfigDificultad { int spawnRate; float velEnemigo; float velBala; };

ConfigDificultad configs[8] = {
    {120, 2.0f, 3.0f},   // EASIEST
    {100, 2.5f, 3.5f},   // EASY
    { 80, 3.0f, 4.0f},   // NORMAL
    { 60, 3.5f, 4.5f},   // HARD
    { 50, 4.0f, 5.0f},   // VERY HARD
    { 40, 4.5f, 5.5f},   // EXTREME
    { 30, 5.0f, 6.0f},   // NIGHTMARE
    { 20, 6.0f, 7.0f}    // INSANE
};

// Tablas de nombres para renderizado en pantalla
const char* nombresDif[] = { "EASIEST","EASY","NORMAL","HARD","VERY HARD","EXTREME","NIGHTMARE","INSANE" };
const char* nombresModo[] = { "ORIGINAL","SLUDGE","MANIAC","MASSACRE" };
const char* nombresArma[] = { "NORMAL","BOTS","VELOCITY","SPREAD","MISSILE","IMPLOSION","PLASMA" };

// =============================================================================
//  colision(x1, y1, x2, y2, r)
//  Detección de colisión AABB simplificada (caja cuadrada de lado 2r).
//  Retorna true si ambos puntos están a menos de 'r' píxeles en X e Y.
//  Se usa para: bala-enemigo, bala-jugador, jugador-moneda.
//  NOTA: hitbox cuadrada es una aproximación; la versión final puede refinar
//  a círculo real usando distancia euclídea.
// =============================================================================
bool colision(float x1, float y1, float x2, float y2, float r) {
    return (fabs(x1 - x2) < r && fabs(y1 - y2) < r);
}

//  FUNCIONES DE LISTA ENLAZADA 

// ── BALA ─────────────────────────────────────────────────────────────────────

// Inserta una nueva bala al frente de la lista.
// Reutiliza el patrón de AgregarInicioInventario.
void AgregarBala(Bala*& Lista, Bala*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

// Elimina una bala específica de la lista dado su puntero directo.
// Se llama cuando la bala impactó un enemigo o salió de pantalla.
void EliminarBala(Bala*& Lista, Bala* nodo)
{
    if (Lista == NULL) return;

    // Caso especial: el nodo a eliminar es la cabeza de la lista
    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    // Buscar el nodo anterior para poder reconectar la lista
    Bala* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    // Desconectar el nodo y liberar su memoria
    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

// Libera toda la memoria de la lista de balas.
// Reutiliza el patrón de DestruirInventario.
void DestruirBalas(Bala*& Lista)
{
    Bala* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

// ── BALA ENEMIGA ──────────────────────────────────────────────────────────────
// Mismo tipo Bala, lista separada para balas disparadas por enemigos.

void AgregarBalaEnemiga(Bala*& Lista, Bala*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarBalaEnemiga(Bala*& Lista, Bala* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Bala* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirBalasEnemigas(Bala*& Lista)
{
    Bala* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

// ── ENEMIGO ───────────────────────────────────────────────────────────────────

void AgregarEnemigo(Enemigo*& Lista, Enemigo*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarEnemigo(Enemigo*& Lista, Enemigo* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Enemigo* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirEnemigos(Enemigo*& Lista)
{
    Enemigo* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

// ── MONEDA ────────────────────────────────────────────────────────────────────

void AgregarMoneda(Moneda*& Lista, Moneda*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarMoneda(Moneda*& Lista, Moneda* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Moneda* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirMonedas(Moneda*& Lista)
{
    Moneda* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

// ── BOT ───────────────────────────────────────────────────────────────────────

void AgregarBot(Bot*& Lista, Bot*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarBot(Bot*& Lista, Bot* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Bot* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirBots(Bot*& Lista)
{
    Bot* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

// ── MISSILE ───────────────────────────────────────────────────────────────────

void AgregarMissile(Missile*& Lista, Missile*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarMissile(Missile*& Lista, Missile* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Missile* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirMissiles(Missile*& Lista)
{
    Missile* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}


int main() {

    // ── 1. Inicialización de Allegro y addons ─────────────────────────────────
    al_init(); // Núcleo de Allegro (obligatorio primero)
    al_install_keyboard(); // Habilita eventos de teclado
    al_init_primitives_addon(); // Habilita dibujo de formas (círculos, rectángulos)
    al_init_font_addon(); // Habilita sistema de fuentes
    al_init_ttf_addon(); // Habilita fuentes TrueType (.ttf)

    // ── 2. Creación de recursos Allegro ───────────────────────────────────────
    ALLEGRO_DISPLAY* display = al_create_display(WIDTH, HEIGHT);
    // Timer que dispara un evento cada 1/60 segundos → 60 UPS (updates per second)
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    // Cola de eventos central: aquí llegan inputs de teclado, timer y cierre de ventana
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    // Fuente built-in de Allegro (no requiere archivo externo)
    ALLEGRO_FONT* font = al_create_builtin_font();

    // Registrar fuentes de eventos en la cola
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_register_event_source(queue, al_get_keyboard_event_source());

    // ── 3. Estado inicial del juego ───────────────────────────────────────────
    EstadoJuego estado = MENU;
    Dificultad  dificultad = DIF_NORMAL;
    ModoJuego   modo = ORIGINAL;
    Arma        armaActual = ARMA_NORMAL;

    // Posición inicial del jugador: centrado horizontalmente, cerca del fondo
    Jugador jugador = { WIDTH / 2, HEIGHT - 50, 5, 3 };

    // Cabezas de las listas enlazadas de entidades activas.
    // NULL representa lista vacía (equivalente al vector vacío anterior).
    Bala* balas = NULL;  // Proyectiles del jugador
    Bala* balasEnemigas = NULL;  // Proyectiles disparados por enemigos
    Enemigo* enemigos = NULL;  // Naves enemigas activas
    Moneda* monedas = NULL;  // DAL Coins en pantalla
    Bot* bots = NULL;  // Drones del arma BOTS
    Missile* misiles = NULL;  // Misiles del arma MISSILE

    ALLEGRO_KEYBOARD_STATE keystate; // Estado del teclado para lectura continua 
    srand(time(NULL)); // Semilla aleatoria basada en tiempo del sistema
    al_start_timer(timer); // Inicia el timer → comienzan los eventos de tick

    bool running = true;  // Control del game loop principal
    bool redraw = true;  // Bandera: ¿hay que redibujar este frame?

    // =========================================================================
    //  GAME LOOP PRINCIPAL — arquitectura Cola de Eventos Allegro 5
    //  Patrón: al_wait_for_event bloquea hasta que hay un evento disponible.
    //  Los eventos de TIMER marcan los ticks de lógica (60/s).
    //  El renderizado solo ocurre cuando redraw=true Y la cola está vacía,
    //  evitando dibujar frames intermedios innecesarios.
    // =========================================================================
    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev); // Bloquea hasta recibir un evento

        // Cierre de ventana → terminar el loop
        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) running = false;

        // Tick del timer → habilitar redibujado y procesar lógica
        if (ev.type == ALLEGRO_EVENT_TIMER) redraw = true;

        // =====================================================================
        //  ESTADO: MENU
        //  Permite al usuario configurar dificultad, modo y arma antes de jugar.
        //  Controles: ENTER=jugar, ARRIBA/ABAJO=dificultad, IZQ/DER=modo, S=stats
        // =====================================================================
        if (estado == MENU) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
                if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) estado = JUGANDO;

                // Ciclo de dificultad con teclas arriba/abajo
                if (ev.keyboard.keycode == ALLEGRO_KEY_UP && dificultad < DIF_INSANE)  dificultad = (Dificultad)(dificultad + 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN && dificultad > DIF_EASIEST) dificultad = (Dificultad)(dificultad - 1);

                // Ciclo de modo de juego con teclas izquierda/derecha
                if (ev.keyboard.keycode == ALLEGRO_KEY_RIGHT && modo < MASSACRE) modo = (ModoJuego)(modo + 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_LEFT && modo > ORIGINAL) modo = (ModoJuego)(modo - 1);

                if (ev.keyboard.keycode == ALLEGRO_KEY_S) estado = STATS;
            }
        }

        // =====================================================================
        //  ESTADO: JUGANDO
        //  Contiene toda la lógica de gameplay:
        //    - Movimiento del jugador (polling continuo de teclado)
        //    - Spawn de enemigos según dificultad y modo
        //    - Movimiento de todas las entidades
        //    - Detección y resolución de colisiones
        //    - Lógica especial de armas (Implosion, Velocity)
        //    - Recolección de monedas
        //    - Conteo de fallos y limpieza de entidades fuera de pantalla
        //    - Transición a GAME_OVER al perder todas las vidas
        // =====================================================================
        else if (estado == JUGANDO) {

            // Obtener config de dificultad actual
            ConfigDificultad cfg = configs[dificultad];

            // multVel: modificador de velocidad según modo de juego
            // SLUDGE=0.5× (lento), MANIAC=1.5× (rápido), resto=1.0×
            float multVel = (modo == SLUDGE ? 0.5f : (modo == MANIAC ? 1.5f : 1.0f));

            // multSpawn: MASSACRE duplica la cantidad de enemigos por tick de spawn
            int   multSpawn = (modo == MASSACRE ? 2 : 1);

            if (ev.type == ALLEGRO_EVENT_TIMER) {

                // ── Movimiento del jugador (polling, no eventos) ───────────
                // Se lee el estado completo del teclado para detectar teclas
                // sostenidas, a diferencia de KEY_DOWN que solo detecta la pulsación inicial.
                al_get_keyboard_state(&keystate);
                if (al_key_down(&keystate, ALLEGRO_KEY_LEFT))  jugador.x -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_RIGHT)) jugador.x += jugador.vel;

                // Clampear posición del jugador dentro de los límites de pantalla
                jugador.x = std::max(10.f, std::min((float)WIDTH - 10, jugador.x));

                // ── Spawn de enemigos ──────────────────────────────────────
                // rand() % spawnRate == 0 da probabilidad 1/spawnRate por tick.
                // A menor spawnRate, mayor frecuencia de aparición.
                if (rand() % cfg.spawnRate == 0) {
                    for (int i = 0; i < multSpawn; i++) {
                        // Crear nodo con new y agregar al frente de la lista
                        Enemigo* Nuevo = new Enemigo;
                        Nuevo->x = (float)(rand() % WIDTH);
                        Nuevo->y = 0;
                        Nuevo->vel = cfg.velEnemigo * multVel;
                        Nuevo->siguiente = NULL;
                        AgregarEnemigo(enemigos, Nuevo);
                    }
                }

                // ── Movimiento de enemigos + disparo enemigo ───────────────
                // Se recorre la lista con puntero auxiliar para poder eliminar
                // nodos durante el recorrido sin perder el hilo de la lista.
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;  // Guardar antes de posible eliminación

                        Aux->y += Aux->vel;  // Desplazamiento vertical hacia abajo

                        // Probabilidad 1/100 de disparar por enemigo por tick
                        if (rand() % 100 == 0) {
                            Bala* NuevaBala = new Bala;
                            NuevaBala->x = Aux->x;
                            NuevaBala->y = Aux->y;
                            NuevaBala->vel = cfg.velBala * multVel;
                            NuevaBala->hit = false;
                            NuevaBala->tipo = 0;
                            NuevaBala->siguiente = NULL;
                            AgregarBalaEnemiga(balasEnemigas, NuevaBala);
                        }

                        // Eliminar enemigos que salieron por la parte inferior
                        if (Aux->y > HEIGHT) EliminarEnemigo(enemigos, Aux);

                        Aux = Siguiente;
                    }
                }

                // ── Movimiento de proyectiles del jugador ──────────────────
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        Aux->y -= Aux->vel;  // Suben

                        // Conteo de fallos: bala salió por arriba sin impactar
                        if (Aux->y < 0 && !Aux->hit) {
                            fallos++;
                            EliminarBala(balas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // ── Movimiento de balas enemigas ───────────────────────────
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        Aux->y += Aux->vel;  // Bajan

                        // Eliminar balas que salieron por la parte inferior
                        if (Aux->y > HEIGHT) EliminarBalaEnemiga(balasEnemigas, Aux);

                        Aux = Siguiente;
                    }
                }

                // ── Movimiento de misiles (homing básico) ──────────────────
                // El misil se acerca horizontalmente al primer enemigo de la lista
                // y sube verticalmente a velocidad fija de 3 px/tick.
                {
                    Missile* Aux = misiles;
                    while (Aux != NULL) {
                        Missile* Siguiente = Aux->siguiente;

                        // Homing: seguir al primer enemigo de la lista
                        if (enemigos != NULL) {
                            if (Aux->x < enemigos->x) Aux->x += 2;
                            if (Aux->x > enemigos->x) Aux->x -= 2;
                        }
                        Aux->y -= 3;

                        // Eliminar misiles que salieron por la parte superior
                        if (Aux->y < 0) EliminarMissile(misiles, Aux);

                        Aux = Siguiente;
                    }
                }

                // ── Colisiones: bala del jugador vs enemigo ────────────────
                {
                    Bala* AuxBala = balas;
                    while (AuxBala != NULL) {
                        Bala* SiguienteBala = AuxBala->siguiente;
                        bool impacto = false;

                        Enemigo* AuxEnemigo = enemigos;
                        while (AuxEnemigo != NULL && !impacto) {
                            Enemigo* SiguienteEnemigo = AuxEnemigo->siguiente;

                            if (colision(AuxBala->x, AuxBala->y, AuxEnemigo->x, AuxEnemigo->y, 10)) {
                                // Soltar moneda en la posición del enemigo destruido
                                Moneda* NuevaMoneda = new Moneda;
                                NuevaMoneda->x = AuxEnemigo->x;
                                NuevaMoneda->y = AuxEnemigo->y;
                                NuevaMoneda->tipo = rand() % 2;
                                NuevaMoneda->siguiente = NULL;
                                AgregarMoneda(monedas, NuevaMoneda);

                                // Eliminar bala y enemigo, sumar puntos
                                EliminarEnemigo(enemigos, AuxEnemigo);
                                EliminarBala(balas, AuxBala);
                                score += 10;
                                aciertos++;
                                impacto = true;
                            }
                            AuxEnemigo = SiguienteEnemigo;
                        }
                        AuxBala = SiguienteBala;
                    }
                }

                // ── Lógica especial: IMPLOSION ─────────────────────────────
                // Campo de atracción gravitacional en radio de 80px.
                // Los enemigos dentro del radio se mueven un 5% hacia el jugador por tick.
                if (armaActual == IMPLOSION) {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 80)) {
                            Aux->x += (jugador.x - Aux->x) * 0.05f;
                            Aux->y += (jugador.y - Aux->y) * 0.05f;
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // ── Movimiento de monedas + recolección ───────────────────
                {
                    Moneda* Aux = monedas;
                    while (Aux != NULL) {
                        Moneda* Siguiente = Aux->siguiente;
                        Aux->y += 2;  // Las monedas caen hacia abajo

                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            // Recolectada: sumar puntos según tipo y eliminar
                            score += (Aux->tipo == 0 ? 5 : 10);  // Azul=5, morada=10
                            EliminarMoneda(monedas, Aux);
                        }
                        else if (Aux->y > HEIGHT) {
                            // Salió de pantalla sin ser recogida
                            EliminarMoneda(monedas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // ── Colisiones: balas enemigas vs jugador ──────────────────
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 10)) {
                            jugador.vidas--;
                            EliminarBalaEnemiga(balasEnemigas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // ── Transición a GAME_OVER ─────────────────────────────────
                if (jugador.vidas <= 0) {
                    guardarStats();  // Persistir estadísticas antes de cambiar estado
                    estado = GAME_OVER;
                }
            }

            // ── Input de disparo y cambio de arma (eventos de tecla) ──────
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {

                // Teclas 0-6: selección de arma secundaria
                if (ev.keyboard.keycode == ALLEGRO_KEY_0) armaActual = ARMA_NORMAL;
                if (ev.keyboard.keycode == ALLEGRO_KEY_1) armaActual = BOTS;
                if (ev.keyboard.keycode == ALLEGRO_KEY_2) armaActual = VELOCITY;
                if (ev.keyboard.keycode == ALLEGRO_KEY_3) armaActual = SPREAD;
                if (ev.keyboard.keycode == ALLEGRO_KEY_4) armaActual = MISSILE;
                if (ev.keyboard.keycode == ALLEGRO_KEY_5) armaActual = IMPLOSION;
                if (ev.keyboard.keycode == ALLEGRO_KEY_6) armaActual = PLASMA;

                // ESPACIO: disparar según el arma seleccionada
                if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                    disparos++;  // Contar intento de disparo

                    switch (armaActual) {
                    case ARMA_NORMAL: {
                        // Un proyectil simple hacia arriba desde la posición del jugador
                        Bala* Nueva = new Bala;
                        Nueva->x = jugador.x;
                        Nueva->y = jugador.y;
                        Nueva->vel = 8;
                        Nueva->hit = false;
                        Nueva->tipo = 0;
                        Nueva->siguiente = NULL;
                        AgregarBala(balas, Nueva);
                        break;
                    }
                    case SPREAD: {
                        // 5 balas en abanico con separación de 5px entre sí
                        for (int i = -2; i <= 2; i++) {
                            Bala* Nueva = new Bala;
                            Nueva->x = jugador.x + i * 5;
                            Nueva->y = jugador.y;
                            Nueva->vel = 6;
                            Nueva->hit = false;
                            Nueva->tipo = 1;
                            Nueva->siguiente = NULL;
                            AgregarBala(balas, Nueva);
                        }
                        break;
                    }
                    case PLASMA: {
                        // Proyectil único más veloz (12 px/tick vs 8 normal)
                        Bala* Nueva = new Bala;
                        Nueva->x = jugador.x;
                        Nueva->y = jugador.y;
                        Nueva->vel = 12;
                        Nueva->hit = false;
                        Nueva->tipo = 2;
                        Nueva->siguiente = NULL;
                        AgregarBala(balas, Nueva);
                        break;
                    }
                    case MISSILE: {
                        // Misil con homing — su movimiento se procesa en el loop
                        Missile* Nuevo = new Missile;
                        Nuevo->x = jugador.x;
                        Nuevo->y = jugador.y;
                        Nuevo->siguiente = NULL;
                        AgregarMissile(misiles, Nuevo);
                        break;
                    }
                    case BOTS: {
                        // Primera activación: crear los 4 drones en offsets fijos
                        if (bots == NULL) {
                            float offsets[4][2] = { {-20,-20},{20,-20},{-20,20},{20,20} };
                            for (int i = 0; i < 4; i++) {
                                Bot* NuevoBot = new Bot;
                                NuevoBot->ox = offsets[i][0];
                                NuevoBot->oy = offsets[i][1];
                                NuevoBot->siguiente = NULL;
                                AgregarBot(bots, NuevoBot);
                            }
                        }
                        // Disparar una bala desde la posición de cada drone
                        Bot* AuxBot = bots;
                        while (AuxBot != NULL) {
                            Bala* Nueva = new Bala;
                            Nueva->x = jugador.x + AuxBot->ox;
                            Nueva->y = jugador.y + AuxBot->oy;
                            Nueva->vel = 8;
                            Nueva->hit = false;
                            Nueva->tipo = 3;
                            Nueva->siguiente = NULL;
                            AgregarBala(balas, Nueva);
                            AuxBot = AuxBot->siguiente;
                        }
                        break;
                    }
                    default:
                        // IMPLOSION y VELOCITY no se activan con ESPACIO:
                        // IMPLOSION es pasivo (siempre activo al estar seleccionado)
                        // VELOCITY se dibuja y evalúa en la sección de renderizado
                        break;
                    }
                }
            }
        }

        // =====================================================================
        //  ESTADO: GAME_OVER
        //  Muestra la pantalla de fin de partida.
        //  ENTER reinicia todos los contadores y vuelve al MENU.
        // =====================================================================
        else if (estado == GAME_OVER) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                // Reiniciar estado del jugador
                jugador.vidas = 3;
                score = 0;
                disparos = aciertos = fallos = 0;

                // Liberar toda la memoria dinámica de las listas
                DestruirEnemigos(enemigos);
                DestruirBalas(balas);
                DestruirBalasEnemigas(balasEnemigas);
                DestruirMonedas(monedas);
                DestruirMissiles(misiles);
                DestruirBots(bots);

                estado = MENU;
            }
        }

        // =====================================================================
        //  ESTADO: STATS
        //  Pantalla informativa simple. ESC regresa al menú.
        //  NOTA: en la versión final mostrará el Top Scores del BST directamente.
        // =====================================================================
        else if (estado == STATS) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                estado = MENU;
        }

        // =====================================================================
        //  RENDERIZADO
        //  Solo se dibuja cuando:
        //    a) redraw == true  (hubo un tick de timer)
        //    b) la cola de eventos está vacía (no hay más eventos pendientes)
        //  Esto evita dibujar frames intermedios y desacopla lógica de render.
        // =====================================================================
        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(0, 0, 0)); // Fondo negro cada frame

            // ── Renderizado: MENU ──────────────────────────────────────────
            if (estado == MENU) {
                al_draw_text(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 - 60, ALLEGRO_ALIGN_CENTER, "ENTER jugar | S stats");
                al_draw_textf(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 - 20, ALLEGRO_ALIGN_CENTER, "Dificultad: %s", nombresDif[(int)dificultad]);
                al_draw_textf(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 + 20, ALLEGRO_ALIGN_CENTER, "Modo: %s", nombresModo[modo]);
            }

            // ── Renderizado: JUGANDO ───────────────────────────────────────
            else if (estado == JUGANDO) {

                // HUD: información en pantalla
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 10, 0, "Score: %d", score);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 30, 0, "Vidas: %d", jugador.vidas);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 50, 0, "Arma: %s", nombresArma[armaActual]);

                // Nave del jugador: rectángulo verde 20×20 px
                al_draw_filled_rectangle(jugador.x - 10, jugador.y - 10,
                    jugador.x + 10, jugador.y + 10,
                    al_map_rgb(0, 255, 0));

                // ── Renderizado de balas del jugador ──────────────────────
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        if (Aux->tipo == 2) {
                            // PLASMA: doble círculo para efecto de destello
                            al_draw_filled_circle(Aux->x, Aux->y, 6, al_map_rgba(0, 255, 0, 120));
                            al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgba(200, 255, 200, 220));
                        }
                        else {
                            ALLEGRO_COLOR color;
                            switch (Aux->tipo) {
                            case 0: color = al_map_rgb(255, 255, 0); break; // Normal: amarillo
                            case 1: color = al_map_rgb(255, 0, 0); break; // Spread: rojo
                            case 3: color = al_map_rgb(0, 255, 255); break; // Bots: cian
                            default:color = al_map_rgb(255, 255, 255); // Otros: blanco
                            }
                            al_draw_filled_circle(Aux->x, Aux->y, 3, color);
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // Balas enemigas: círculo cian
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Enemigos: rectángulo rojo 20×20 px
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;  // guardar antes de posible eliminación
                        al_draw_filled_rectangle(Aux->x - 10, Aux->y - 10,
                            Aux->x + 10, Aux->y + 10,
                            al_map_rgb(255, 0, 0));
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            jugador.vidas--;
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;  // avanzar con el puntero guardado
                    }
                }

                // Monedas: círculo azul pequeño (radio 4px)
                {
                    Moneda* Aux = monedas;
                    while (Aux != NULL) {
                        al_draw_filled_circle(Aux->x, Aux->y, 4, al_map_rgb(0, 0, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Misiles: círculo magenta
                {
                    Missile* Aux = misiles;
                    while (Aux != NULL) {
                        al_draw_filled_circle(Aux->x, Aux->y, 4, al_map_rgb(255, 0, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Drones BOTS: círculo cian en posición offset del jugador
                {
                    Bot* Aux = bots;
                    while (Aux != NULL) {
                        al_draw_filled_circle(jugador.x + Aux->ox, jugador.y + Aux->oy, 4, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // ── Lógica y render especial: VELOCITY CANNON ─────────────
                // Dibuja un rayo blanco vertical desde el jugador hasta la cima.
                // Elimina instantáneamente cualquier enemigo alineado (±10px en X).
                // NOTA: la lógica de eliminación está aquí dentro del render,
                // lo cual mezcla responsabilidades — a refactorizar en versión final.
                if (armaActual == VELOCITY) {
                    al_draw_line(jugador.x, jugador.y, jugador.x, 0, al_map_rgb(255, 255, 255), 3);
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        if (fabs(Aux->x - jugador.x) < 10) {
                            score += 20;
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;
                    }
                }
            }

            // ── Renderizado: GAME_OVER ─────────────────────────────────────
            else if (estado == GAME_OVER) {
                al_draw_text(font, al_map_rgb(255, 0, 0),
                    WIDTH / 2, HEIGHT / 2,
                    ALLEGRO_ALIGN_CENTER, "GAME OVER");
            }

            // ── Renderizado: STATS ─────────────────────────────────────────
            else if (estado == STATS) {
                al_draw_text(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2,
                    ALLEGRO_ALIGN_CENTER, "Revisa stats.txt");
            }

            // Intercambiar buffers: muestra el frame dibujado en pantalla
            al_flip_display();
        }

    } // ── FIN DEL GAME LOOP ────────────────────────────────────────────────

    // ── Limpieza de recursos al salir ─────────────────────────────────────────
    // Liberar toda la memoria dinámica de las listas enlazadas
    DestruirBalas(balas);
    DestruirBalasEnemigas(balasEnemigas);
    DestruirEnemigos(enemigos);
    DestruirMonedas(monedas);
    DestruirMissiles(misiles);
    DestruirBots(bots);

    // Liberar recursos de Allegro en orden inverso a la creación
    al_destroy_display(display);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}