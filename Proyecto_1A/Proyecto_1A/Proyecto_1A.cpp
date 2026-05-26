// PROYECTO 1A — VIDEOJUEGO XOP
// Curso: Estructuras de Datos IC2001
// Réplica del bullet hell XOP usando Allegro 5. Implementa nave del jugador,
// enemigos, 6 armas secundarias, DAL Coins, sistema de niveles con modo arcade
// e infinito, jefes, Reflect Shield, Velocity Cannon con cooldown,
// estadísticas persistentes en BST con identificador de modo, y sprites.

#define _CRT_SECURE_NO_WARNINGS

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_native_dialog.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cmath>

const int   WIDTH = 1024;
const int   HEIGHT = 768;
const float FPS = 60;

// --- Enumeraciones ---

enum EstadoJuego { MENU, JUGANDO, GAME_OVER, STATS, TRANSICION };
enum Nivel { CITY, OCEAN, VOLCANO, SPACE };

// Dificultad escalada (0=más fácil, 7=más difícil). Indexa configs[].
enum Dificultad {
    DIF_EASIEST, DIF_EASY, DIF_NORMAL, DIF_HARD,
    DIF_VERY_HARD, DIF_EXTREME, DIF_NIGHTMARE, DIF_INSANE
};

enum ModoJuego { ORIGINAL, SLUDGE, MANIAC, MASSACRE };

// Armas del jugador. Se seleccionan con 0-6 y se disparan con ESPACIO.
enum Arma { ARMA_NORMAL, BOTS, VELOCITY, SPREAD, MISSILE, IMPLOSION, PLASMA };

// --- Structs de entidades ---

// 'hit' evita contar fallos dobles. 'tipo' define sprite/color.
// 'velX' permite dispersión horizontal; 0 en balas rectas.
struct Bala {
    float x, y, vel, velX;
    bool hit;
    int tipo;
    Bala* siguiente;
};

struct Jugador { float x, y, vel; int vidas; };

// 'tipo': 0=Enemie (City/Ocean/Volcano), 1=ArchEnemie (Space y modo libre Extreme+)
// 'velX': movimiento lateral activo en Space y dificultades Extreme/Nightmare/Insane
struct Enemigo {
    float x, y, vel, velX;
    int tipo;
    Enemigo* siguiente;
};

// tipo 0 = DAL Coin azul (+5 pts), tipo 1 = DAL Coin morada (+10 pts)
// tipo 2 = Reflect Shield drop (otorga un RS al jugador)
struct Moneda {
    float x, y;
    int tipo;
    Moneda* siguiente;
};

// 'ox', 'oy' son offsets relativos al jugador
struct Bot {
    float ox, oy;
    Bot* siguiente;
};

struct Missile {
    float x, y;
    Missile* siguiente;
};

// Aparece en VOLCANO y SPACE al alcanzar el umbral de eliminados.
// 'fase' cambia a 2 al llegar a la mitad de vida, aumentando agresividad.
struct Jefe {
    float x, y, velX;
    int vida, fase;
    bool vivo;
    Jefe* siguiente;
};

// Animación de explosión: 7 frames, se reproduce una vez y desaparece.
struct Explosion {
    float x, y;
    int frame, tick;
    bool terminada;
    Explosion* siguiente;
};

// --- Sistema de assets ---
// Todos los bitmaps se cargan una vez al inicio y se reutilizan en cada frame.
struct Assets {
    // Backgrounds
    ALLEGRO_BITMAP* bgMenu, * bgCity, * bgOcean, * bgVolcano, * bgSpace, * bgStats, * bgLibre;

    // UI
    ALLEGRO_BITMAP* logo; // 540x480, menú principal
    ALLEGRO_BITMAP* scoreUI; // 81x25
    ALLEGRO_BITMAP* vidaUI; // 17x17, corazón
    ALLEGRO_BITMAP* rsOn; // 48x48, RS disponible
    ALLEGRO_BITMAP* rsOff; // 48x48, RS no disponible
    ALLEGRO_BITMAP* vcOn; // 26x24, Velocity disponible
    ALLEGRO_BITMAP* vcOff; // 26x24, Velocity en cooldown

    // Entidades animadas — 2 frames
    ALLEGRO_BITMAP* player[2];
    ALLEGRO_BITMAP* enemie[2];
    ALLEGRO_BITMAP* archEnemie[2];
    ALLEGRO_BITMAP* boss[2];
    ALLEGRO_BITMAP* dron[2];

    // Explosion — 7 frames, no loop
    ALLEGRO_BITMAP* explosion[7];

    // Items
    ALLEGRO_BITMAP* coin; // DAL Coin azul 18x18
    ALLEGRO_BITMAP* xcoin; // DAL Coin morada 18x18
    ALLEGRO_BITMAP* rsDrop; // Reflect Shield drop 24x24

    // Bullets — estáticos
    ALLEGRO_BITMAP* bulletPlayer; // 16x17
    ALLEGRO_BITMAP* bulletEnemie; // 16x17
    ALLEGRO_BITMAP* missileSpr; // 16x16
    ALLEGRO_BITMAP* ray; // 80x768, Velocity Cannon

    // Reflect Shield — estático con transparencia
    ALLEGRO_BITMAP* reflectShield; // 74x70
};

// --- Anim2 ---
// Animación de 2 frames en loop. ticksPorFrame=15 → ~4 ciclos/seg a 60 FPS.
struct Anim2 { int frame, tick, ticksPorFrame; };

void ActualizarAnim2(Anim2& a) {
    a.tick++;
    if (a.tick >= a.ticksPorFrame) { a.tick = 0; a.frame = 1 - a.frame; }
}

// Carga todos los bitmaps desde disco. Punteros NULL se manejan con placeholders.
bool CargarAssets(Assets& a) {
    a.bgMenu = al_load_bitmap("Assets/Background/mainmenu.png");
    a.bgCity = al_load_bitmap("Assets/Background/city.png");
    a.bgOcean = al_load_bitmap("Assets/Background/ocean.png");
    a.bgVolcano = al_load_bitmap("Assets/Background/volcano.png");
    a.bgSpace = al_load_bitmap("Assets/Background/space.png");
    a.bgStats = al_load_bitmap("Assets/Background/stats.png");
    a.bgLibre = al_load_bitmap("Assets/Background/supernova.png");
    a.logo = al_load_bitmap("Assets/UI/logo.png");
    a.scoreUI = al_load_bitmap("Assets/UI/score.png");
    a.vidaUI = al_load_bitmap("Assets/UI/vidas.png");
    a.rsOn = al_load_bitmap("Assets/UI/rson.png");
    a.rsOff = al_load_bitmap("Assets/UI/rsoff.png");
    a.vcOn = al_load_bitmap("Assets/UI/vcon.png");
    a.vcOff = al_load_bitmap("Assets/UI/vcoff.png");
    a.player[0] = al_load_bitmap("Assets/Player/player1.png");
    a.player[1] = al_load_bitmap("Assets/Player/player2.png");
    a.enemie[0] = al_load_bitmap("Assets/Enemie/enemie1.png");
    a.enemie[1] = al_load_bitmap("Assets/Enemie/enemie2.png");
    a.archEnemie[0] = al_load_bitmap("Assets/ArchEnemie/archenemie1.png");
    a.archEnemie[1] = al_load_bitmap("Assets/ArchEnemie/archenemie2.png");
    a.boss[0] = al_load_bitmap("Assets/Boss/boss1.png");
    a.boss[1] = al_load_bitmap("Assets/Boss/boss2.png");
    a.dron[0] = al_load_bitmap("Assets/Dron/dron1.png");
    a.dron[1] = al_load_bitmap("Assets/Dron/dron2.png");
    for (int i = 0; i < 7; i++) {
        char path[64];
        sprintf(path, "Assets/Explosion/explosion%d.png", i + 1);
        a.explosion[i] = al_load_bitmap(path);
    }
    a.coin = al_load_bitmap("Assets/Items/coin.png");
    a.xcoin = al_load_bitmap("Assets/Items/xcoin.png");
    a.rsDrop = al_load_bitmap("Assets/Items/rsdrop.png");
    a.bulletPlayer = al_load_bitmap("Assets/Bullet/bulletplayer.png");
    a.bulletEnemie = al_load_bitmap("Assets/Bullet/bulletenemie.png");
    a.missileSpr = al_load_bitmap("Assets/Bullet/missile.png");
    a.ray = al_load_bitmap("Assets/Bullet/ray.png");
    a.reflectShield = al_load_bitmap("Assets/Reflect/reflect.png");
    return true;
}

void DestruirAssets(Assets& a) {
    al_destroy_bitmap(a.bgMenu); al_destroy_bitmap(a.bgCity);
    al_destroy_bitmap(a.bgOcean); al_destroy_bitmap(a.bgVolcano);
    al_destroy_bitmap(a.bgSpace); al_destroy_bitmap(a.bgStats);
    al_destroy_bitmap(a.bgLibre); al_destroy_bitmap(a.logo);
    al_destroy_bitmap(a.scoreUI); al_destroy_bitmap(a.vidaUI);
    al_destroy_bitmap(a.rsOn); al_destroy_bitmap(a.rsOff);
    al_destroy_bitmap(a.vcOn); al_destroy_bitmap(a.vcOff);
    al_destroy_bitmap(a.coin); al_destroy_bitmap(a.xcoin);
    al_destroy_bitmap(a.rsDrop); al_destroy_bitmap(a.bulletPlayer);
    al_destroy_bitmap(a.bulletEnemie);al_destroy_bitmap(a.missileSpr);
    al_destroy_bitmap(a.ray); al_destroy_bitmap(a.reflectShield);
    for (int i = 0; i < 2; i++) {
        al_destroy_bitmap(a.player[i]); al_destroy_bitmap(a.enemie[i]);
        al_destroy_bitmap(a.archEnemie[i]); al_destroy_bitmap(a.boss[i]);
        al_destroy_bitmap(a.dron[i]);
    }
    for (int i = 0; i < 7; i++) al_destroy_bitmap(a.explosion[i]);
}

// --- Variables globales de estadísticas ---

int disparos = 0, aciertos = 0, fallos = 0, score = 0;

// --- Configuración de dificultad ---
// spawnRate: cada cuántos ticks aparece un enemigo (menor = más frecuente)
// velEnemigo / velBala: velocidades en px/tick

struct ConfigDificultad { int spawnRate; float velEnemigo; float velBala; };

ConfigDificultad configs[8] = {
    {120, 2.0f, 3.0f}, {100, 2.5f, 3.5f}, { 80, 2.5f, 3.2f}, { 60, 3.0f, 3.6f},
    { 50, 3.5f, 4.0f}, { 40, 4.5f, 5.5f}, { 30, 5.0f, 6.0f}, { 20, 6.0f, 7.0f}
};

const char* nombresDif[] = { "EASIEST","EASY","NORMAL","HARD","VERY HARD","EXTREME","NIGHTMARE","INSANE" };
const char* nombresModo[] = { "ORIGINAL","SLUDGE","MANIAC","MASSACRE" };
const char* nombresArma[] = { "NORMAL","BOTS","VELOCITY","SPREAD","MISSILE","IMPLOSION","PLASMA" };

// --- colision ---
// AABB simplificada (caja cuadrada de lado 2r).
// Aproximación suficiente para bullet hell 2D; hitbox real sería euclídea.
bool colision(float x1, float y1, float x2, float y2, float r) {
    return (fabs(x1 - x2) < r && fabs(y1 - y2) < r);
}

// --- obtenerUmbral ---
// Arcade: 50 en City/Ocean, 64 en Volcano/Space. Modo libre: siempre 50.
int obtenerUmbral(Nivel n) {
    if (n == CITY || n == OCEAN) return 50;
    return 64;
}

// --- patronPorDificultad ---
// Retorna índice de patrón de disparo para modo selección libre.
// 0=simple, 1=ráfaga, 2=abanico3, 3=abanico5
int patronPorDificultad(Dificultad d) {
    if (d <= DIF_EASY) return 0;
    if (d <= DIF_HARD) return 1;
    if (d <= DIF_EXTREME) return 2;
    return 3;
}

// --- Funciones de lista enlazada ---
// Patrón reutilizado de AgregarInicioInventario / DestruirInventario del laboratorio.
// Cada entidad tiene: Agregar (O(1) al frente), Eliminar (reconecta antes de liberar), Destruir.

void AgregarBala(Bala*& Lista, Bala*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarBala(Bala*& Lista, Bala* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Bala* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirBalas(Bala*& Lista) {
    Bala* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

// Mismo tipo Bala, lista separada para proyectiles enemigos.
void AgregarBalaEnemiga(Bala*& Lista, Bala*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarBalaEnemiga(Bala*& Lista, Bala* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Bala* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirBalasEnemigas(Bala*& Lista) {
    Bala* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarEnemigo(Enemigo*& Lista, Enemigo*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarEnemigo(Enemigo*& Lista, Enemigo* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Enemigo* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirEnemigos(Enemigo*& Lista) {
    Enemigo* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarMoneda(Moneda*& Lista, Moneda*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarMoneda(Moneda*& Lista, Moneda* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Moneda* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirMonedas(Moneda*& Lista) {
    Moneda* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarBot(Bot*& Lista, Bot*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarBot(Bot*& Lista, Bot* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Bot* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirBots(Bot*& Lista) {
    Bot* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarMissile(Missile*& Lista, Missile*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarMissile(Missile*& Lista, Missile* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Missile* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirMissiles(Missile*& Lista) {
    Missile* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarJefe(Jefe*& Lista, Jefe*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarJefe(Jefe*& Lista, Jefe* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Jefe* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirJefes(Jefe*& Lista) {
    Jefe* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

void AgregarExplosion(Explosion*& Lista, Explosion*& Nuevo) { Nuevo->siguiente = Lista; Lista = Nuevo; }
void EliminarExplosion(Explosion*& Lista, Explosion* nodo) {
    if (Lista == NULL) return;
    if (Lista == nodo) { Lista = Lista->siguiente; delete nodo; return; }
    Explosion* Aux = Lista;
    while (Aux->siguiente != NULL && Aux->siguiente != nodo) Aux = Aux->siguiente;
    if (Aux->siguiente == nodo) { Aux->siguiente = nodo->siguiente; delete nodo; }
}
void DestruirExplosiones(Explosion*& Lista) {
    Explosion* Aux = Lista;
    while (Aux != NULL) { Lista = Lista->siguiente; delete Aux; Aux = Lista; }
}

// --- BST de estadísticas ---
// Historial acumulativo de partidas ordenado por score.
// No se permite eliminar nodos. Basado en funciones del laboratorio de árboles.
// 'arcade' identifica si la partida fue en modo arcade o selección libre.

struct NodoBST {
    char nombre[6]; // 5 caracteres estilo arcade + '\0'
    int disparos, aciertos, fallos, score;
    bool arcade;// true=partida arcade, false=selección libre
    NodoBST* izq;
    NodoBST* der;
};

// Inserta ordenado por score. Scores iguales van a la derecha (permite duplicados).
// Basado en Insertar() del laboratorio.
void Insertar(NodoBST*& Raiz, char nombre[6], int disparos, int aciertos, int fallos, int score, bool arcade) {
    if (Raiz == NULL) {
        Raiz = new NodoBST;
        Raiz->disparos = disparos;
        Raiz->aciertos = aciertos;
        Raiz->fallos = fallos;
        Raiz->score = score;
        Raiz->arcade = arcade;
        Raiz->izq = NULL;
        Raiz->der = NULL;
        for (int i = 0; i < 6; i++) Raiz->nombre[i] = nombre[i];
    }
    else {
        if (score < Raiz->score) Insertar(Raiz->izq, nombre, disparos, aciertos, fallos, score, arcade);
        else Insertar(Raiz->der, nombre, disparos, aciertos, fallos, score, arcade);
    }
}

// Libera toda la memoria del árbol en post-order. Basado en PodarHojas() del laboratorio.
void DestruirBST(NodoBST*& Raiz) {
    if (Raiz != NULL) {
        DestruirBST(Raiz->izq);
        DestruirBST(Raiz->der);
        delete Raiz;
        Raiz = NULL;
    }
}

// Recorre en order inverso (der->raiz->izq) para mostrar ranking mayor a menor.
// 'y' y 'lugar' por referencia para acumular posición entre llamadas recursivas.
// Espaciado de 16px permite mostrar 25 entradas en pantalla de 768px.
// Muestra [A] para arcade y [L] para libre junto a cada entrada.
void MostrarTopScores(NodoBST* Raiz, ALLEGRO_FONT* font, int& y, int& lugar, int limite) {
    if (Raiz != NULL && lugar <= limite) {
        MostrarTopScores(Raiz->der, font, y, lugar, limite);
        if (lugar <= limite) {
            ALLEGRO_COLOR color = Raiz->arcade ? al_map_rgb(255, 255, 0) : al_map_rgb(0, 220, 255);
            al_draw_textf(font, color, WIDTH / 2, y, ALLEGRO_ALIGN_CENTER,
                "#%d  %s  %d pts  [D:%d A:%d F:%d]  %s",
                lugar, Raiz->nombre, Raiz->score,
                Raiz->disparos, Raiz->aciertos, Raiz->fallos,
                Raiz->arcade ? "[A]" : "[L]");
            y += 16; lugar++;
        }
        MostrarTopScores(Raiz->izq, font, y, lugar, limite);
    }
}

// Serializa en pre-order a "stats.bin". fflush() fuerza escritura inmediata al disco.
// Pre-order garantiza reconstrucción con la misma estructura al recargar.
void GuardarBST(NodoBST* Raiz, FILE* archivo) {
    if (Raiz != NULL) {
        fwrite(Raiz, sizeof(NodoBST), 1, archivo);
        fflush(archivo);
        GuardarBST(Raiz->izq, archivo);
        GuardarBST(Raiz->der, archivo);
    }
}

// Lee "stats.bin" y reconstruye el BST. Si no existe retorna sin hacer nada (primera ejecucion).
void CargarBST(NodoBST*& Raiz) {
    FILE* archivo = fopen("stats.bin", "rb");
    if (archivo == NULL) return;
    NodoBST temp;
    while (fread(&temp, sizeof(NodoBST), 1, archivo) == 1)
        Insertar(Raiz, temp.nombre, temp.disparos, temp.aciertos, temp.fallos, temp.score, temp.arcade);
    fclose(archivo);
}


int main() {

    // --- Inicialización de Allegro ---
    al_init();
    al_install_keyboard();
    al_init_primitives_addon();
    al_init_font_addon();
    al_init_ttf_addon();
    al_init_image_addon();         // Necesario para al_load_bitmap con PNGs
    al_init_native_dialog_addon(); // Para mensajes de debug

    ALLEGRO_DISPLAY* display = al_create_display(WIDTH, HEIGHT);
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    ALLEGRO_FONT* font = al_create_builtin_font();

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_register_event_source(queue, al_get_keyboard_event_source());

    // --- Carga de assets ---
    Assets assets;
    CargarAssets(assets);

    // --- Animaciones globales ---
    // ticksPorFrame=15: cicla ~4 veces/seg a 60 FPS. Boss anima más rápido.
    Anim2 animPlayer = { 0, 0, 15 };
    Anim2 animEnemie = { 0, 0, 15 };
    Anim2 animBoss = { 0, 0, 10 };
    Anim2 animDron = { 0, 0, 15 };

    // --- Estado inicial ---
    EstadoJuego estado = MENU;
    Dificultad  dificultad = DIF_NORMAL;
    ModoJuego   modo = ORIGINAL;
    Arma        armaActual = ARMA_NORMAL;

    Jugador jugador = { WIDTH / 2, HEIGHT - 50, 5, 3 };

    // Nombre estilo arcade: 5 caracteres, se edita en el menú con tecla N.
    char nombreJugador[6] = "AAA";
    bool editandoNombre = false;
    int  letraNombre = 0;

    NodoBST* raizBST = NULL;
    CargarBST(raizBST);

    // --- Listas de entidades ---
    Bala* balas = NULL;
    Bala* balasEnemigas = NULL;
    Enemigo* enemigos = NULL;
    Moneda* monedas = NULL;
    Bot* bots = NULL;
    Missile* misiles = NULL;
    Jefe* jefes = NULL;
    Explosion* explosiones = NULL;

    // --- Sistema de niveles ---
    // modoArcade: true=4 niveles consecutivos, false=modo libre infinito
    // tickTransicion: cuenta ticks en TRANSICION (180 = 3 segundos a 60 FPS)
    bool  modoArcade = true;
    Nivel nivelActual = CITY;
    int   enemigosEliminados = 0;
    int   umbralNivel = obtenerUmbral(CITY);
    bool  jefePendiente = false;
    bool  jefeVivo = false;
    int   tickTransicion = 0;

    // --- Reflect Shield ---
    // rsDisponible: el jugador tiene un RS listo para usar (tecla R)
    // rsActivo: la onda está expandiéndose actualmente
    // rsRadio: radio actual de la onda en px (crece hasta RS_RADIO_MAX)
    // rsTick: contador de ticks de duración de la onda
    bool  rsDisponible = true;
    bool  rsActivo = false;
    float rsRadio = 0.0f;
    const float RS_RADIO_MAX = 150.0f;
    const int   RS_TICKS_MAX = 30;
    int   rsTick = 0;

    // --- Velocity Cannon ---
    // velocityTicks: ticks activos (max 180 = 3 seg). Al agotarse o cambiar arma inicia cooldown.
    // velocityCooldown: ticks restantes de cooldown (max 1200 = 20 seg).
    int velocityTicks = 0;
    int velocityCooldown = 0;

    // --- Dev menu ---
    // CTRL+I: invencibilidad (golpes suman vida). CTRL+F: fast kill (cuenta x10).
    // Solo activos en estado JUGANDO.
    bool cheatInvencible = false;
    bool cheatFastKill = false;

    ALLEGRO_KEYBOARD_STATE keystate;
    srand(time(NULL));
    al_start_timer(timer);

    bool running = true;
    bool redraw = true;

    // --- Game loop principal ---
    // Cola de eventos Allegro 5: TIMER dispara lógica a 60 UPS.
    // Renderizado solo cuando redraw=true y la cola está vacía.
    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev);

        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            // Guardar partida incompleta si se estaba jugando en modo libre
            if (estado == JUGANDO && !modoArcade && score > 0) {
                Insertar(raizBST, nombreJugador, disparos, aciertos, fallos, score, false);
                FILE* archivo = fopen("stats.bin", "wb");
                if (archivo != NULL) { GuardarBST(raizBST, archivo); fclose(archivo); }
            }
            running = false;
        }
        if (ev.type == ALLEGRO_EVENT_TIMER) redraw = true;

        // --- MENU ---
        // ENTER=selección libre, SPACE=modo arcade, S=stats, N=nombre
        // En edición de nombre: UP/DOWN=letra, LEFT/RIGHT=cursor, ENTER=confirmar
        if (estado == MENU) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
                if (editandoNombre) {
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
                        nombreJugador[5] = '\0'; editandoNombre = false; letraNombre = 0;
                    }
                }
                else {
                    // ENTER: modo selección libre — infinito con patrón por dificultad
                    if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                        modoArcade = false;
                        nivelActual = CITY; enemigosEliminados = 0;
                        umbralNivel = 50;
                        jefePendiente = false; jefeVivo = false;
                        rsDisponible = true; rsActivo = false;
                        velocityTicks = 0; velocityCooldown = 0;
                        // Vidas según dificultad: Nightmare/Insane arrancan con 10
                        jugador.vidas = (dificultad >= DIF_NIGHTMARE ? 10 : 3);
                        estado = JUGANDO;
                    }
                    // SPACE: modo arcade — 4 niveles consecutivos, dificultad progresiva
                    if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                        modoArcade = true;
                        nivelActual = CITY; enemigosEliminados = 0;
                        umbralNivel = obtenerUmbral(CITY);
                        jefePendiente = false; jefeVivo = false;
                        rsDisponible = true; rsActivo = false;
                        velocityTicks = 0; velocityCooldown = 0;
                        dificultad = DIF_EASIEST; modo = ORIGINAL;
                        jugador.vidas = 3;
                        estado = JUGANDO;
                    }
                    if (ev.keyboard.keycode == ALLEGRO_KEY_N) editandoNombre = true;
                    if (ev.keyboard.keycode == ALLEGRO_KEY_S) estado = STATS;
                    if (ev.keyboard.keycode == ALLEGRO_KEY_UP && dificultad < DIF_INSANE)  dificultad = (Dificultad)(dificultad + 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN && dificultad > DIF_EASIEST) dificultad = (Dificultad)(dificultad - 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_RIGHT && modo < MASSACRE) modo = (ModoJuego)(modo + 1);
                    if (ev.keyboard.keycode == ALLEGRO_KEY_LEFT && modo > ORIGINAL) modo = (ModoJuego)(modo - 1);
                }
            }
        }

        // --- JUGANDO ---
        else if (estado == JUGANDO) {

            ConfigDificultad cfg = configs[dificultad];
            float multVel = (modo == SLUDGE ? 0.5f : (modo == MANIAC ? 1.5f : 1.0f));
            int   multSpawn = (modo == MASSACRE ? 2 : 1);

            // velX lateral para enemigos en Space (arcade) o Extreme+ (libre)
            bool usarVelX = (nivelActual == SPACE) ||
                (!modoArcade && dificultad >= DIF_EXTREME);

            if (ev.type == ALLEGRO_EVENT_TIMER) {

                al_get_keyboard_state(&keystate);

                // --- Actualizar animaciones ---
                ActualizarAnim2(animPlayer);
                ActualizarAnim2(animEnemie);
                ActualizarAnim2(animBoss);
                ActualizarAnim2(animDron);

                // --- Actualizar Reflect Shield ---
                // La onda crece linealmente hasta RS_RADIO_MAX en RS_TICKS_MAX ticks.
                if (rsActivo) {
                    rsTick++;
                    rsRadio = RS_RADIO_MAX * ((float)rsTick / RS_TICKS_MAX);
                    if (rsTick >= RS_TICKS_MAX) {
                        rsActivo = false; rsRadio = 0.0f; rsTick = 0;
                    }
                }

                // --- Actualizar Velocity Cannon ---
                // Al agotarse el tiempo activo se fuerza cambio de arma e inicia cooldown.
                if (armaActual == VELOCITY && velocityCooldown == 0) {
                    velocityTicks++;
                    if (velocityTicks >= 180) {
                        armaActual = ARMA_NORMAL;
                        velocityTicks = 0;
                        velocityCooldown = 1200;
                    }
                }
                if (velocityCooldown > 0) velocityCooldown--;

                // --- Movimiento del jugador ---
                if (al_key_down(&keystate, ALLEGRO_KEY_LEFT))  jugador.x -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_RIGHT)) jugador.x += jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_UP)) jugador.y -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_DOWN))  jugador.y += jugador.vel;
                jugador.x = std::max(10.f, std::min((float)WIDTH - 10, jugador.x));
                jugador.y = std::max(HEIGHT * 0.3f, std::min((float)HEIGHT - 10, jugador.y));

                // --- Reflect Shield: convertir balas enemigas en balas del jugador ---
                // Balas dentro del radio de la onda activa se transfieren a 'balas'.
                if (rsActivo) {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        float dx = Aux->x - jugador.x;
                        float dy = Aux->y - jugador.y;
                        if (sqrt(dx * dx + dy * dy) <= rsRadio) {
                            Bala* Reflejada = new Bala;
                            Reflejada->x = Aux->x;
                            Reflejada->y = Aux->y;
                            Reflejada->vel = Aux->vel;
                            Reflejada->velX = -Aux->velX;
                            Reflejada->hit = false;
                            Reflejada->tipo = 0;
                            Reflejada->siguiente = NULL;
                            AgregarBala(balas, Reflejada);
                            EliminarBalaEnemiga(balasEnemigas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // --- Spawn de enemigos ---
                // Suspendido mientras el jefe está pendiente o vivo.
                if (!jefePendiente && !jefeVivo) {
                    if (rand() % cfg.spawnRate == 0) {
                        for (int i = 0; i < multSpawn; i++) {
                            Enemigo* Nuevo = new Enemigo;
                            Nuevo->x = (float)(rand() % WIDTH);
                            Nuevo->y = 0;
                            Nuevo->vel = cfg.velEnemigo * multVel;
                            Nuevo->velX = usarVelX ? (rand() % 2 == 0 ? 1.5f : -1.5f) : 0.0f;
                            Nuevo->tipo = (nivelActual == SPACE || (!modoArcade && dificultad >= DIF_EXTREME)) ? 1 : 0;
                            Nuevo->siguiente = NULL;
                            AgregarEnemigo(enemigos, Nuevo);
                        }
                    }
                }

                // --- Movimiento de enemigos + patrones de disparo ---
                // Arcade: patrón según nivel. Libre: patrón según dificultad.
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        Aux->y += Aux->vel;
                        Aux->x += Aux->velX;
                        if (Aux->x <= 10 || Aux->x >= WIDTH - 10) Aux->velX = -Aux->velX;

                        int patron;
                        if (modoArcade) {
                            if (nivelActual == CITY) patron = 0;
                            else if (nivelActual == OCEAN) patron = 1;
                            else if (nivelActual == VOLCANO) patron = 2;
                            else patron = 3;
                        }
                        else patron = patronPorDificultad(dificultad);

                        // Frecuencia de disparo reducida cuando hay movimiento lateral
                        int freqDisparo = (usarVelX ? 90 : 100);

                        if (patron == 0) {
                            // Patron 1: bala simple recta
                            if (rand() % freqDisparo == 0) {
                                Bala* B = new Bala;
                                B->x = Aux->x; B->y = Aux->y;
                                B->vel = cfg.velBala * multVel; B->velX = 0.0f;
                                B->hit = false; B->tipo = 0; B->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, B);
                            }
                        }
                        else if (patron == 1) {
                            // Patron 2: rafaga de 3 balas con offset vertical
                            if (rand() % 120 == 0) {
                                for (int r = 0; r < 3; r++) {
                                    Bala* B = new Bala;
                                    B->x = Aux->x; B->y = Aux->y + r * 8;
                                    B->vel = cfg.velBala * multVel * 1.2f; B->velX = 0.0f;
                                    B->hit = false; B->tipo = 0; B->siguiente = NULL;
                                    AgregarBalaEnemiga(balasEnemigas, B);
                                }
                            }
                        }
                        else if (patron == 2) {
                            // Patron 3: abanico de 3 balas grandes (centro + diagonales)
                            if (rand() % 80 == 0) {
                                Bala* C = new Bala;
                                C->x = Aux->x; C->y = Aux->y;
                                C->vel = cfg.velBala * multVel; C->velX = 0.0f;
                                C->hit = false; C->tipo = 4; C->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, C);
                                Bala* I = new Bala;
                                I->x = Aux->x; I->y = Aux->y;
                                I->vel = cfg.velBala * multVel * 0.9f; I->velX = -1.5f;
                                I->hit = false; I->tipo = 4; I->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, I);
                                Bala* D = new Bala;
                                D->x = Aux->x; D->y = Aux->y;
                                D->vel = cfg.velBala * multVel * 0.9f; D->velX = 1.5f;
                                D->hit = false; D->tipo = 4; D->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, D);
                            }
                        }
                        else {
                            // Patron 4: abanico de 5 balas con dispersion real
                            if (rand() % (usarVelX ? 90 : 60) == 0) {
                                float vxs[5] = { -2.3f, -1.1f, 0.0f, 1.1f, 2.3f };
                                for (int r = 0; r < 5; r++) {
                                    Bala* B = new Bala;
                                    B->x = Aux->x; B->y = Aux->y;
                                    B->vel = cfg.velBala * multVel * 1.4f; B->velX = vxs[r];
                                    B->hit = false; B->tipo = 4; B->siguiente = NULL;
                                    AgregarBalaEnemiga(balasEnemigas, B);
                                }
                            }
                        }

                        if (Aux->y > HEIGHT) EliminarEnemigo(enemigos, Aux);
                        Aux = Siguiente;
                    }
                }

                // --- Movimiento del jefe ---
                // Zigzag horizontal con rebote en bordes.
                // Fase 1: 5 balas dispersion media. Fase 2: 7 balas mas rapidas.
                {
                    Jefe* Aux = jefes;
                    while (Aux != NULL) {
                        Jefe* Siguiente = Aux->siguiente;
                        Aux->x += Aux->velX;
                        if (Aux->x <= 30 || Aux->x >= WIDTH - 30) Aux->velX = -Aux->velX;

                        int   numBalas = (Aux->fase == 1 ? 5 : 7);
                        float velBala = (Aux->fase == 1 ? 4.0f : 6.0f);

                        // Dispersion ~30 grados para cada posicion del abanico
                        float dF1[5] = { -2.3f, -1.1f, 0.0f, 1.1f, 2.3f };
                        float dF2[7] = { -3.5f, -2.3f, -1.1f, 0.0f, 1.1f, 2.3f, 3.5f };
                        float* disp = (Aux->fase == 1 ? dF1 : dF2);

                        if (rand() % 60 == 0) {
                            for (int r = 0; r < numBalas; r++) {
                                Bala* B = new Bala;
                                B->x = Aux->x; B->y = Aux->y + 30;
                                B->vel = velBala; B->velX = disp[r];
                                B->hit = false; B->tipo = 4; B->siguiente = NULL;
                                AgregarBalaEnemiga(balasEnemigas, B);
                            }
                        }
                        Aux = Siguiente;
                    }
                }

                // --- Movimiento de balas del jugador ---
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        Aux->y -= Aux->vel;
                        Aux->x += Aux->velX;
                        if (Aux->y < 0 && !Aux->hit) { fallos++; EliminarBala(balas, Aux); }
                        Aux = Siguiente;
                    }
                }

                // --- Movimiento de balas enemigas ---
                // velX permite dispersion horizontal. Se eliminan al salir por cualquier borde.
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        Aux->y += Aux->vel; Aux->x += Aux->velX;
                        if (Aux->y > HEIGHT || Aux->x < 0 || Aux->x > WIDTH)
                            EliminarBalaEnemiga(balasEnemigas, Aux);
                        Aux = Siguiente;
                    }
                }

                // --- Movimiento de misiles ---
                // Homing rudimentario: sigue al primer enemigo de la lista en X.
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

                // --- Actualizacion de explosiones ---
                // Avanza un frame cada 5 ticks. Se elimina al terminar el frame 7.
                {
                    Explosion* Aux = explosiones;
                    while (Aux != NULL) {
                        Explosion* Siguiente = Aux->siguiente;
                        Aux->tick++;
                        if (Aux->tick >= 5) {
                            Aux->tick = 0; Aux->frame++;
                            if (Aux->frame >= 7) Aux->terminada = true;
                        }
                        if (Aux->terminada) EliminarExplosion(explosiones, Aux);
                        Aux = Siguiente;
                    }
                }

                // --- Colisiones: balas del jugador vs enemigos ---
                {
                    Bala* AuxBala = balas;
                    while (AuxBala != NULL) {
                        Bala* SiguienteBala = AuxBala->siguiente;
                        bool     impacto = false;
                        Enemigo* AuxEnemigo = enemigos;
                        while (AuxEnemigo != NULL && !impacto) {
                            Enemigo* SiguienteEnemigo = AuxEnemigo->siguiente;
                            if (colision(AuxBala->x, AuxBala->y, AuxEnemigo->x, AuxEnemigo->y, 18)) {
                                Explosion* E = new Explosion;
                                E->x = AuxEnemigo->x; E->y = AuxEnemigo->y;
                                E->frame = 0; E->tick = 0;
                                E->terminada = false; E->siguiente = NULL;
                                AgregarExplosion(explosiones, E);

                                // Drop: ~8% RS drop (solo si no se tiene), resto DAL Coins
                                int dropRoll = rand() % 100;
                                Moneda* M = new Moneda;
                                M->x = AuxEnemigo->x; M->y = AuxEnemigo->y;
                                M->siguiente = NULL;
                                if (dropRoll < 8 && !rsDisponible) M->tipo = 2;
                                else                                M->tipo = rand() % 2;
                                AgregarMoneda(monedas, M);

                                EliminarEnemigo(enemigos, AuxEnemigo);
                                EliminarBala(balas, AuxBala);
                                score += 10; aciertos++;
                                enemigosEliminados += (cheatFastKill ? 10 : 1);
                                impacto = true;
                            }
                            AuxEnemigo = SiguienteEnemigo;
                        }
                        AuxBala = SiguienteBala;
                    }
                }

                // --- Colisiones: misiles vs enemigos ---
                {
                    Missile* AuxMissile = misiles;
                    while (AuxMissile != NULL) {
                        Missile* SiguienteMissile = AuxMissile->siguiente;
                        bool     impacto = false;
                        Enemigo* AuxEnemigo = enemigos;
                        while (AuxEnemigo != NULL && !impacto) {
                            Enemigo* SiguienteEnemigo = AuxEnemigo->siguiente;
                            if (colision(AuxMissile->x, AuxMissile->y, AuxEnemigo->x, AuxEnemigo->y, 18)) {
                                Explosion* E = new Explosion;
                                E->x = AuxEnemigo->x; E->y = AuxEnemigo->y;
                                E->frame = 0; E->tick = 0;
                                E->terminada = false; E->siguiente = NULL;
                                AgregarExplosion(explosiones, E);
                                Moneda* M = new Moneda;
                                M->x = AuxEnemigo->x; M->y = AuxEnemigo->y;
                                M->tipo = rand() % 2; M->siguiente = NULL;
                                AgregarMoneda(monedas, M);
                                EliminarEnemigo(enemigos, AuxEnemigo);
                                EliminarMissile(misiles, AuxMissile);
                                score += 10; aciertos++;
                                enemigosEliminados += (cheatFastKill ? 10 : 1);
                                impacto = true;
                            }
                            AuxEnemigo = SiguienteEnemigo;
                        }
                        AuxMissile = SiguienteMissile;
                    }
                }

                // --- Colisiones: balas del jugador vs jefe ---
                {
                    Bala* AuxBala = balas;
                    while (AuxBala != NULL) {
                        Bala* SiguienteBala = AuxBala->siguiente;
                        bool  impacto = false;
                        Jefe* AuxJefe = jefes;
                        while (AuxJefe != NULL && !impacto) {
                            Jefe* SiguienteJefe = AuxJefe->siguiente;
                            if (colision(AuxBala->x, AuxBala->y, AuxJefe->x, AuxJefe->y, 60)) {
                                AuxJefe->vida--;
                                EliminarBala(balas, AuxBala);
                                score += 50; aciertos++; impacto = true;
                                if (AuxJefe->vida <= 5 && AuxJefe->fase == 1) AuxJefe->fase = 2;
                                if (AuxJefe->vida <= 0) {
                                    Explosion* E = new Explosion;
                                    E->x = AuxJefe->x; E->y = AuxJefe->y;
                                    E->frame = 0; E->tick = 0;
                                    E->terminada = false; E->siguiente = NULL;
                                    AgregarExplosion(explosiones, E);
                                    score += 500;
                                    EliminarJefe(jefes, AuxJefe);
                                    jefeVivo = false;

                                    if (modoArcade) {
                                        if (nivelActual == VOLCANO) {
                                            nivelActual = SPACE; dificultad = DIF_VERY_HARD;
                                            umbralNivel = obtenerUmbral(SPACE);
                                            enemigosEliminados = 0; jefePendiente = false;
                                            estado = TRANSICION; tickTransicion = 0;
                                        }
                                        else if (nivelActual == SPACE) {
                                            Insertar(raizBST, nombreJugador, disparos, aciertos, fallos, score, true);
                                            FILE* archivo = fopen("stats.bin", "wb");
                                            if (archivo != NULL) { GuardarBST(raizBST, archivo); fclose(archivo); }
                                            estado = GAME_OVER;
                                        }
                                    }
                                    else {
                                        // Modo libre: resetear contador y continuar loop
                                        enemigosEliminados = 0; jefePendiente = false;
                                    }
                                }
                            }
                            AuxJefe = SiguienteJefe;
                        }
                        AuxBala = SiguienteBala;
                    }
                }

                // --- Implosion ---
                // Campo pasivo: atrae enemigos dentro de radio 80px un 5% por tick.
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

                // --- Movimiento de monedas + recolección ---
                {
                    Moneda* Aux = monedas;
                    while (Aux != NULL) {
                        Moneda* Siguiente = Aux->siguiente;
                        Aux->y += 2;
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            if (Aux->tipo == 2) rsDisponible = true;
                            else score += (Aux->tipo == 0 ? 5 : 10);
                            EliminarMoneda(monedas, Aux);
                        }
                        else if (Aux->y > HEIGHT) EliminarMoneda(monedas, Aux);
                        Aux = Siguiente;
                    }
                }

                // --- Colisiones: balas enemigas vs jugador ---
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        Bala* Siguiente = Aux->siguiente;
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 10)) {
                            if (cheatInvencible) jugador.vidas++;
                            else jugador.vidas--;
                            EliminarBalaEnemiga(balasEnemigas, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // --- Progresion de nivel (solo modo arcade) ---
                if (modoArcade && !jefePendiente && !jefeVivo && enemigosEliminados >= umbralNivel) {
                    if (nivelActual == CITY || nivelActual == OCEAN) {
                        if (nivelActual == CITY) {
                            nivelActual = OCEAN; dificultad = DIF_NORMAL;
                            umbralNivel = obtenerUmbral(OCEAN); enemigosEliminados = 0;
                        }
                        else {
                            nivelActual = VOLCANO; dificultad = DIF_HARD;
                            umbralNivel = obtenerUmbral(VOLCANO); enemigosEliminados = 0;
                        }
                        DestruirEnemigos(enemigos); DestruirBalasEnemigas(balasEnemigas);
                        estado = TRANSICION; tickTransicion = 0;
                    }
                    else {
                        // VOLCANO y SPACE arcade: spawnear jefe
                        jefePendiente = true;
                        DestruirEnemigos(enemigos); DestruirBalasEnemigas(balasEnemigas);
                        Jefe* NuevoJefe = new Jefe;
                        NuevoJefe->x = WIDTH / 2; NuevoJefe->y = 80;
                        NuevoJefe->velX = (nivelActual == SPACE ? 3.0f : 2.0f);
                        NuevoJefe->vida = 10; NuevoJefe->fase = 1;
                        NuevoJefe->vivo = true; NuevoJefe->siguiente = NULL;
                        AgregarJefe(jefes, NuevoJefe);
                        jefePendiente = false; jefeVivo = true;
                    }
                }

                // --- Progresion modo libre: jefe cada 50 eliminados ---
                if (!modoArcade && !jefePendiente && !jefeVivo && enemigosEliminados >= 50) {
                    jefePendiente = true;
                    DestruirEnemigos(enemigos); DestruirBalasEnemigas(balasEnemigas);
                    Jefe* NuevoJefe = new Jefe;
                    NuevoJefe->x = WIDTH / 2; NuevoJefe->y = 80;
                    NuevoJefe->velX = 2.5f;
                    NuevoJefe->vida = 10; NuevoJefe->fase = 1;
                    NuevoJefe->vivo = true; NuevoJefe->siguiente = NULL;
                    AgregarJefe(jefes, NuevoJefe);
                    jefePendiente = false; jefeVivo = true;
                }

                // --- Transicion a GAME_OVER ---
                if (jugador.vidas <= 0) {
                    Insertar(raizBST, nombreJugador, disparos, aciertos, fallos, score, modoArcade);
                    FILE* archivo = fopen("stats.bin", "wb"); // "wb" sobreescribe — evita duplicados
                    if (archivo != NULL) { GuardarBST(raizBST, archivo); fclose(archivo); }
                    estado = GAME_OVER;
                }
            }

            // --- Input: cheats, RS, Velocity y disparo ---
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
                al_get_keyboard_state(&keystate);
                bool ctrl = al_key_down(&keystate, ALLEGRO_KEY_LCTRL) ||
                    al_key_down(&keystate, ALLEGRO_KEY_RCTRL);
                if (ctrl && ev.keyboard.keycode == ALLEGRO_KEY_I) cheatInvencible = !cheatInvencible;
                if (ctrl && ev.keyboard.keycode == ALLEGRO_KEY_F) cheatFastKill = !cheatFastKill;

                // Tecla R: activar Reflect Shield si está disponible
                if (ev.keyboard.keycode == ALLEGRO_KEY_R && rsDisponible && !rsActivo) {
                    rsDisponible = false; rsActivo = true; rsRadio = 0.0f; rsTick = 0;
                }

                // Si se cambia de arma mientras Velocity está activa → iniciar cooldown
                if (armaActual == VELOCITY && velocityCooldown == 0) {
                    if (ev.keyboard.keycode == ALLEGRO_KEY_0 || ev.keyboard.keycode == ALLEGRO_KEY_1 ||
                        ev.keyboard.keycode == ALLEGRO_KEY_3 || ev.keyboard.keycode == ALLEGRO_KEY_4 ||
                        ev.keyboard.keycode == ALLEGRO_KEY_5 || ev.keyboard.keycode == ALLEGRO_KEY_6) {
                        velocityTicks = 0;
                        velocityCooldown = 1200;
                    }
                }

                if (ev.keyboard.keycode == ALLEGRO_KEY_0) armaActual = ARMA_NORMAL;
                if (ev.keyboard.keycode == ALLEGRO_KEY_1) armaActual = BOTS;
                // Tecla 2: Velocity solo si no está en cooldown
                if (ev.keyboard.keycode == ALLEGRO_KEY_2 && velocityCooldown == 0) {
                    armaActual = VELOCITY;
                    velocityTicks = 0;
                }
                if (ev.keyboard.keycode == ALLEGRO_KEY_3) armaActual = SPREAD;
                if (ev.keyboard.keycode == ALLEGRO_KEY_4) armaActual = MISSILE;
                if (ev.keyboard.keycode == ALLEGRO_KEY_5) armaActual = IMPLOSION;
                if (ev.keyboard.keycode == ALLEGRO_KEY_6) armaActual = PLASMA;

                if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                    disparos++;
                    switch (armaActual) {
                    case ARMA_NORMAL: {
                        Bala* B = new Bala; B->x = jugador.x; B->y = jugador.y;
                        B->vel = 8; B->velX = 0.0f; B->hit = false; B->tipo = 0; B->siguiente = NULL;
                        AgregarBala(balas, B); break;
                    }
                    case SPREAD: {
                        // Abanico de 5 balas con dispersion real
                        float vxs[5] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };
                        for (int i = 0; i < 5; i++) {
                            Bala* B = new Bala; B->x = jugador.x; B->y = jugador.y;
                            B->vel = 6; B->velX = vxs[i]; B->hit = false; B->tipo = 1; B->siguiente = NULL;
                            AgregarBala(balas, B);
                        }
                        break;
                    }
                    case PLASMA: {
                        Bala* B = new Bala; B->x = jugador.x; B->y = jugador.y;
                        B->vel = 12; B->velX = 0.0f; B->hit = false; B->tipo = 2; B->siguiente = NULL;
                        AgregarBala(balas, B); break;
                    }
                    case MISSILE: {
                        Missile* M = new Missile; M->x = jugador.x; M->y = jugador.y; M->siguiente = NULL;
                        AgregarMissile(misiles, M); break;
                    }
                    case BOTS: {
                        if (bots == NULL) {
                            float offsets[4][2] = { {-20,-20},{20,-20},{-20,20},{20,20} };
                            for (int i = 0; i < 4; i++) {
                                Bot* NB = new Bot; NB->ox = offsets[i][0]; NB->oy = offsets[i][1];
                                NB->siguiente = NULL; AgregarBot(bots, NB);
                            }
                        }
                        Bot* AuxBot = bots;
                        while (AuxBot != NULL) {
                            Bala* B = new Bala;
                            B->x = jugador.x + AuxBot->ox; B->y = jugador.y + AuxBot->oy;
                            B->vel = 8; B->velX = 0.0f; B->hit = false; B->tipo = 3; B->siguiente = NULL;
                            AgregarBala(balas, B);
                            AuxBot = AuxBot->siguiente;
                        }
                        break;
                    }
                    default: break;
                    }
                }
            }
        }

        // --- TRANSICION ---
        // Pantalla automatica de 3 segundos entre niveles, sin input requerido.
        else if (estado == TRANSICION) {
            if (ev.type == ALLEGRO_EVENT_TIMER) {
                tickTransicion++;
                if (tickTransicion >= 180) { tickTransicion = 0; estado = JUGANDO; }
            }
        }

        // --- GAME OVER ---
        else if (estado == GAME_OVER) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                jugador.vidas = 3; jugador.x = WIDTH / 2; jugador.y = HEIGHT - 50;
                score = 0; disparos = aciertos = fallos = 0;
                modoArcade = true;
                nivelActual = CITY; enemigosEliminados = 0; umbralNivel = obtenerUmbral(CITY);
                jefePendiente = false; jefeVivo = false;
                rsDisponible = true; rsActivo = false; rsRadio = 0.0f; rsTick = 0;
                velocityTicks = 0; velocityCooldown = 0; armaActual = ARMA_NORMAL;
                cheatInvencible = false; cheatFastKill = false;
                DestruirEnemigos(enemigos); DestruirBalas(balas);
                DestruirBalasEnemigas(balasEnemigas); DestruirMonedas(monedas);
                DestruirMissiles(misiles); DestruirBots(bots);
                DestruirJefes(jefes); DestruirExplosiones(explosiones);
                estado = MENU;
            }
        }

        // --- STATS ---
        else if (estado == STATS) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                estado = MENU;
        }

        // --- Renderizado ---
        // Solo cuando redraw=true y la cola está vacía — desacopla lógica del render.
        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // --- Render: MENU ---
            if (estado == MENU) {
                if (assets.bgMenu)
                    al_draw_scaled_bitmap(assets.bgMenu, 0, 0,
                        al_get_bitmap_width(assets.bgMenu), al_get_bitmap_height(assets.bgMenu),
                        0, 0, WIDTH, HEIGHT, 0);
                if (assets.logo)
                    al_draw_bitmap(assets.logo, (WIDTH - 540) / 2, 20, 0);

                if (editandoNombre) {
                    al_draw_text(font, al_map_rgb(255, 255, 0),
                        WIDTH / 2, HEIGHT / 2 - 40, ALLEGRO_ALIGN_CENTER, "INGRESA TU NOMBRE:");
                    for (int i = 0; i < 5; i++) {
                        ALLEGRO_COLOR c = (i == letraNombre) ? al_map_rgb(0, 255, 255) : al_map_rgb(255, 255, 255);
                        al_draw_textf(font, c, WIDTH / 2 - 20 + i * 10, HEIGHT / 2, 0, "%c", nombreJugador[i]);
                    }
                    al_draw_text(font, al_map_rgb(150, 150, 150),
                        WIDTH / 2, HEIGHT / 2 + 30, ALLEGRO_ALIGN_CENTER, "UP/DOWN letra  LEFT/RIGHT mover  ENTER confirmar");
                }
                else {
                    al_draw_textf(font, al_map_rgb(0, 255, 255),
                        WIDTH / 2, HEIGHT / 2 + 70, ALLEGRO_ALIGN_CENTER, "Jugador: %s", nombreJugador);
                    al_draw_text(font, al_map_rgb(255, 255, 255),
                        WIDTH / 2, HEIGHT / 2 + 100, ALLEGRO_ALIGN_CENTER, "ENTER = Seleccion libre  |  SPACE = Modo Arcade");
                    al_draw_text(font, al_map_rgb(255, 255, 255),
                        WIDTH / 2, HEIGHT / 2 + 120, ALLEGRO_ALIGN_CENTER, "S = Stats  |  N = Nombre  |  R = Reflect Shield");
                    al_draw_textf(font, al_map_rgb(200, 200, 200),
                        WIDTH / 2, HEIGHT / 2 + 150, ALLEGRO_ALIGN_CENTER, "Dificultad (sel. libre): %s", nombresDif[(int)dificultad]);
                    al_draw_textf(font, al_map_rgb(200, 200, 200),
                        WIDTH / 2, HEIGHT / 2 + 170, ALLEGRO_ALIGN_CENTER, "Modo (sel. libre): %s", nombresModo[modo]);
                }
            }

            // --- Render: JUGANDO ---
            else if (estado == JUGANDO) {

                // Fondo: modo libre usa supernova, arcade usa fondo del nivel activo
                if (!modoArcade) {
                    if (assets.bgLibre) {
                        al_draw_scaled_bitmap(assets.bgLibre, 0, 0,
                            al_get_bitmap_width(assets.bgLibre), al_get_bitmap_height(assets.bgLibre),
                            0, 0, WIDTH, HEIGHT, 0);
                    }
                }
                else {
                    ALLEGRO_BITMAP* bg = NULL;
                    if (nivelActual == CITY) bg = assets.bgCity;
                    else if (nivelActual == OCEAN) bg = assets.bgOcean;
                    else if (nivelActual == VOLCANO) bg = assets.bgVolcano;
                    else  bg = assets.bgSpace;
                    if (bg) {
                        al_draw_scaled_bitmap(bg, 0, 0,
                            al_get_bitmap_width(bg), al_get_bitmap_height(bg), 0, 0, WIDTH, HEIGHT, 0);
                    }
                }

                // HUD: score con asset + numero a la derecha
                if (assets.scoreUI) al_draw_bitmap(assets.scoreUI, 10, 10, 0);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 100, 14, 0, "%d", score);

                // Vidas: corazones normales, o icono+numero si cheat activo
                if (cheatInvencible) {
                    if (assets.vidaUI) al_draw_bitmap(assets.vidaUI, 10, 40, 0);
                    al_draw_textf(font, al_map_rgb(255, 50, 50), 32, 40, 0, "x%d", jugador.vidas);
                }
                else {
                    int vidasMostrar = std::min(jugador.vidas, 6);
                    for (int i = 0; i < vidasMostrar; i++) {
                        if (assets.vidaUI) al_draw_bitmap(assets.vidaUI, 10 + i * 22, 40, 0);
                        else al_draw_filled_circle(19 + i * 22, 48, 7, al_map_rgb(255, 0, 0));
                    }
                }

                // Reflect Shield indicator
                if (assets.rsOn && assets.rsOff)
                    al_draw_bitmap(rsDisponible ? assets.rsOn : assets.rsOff, 10, 65, 0);
                else
                    al_draw_textf(font, rsDisponible ? al_map_rgb(0, 255, 0) : al_map_rgb(150, 150, 150),
                        10, 65, 0, "[RS]");

                // Velocity Cannon indicator: vcOn disponible, vcOff en cooldown con segundos restantes
                if (assets.vcOn && assets.vcOff) {
                    al_draw_bitmap(velocityCooldown == 0 ? assets.vcOn : assets.vcOff, 10, 118, 0);
                    if (velocityCooldown > 0)
                        al_draw_textf(font, al_map_rgb(255, 50, 50), 40, 120, 0, "%ds", velocityCooldown / 60);
                }
                else {
                    al_draw_textf(font, velocityCooldown == 0 ? al_map_rgb(0, 255, 0) : al_map_rgb(150, 150, 150),
                        10, 118, 0, velocityCooldown == 0 ? "[VC]" : "[VC %ds]", velocityCooldown / 60);
                }

                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 148, 0, "Arma: %s", nombresArma[armaActual]);
                al_draw_textf(font, al_map_rgb(0, 255, 255), 10, 163, 0, "%s", nombreJugador);

                if (!modoArcade) {
                    al_draw_textf(font, al_map_rgb(200, 200, 200), 10, 178, 0,
                        "%s  %s  %d/50", nombresDif[(int)dificultad], nombresModo[modo], enemigosEliminados);
                }
                else {
                    al_draw_textf(font, al_map_rgb(200, 200, 200), 10, 178, 0, "%s  %d/%d",
                        nivelActual == CITY ? "CITY" : nivelActual == OCEAN ? "OCEAN" :
                        nivelActual == VOLCANO ? "VOLCANO" : "SPACE",
                        enemigosEliminados, umbralNivel);
                }

                if (cheatFastKill)
                    al_draw_text(font, al_map_rgb(255, 50, 50), WIDTH - 10, 10, ALLEGRO_ALIGN_RIGHT, "[FK]");

                // Barra de vida del jefe
                if (jefeVivo && jefes != NULL) {
                    float barW = 400.0f, barX = (WIDTH - barW) / 2, barY = HEIGHT - 25;
                    al_draw_filled_rectangle(barX, barY, barX + barW, barY + 14, al_map_rgb(80, 0, 0));
                    al_draw_filled_rectangle(barX, barY, barX + barW * jefes->vida / 10, barY + 14, al_map_rgb(220, 0, 0));
                    al_draw_text(font, al_map_rgb(255, 255, 255), WIDTH / 2, barY - 16, ALLEGRO_ALIGN_CENTER, "JEFE");
                }

                // Jugador
                if (assets.player[0])
                    al_draw_bitmap(assets.player[animPlayer.frame], jugador.x - 32, jugador.y - 32, 0);
                else
                    al_draw_filled_rectangle(jugador.x - 10, jugador.y - 10, jugador.x + 10, jugador.y + 10, al_map_rgb(0, 255, 0));

                // Reflect Shield sobre el jugador + onda circular mientras está activo
                if (rsActivo) {
                    if (assets.reflectShield)
                        al_draw_bitmap(assets.reflectShield, jugador.x - 37, jugador.y - 35, 0);
                    al_draw_circle(jugador.x, jugador.y, rsRadio, al_map_rgba(100, 200, 255, 180), 2);
                }

                // Balas del jugador
                {
                    Bala* Aux = balas;
                    while (Aux != NULL) {
                        if (Aux->tipo == 2) {
                            al_draw_filled_circle(Aux->x, Aux->y, 6, al_map_rgba(0, 255, 0, 120));
                            al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgba(200, 255, 200, 220));
                        }
                        else if (assets.bulletPlayer)
                            al_draw_bitmap(assets.bulletPlayer, Aux->x - 8, Aux->y - 8, 0);
                        else {
                            ALLEGRO_COLOR color;
                            if (Aux->tipo == 0) color = al_map_rgb(255, 255, 0);
                            else if (Aux->tipo == 1) color = al_map_rgb(255, 0, 0);
                            else if (Aux->tipo == 3) color = al_map_rgb(0, 255, 255);
                            else color = al_map_rgb(255, 255, 255);
                            al_draw_filled_circle(Aux->x, Aux->y, 3, color);
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // Balas enemigas
                {
                    Bala* Aux = balasEnemigas;
                    while (Aux != NULL) {
                        if (assets.bulletEnemie)
                            al_draw_bitmap(assets.bulletEnemie, Aux->x - 8, Aux->y - 8, 0);
                        else if (Aux->tipo == 4) al_draw_filled_circle(Aux->x, Aux->y, 6, al_map_rgb(255, 140, 0));
                        else                     al_draw_filled_circle(Aux->x, Aux->y, 3, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Enemigos
                {
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        ALLEGRO_BITMAP* spr = (Aux->tipo == 1) ? assets.archEnemie[animEnemie.frame] : assets.enemie[animEnemie.frame];
                        if (spr) al_draw_bitmap(spr, Aux->x - 21, Aux->y - 16, 0);
                        else al_draw_filled_rectangle(Aux->x - 10, Aux->y - 10, Aux->x + 10, Aux->y + 10, al_map_rgb(255, 0, 0));
                        if (colision(jugador.x, jugador.y, Aux->x, Aux->y, 15)) {
                            if (cheatInvencible) jugador.vidas++;
                            else                 jugador.vidas--;
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;
                    }
                }

                // Jefe
                {
                    Jefe* Aux = jefes;
                    while (Aux != NULL) {
                        if (assets.boss[0])
                            al_draw_bitmap(assets.boss[animBoss.frame], Aux->x - 69, Aux->y - 69, 0);
                        else {
                            ALLEGRO_COLOR c = (Aux->fase == 2) ? al_map_rgb(220, 0, 100) : al_map_rgb(180, 0, 220);
                            al_draw_filled_rectangle(Aux->x - 40, Aux->y - 25, Aux->x + 40, Aux->y + 25, c);
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // Monedas con sprites diferenciados por tipo
                {
                    Moneda* Aux = monedas;
                    while (Aux != NULL) {
                        if (Aux->tipo == 2) {
                            if (assets.rsDrop) al_draw_bitmap(assets.rsDrop, Aux->x - 12, Aux->y - 12, 0);
                            else al_draw_filled_circle(Aux->x, Aux->y, 8, al_map_rgb(100, 200, 255));
                        }
                        else if (Aux->tipo == 0) {
                            if (assets.coin) al_draw_bitmap(assets.coin, Aux->x - 9, Aux->y - 9, 0);
                            else al_draw_filled_circle(Aux->x, Aux->y, 5, al_map_rgb(0, 100, 255));
                        }
                        else {
                            if (assets.xcoin) al_draw_bitmap(assets.xcoin, Aux->x - 9, Aux->y - 9, 0);
                            else al_draw_filled_circle(Aux->x, Aux->y, 5, al_map_rgb(180, 0, 255));
                        }
                        Aux = Aux->siguiente;
                    }
                }

                // Misiles
                {
                    Missile* Aux = misiles;
                    while (Aux != NULL) {
                        if (assets.missileSpr) al_draw_bitmap(assets.missileSpr, Aux->x - 8, Aux->y - 8, 0);
                        else al_draw_filled_circle(Aux->x, Aux->y, 4, al_map_rgb(255, 0, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Drones BOTS
                {
                    Bot* Aux = bots;
                    while (Aux != NULL) {
                        if (assets.dron[0])
                            al_draw_bitmap(assets.dron[animDron.frame], jugador.x + Aux->ox - 36, jugador.y + Aux->oy - 25, 0);
                        else
                            al_draw_filled_circle(jugador.x + Aux->ox, jugador.y + Aux->oy, 4, al_map_rgb(0, 255, 255));
                        Aux = Aux->siguiente;
                    }
                }

                // Explosiones
                {
                    Explosion* Aux = explosiones;
                    while (Aux != NULL) {
                        if (Aux->frame < 7 && assets.explosion[Aux->frame])
                            al_draw_bitmap(assets.explosion[Aux->frame], Aux->x - 29, Aux->y - 24, 0);
                        Aux = Aux->siguiente;
                    }
                }

                // Velocity Cannon
                // NOTA: logica de eliminacion dentro del render — pendiente de refactorizar.
                if (armaActual == VELOCITY) {
                    if (assets.ray) al_draw_bitmap(assets.ray, jugador.x - 40, 0, 0);
                    else al_draw_line(jugador.x, jugador.y, jugador.x, 0, al_map_rgb(255, 255, 255), 3);
                    Enemigo* Aux = enemigos;
                    while (Aux != NULL) {
                        Enemigo* Siguiente = Aux->siguiente;
                        if (fabs(Aux->x - jugador.x) < 10) {
                            score += 20; enemigosEliminados += (cheatFastKill ? 10 : 1);
                            EliminarEnemigo(enemigos, Aux);
                        }
                        Aux = Siguiente;
                    }
                }
            }

            // --- Render: TRANSICION ---
            else if (estado == TRANSICION) {
                const char* nombreNivel =
                    nivelActual == CITY ? "CITY — Easy" :
                    nivelActual == OCEAN ? "OCEAN — Medium" :
                    nivelActual == VOLCANO ? "VOLCANO — Hard" : "SPACE — Maniac";
                al_draw_text(font, al_map_rgb(255, 255, 0),
                    WIDTH / 2, HEIGHT / 2 - 20, ALLEGRO_ALIGN_CENTER, "NIVEL COMPLETADO");
                al_draw_textf(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2 + 10, ALLEGRO_ALIGN_CENTER, "Siguiente: %s", nombreNivel);
            }

            // --- Render: GAME_OVER ---
            else if (estado == GAME_OVER) {
                al_draw_text(font, al_map_rgb(255, 0, 0),
                    WIDTH / 2, HEIGHT / 2 - 40, ALLEGRO_ALIGN_CENTER, "GAME OVER");
                al_draw_textf(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2, ALLEGRO_ALIGN_CENTER,
                    "Jugador: %s  |  Score: %d", nombreJugador, score);
                al_draw_textf(font, al_map_rgb(150, 150, 150),
                    WIDTH / 2, HEIGHT / 2 + 30, ALLEGRO_ALIGN_CENTER,
                    "D:%d  A:%d  F:%d", disparos, aciertos, fallos);
                al_draw_text(font, al_map_rgb(255, 255, 255),
                    WIDTH / 2, HEIGHT / 2 + 60, ALLEGRO_ALIGN_CENTER, "ENTER para volver al menu");
            }

            // --- Render: STATS ---
            // [A]=arcade (amarillo), [L]=seleccion libre (cian)
            else if (estado == STATS) {
                if (assets.bgStats)
                    al_draw_scaled_bitmap(assets.bgStats, 0, 0,
                        al_get_bitmap_width(assets.bgStats), al_get_bitmap_height(assets.bgStats),
                        0, 0, WIDTH, HEIGHT, 0);
                al_draw_text(font, al_map_rgb(255, 255, 0),
                    WIDTH / 2, 20, ALLEGRO_ALIGN_CENTER, "TOP SCORES");
                al_draw_text(font, al_map_rgb(150, 150, 150),
                    WIDTH / 2, 40, ALLEGRO_ALIGN_CENTER, "[A]=Arcade  [L]=Libre  |  ESC para volver");
                if (raizBST == NULL)
                    al_draw_text(font, al_map_rgb(150, 150, 150),
                        WIDTH / 2, HEIGHT / 2, ALLEGRO_ALIGN_CENTER, "Sin partidas registradas aun");
                else {
                    int y = 65, lugar = 1;
                    MostrarTopScores(raizBST, font, y, lugar, 25);
                }
            }

            al_flip_display();
        }

    } // --- Fin del game loop ---

    // --- Limpieza ---
    DestruirAssets(assets);
    DestruirBST(raizBST);
    DestruirBalas(balas);
    DestruirBalasEnemigas(balasEnemigas);
    DestruirEnemigos(enemigos);
    DestruirMonedas(monedas);
    DestruirMissiles(misiles);
    DestruirBots(bots);
    DestruirJefes(jefes);
    DestruirExplosiones(explosiones);
    al_destroy_display(display);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}