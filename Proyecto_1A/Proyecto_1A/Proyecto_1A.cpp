#define _CRT_SECURE_NO_WARNINGS
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cmath>

const int WIDTH = 800;
const int HEIGHT = 600;
const float FPS = 60;

// ===== ENUMS =====
enum EstadoJuego { MENU, JUGANDO, GAME_OVER, STATS };
enum Dificultad {
    DIF_EASIEST,
    DIF_EASY,
    DIF_NORMAL,
    DIF_HARD,
    DIF_VERY_HARD,
    DIF_EXTREME,
    DIF_NIGHTMARE,
    DIF_INSANE
};
enum ModoJuego { ORIGINAL, SLUDGE, MANIAC, MASSACRE };
enum Arma {
    ARMA_NORMAL,
    BOTS,
    VELOCITY,
    SPREAD,
    MISSILE,
    IMPLOSION,
    PLASMA
};

// ===== STRUCTS =====
struct Bala { float x, y, vel; bool hit; int tipo; };
struct Jugador { float x, y, vel; int vidas; };
struct Enemigo { float x, y, vel; };
struct Moneda { float x, y; int tipo; };
struct Bot { float ox, oy; };
struct Missile { float x, y; };

// ===== STATS =====
int disparos = 0, aciertos = 0, fallos = 0;
int score = 0;

void guardarStats() {
    FILE* file = fopen("stats.txt", "a");
    if (file) {
        fprintf(file, "%d %d %d %d\n", disparos, aciertos, fallos, score);
        fclose(file);
    }
}

// ===== DIFICULTAD =====
struct ConfigDificultad { int spawnRate; float velEnemigo; float velBala; };
ConfigDificultad configs[8] = {
    {120,2,3},{100,2.5,3.5},{80,3,4},{60,3.5,4.5},
    {50,4,5},{40,4.5,5.5},{30,5,6},{20,6,7}
};

const char* nombresDif[] = { "EASIEST","EASY","NORMAL","HARD","VERY HARD","EXTREME","NIGHTMARE","INSANE" };
const char* nombresModo[] = { "ORIGINAL","SLUDGE","MANIAC","MASSACRE" };
const char* nombresArma[] = { "NORMAL","BOTS","VELOCITY","SPREAD","MISSILE","IMPLOSION","PLASMA" };

bool colision(float x1, float y1, float x2, float y2, float r) {
    return (fabs(x1 - x2) < r && fabs(y1 - y2) < r);
}

int main() {
    al_init();
    al_install_keyboard();
    al_init_primitives_addon();
    al_init_font_addon();
    al_init_ttf_addon();

    ALLEGRO_DISPLAY* display = al_create_display(WIDTH, HEIGHT);
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    ALLEGRO_FONT* font = al_create_builtin_font();

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_register_event_source(queue, al_get_keyboard_event_source());

    EstadoJuego estado = MENU;
    Dificultad dificultad = DIF_NORMAL;
    ModoJuego modo = ORIGINAL;
    Arma armaActual = ARMA_NORMAL;

    Jugador jugador = { WIDTH / 2, HEIGHT - 50, 5, 3 };

    std::vector<Bala> balas, balasEnemigas;
    std::vector<Enemigo> enemigos;
    std::vector<Moneda> monedas;
    std::vector<Bot> bots;
    std::vector<Missile> missiles;

    ALLEGRO_KEYBOARD_STATE keystate;
    srand(time(NULL));
    al_start_timer(timer);

    bool running = true, redraw = true;

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev);

        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) running = false;
        if (ev.type == ALLEGRO_EVENT_TIMER) redraw = true;

        // ===== MENU =====
        if (estado == MENU) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
                if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) estado = JUGANDO;
                if (ev.keyboard.keycode == ALLEGRO_KEY_UP && dificultad < DIF_INSANE) dificultad = (Dificultad)(dificultad + 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_DOWN && dificultad > DIF_EASIEST) dificultad = (Dificultad)(dificultad - 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_RIGHT && modo < MASSACRE) modo = (ModoJuego)(modo + 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_LEFT && modo > ORIGINAL) modo = (ModoJuego)(modo - 1);
                if (ev.keyboard.keycode == ALLEGRO_KEY_S) estado = STATS;
            }
        }

        // ===== JUEGO =====
        else if (estado == JUGANDO) {
            ConfigDificultad cfg = configs[dificultad];
            float multVel = (modo == SLUDGE ? 0.5f : (modo == MANIAC ? 1.5f : 1.0f));
            int multSpawn = (modo == MASSACRE ? 2 : 1);

            if (ev.type == ALLEGRO_EVENT_TIMER) {
                al_get_keyboard_state(&keystate);
                if (al_key_down(&keystate, ALLEGRO_KEY_LEFT)) jugador.x -= jugador.vel;
                if (al_key_down(&keystate, ALLEGRO_KEY_RIGHT)) jugador.x += jugador.vel;
                jugador.x = std::max(10.f, std::min((float)WIDTH - 10, jugador.x));

                // spawn enemigos
                if (rand() % cfg.spawnRate == 0) {
                    for (int i = 0; i < multSpawn; i++) {
                        Enemigo e{ (float)(rand() % WIDTH),0,cfg.velEnemigo * multVel };
                        enemigos.push_back(e);
                    }
                }

                // disparo enemigos
                for (auto& e : enemigos) {
                    e.y += e.vel;
                    if (rand() % 100 == 0) {
                        balasEnemigas.push_back({ e.x,e.y,cfg.velBala * multVel,false });
                    }
                }

                // movimiento
                for (auto& b : balas) b.y -= b.vel;
                for (auto& b : balasEnemigas) b.y += b.vel;
                for (auto& m : missiles) {
                    if (!enemigos.empty()) {
                        Enemigo t = enemigos[0];
                        if (m.x < t.x) m.x += 2; if (m.x > t.x) m.x -= 2;
                    }
                    m.y -= 3;
                }

                // colisiones bala-enemigo
                for (size_t i = 0; i < balas.size(); i++) {
                    for (size_t j = 0; j < enemigos.size(); j++) {
                        if (colision(balas[i].x, balas[i].y, enemigos[j].x, enemigos[j].y, 10)) {
                            Moneda mo{ enemigos[j].x, enemigos[j].y, rand() % 2 };
                            monedas.push_back(mo);
                            balas[i].hit = true;
                            balas.erase(balas.begin() + i);
                            enemigos.erase(enemigos.begin() + j);
                            score += 10; aciertos++; i--; break;
                        }
                    }
                }

                // implosion
                if (armaActual == IMPLOSION) {
                    for (auto& e : enemigos) {
                        if (colision(jugador.x, jugador.y, e.x, e.y, 80)) {
                            e.x += (jugador.x - e.x) * 0.05f;
                            e.y += (jugador.y - e.y) * 0.05f;
                        }
                    }
                }

                // monedas
                for (auto& m : monedas) {
                    m.y += 2;
                    if (colision(jugador.x, jugador.y, m.x, m.y, 15)) {
                        score += (m.tipo == 0 ? 5 : 10);
                        m.y = HEIGHT + 100;
                    }
                }

                // daño jugador
                for (auto& b : balasEnemigas) {
                    if (colision(jugador.x, jugador.y, b.x, b.y, 10)) {
                        jugador.vidas--; b.y = HEIGHT + 100;
                    }
                }

                // fallos
                for (auto& b : balas) if (b.y < 0 && !b.hit) fallos++;

                // limpiar
                balas.erase(remove_if(balas.begin(), balas.end(), [](Bala b) {return b.y < 0; }), balas.end());
                balasEnemigas.erase(remove_if(balasEnemigas.begin(), balasEnemigas.end(), [](Bala b) {return b.y > HEIGHT; }), balasEnemigas.end());
                enemigos.erase(remove_if(enemigos.begin(), enemigos.end(), [](Enemigo e) {return e.y > HEIGHT; }), enemigos.end());
                monedas.erase(remove_if(monedas.begin(), monedas.end(), [](Moneda m) {return m.y > HEIGHT; }), monedas.end());

                if (jugador.vidas <= 0) { guardarStats(); estado = GAME_OVER; }
            }

            // ===== INPUT =====
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
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
                    case ARMA_NORMAL: balas.push_back({ jugador.x,jugador.y,8,false,0 }); break;
                    case SPREAD:
                        for (int i = -2; i <= 2; i++) balas.push_back({ jugador.x + i * 5,jugador.y,6,false ,1 });
                        break;
                    case PLASMA:
                        balas.push_back({ jugador.x,jugador.y,12,false,2 });
                        break;
                    case MISSILE:
                        missiles.push_back({ jugador.x,jugador.y });
                        break;
                    case BOTS:
                        if (bots.empty()) {
                            bots.push_back({ -20,-20 }); bots.push_back({ 20,-20 });
                            bots.push_back({ -20,20 }); bots.push_back({ 20,20 });
                        }
                        for (auto& b : bots) balas.push_back({ jugador.x + b.ox,jugador.y + b.oy,8,false });
                        break;
                    default: break;
                    }
                }
            }
        }

        else if (estado == GAME_OVER) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                jugador.vidas = 3; score = 0; disparos = aciertos = fallos = 0;
                enemigos.clear(); balas.clear(); balasEnemigas.clear(); monedas.clear(); missiles.clear(); bots.clear();
                estado = MENU;
            }
        }

        else if (estado == STATS) {
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) estado = MENU;
        }

        // ===== DIBUJO =====
        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(0, 0, 0));

            if (estado == MENU) {
                al_draw_text(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 - 60, ALLEGRO_ALIGN_CENTER, "ENTER jugar | S stats");
                al_draw_textf(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 - 20, ALLEGRO_ALIGN_CENTER, "Dificultad: %s", nombresDif[(int)dificultad]);
                al_draw_textf(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2 + 20, ALLEGRO_ALIGN_CENTER, "Modo: %s", nombresModo[modo]);
            }
            else if (estado == JUGANDO) {
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 10, 0, "Score: %d", score);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 30, 0, "Vidas: %d", jugador.vidas);
                al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 50, 0, "Arma: %s", nombresArma[armaActual]);

                al_draw_filled_rectangle(jugador.x - 10, jugador.y - 10, jugador.x + 10, jugador.y + 10, al_map_rgb(0, 255, 0));

                for (auto& b : balas) {
                    if (b.tipo == 2) {
                        // Plasma: larger outer glow (semi-transparent) and brighter core
                        al_draw_filled_circle(b.x, b.y, 6, al_map_rgba(0, 255, 0, 120));
                        al_draw_filled_circle(b.x, b.y, 3, al_map_rgba(200, 255, 200, 220));
                    }
                    else {
                        ALLEGRO_COLOR color;

                        switch (b.tipo) {
                        case 0: color = al_map_rgb(255, 255, 0); break; // normal
                        case 1: color = al_map_rgb(255, 0, 0); break;   // spread
                        case 3: color = al_map_rgb(0, 255, 255); break; // bots
                        default: color = al_map_rgb(255, 255, 255);
                        }

                        al_draw_filled_circle(b.x, b.y, 3, color);
                    }
                }
                for (auto& b : balasEnemigas) al_draw_filled_circle(b.x, b.y, 3, al_map_rgb(0, 255, 255));
                for (auto& e : enemigos) al_draw_filled_rectangle(e.x - 10, e.y - 10, e.x + 10, e.y + 10, al_map_rgb(255, 0, 0));
                for (auto& m : monedas) al_draw_filled_circle(m.x, m.y, 4, al_map_rgb(0, 0, 255));
                for (auto& m : missiles) al_draw_filled_circle(m.x, m.y, 4, al_map_rgb(255, 0, 255));
                for (auto& b : bots) al_draw_filled_circle(jugador.x + b.ox, jugador.y + b.oy, 4, al_map_rgb(0, 255, 255));


                if (armaActual == VELOCITY) {
                    al_draw_line(jugador.x, jugador.y, jugador.x, 0, al_map_rgb(255, 255, 255), 3);
                    for (auto& e : enemigos) { if (fabs(e.x - jugador.x) < 10) { e.y = HEIGHT + 100; score += 20; } }
                }
            }
            else if (estado == GAME_OVER) {
                al_draw_text(font, al_map_rgb(255, 0, 0), WIDTH / 2, HEIGHT / 2, ALLEGRO_ALIGN_CENTER, "GAME OVER");
            }
            else if (estado == STATS) {
                al_draw_text(font, al_map_rgb(255, 255, 255), WIDTH / 2, HEIGHT / 2, ALLEGRO_ALIGN_CENTER, "Revisa stats.txt");
            }

            al_flip_display();
        }
    }

    al_destroy_display(display);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);
    return 0;
}