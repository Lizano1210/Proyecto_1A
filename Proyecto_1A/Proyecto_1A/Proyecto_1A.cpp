 
//  PROYECTO 1A — VIDEOJUEGO XOP (Versión Preliminar)
//  Curso: Estructuras de Datos IC2001
//  Descripción: Réplica preliminar del bullet hell XOP usando Allegro 5.
//               Implementa nave del jugador, enemigos, 6 armas secundarias,
//               sistema de monedas (DAL Coins), estadísticas básicas y
//               múltiples modos/dificultades. Esta versión es un prototipo
//               funcional sobre el cual se seguirá construyendo.
 

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

 
//  CONSTANTES GLOBALES
 

const int WIDTH = 800;   // Ancho de la ventana en píxeles
const int HEIGHT = 600;   // Alto  de la ventana en píxeles
const float FPS = 60;    // Frames (ticks de lógica) por segundo

 
//  ENUMERACIONES — definen los estados y tipos del juego
 

// Estados posibles de la máquina de estados del juego.
// El game loop usa este valor para decidir qué lógica y qué pantalla ejecutar.
enum EstadoJuego {
    MENU, 
    JUGANDO,
    GAME_OVER,  
    STATS,
    TRANSICION 
};

enum Nivel { CITY, OCEAN, VOLCANO, SPACE };

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

 
//  STRUCTS — representación de entidades del juego
 

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

// Jefe de nivel. Aparece al alcanzar el umbral de enemigos eliminados.
// Tiene vida propia (varios impactos para matarlo) y patron de disparo propio.
// Solo presente en niveles VOLCANO y SPACE.
struct Jefe {
    float x, y;       // Posicion en pantalla
    float velX;       // Velocidad de movimiento lateral (zigzag)
    int   vida;       // Impactos necesarios para destruirlo
    int   fase;       // Fase del patron de disparo (cambia al perder vida)
    bool  vivo;       // Estado del jefe
    Jefe* siguiente;  // Puntero para lista enlazada
};

//  VARIABLES GLOBALES DE ESTADÍSTICAS

int disparos = 0;  // Total de veces que el jugador presionó ESPACIO
int aciertos = 0;  // Balas que impactaron un enemigo
int fallos = 0;  // Balas que salieron de pantalla sin impactar
int score = 0;  // Puntuación acumulada en la partida actual

 
//  guardarStats()
//  Persiste las estadísticas de la partida actual al archivo "stats.txt"
//  en modo append (no sobreescribe partidas anteriores).
//  NOTA: en la versión final se reemplazará por serialización del BST
//  con escritura incremental + fflush() para resistencia ante caídas abruptas.
 
void guardarStats() {
    FILE* file = fopen("stats.txt", "a");  // "a" = append — preserva historial
    if (file) {
        // Formato por línea: disparos aciertos fallos score
        fprintf(file, "%d %d %d %d\n", disparos, aciertos, fallos, score);
        fclose(file);
    }
}

 
//  CONFIGURACIÓN DE DIFICULTAD
//  Cada índice corresponde a un valor del enum Dificultad (0-7).
//  spawnRate: cada cuántos ticks aparece un enemigo (menor = más rápido)
//  velEnemigo: velocidad vertical de bajada del enemigo (px/tick)
//  velBala:    velocidad vertical de las balas enemigas (px/tick)
 
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

 
//  colision(x1, y1, x2, y2, r)
//  Detección de colisión AABB simplificada (caja cuadrada de lado 2r).
//  Retorna true si ambos puntos están a menos de 'r' píxeles en X e Y.
//  Se usa para: bala-enemigo, bala-jugador, jugador-moneda.
//  NOTA: hitbox cuadrada es una aproximación; la versión final puede refinar
//  a círculo real usando distancia euclídea.
bool colision(float x1, float y1, float x2, float y2, float r) {
    return (fabs(x1 - x2) < r && fabs(y1 - y2) < r);
}

// Retorna la cantidad de enemigos a eliminar para avanzar al siguiente nivel.
// City y Ocean son mas cortos (50), Volcano y Space mas largos (64).
int obtenerUmbral(Nivel n)
{
    if (n == CITY || n == OCEAN) return 50;
    return 64;
}

//  FUNCIONES DE LISTA ENLAZADA 

// BALA

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

// BALA ENEMIGA
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

// ENEMIGO 

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

// MONEDA 

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

// ── BOT v──

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

// MISSILE

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

// JEFE

void AgregarJefe(Jefe*& Lista, Jefe*& Nuevo)
{
    Nuevo->siguiente = Lista;
    Lista = Nuevo;
}

void EliminarJefe(Jefe*& Lista, Jefe* nodo)
{
    if (Lista == NULL) return;

    if (Lista == nodo) {
        Lista = Lista->siguiente;
        delete nodo;
        return;
    }

    Jefe* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo)
        Aux = Aux->siguiente;

    if (Aux->siguiente == nodo) {
        Aux->siguiente = nodo->siguiente;
        delete nodo;
    }
}

void DestruirJefes(Jefe*& Lista)
{
    Jefe* Aux = Lista;
    while (Aux != NULL) {
        Lista = Lista->siguiente;
        delete Aux;
        Aux = Lista;
    }
}

 
//  BST DE ESTADÍSTICAS — Historial acumulativo de partidas
//  Ordenado por score. No se permite eliminar nodos (historial permanente).
//  Basado en funciones de laboratorio previo, adaptadas al contexto del juego.
 

struct NodoBST {
    char nombre[6];   // 5 caracteres estilo arcade + '\0'
    int disparos;
    int aciertos;
    int fallos;
    int score;

    NodoBST* izq;
    NodoBST* der;
};

 
//  Insertar()
//  Agrega una nueva partida al BST ordenada por score.
//  Scores iguales van a la derecha para permitir duplicados.
//  Basado en Insertar() del laboratorio, adaptado para NodoBST.
void Insertar(NodoBST*& Raiz, char nombre[6], int disparos, int aciertos, int fallos, int score)
{
    if (Raiz == NULL)
    {
        Raiz = new NodoBST;
        Raiz->disparos = disparos;
        Raiz->aciertos = aciertos;
        Raiz->fallos = fallos;
        Raiz->score = score;
        Raiz->izq = NULL;
        Raiz->der = NULL;
        // Copiar nombre carácter por carácter (sin std::string)
        for (int i = 0; i < 6; i++) Raiz->nombre[i] = nombre[i];
    }
    else
    {
        // Scores menores van a la izquierda, mayores o iguales a la derecha
        if (score < Raiz->score)
            Insertar(Raiz->izq, nombre, disparos, aciertos, fallos, score);
        else
            Insertar(Raiz->der, nombre, disparos, aciertos, fallos, score);
    }
}

 
//  DestruirBST()
//  Libera toda la memoria del árbol recursivamente (post-order).
//  Basado en PodarHojas() del laboratorio.
//  Se llama al cerrar el programa.
void DestruirBST(NodoBST*& Raiz)
{
    if (Raiz != NULL)
    {
        DestruirBST(Raiz->izq);
        DestruirBST(Raiz->der);
        delete Raiz;
        Raiz = NULL;
    }
}

 
//  MostrarTopScores()
//  Recorre el árbol en order inverso (der → raiz → izq) para mostrar
//  el ranking de mayor a menor score en pantalla con Allegro.
void MostrarTopScores(NodoBST* Raiz, ALLEGRO_FONT* font, int& y, int& lugar, int limite)
{
    if (Raiz != NULL && lugar <= limite)
    {
        // Primero el subárbol derecho (scores mayores)
        MostrarTopScores(Raiz->der, font, y, lugar, limite);

        if (lugar <= limite)
        {
            // Dibujar línea del ranking
            al_draw_textf(font, al_map_rgb(255, 255, 0),
                WIDTH / 2, y, ALLEGRO_ALIGN_CENTER,
                "#%d  %s  %d pts  [D:%d A:%d F:%d]",
                lugar, Raiz->nombre, Raiz->score,
                Raiz->disparos, Raiz->aciertos, Raiz->fallos);
            y += 20;
            lugar++;
        }

        // Luego el subárbol izquierdo (scores menores)
        MostrarTopScores(Raiz->izq, font, y, lugar, limite);
    }
}

 
//  GuardarBST()
//  Serializa el árbol en pre-orden al archivo "stats.bin".
//  Pre-orden garantiza que al leer en el mismo orden se reconstruye
//  el árbol con la misma estructura sin necesidad de rebalancear.
void GuardarBST(NodoBST* Raiz, FILE* archivo)
{
    if (Raiz != NULL)
    {
        // Escribir nodo actual
        fwrite(Raiz, sizeof(NodoBST), 1, archivo);
        fflush(archivo);  // Forzar escritura al disco inmediatamente

        // Luego subárboles izquierdo y derecho
        GuardarBST(Raiz->izq, archivo);
        GuardarBST(Raiz->der, archivo);
    }
}

 
//  CargarBST()
//  Lee el archivo "stats.bin" y reconstruye el BST insertando cada nodo.
//  Como se guardó en pre-order, insertar en el mismo orden reconstruye
//  el árbol con la misma estructura original.
//  Se llama una sola vez al iniciar el programa.
void CargarBST(NodoBST*& Raiz)
{
    FILE* archivo = fopen("stats.bin", "rb");
    if (archivo == NULL) return;  // Primera ejecución, no hay historial aún

    NodoBST temp;
    // Leer nodo por nodo e insertar en el árbol
    while (fread(&temp, sizeof(NodoBST), 1, archivo) == 1)
    {
        Insertar(Raiz, temp.nombre, temp.disparos,
            temp.aciertos, temp.fallos, temp.score);
    }
    fclose(archivo);
}


int main() {

    // ── 1. Inicialización de Allegro y addons ─────────────────────────────────
    al_init();                    // Núcleo de Allegro (obligatorio primero)
    al_install_keyboard();        // Habilita eventos de teclado
    al_init_primitives_addon();   // Habilita dibujo de formas (círculos, rectángulos)
    al_init_font_addon();         // Habilita sistema de fuentes
    al_init_ttf_addon();          // Habilita fuentes TrueType (.ttf)

    // ── 2. Creación de recursos Allegro ───────────────────────────────────────
    ALLEGRO_DISPLAY* display = al_create_display(WIDTH, HEIGHT);
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    ALLEGRO_FONT* font = al_create_builtin_font();

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

    // ── Nombre del jugador estilo arcade (5 caracteres) ───────────────────────
    char nombreJugador[6] = "AAA";
    bool editandoNombre = false;
    int  letraNombre = 0;

    // ── BST de estadísticas ───────────────────────────────────────────────────
    NodoBST* raizBST = NULL;
    CargarBST(raizBST);

    // ── Listas enlazadas de entidades ─────────────────────────────────────────
    Bala* balas = NULL;
    Bala* balasEnemigas = NULL;
    Enemigo* enemigos = NULL;
    Moneda* monedas = NULL;
    Bot* bots = NULL;
    Missile* misiles = NULL;
    Jefe* jefes = NULL;

    // ── Sistema de niveles ────────────────────────────────────────────────────
    // nivelActual: nivel en curso en modo arcade
    // enemigosEliminados: contador de enemigos derrotados en el nivel actual
    // umbralNivel: cantidad de enemigos a eliminar para avanzar (50 o 64)
    // jefePendiente: al alcanzar el umbral en niveles 3 y 4, se activa
    // jefeVivo: hay un jefe actualmente en pantalla
    // tickTransicion: contador de ticks para la pantalla de transición entre niveles
    Nivel nivelActual = CITY;
    int   enemigosEliminados = 0;
    int   umbralNivel = obtenerUmbral(CITY);
    bool  jefePendiente = false;
    bool  jefeVivo = false;
    int   tickTransicion = 0;  // Cuenta ticks en estado TRANSICION

    // ── Dev menu ──────────────────────────────────────────────────────────────
    // Cheats activables con CTRL+I (invencibilidad) y CTRL+F (fast kill)
    // Solo funcionan durante el estado JUGANDO.
    bool cheatInvencible = false;  // Los golpes suman vida en vez de restarla
    bool cheatFastKill = false;  // Cada enemigo cuenta como 10 eliminados

    ALLEGRO_KEYBOARD_STATE keystate;
    srand(time(NULL));
    al_start_timer(timer);

    bool running = true;
    bool redraw = true;

    // =========================================================================
    //  GAME LOOP PRINCIPAL — arquitectura Cola de Eventos Allegro 5
    // =========================================================================
    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev);

        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) running = false;
        if (ev.type == ALLEGRO_EVENT_TIMER)         redraw = true;

        // =====================================================================
        //  ESTADO: MENU
        //  ENTER=jugar (selección libre), SPACE=modo arcade, S=stats, N=nombre
        //  ↑/↓=dificultad, ←/→=modo (solo aplican en modo selección)
        // =====================================================================
        if (estado == MENU) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
                if (editandoNombre) {
                    // ── Edición de nombre estilo arcade ───────────────────
                    if (ev.keyboard.keycode == ALLEGRO_KEY_UP) {
                        nombreJugador[letraNombre]++;
                        if (nombreJugador[letraNombre] > 'Z') nombreJugador[letraNombre] = 'A';
                    }
                    if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN) {
                        nombreJugador[letraNombre]--;
                        if (nombreJugador[letraNombre] < 'A') nombreJugador[letraNombre] = 'Z';
                    }
                    if (ev.keyboard.keycode == ALLEGRO_KEY_RIGHT && letraNombre < 4) letraNombre++;
                    if (ev.keyboard.keycode == ALLEGRO_KEY_LEFT && letraNombre > 0) letraNombre--;
                    if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                        nombreJugador[5] = '\0';
                        editandoNombre = false;
                        letraNombre = 0;
                    }
                }
                else {
                    // ── Controles normales del menú ────────────────────────
                    // ENTER: modo selección libre (infinito, con dificultad elegida)
                    if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                        nivelActual = CITY;
                        enemigosEliminados = 0;
                        umbralNivel = obtenerUmbral(CITY);
                        jefePendiente = false;
                        jefeVivo = false;
                        estado = JUGANDO;
                    }
                    // SPACE: modo arcade (4 niveles consecutivos, dificultad progresiva)
                    if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                        nivelActual = CITY;
                        enemigosEliminados = 0;
                        umbralNivel = obtenerUmbral(CITY);
                        jefePendiente = false;
                        jefeVivo = false;
                        dificultad = DIF_EASIEST;  // Arcade siempre empieza en fácil
                        modo = ORIGINAL;     // Arcade siempre en Original
                        estado = JUGANDO;
                    }
                    if (ev.keyboard.keycode == ALLEGRO_KEY_N) editandoNombre = true;
                    if (ev.keyboard.keycode == ALLEGRO_KEY_S) estado = STATS;

                    // Dificultad y modo solo en selección libre
                    if (ev.keyboard.keycode == ALLEGRO_KEY_UP && dificultad < DIF_INSANE)  dificultad = (Dificultad)(dificultad + 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN && dificultad > DIF_EASIEST) dificultad = (Dificultad)(dificultad - 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_RIGHT && modo < MASSACRE) modo = (ModoJuego)(modo + 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_LEFT && modo > ORIGINAL) modo = (ModoJuego)(modo - 1);
                }
            }
        }

        // =====================================================================
        //  ESTADO: JUGANDO
        // =====================================================================
        else if (estado == JUGANDO) {

            ConfigDificultad cfg = configs[dificultad];
            float multVel = (modo == SLUDGE ? 0.5f : (modo == MANIAC ? 1.5f : 1.0f));
            int   multSpawn = (modo == MASSACRE ? 2 : 1);

            if (ev.type == ALLEGRO_EVENT_TIMER) {

                // ── Dev menu: CTRL+I y CTRL+F ──────────────────────────────
                al_get_keyboard_state(&keystate);
                if (al_key_down(&keystate, ALLEGRO_KEY_LCTRL) || al_key_down(&keystate, ALLEGRO_KEY_RCTRL)) {
                    if (ev.type == ALLEGRO_EVENT_TIMER) {
                        // Se detectan en KEY_DOWN más abajo, aquí solo se leen
                    }
                }

                // ── Movimiento del jugador ─────────────────────────────────
                if (al_key_down(&keystate, ALLEGRO_KEY_LEFT))  jugador.x -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_RIGHT)) jugador.x += jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_UP))    jugador.y -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_DOWN))  jugador.y += jugador.vel;

                jugador.x = std::max(10.f, std::min((float)WIDTH - 10, jugador.x));
                jugador.y = std::max(HEIGHT * 0.4f, std::min((float)HEIGHT - 10, jugador.y));

                // ── Spawn de enemigos ──────────────────────────────────────
                // No spawnear si el jefe está pendiente o vivo
                if (!jefePendiente && !jefeVivo) {
                    if (rand() % cfg.spawnRate == 0) {
                        for (int i = 0; i < multSpawn; i++) {
                            Enemigo* Nuevo = new Enemigo;
                            Nuevo->x = (float)(rand() % WIDTH);
                            Nuevo->y = 0;
                            Nuevo->vel = cfg.velEnemigo * multVel;
                            Nuevo->siguiente = NULL;
                            AgregarEnemigo(enemigos, Nuevo);
                        }
                    }
                }

                // ── Movimiento de enemigos + patrones de disparo por nivel ─
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        Aux->y += Aux->vel;

                        // Patrón de disparo según nivel actual
                        if (nivelActual == CITY) {
                            // Patrón 1: bala simple recta hacia abajo
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
                        }
                        else if (nivelActual == OCEAN) {
                            // Patrón 2: ráfaga de 3 balas seguidas con pequeño offset vertical
                            if (rand() % 120 == 0) {
                                for (int r = 0; r < 3; r++) {
                                    Bala* NuevaBala = new Bala;
                                    NuevaBala->x = Aux->x;
                                    NuevaBala->y = Aux->y + r * 8;  // Offset para simular ráfaga
                                    NuevaBala->vel = cfg.velBala * multVel * 1.2f;
                                    NuevaBala->hit = false;
                                    NuevaBala->tipo = 0;
                                    NuevaBala->siguiente = NULL;
                                    AgregarBalaEnemiga(balasEnemigas, NuevaBala);
                                }
                            }
                        }
                        else if (nivelActual == VOLCANO) {
                            // Patrón 3: abanico de 3 balas (centro + dos diagonales)
                            if (rand() % 80 == 0) {
                                // Bala central recta
                                Bala* Centro = new Bala;
                                Centro->x = Aux->x;
                                Centro->y = Aux->y;
                                Centro->vel = cfg.velBala * multVel;
                                Centro->hit = false;
                                Centro->tipo = 4;  // tipo 4 = bala grande (renderizado especial)
                                Centro->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, Centro);

                                // Bala diagonal izquierda
                                Bala* Izq = new Bala;
                                Izq->x = Aux->x - 6;
                                Izq->y = Aux->y;
                                Izq->vel = cfg.velBala * multVel * 0.9f;
                                Izq->hit = false;
                                Izq->tipo = 4;
                                Izq->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, Izq);

                                // Bala diagonal derecha
                                Bala* Der = new Bala;
                                Der->x = Aux->x + 6;
                                Der->y = Aux->y;
                                Der->vel = cfg.velBala * multVel * 0.9f;
                                Der->hit = false;
                                Der->tipo = 4;
                                Der->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, Der);
                            }
                        }
                        else if (nivelActual == SPACE) {
                            // Patrón 4: abanico mejorado — 5 balas, mayor velocidad y cadencia
                            if (rand() % 60 == 0) {
                                for (int r = -2; r <= 2; r++) {
                                    Bala* NuevaBala = new Bala;
                                    NuevaBala->x = Aux->x + r * 8;
                                    NuevaBala->y = Aux->y;
                                    NuevaBala->vel = cfg.velBala * multVel * 1.4f;
                                    NuevaBala->hit = false;
                                    NuevaBala->tipo = 4;
                                    NuevaBala->siguiente = NULL;
                                    AgregarBalaEnemiga(balasEnemigas, NuevaBala);
                                }
                            }
                        }

                        if (Aux->y > HEIGHT) EliminarEnemigo(enemigos, Aux);
                        Aux = Siguiente;
                    }
                }

                // ── Movimiento del jefe ────────────────────────────────────
                // El jefe se mueve en zigzag horizontal en la parte superior
                // y dispara un abanico amplio cada ciertos ticks.
                {
                    Jefe* Aux = jefes;
                    while (Aux != NULL) {
                        Jefe* Siguiente = Aux->siguiente;

                        // Movimiento zigzag horizontal
                        Aux->x += Aux->velX;
                        if (Aux->x <= 30 || Aux->x >= WIDTH - 30)
                            Aux->velX = -Aux->velX;  // Rebotar en los bordes

                        // Disparo del jefe: abanico de 5 balas cada 60 ticks
                        // Fase 2 (mitad de vida) dispara 7 balas más rápidas
                        int numBalas = (Aux->fase == 1 ? 5 : 7);
                        float velBala = (Aux->fase == 1 ? 4.0f : 6.0f);

                        if (rand() % 60 == 0) {
                            for (int r = -(numBalas / 2); r <= numBalas / 2; r++) {
                                Bala* NuevaBala = new Bala;
                                NuevaBala->x = Aux->x + r * 12;
                                NuevaBala->y = Aux->y + 30;
                                NuevaBala->vel = velBala;
                                NuevaBala->hit = false;
                                NuevaBala->tipo = 4;
                                NuevaBala->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, NuevaBala);
                            }
                        }

                        Aux = Siguiente;
                    }
                }

                // ── Movimiento de proyectiles del jugador ──────────────────
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        Aux->y -= Aux->vel;
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
                        Aux->y += Aux->vel;
                        if (Aux->y > HEIGHT) EliminarBalaEnemiga(balasEnemigas, Aux);
                        Aux = Siguiente;
                    }
                }

                // ── Movimiento de misiles (homing básico) ──────────────────
                {
                    Missile* Aux = misiles;
                    while (Aux != NULL) {
                        Missile* Siguiente = Aux->siguiente;
                        if (enemigos != NULL) {
                            if (Aux->x < enemigos->x) Aux->x += 2;
                            if (Aux->x > enemigos->x) Aux->x -= 2;
                        }
                        Aux->y -= 3;
                        if (Aux->y < 0) EliminarMissile(misiles, Aux);
                        Aux = Siguiente;
                    }
                }

                // ── Colisiones: bala del jugador vs enemigo ────────────────
                {
                    Bala* AuxBala = balas;
                    while (AuxBala != NULL) {
                        Bala* SiguienteBala = AuxBala->siguiente;
                        bool  impacto = false;

                        Enemigo* AuxEnemigo = enemigos;
                        while (AuxEnemigo != NULL && !impacto) {
                            Enemigo* SiguienteEnemigo = AuxEnemigo->siguiente;
                            if (colision(AuxBala->x, AuxBala->y, AuxEnemigo->x, AuxEnemigo->y, 18)) {
                                Moneda* NuevaMoneda = new Moneda;
                                NuevaMoneda->x = AuxEnemigo->x;
                                NuevaMoneda->y = AuxEnemigo->y;
                                NuevaMoneda->tipo = rand() % 2;
                                NuevaMoneda->siguiente = NULL;
                                AgregarMoneda(monedas, NuevaMoneda);

                                EliminarEnemigo(enemigos, AuxEnemigo);
                                EliminarBala(balas, AuxBala);
                                score += 10;
                                aciertos++;

                                // cheatFastKill: cada enemigo cuenta como 10
                                enemigosEliminados += (cheatFastKill ? 10 : 1);
                                impacto = true;
                            }
                            AuxEnemigo = SiguienteEnemigo;
                        }
                        AuxBala = SiguienteBala;
                    }
                }

                // ── Colisiones: bala del jugador vs jefe ───────────────────
                {
                    Bala* AuxBala = balas;
                    while (AuxBala != NULL) {
                        Bala* SiguienteBala = AuxBala->siguiente;
                        bool  impacto = false;

                        Jefe* AuxJefe = jefes;
                        while (AuxJefe != NULL && !impacto) {
                            Jefe* SiguienteJefe = AuxJefe->siguiente;
                            if (colision(AuxBala->x, AuxBala->y, AuxJefe->x, AuxJefe->y, 40)) {
                                AuxJefe->vida--;
                                EliminarBala(balas, AuxBala);
                                score += 50;
                                aciertos++;
                                impacto = true;

                                // Cambiar a fase 2 al perder la mitad de vida
                                if (AuxJefe->vida <= 5 && AuxJefe->fase == 1)
                                    AuxJefe->fase = 2;

                                // Jefe derrotado
                                if (AuxJefe->vida <= 0) {
                                    score += 500;
                                    EliminarJefe(jefes, AuxJefe);
                                    jefeVivo = false;

                                    // Avanzar al siguiente nivel
                                    if (nivelActual == VOLCANO) {
                                        nivelActual = SPACE;
                                        dificultad = DIF_VERY_HARD;
                                        umbralNivel = obtenerUmbral(SPACE);
                                        enemigosEliminados = 0;
                                        jefePendiente = false;
                                        estado = TRANSICION;
                                        tickTransicion = 0;
                                    }
                                    else if (nivelActual == SPACE) {
                                        // Último nivel completado → victoria
                                        Insertar(raizBST, nombreJugador, disparos, aciertos, fallos, score);
                                        FILE* archivo = fopen("stats.bin", "ab");
                                        if (archivo != NULL) { GuardarBST(raizBST, archivo); fclose(archivo); }
                                        estado = GAME_OVER;
                                    }
                                }
                            }
                            AuxJefe = SiguienteJefe;
                        }
                        AuxBala = SiguienteBala;
                    }
                }

                // ── Lógica especial: IMPLOSION ─────────────────────────────
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
                        Aux->y += 2;
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            score += (Aux->tipo == 0 ? 5 : 10);
                            EliminarMoneda(monedas, Aux);
                        }
                        else if (Aux->y > HEIGHT) EliminarMoneda(monedas, Aux);
                        Aux = Siguiente;
                    }
                }

                // ── Colisiones: balas enemigas vs jugador ──────────────────
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 10)) {
                            // cheatInvencible: los golpes suman vida en vez de restarla
                            if (cheatInvencible) jugador.vidas++;
                            else                 jugador.vidas--;
                            EliminarBalaEnemiga(balasEnemigas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // ── Progresión de nivel por enemigos eliminados ────────────
                if (!jefePendiente && !jefeVivo && enemigosEliminados >= umbralNivel) {
                    if (nivelActual == CITY || nivelActual == OCEAN) {
                        // Sin jefe: transición directa al siguiente nivel
                        if (nivelActual == CITY) {
                            nivelActual = OCEAN;
                            dificultad = DIF_NORMAL;
                            umbralNivel = obtenerUmbral(OCEAN);
                            enemigosEliminados = 0;
                        }
                        else {
                            nivelActual = VOLCANO;
                            dificultad = DIF_HARD;
                            umbralNivel = obtenerUmbral(VOLCANO);
                            enemigosEliminados = 0;
                        }
                        // Limpiar enemigos en pantalla al cambiar de nivel
                        DestruirEnemigos(enemigos);
                        DestruirBalasEnemigas(balasEnemigas);
                        estado = TRANSICION;
                        tickTransicion = 0;
                    }
                    else {
                        // VOLCANO y SPACE: spawnear jefe al alcanzar el umbral
                        jefePendiente = true;
                        DestruirEnemigos(enemigos);  // Limpiar enemigos normales
                        DestruirBalasEnemigas(balasEnemigas);

                        Jefe* NuevoJefe = new Jefe;
                        NuevoJefe->x = WIDTH / 2;
                        NuevoJefe->y = 60;
                        NuevoJefe->velX = (nivelActual == SPACE ? 3.0f : 2.0f);
                        NuevoJefe->vida = 10;
                        NuevoJefe->fase = 1;
                        NuevoJefe->vivo = true;
                        NuevoJefe->siguiente = NULL;
                        AgregarJefe(jefes, NuevoJefe);
                        jefePendiente = false;
                        jefeVivo = true;
                    }
                }

                // ── Transición a GAME_OVER ─────────────────────────────────
                if (jugador.vidas <= 0) {
                    Insertar(raizBST, nombreJugador, disparos, aciertos, fallos, score);
                    FILE* archivo = fopen("stats.bin", "ab");
                    if (archivo != NULL) { GuardarBST(raizBST, archivo); fclose(archivo); }
                    estado = GAME_OVER;
                }
            }

            // ── Input: cheats y disparo ────────────────────────────────────
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {

                // Dev menu: CTRL+I = invencibilidad, CTRL+F = fast kill
                al_get_keyboard_state(&keystate);
                bool ctrl = al_key_down(&keystate, ALLEGRO_KEY_LCTRL) ||
                    al_key_down(&keystate, ALLEGRO_KEY_RCTRL);
                if (ctrl && ev.keyboard.keycode == ALLEGRO_KEY_I)
                    cheatInvencible = !cheatInvencible;
                if (ctrl && ev.keyboard.keycode == ALLEGRO_KEY_F)
                    cheatFastKill = !cheatFastKill;

                // Teclas 0-6: selección de arma secundaria
                if (ev.keyboard.keycode == ALLEGRO_KEY_0) armaActual = ARMA_NORMAL;
                if (ev.keyboard.keycode == ALLEGRO_KEY_1) armaActual = BOTS;
                if (ev.keyboard.keycode == ALLEGRO_KEY_2) armaActual = VELOCITY;
                if (ev.keyboard.keycode == ALLEGRO_KEY_3) armaActual = SPREAD;
                if (ev.keyboard.keycode == ALLEGRO_KEY_4) armaActual = MISSILE;
                if (ev.keyboard.keycode == ALLEGRO_KEY_5) armaActual = IMPLOSION;
                if (ev.keyboard.keycode == ALLEGRO_KEY_6) armaActual = PLASMA;

                if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                    disparos++;
                    switch (armaActual) {
                    case ARMA_NORMAL: {
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
                        Missile* Nuevo = new Missile;
                        Nuevo->x = jugador.x;
                        Nuevo->y = jugador.y;
                        Nuevo->siguiente = NULL;
                        AgregarMissile(misiles, Nuevo);
                        break;
                    }
                    case BOTS: {
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
                    default: break;
                    }
                }
            }
        }

        // =====================================================================
        //  ESTADO: TRANSICION
        //  Pantalla breve (3 segundos = 180 ticks) que muestra el nombre
        //  del nivel siguiente antes de continuar.
        //  No requiere input — avanza automáticamente.
        // =====================================================================
        else if (estado == TRANSICION) {
            if (ev.type == ALLEGRO_EVENT_TIMER) {
                tickTransicion++;
                if (tickTransicion >= 180) {  // 3 segundos a 60 FPS
                    tickTransicion = 0;
                    estado = JUGANDO;
                }
            }
        }

        // =====================================================================
        //  ESTADO: GAME_OVER
        // =====================================================================
        else if (estado == GAME_OVER) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                jugador.vidas = 3;
                jugador.x = WIDTH / 2;
                jugador.y = HEIGHT - 50;
                score = 0;
                disparos = aciertos = fallos = 0;

                nivelActual = CITY;
                enemigosEliminados = 0;
                umbralNivel = obtenerUmbral(CITY);
                jefePendiente = false;
                jefeVivo = false;
                cheatInvencible = false;
                cheatFastKill = false;

                DestruirEnemigos(enemigos);
                DestruirBalas(balas);
                DestruirBalasEnemigas(balasEnemigas);
                DestruirMonedas(monedas);
                DestruirMissiles(misiles);
                DestruirBots(bots);
                DestruirJefes(jefes);

                estado = MENU;
            }
        }

        // =====================================================================
        //  ESTADO: STATS
        // =====================================================================
        else if (estado == STATS) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                estado = MENU;
        }

        // =====================================================================
        //  RENDERIZADO
        // =====================================================================
        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // ── Renderizado: MENU ──────────────────────────────────────────
            if (estado == MENU) {
                if (editandoNombre) {
                    al_draw_text(font, al_map_rgb(255, 255, 0),
                        WIDTH / 2, HEIGHT / 2 - 80,
                        ALLEGRO_ALIGN_CENTER, "INGRESA TU NOMBRE:");
                    for (int i = 0; i < 5; i++) {
                        ALLEGRO_COLOR c = (i == letraNombre)
                            ? al_map_rgb(0, 255, 255)
                            : al_map_rgb(255, 255, 255);
                        al_draw_textf(font, c, WIDTH / 2 - 20 + i * 10, HEIGHT / 2 - 40, 0, "%c", nombreJugador[i]);
                    }
                    al_draw_text(font, al_map_rgb(150, 150, 150),
                        WIDTH / 2, HEIGHT / 2,
                        ALLEGRO_ALIGN_CENTER, "UP/DOWN letra  LEFT/RIGHT mover  ENTER confirmar");
                }
                else {
                    al_draw_textf(font, al_map_rgb(0, 255, 255),
                        WIDTH / 2, HEIGHT / 2 - 100,
                        ALLEGRO_ALIGN_CENTER, "Jugador: %s", nombreJugador);
                    al_draw_text(font, al_map_rgb(255, 255, 0),
                        WIDTH / 2, HEIGHT / 2 - 70,
                        ALLEGRO_ALIGN_CENTER, "XOP");
                    al_draw_text(font, al_map_rgb(255, 255, 255),
                        WIDTH / 2, HEIGHT / 2 - 40,
                        ALLEGRO_ALIGN_CENTER, "ENTER = Seleccion libre  |  SPACE = Modo Arcade");
                    al_draw_text(font, al_map_rgb(255, 255, 255),
                        WIDTH / 2, HEIGHT / 2 - 20,
                        ALLEGRO_ALIGN_CENTER, "S = Stats  |  N = Nombre");
                    al_draw_textf(font, al_map_rgb(200, 200, 200),
                        WIDTH / 2, HEIGHT / 2 + 10,
                        ALLEGRO_ALIGN_CENTER, "Dificultad (sel. libre): %s", nombresDif[(int)dificultad]);
                    al_draw_textf(font, al_map_rgb(200, 200, 200),
                        WIDTH / 2, HEIGHT / 2 + 30,
                        ALLEGRO_ALIGN_CENTER, "Modo (sel. libre): %s", nombresModo[modo]);
                }
            }

            // ── Renderizado: JUGANDO ───────────────────────────────────────
            else if (estado == JUGANDO) {

                // HUD
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 10, 0, "Score: %d", score);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 30, 0, "Vidas: %d", jugador.vidas);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 50, 0, "Arma: %s", nombresArma[armaActual]);
                al_draw_textf(font, al_map_rgb(0, 255, 255), 10, 70, 0, "Jugador: %s", nombreJugador);
                al_draw_textf(font, al_map_rgb(200, 200, 200), 10, 90, 0, "Nivel: %s",
                    nivelActual == CITY ? "CITY" :
                    nivelActual == OCEAN ? "OCEAN" :
                    nivelActual == VOLCANO ? "VOLCANO" : "SPACE");
                al_draw_textf(font, al_map_rgb(200, 200, 200), 10, 110, 0,
                    "Enemigos: %d/%d", enemigosEliminados, umbralNivel);

                // Indicadores de cheats activos
                if (cheatInvencible)
                    al_draw_text(font, al_map_rgb(255, 50, 50),
                        WIDTH - 10, 10, ALLEGRO_ALIGN_RIGHT, "[INV]");
                if (cheatFastKill)
                    al_draw_text(font, al_map_rgb(255, 50, 50),
                        WIDTH - 10, 30, ALLEGRO_ALIGN_RIGHT, "[FK]");

                // Barra de vida del jefe
                if (jefeVivo && jefes != NULL) {
                    int vidaMax = 10;
                    int vidaAct = jefes->vida;
                    float barW = 300.0f;
                    float barX = (WIDTH - barW) / 2;
                    float barY = HEIGHT - 20;
                    al_draw_filled_rectangle(barX, barY, barX + barW, barY + 12, al_map_rgb(80, 0, 0));
                    al_draw_filled_rectangle(barX, barY, barX + barW * vidaAct / vidaMax, barY + 12, al_map_rgb(220, 0, 0));
                    al_draw_text(font, al_map_rgb(255, 255, 255),
                        WIDTH / 2, barY - 14, ALLEGRO_ALIGN_CENTER, "JEFE");
                }

                // Nave del jugador: rectángulo verde
                al_draw_filled_rectangle(jugador.x - 10, jugador.y - 10,
                    jugador.x + 10, jugador.y + 10,
                    al_map_rgb(0, 255, 0));

                // Balas del jugador
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        if (Aux->tipo == 2) {
                            al_draw_filled_circle(Aux->x, Aux->y, 6, al_map_rgba(0, 255, 0, 120));
                            al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgba(200, 255, 200, 220));
                        }
                        else {
                            ALLEGRO_COLOR color;
                            switch (Aux->tipo) {
                            case 0: color = al_map_rgb(255, 255, 0); break;
                            case 1: color = al_map_rgb(255, 0, 0); break;
                            case 3: color = al_map_rgb(0, 255, 255); break;
                            default:color = al_map_rgb(255, 255, 255);
                            }
                            al_draw_filled_circle(Aux->x, Aux->y, 3, color);
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // Balas enemigas (tipo 4 = bala grande naranja)
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        if (Aux->tipo == 4)
                            al_draw_filled_circle(Aux->x, Aux->y, 6, al_map_rgb(255, 140, 0));
                        else
                            al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Enemigos
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        al_draw_filled_rectangle(Aux->x - 10, Aux->y - 10,
                            Aux->x + 10, Aux->y + 10,
                            al_map_rgb(255, 0, 0));
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            if (cheatInvencible) jugador.vidas++;
                            else                 jugador.vidas--;
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // Jefe: rectángulo grande morado
                {
                    Jefe* Aux = jefes;
                    while (Aux != NULL) {
                        al_draw_filled_rectangle(Aux->x - 40, Aux->y - 25,
                            Aux->x + 40, Aux->y + 25,
                            al_map_rgb(180, 0, 220));
                        // Indicador de fase
                        if (Aux->fase == 2)
                            al_draw_filled_rectangle(Aux->x - 40, Aux->y - 25,
                                Aux->x + 40, Aux->y + 25,
                                al_map_rgb(220, 0, 100));
                        Aux = Aux->siguiente;
                    }
                }

                // Monedas
                {
                    Moneda* Aux = monedas;
                    while (Aux != NULL) {
                        al_draw_filled_circle(Aux->x, Aux->y, 4, al_map_rgb(0, 0, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Misiles
                {
                    Missile* Aux = misiles;
                    while (Aux != NULL) {
                        al_draw_filled_circle(Aux->x, Aux->y, 4, al_map_rgb(255, 0, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Drones BOTS
                {
                    Bot* Aux = bots;
                    while (Aux != NULL) {
                        al_draw_filled_circle(jugador.x + Aux->ox, jugador.y + Aux->oy, 4, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // VELOCITY CANNON
                if (armaActual == VELOCITY) {
                    al_draw_line(jugador.x, jugador.y, jugador.x, 0, al_map_rgb(255, 255, 255), 3);
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        if (fabs(Aux->x - jugador.x) < 10) {
                            score += 20;
                            enemigosEliminados += (cheatFastKill ? 10 : 1);
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;
                    }
                }
            }

            // ── Renderizado: TRANSICION ──────────────────────────────
            else if (estado == TRANSICION) {
                const char* nombreNivel =
                    nivelActual == CITY ? "CITY — Easy" :
                    nivelActual == OCEAN ? "OCEAN — Medium" :
                    nivelActual == VOLCANO ? "VOLCANO — Hard" : "SPACE — Maniac";

                al_draw_text(font, al_map_rgb(255, 255, 0),
                    WIDTH / 2, HEIGHT / 2 - 20,
                    ALLEGRO_ALIGN_CENTER, "NIVEL COMPLETADO");
                al_draw_textf(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2 + 10,
                    ALLEGRO_ALIGN_CENTER, "Siguiente: %s", nombreNivel);
            }

            // ── Renderizado: GAME_OVER ─────────────────────────────────────
            else if (estado == GAME_OVER) {
                al_draw_text(font, al_map_rgb(255, 0, 0),
                    WIDTH / 2, HEIGHT / 2 - 40,
                    ALLEGRO_ALIGN_CENTER, "GAME OVER");
                al_draw_textf(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2,
                    ALLEGRO_ALIGN_CENTER, "Jugador: %s  |  Score: %d", nombreJugador, score);
                al_draw_textf(font, al_map_rgb(150, 150, 150),
                    WIDTH / 2, HEIGHT / 2 + 30,
                    ALLEGRO_ALIGN_CENTER, "D:%d  A:%d  F:%d", disparos, aciertos, fallos);
                al_draw_text(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2 + 60,
                    ALLEGRO_ALIGN_CENTER, "ENTER para volver al menu");
            }

            // ── Renderizado: STATS ─────────────────────────────────────────
            else if (estado == STATS) {
                al_draw_text(font, al_map_rgb(255, 255, 0),
                    WIDTH / 2, 20, ALLEGRO_ALIGN_CENTER, "TOP SCORES");
                al_draw_text(font, al_map_rgb(150, 150, 150),
                    WIDTH / 2, 40, ALLEGRO_ALIGN_CENTER, "ESC para volver");
                if (raizBST == NULL) {
                    al_draw_text(font, al_map_rgb(150, 150, 150),
                        WIDTH / 2, HEIGHT / 2,
                        ALLEGRO_ALIGN_CENTER, "Sin partidas registradas aun");
                }
                else {
                    int y = 80;
                    int lugar = 1;
                    MostrarTopScores(raizBST, font, y, lugar, 10);
                }
            }

            al_flip_display();
        }

    } // ── FIN DEL GAME LOOP ────────────────────────────────────────────────

    // ── Limpieza de recursos al salir ─────────────────────────────────────────
    DestruirBST(raizBST);
    DestruirBalas(balas);
    DestruirBalasEnemigas(balasEnemigas);
    DestruirEnemigos(enemigos);
    DestruirMonedas(monedas);
    DestruirMissiles(misiles);
    DestruirBots(bots);
    DestruirJefes(jefes);

    al_destroy_display(display);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}