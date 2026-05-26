/*
 * Module:      hud
 * File:        hud.c
 * Description: Painel lateral único em português claro: status do sistema,
 *              tira de racks colorida, narrativa do controlador fuzzy e
 *              controles de operação. Sem abas — tudo visível de uma vez.
 */

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "hud.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define PANEL_W    305
#define HELP_PAGES   3

/* ------------------------------------------------------------------ */
/* Utilitários de cor                                                   */
/* ------------------------------------------------------------------ */

static Color rack_col(float t)
{
    if (t < 30.0f) return (Color){  55, 130, 255, 255 };
    if (t < 36.0f) return (Color){  55, 210,  80, 255 };
    if (t < 42.0f) return (Color){ 240, 175,  25, 255 };
    return                (Color){ 255,  55,  45, 255 };
}

static Color fan_col(float f)
{
    if (f < 45.0f) return (Color){  60, 190, 255, 255 };
    if (f < 75.0f) return (Color){ 240, 175,  25, 255 };
    return                (Color){ 255,  90,  55, 255 };
}

/* ------------------------------------------------------------------ */
/* Layout: coordenadas base do painel                                   */
/* ------------------------------------------------------------------ */

static int PX(void) { return GetScreenWidth() - PANEL_W; }

static void sep(int *y, int margin)
{
    *y += margin / 2;
    DrawRectangle(PX() + 8, *y, PANEL_W - 16, 1, (Color){ 42, 45, 56, 255 });
    *y += margin / 2 + 1;
}

static void section_hdr(int *y, const char *title, Color accent)
{
    DrawRectangle(PX() + 8, *y, 3, 13, accent);
    DrawText(title, PX() + 15, *y, 12, (Color){ 145, 150, 165, 255 });
    *y += 17;
}

/* Barra de progresso com borda */
static void pbar(int x, int y, int w, int h, float pct, Color fill)
{
    DrawRectangle(x, y, w, h, (Color){ 26, 28, 36, 255 });
    int filled = (int)(w * (pct / 100.0f));
    if (filled > 0) DrawRectangle(x, y, filled, h, fill);
    DrawRectangleLines(x, y, w, h, (Color){ 52, 55, 66, 255 });
}

/* ------------------------------------------------------------------ */
/* Histório de sparkline                                                 */
/* ------------------------------------------------------------------ */

static void draw_sparkline(const float *data, int total, int head,
                            Rectangle r, float vmin, float vmax, Color col,
                            float line_w)
{
    if (total < 2) return;
    float range = (vmax - vmin < 1.0f) ? 1.0f : (vmax - vmin);
    for (int i = 1; i < total; i++) {
        int i0 = (head - total + i - 1 + HUD_HISTORY_LEN) % HUD_HISTORY_LEN;
        int i1 = (head - total + i     + HUD_HISTORY_LEN) % HUD_HISTORY_LEN;
        float t0 = (data[i0] - vmin) / range;
        float t1 = (data[i1] - vmin) / range;
        if (t0 < 0.0f) t0 = 0.0f; else if (t0 > 1.0f) t0 = 1.0f;
        if (t1 < 0.0f) t1 = 0.0f; else if (t1 > 1.0f) t1 = 1.0f;
        float x0 = r.x + (i-1) * r.width / (float)(total-1);
        float x1 = r.x +  i    * r.width / (float)(total-1);
        float y0 = r.y + r.height * (1.0f - t0);
        float y1 = r.y + r.height * (1.0f - t1);
        DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, line_w, col);
    }
}

/* ------------------------------------------------------------------ */
/* Narrativa do controlador fuzzy                                        */
/* ------------------------------------------------------------------ */

static int dominant(const float mu[FUZZY_MAX_SETS])
{
    int best = 0;
    for (int i = 1; i < FUZZY_MAX_SETS; i++)
        if (mu[i] > mu[best]) best = i;
    return best;
}

static const char *PT_UPPER[] = {
    "MUITO BAIXA", "BAIXA", "MEDIA", "ALTA", "MUITO ALTA"
};

static Color label_col(int lbl)
{
    switch (lbl) {
        case FS_VERY_LOW:  return (Color){  60, 130, 255, 255 };
        case FS_LOW:       return (Color){  60, 210,  80, 255 };
        case FS_MEDIUM:    return (Color){ 220, 195,  50, 255 };
        case FS_HIGH:      return (Color){ 240, 135,  35, 255 };
        case FS_VERY_HIGH: return (Color){ 255,  55,  45, 255 };
        default:           return WHITE;
    }
}

/* Gera duas linhas de narrativa em português */
static void narrative(int tdom, int ldom, float fan,
                      int hot_rack, float hot_temp,
                      const char **line1, const char **line2)
{
    (void)fan;
    if (hot_temp > 42.0f) {
        *line1 = "CRITICO: temperatura acima do limite!";
        *line2 = "Refrigeracao maxima ativada.";
    } else if (tdom >= FS_HIGH && ldom >= FS_HIGH) {
        *line1 = "Temperatura e carga elevadas.";
        *line2 = "Fan na maxima para proteger os racks.";
    } else if (tdom >= FS_HIGH) {
        *line1 = "Temperatura acima do ideal.";
        *line2 = "Fan aumentado para resfriar os racks.";
    } else if (ldom >= FS_HIGH) {
        *line1 = "Alta carga nos servidores.";
        *line2 = "Fan elevado preventivamente.";
    } else if (tdom <= FS_LOW && ldom <= FS_LOW) {
        *line1 = "Sistema em carga leve.";
        *line2 = "Ventilacao minima — economia de energia.";
    } else {
        *line1 = "Sistema operando normalmente.";
        *line2 = "Controlador mantendo equilibrio.";
    }
    (void)hot_rack;
}

/* ------------------------------------------------------------------ */
/* Sparkline para buffer com comprimento arbitrário                     */
/* ------------------------------------------------------------------ */

static void draw_sparkline_n(const float *data, int n_buf, int total, int head,
                              Rectangle r, float vmin, float vmax,
                              Color col, float line_w)
{
    if (total < 2) return;
    float range = (vmax - vmin < 1.0f) ? 1.0f : (vmax - vmin);
    for (int i = 1; i < total; i++) {
        int i0 = (head - total + i - 1 + n_buf) % n_buf;
        int i1 = (head - total + i     + n_buf) % n_buf;
        float t0 = (data[i0] - vmin) / range;
        float t1 = (data[i1] - vmin) / range;
        if (t0 < 0.0f) t0 = 0.0f; else if (t0 > 1.0f) t0 = 1.0f;
        if (t1 < 0.0f) t1 = 0.0f; else if (t1 > 1.0f) t1 = 1.0f;
        float x0 = r.x + (i-1) * r.width  / (float)(total-1);
        float x1 = r.x +  i    * r.width  / (float)(total-1);
        float y0 = r.y + r.height * (1.0f - t0);
        float y1 = r.y + r.height * (1.0f - t1);
        DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, line_w, col);
    }
}

/* ================================================================== */
/* PAINEL DE AJUDA                                                      */
/* ================================================================== */

#define HC_TITLE  (Color){ 195, 200, 215, 255 }
#define HC_DIM    (Color){ 110, 115, 135, 255 }
#define HC_BLUE   (Color){  75, 125, 255, 255 }
#define HC_GREEN  (Color){  55, 195, 115, 255 }
#define HC_RED    (Color){ 255,  80,  60, 255 }
#define HC_YEL    (Color){ 220, 185,  50, 255 }
#define HC_BG     (Color){  13,  15,  22, 252 }

static void hline(int *y, int x, const char *txt, Color col, int sz)
{
    DrawText(txt, x, *y, sz, col);
    *y += sz + 5;
}

static void hhdr(int *y, int x, const char *txt, Color col)
{
    DrawRectangle(x, *y, 4, 16, col);
    DrawText(txt, x + 10, *y + 1, 14, col);
    *y += 24;
}

static void help_pg0(int px, int py, int pw)
{
    int x = px + 20;  int y = py + 52;
    (void)pw;

    hhdr(&y, x, "O QUE VOCE ESTA VENDO", HC_BLUE);
    hline(&y, x, "Este simulador representa um data center com 8 racks de servidores,", HC_TITLE, 13);
    hline(&y, x, "2 unidades de ar-condicionado CRAC, um Arduino Mega 2560 que coleta", HC_TITLE, 13);
    hline(&y, x, "os dados dos sensores, e um controlador fuzzy que decide a velocidade", HC_TITLE, 13);
    hline(&y, x, "dos ventiladores e o balanceamento de energia via PDU.", HC_TITLE, 13);
    y += 8;

    hhdr(&y, x, "O PAINEL LATERAL (HUD)", HC_BLUE);
    hline(&y, x, "  Barra colorida (topo) : status geral — verde OK / amarelo atencao / vermelho critico.", HC_DIM, 12);
    hline(&y, x, "  Tira de racks         : 8 blocos. Azul = frio, Verde = ok, Amarelo = quente, Vermelho = critico.", HC_DIM, 12);
    hline(&y, x, "  Controlador fuzzy     : o que o sistema esta 'pensando' em texto simples.", HC_DIM, 12);
    hline(&y, x, "  Grafico               : historico de temperatura (linha vermelha) e fan (linha azul).", HC_DIM, 12);
    hline(&y, x, "  Atuadores             : barras de Fan (ventiladores) e PDU (distribuicao de energia).", HC_DIM, 12);
    hline(&y, x, "  AUTO / MANUAL         : no modo manual voce controla Fan e PDU com sliders.", HC_DIM, 12);
    hline(&y, x, "  Log de eventos        : ultimas ocorrencias importantes do sistema.", HC_DIM, 12);
    y += 8;

    hhdr(&y, x, "CONTROLES DE CAMARA", HC_BLUE);
    hline(&y, x, "  Botao Direito + Arrastar  :  rotacionar a camera ao redor do data center.", HC_DIM, 12);
    hline(&y, x, "  Scroll do Mouse           :  aproximar ou afastar.", HC_DIM, 12);
    hline(&y, x, "  Botao do Meio + Arrastar  :  deslocar o ponto de foco.", HC_DIM, 12);
    hline(&y, x, "  Tecla W                   :  alternar modo wireframe (so as arestas, sem superficies).", HC_DIM, 12);
    hline(&y, x, "  Mouse sobre objetos       :  tooltip com dados ao vivo dos sensores.", HC_DIM, 12);
}

static void help_pg1(int px, int py, int pw)
{
    int x = px + 20;  int y = py + 52;
    (void)pw;

    hhdr(&y, x, "O QUE E LOGICA FUZZY?", HC_GREEN);
    hline(&y, x, "Logica classica so conhece 'verdadeiro' ou 'falso'. A logica fuzzy trabalha com", HC_TITLE, 13);
    hline(&y, x, "graus de pertinencia — algo pode ser 'um pouco quente' ou 'bastante quente',", HC_TITLE, 13);
    hline(&y, x, "exatamente como um tecnico humano pensaria ao ajustar o ar-condicionado.", HC_TITLE, 13);
    y += 8;

    hhdr(&y, x, "COMO FUNCIONA O CONTROLADOR DESTE SIMULADOR", HC_GREEN);
    hline(&y, x, "1. FUZZIFICACAO  : os valores dos sensores (ex: 41.3 C) sao convertidos em categorias", HC_TITLE, 13);
    hline(&y, x, "                   linguisticas: MUITO BAIXA, BAIXA, MEDIA, ALTA, MUITO ALTA.", HC_DIM, 12);
    y += 4;
    hline(&y, x, "2. REGRAS        : 25 regras do tipo 'SE temperatura ALTA E carga ALTA,", HC_TITLE, 13);
    hline(&y, x, "                   ENTAO fan MUITO ALTO'. Todas disparam simultaneamente com pesos.", HC_DIM, 12);
    y += 4;
    hline(&y, x, "3. DEFUZZIFICACAO: combina todas as respostas pelo metodo do centroide —", HC_TITLE, 13);
    hline(&y, x, "                   calcula a media ponderada e entrega um numero final (0-100%).", HC_DIM, 12);
    y += 8;

    hhdr(&y, x, "ENTRADAS E SAIDAS", HC_GREEN);
    hline(&y, x, "  Entradas (3) :  Temperatura media dos racks (C)  |  Carga CPU media (%)  |  Umidade (%)", HC_DIM, 12);
    hline(&y, x, "  Saidas  (2) :  Velocidade do Fan (0-100%)        |  Balanceamento PDU (0-100%)", HC_DIM, 12);
    y += 8;

    hhdr(&y, x, "POR QUE FUZZY E MELHOR AQUI?", HC_GREEN);
    hline(&y, x, "Um controlador simples ligaria o AC ao atingir 40 C e desligaria ao baixar de 38 C.", HC_TITLE, 13);
    hline(&y, x, "Com fuzzy, o sistema reage de forma progressiva e suave, sem ligar/desligar bruscamente.", HC_TITLE, 13);
    hline(&y, x, "Isso economiza energia, reduz desgaste mecanico e elimina oscilacoes de temperatura.", HC_TITLE, 13);
}

static void help_pg2(int px, int py, int pw)
{
    int x = px + 20;  int y = py + 52;
    (void)pw;

    hhdr(&y, x, "O DESAFIO DO CALOR EM DATA CENTERS", HC_RED);
    hline(&y, x, "Servidores transformam eletricidade em calculo — e em calor. Um unico rack moderno pode", HC_TITLE, 13);
    hline(&y, x, "dissipar ate 20 kW de calor. Sem resfriamento adequado, o hardware falha em minutos.", HC_TITLE, 13);
    hline(&y, x, "A temperatura ideal fica entre 18 C e 27 C, conforme a norma ASHRAE A1.", HC_TITLE, 13);
    y += 8;

    hhdr(&y, x, "COMO FUNCIONA A REFRIGERACAO (modelo CRAC)", HC_RED);
    hline(&y, x, "  * O ar quente sobe pelos fundos dos racks (corredor quente — atras dos racks).", HC_DIM, 12);
    hline(&y, x, "  * O CRAC aspira esse ar, resfria com serpentinas frias e devolve pelo piso elevado.", HC_DIM, 12);
    hline(&y, x, "  * O corredor frio — bloco azul semitransparente no simulador — distribui o ar frio.", HC_DIM, 12);
    hline(&y, x, "  * Os ventiladores dos servidores puxam esse ar frio pela frente dos racks.", HC_DIM, 12);
    hline(&y, x, "  * 2 unidades CRAC garantem redundancia: se uma falhar, a outra sustenta o sistema.", HC_DIM, 12);
    y += 8;

    hhdr(&y, x, "O QUE PODE CAUSAR PROBLEMAS", HC_RED);
    hline(&y, x, "  PONTO QUENTE (hotspot) : rack com alta carga aquece mais que os vizinhos. O simulador", HC_TITLE, 13);
    hline(&y, x, "                          destaca o rack mais quente com borda pulsante na cena 3D.", HC_DIM, 12);
    y += 4;
    hline(&y, x, "  MISTURA DE AR          : ar quente e frio se misturam e reduzem a eficiencia do resfriamento.", HC_TITLE, 13);
    hline(&y, x, "                          Corredores alternados (frio/quente) separam os fluxos.", HC_DIM, 12);
    y += 4;
    hline(&y, x, "  UMIDADE ERRADA         : muito seca gera eletrostatica nos componentes;", HC_TITLE, 13);
    hline(&y, x, "                          muito umida pode causar condensacao e curto-circuito.", HC_DIM, 12);
    y += 4;
    hline(&y, x, "  SOBRECARGA CONCENTRADA : muita carga em poucos racks cria hotspots. O PDU redistribui", HC_TITLE, 13);
    hline(&y, x, "                          a carga entre os racks para equilibrar o calor gerado.", HC_DIM, 12);
    y += 8;

    hhdr(&y, x, "O QUE ESTE SIMULADOR FAZ POR VOCE", HC_YEL);
    hline(&y, x, "  * Controle progressivo (fuzzy) — sem liga/desliga brusco.", HC_DIM, 12);
    hline(&y, x, "  * Monitoramento continuo de temperatura, carga e umidade em todos os racks.", HC_DIM, 12);
    hline(&y, x, "  * Redundancia de resfriamento com 2 CRACs independentes.", HC_DIM, 12);
    hline(&y, x, "  * Balanceamento de carga via PDU para evitar pontos quentes.", HC_DIM, 12);
}

static void draw_help_overlay(HudState *hud, int px, int sh)
{
    /* Fundo semitransparente sobre a area 3D */
    DrawRectangle(0, 0, px, sh, (Color){ 5, 7, 12, 210 });

    /* Painel central */
    int pw = (px - 40 < 780) ? px - 40 : 780;
    int ph = sh - 48;
    int panel_x = (px - pw) / 2;
    int panel_y = 24;

    DrawRectangle(panel_x, panel_y, pw, ph, HC_BG);
    DrawRectangleLines(panel_x, panel_y, pw, ph, (Color){ 50, 55, 75, 255 });

    /* Linha decorativa de topo */
    DrawRectangle(panel_x, panel_y, pw, 3, HC_BLUE);

    /* Titulo */
    static const char *titles[HELP_PAGES] = {
        "O SIMULADOR",
        "CONTROLADOR FUZZY",
        "REFRIGERACAO DE DATA CENTERS",
    };
    DrawText("AJUDA  —", panel_x + 20, panel_y + 14, 16, HC_DIM);
    DrawText(titles[hud->help_page],
             panel_x + 20 + MeasureText("AJUDA  — ", 16),
             panel_y + 14, 16, HC_TITLE);

    /* Indicador de pagina */
    char pg_buf[16];
    snprintf(pg_buf, sizeof(pg_buf), "%d / %d", hud->help_page + 1, HELP_PAGES);
    DrawText(pg_buf,
             panel_x + pw - MeasureText(pg_buf, 12) - 48,
             panel_y + 17, 12, HC_DIM);

    /* Conteudo da pagina */
    switch (hud->help_page) {
        case 0: help_pg0(panel_x, panel_y, pw); break;
        case 1: help_pg1(panel_x, panel_y, pw); break;
        case 2: help_pg2(panel_x, panel_y, pw); break;
    }

    /* Separador dos botoes */
    DrawRectangle(panel_x + 16, panel_y + ph - 50,
                  pw - 32, 1, (Color){ 40, 44, 56, 255 });

    int btn_y = panel_y + ph - 38;
    int btn_h = 28;
    Vector2 mp = GetMousePosition();

    /* Botao Anterior */
    if (hud->help_page > 0) {
        int bx = panel_x + 16;
        DrawRectangle(bx, btn_y, 110, btn_h, (Color){ 28, 32, 48, 255 });
        DrawRectangleLines(bx, btn_y, 110, btn_h, (Color){ 55, 60, 80, 255 });
        DrawText("< Anterior", bx + 14, btn_y + 7, 13, HC_DIM);
        if (mp.x >= bx && mp.x <= bx + 110 && mp.y >= btn_y && mp.y <= btn_y + btn_h) {
            DrawRectangle(bx, btn_y, 110, btn_h, (Color){ 50, 55, 80, 80 });
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->help_page--;
        }
    }

    /* Botao Proximo */
    if (hud->help_page < HELP_PAGES - 1) {
        int bx = panel_x + pw - 126;
        DrawRectangle(bx, btn_y, 110, btn_h, (Color){ 18, 55, 100, 255 });
        DrawRectangleLines(bx, btn_y, 110, btn_h, (Color){ 35, 85, 155, 255 });
        DrawText("Proximo >", bx + 16, btn_y + 7, 13, HC_TITLE);
        if (mp.x >= bx && mp.x <= bx + 110 && mp.y >= btn_y && mp.y <= btn_y + btn_h) {
            DrawRectangle(bx, btn_y, 110, btn_h, (Color){ 35, 90, 160, 80 });
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->help_page++;
        }
    }

    /* Botao fechar (X) */
    int cx = panel_x + pw - 30;
    int cy = panel_y + 10;
    DrawRectangle(cx, cy, 20, 20, (Color){ 50, 20, 20, 255 });
    DrawText("X", cx + 5, cy + 3, 13, HC_RED);
    if (mp.x >= cx && mp.x <= cx + 20 && mp.y >= cy && mp.y <= cy + 20) {
        DrawRectangle(cx, cy, 20, 20, (Color){ 90, 30, 30, 100 });
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->help_open = 0;
    }

    /* ESC tambem fecha */
    if (IsKeyPressed(KEY_ESCAPE)) hud->help_open = 0;
}

/* ================================================================== */
/* OVERLAY DA REDE NEURAL                                               */
/* ================================================================== */

/* Cor de um neurônio baseada na ativação e na camada */
static Color nn_node_color(float act, int layer)
{
    unsigned char r, g, b;
    if (layer == 0) {           /* entrada — familia azul */
        r = (unsigned char)(25  + act * 55);
        g = (unsigned char)(90  + act * 130);
        b = (unsigned char)(200 + act * 55);
    } else if (layer == 1) {    /* oculta — familia verde */
        r = (unsigned char)(20  + act * 80);
        g = (unsigned char)(155 + act * 85);
        b = (unsigned char)(55  + act * 45);
    } else {                    /* saída — familia laranja */
        r = (unsigned char)(200 + act * 55);
        g = (unsigned char)(90  + (int)(act * 110 - act*act*60));
        b = (unsigned char)(20  + act * 20);
    }
    return (Color){ r, g, b, 255 };
}

/* Cor da conexão baseada no sinal do peso */
static Color nn_conn_color(float w, float max_abs, int alpha)
{
    float t = (max_abs > 0.0f) ? fabsf(w) / max_abs : 0.5f;
    int a = 60 + (int)(t * 170);
    if (a > 255) a = 255;
    if (alpha > 0) a = alpha;
    if (w >= 0.0f)
        return (Color){ 55, 130, 255, (unsigned char)a };   /* azul positivo */
    else
        return (Color){ 255, 130, 40,  (unsigned char)a };  /* laranja negativo */
}

/* ------------------------------------------------------------------ */
/* Diagrama compacto — sempre visível no canto superior esquerdo        */
/* ------------------------------------------------------------------ */
static void draw_nn_compact(const MlpNet *mlp)
{
    int npx = 8;
    int npy = 8;
    int npw = 162;
    int nph = 186;

    DrawRectangle(npx, npy, npw, nph, (Color){ 8, 10, 18, 192 });
    DrawRectangleLines(npx, npy, npw, nph, (Color){ 36, 42, 60, 175 });
    DrawRectangle(npx, npy, 2, nph, HC_GREEN);

    DrawText("REDE", npx + 8, npy + 5, 9, HC_DIM);
    DrawText("NEURAL", npx + 8 + MeasureText("REDE ", 9), npy + 5, 9, HC_GREEN);

    int cx_in  = npx + 32;
    int cx_hid = npx + 86;
    int cx_out = npx + 144;

    int yin [MLP_IN]  = { npy + 38, npy + 80, npy + 122 };
    int yhid[MLP_H1]  = { npy + 24, npy + 59, npy + 95,  npy + 130 };
    int yout[MLP_OUT] = { npy + 62, npy + 108 };

    int nr = 9;
    float t = (float)GetTime();

    /* Conexoes entrada → oculta */
    int ci = 0;
    for (int j = 0; j < MLP_H1; j++) {
        for (int i = 0; i < MLP_IN; i++) {
            float w = mlp->w1[j][i];
            unsigned char al = (unsigned char)(50 + 60 * fabsf(w) / 2.4f);
            Color cc = w >= 0.0f
                ? (Color){ 50, 115, 205, al } : (Color){ 205, 108, 35, al };
            DrawLineEx(
                (Vector2){ (float)cx_in,  (float)yin[i]  },
                (Vector2){ (float)cx_hid, (float)yhid[j] }, 0.9f, cc);
            float ph = fmodf(t * 0.68f + ci * 0.11f, 1.0f);
            DrawCircleV(
                (Vector2){
                    cx_in  + ph * (cx_hid - cx_in),
                    yin[i] + ph * (yhid[j] - yin[i]) }, 2.3f,
                w >= 0.0f ? (Color){ 105, 172, 255, 195 }
                           : (Color){ 255, 155,  55, 195 });
            ci++;
        }
    }

    /* Conexoes oculta → saida */
    for (int k = 0; k < MLP_OUT; k++) {
        for (int j = 0; j < MLP_H1; j++) {
            float w = mlp->w2[k][j];
            unsigned char al = (unsigned char)(50 + 60 * fabsf(w) / 1.4f);
            if (al > 110) al = 110;
            Color cc = w >= 0.0f
                ? (Color){ 50, 115, 205, al } : (Color){ 205, 108, 35, al };
            DrawLineEx(
                (Vector2){ (float)cx_hid, (float)yhid[j] },
                (Vector2){ (float)cx_out, (float)yout[k] }, 0.9f, cc);
            float ph = fmodf(t * 0.68f + ci * 0.11f, 1.0f);
            DrawCircleV(
                (Vector2){
                    cx_hid + ph * (cx_out - cx_hid),
                    yhid[j]+ ph * (yout[k] - yhid[j]) }, 2.3f,
                w >= 0.0f ? (Color){ 105, 172, 255, 195 }
                           : (Color){ 255, 155,  55, 195 });
            ci++;
        }
    }

    /* Neuronios */
    for (int i = 0; i < MLP_IN; i++) {
        Color nc = nn_node_color(mlp->a_in[i], 0);
        DrawCircle(cx_in, yin[i], nr + 3, (Color){ nc.r, nc.g, nc.b, 28 });
        DrawCircle(cx_in, yin[i], nr, nc);
        DrawCircleLines(cx_in, yin[i], nr, (Color){ 175, 185, 210, 145 });
    }
    for (int j = 0; j < MLP_H1; j++) {
        Color nc = nn_node_color(mlp->a_h1[j], 1);
        DrawCircle(cx_hid, yhid[j], nr + 3, (Color){ nc.r, nc.g, nc.b, 28 });
        DrawCircle(cx_hid, yhid[j], nr, nc);
        DrawCircleLines(cx_hid, yhid[j], nr, (Color){ 175, 185, 210, 128 });
    }
    for (int k = 0; k < MLP_OUT; k++) {
        Color nc = nn_node_color(mlp->a_out[k], 2);
        DrawCircle(cx_out, yout[k], nr + 3, (Color){ nc.r, nc.g, nc.b, 28 });
        DrawCircle(cx_out, yout[k], nr, nc);
        DrawCircleLines(cx_out, yout[k], nr, (Color){ 175, 185, 210, 128 });
    }

    /* Previsoes no rodape */
    int sep_y = npy + nph - 48;
    DrawRectangle(npx + 6, sep_y, npw - 12, 1, (Color){ 35, 40, 55, 175 });
    char buf[20];
    snprintf(buf, sizeof(buf), "T+5s: %.1fC", mlp->pred_temp);
    DrawText(buf, npx + 10, sep_y + 6,  11, nn_node_color(mlp->a_out[0], 2));
    snprintf(buf, sizeof(buf), "L+5s: %.1f%%", mlp->pred_load);
    DrawText(buf, npx + 10, sep_y + 21, 11, nn_node_color(mlp->a_out[1], 2));
}

static void draw_nn_overlay(HudState *hud, const MlpNet *mlp, int px, int sh)
{
    /* Fundo semitransparente sobre area 3D */
    DrawRectangle(0, 0, px, sh, (Color){ 5, 7, 12, 200 });

    /* Dimensoes do painel */
    int pw = (px - 40 < 700) ? px - 40 : 700;
    int ph = 444;
    int panel_x = (px - pw) / 2;
    int panel_y = (sh - ph) / 2;

    /* Fundo e borda */
    DrawRectangle(panel_x, panel_y, pw, ph, (Color){ 10, 12, 20, 252 });
    DrawRectangleLines(panel_x, panel_y, pw, ph, (Color){ 50, 55, 75, 255 });
    DrawRectangle(panel_x, panel_y, pw, 3, HC_GREEN);

    /* Titulo */
    DrawText("AJUDA  —", panel_x + 20, panel_y + 14, 16, HC_DIM);
    DrawText("REDE NEURAL PREDITIVA",
             panel_x + 20 + MeasureText("AJUDA  — ", 16),
             panel_y + 14, 16, HC_TITLE);
    DrawText("previsao de temp e carga para +5 s",
             panel_x + pw - MeasureText("previsao de temp e carga para +5 s", 11) - 20,
             panel_y + 17, 11, HC_DIM);

    /* Separador entre titulo e conteudo */
    DrawRectangle(panel_x + 16, panel_y + 44, pw - 32, 1,
                  (Color){ 40, 44, 56, 255 });

    /* -------- Divisao: diagrama (esq) | info (dir) -------- */
    int diag_w = 340;   /* largura da secao do diagrama */
    int info_x = panel_x + diag_w + 16;
    int info_w = pw - diag_w - 32;

    /* Divisor vertical */
    DrawRectangle(panel_x + diag_w, panel_y + 48,
                  1, ph - 96, (Color){ 40, 44, 56, 255 });

    /* -------- Posicoes absolutas dos neuronios -------- */
    /* Coluna X */
    int cx_in  = panel_x + 72;
    int cx_hid = panel_x + 190;
    int cx_out = panel_x + 308;

    /* Linha Y (relativa ao panel_y) */
    int dy_top = panel_y + 52;
    int yin[MLP_IN]  = { dy_top + 45,  dy_top + 160, dy_top + 275 };
    int yhid[MLP_H1] = { dy_top + 18,  dy_top + 104, dy_top + 195, dy_top + 281 };
    int yout[MLP_OUT]= { dy_top + 110, dy_top + 240 };

    int nr = 18;  /* raio dos neuronios */

    float t_anim = (float)GetTime();

    /* ---- Conexoes camada 1→2 (entrada → oculta) ---- */
    float max_w1 = 2.40f;
    int conn_idx = 0;
    for (int j = 0; j < MLP_H1; j++) {
        for (int i = 0; i < MLP_IN; i++) {
            float w = mlp->w1[j][i];
            Color cc = nn_conn_color(w, max_w1, -1);

            /* Linha da conexao */
            float thick = 0.8f + 1.8f * fabsf(w) / max_w1;
            DrawLineEx(
                (Vector2){ (float)cx_in,  (float)yin[i]  },
                (Vector2){ (float)cx_hid, (float)yhid[j] },
                thick, cc);

            /* Pulsos animados (2 por conexao) */
            for (int p = 0; p < 2; p++) {
                float phase = fmodf(t_anim * 0.65f
                                    + conn_idx * 0.09f
                                    + p * 0.5f, 1.0f);
                float px2 = cx_in  + phase * (cx_hid - cx_in);
                float py2 = yin[i] + phase * (yhid[j] - yin[i]);
                Color pc = w >= 0.0f
                    ? (Color){ 120, 190, 255, 200 }
                    : (Color){ 255, 170,  80, 200 };
                DrawCircleV((Vector2){ px2, py2 }, 3.5f, pc);
            }
            conn_idx++;
        }
    }

    /* ---- Conexoes camada 2→3 (oculta → saida) ---- */
    float max_w2 = 1.40f;
    for (int k = 0; k < MLP_OUT; k++) {
        for (int j = 0; j < MLP_H1; j++) {
            float w = mlp->w2[k][j];
            Color cc = nn_conn_color(w, max_w2, -1);

            float thick = 0.8f + 1.8f * fabsf(w) / max_w2;
            DrawLineEx(
                (Vector2){ (float)cx_hid, (float)yhid[j] },
                (Vector2){ (float)cx_out, (float)yout[k] },
                thick, cc);

            for (int p = 0; p < 2; p++) {
                float phase = fmodf(t_anim * 0.65f
                                    + conn_idx * 0.09f
                                    + p * 0.5f, 1.0f);
                float px2 = cx_hid + phase * (cx_out - cx_hid);
                float py2 = yhid[j]+ phase * (yout[k] - yhid[j]);
                Color pc = w >= 0.0f
                    ? (Color){ 120, 190, 255, 200 }
                    : (Color){ 255, 170,  80, 200 };
                DrawCircleV((Vector2){ px2, py2 }, 3.5f, pc);
            }
            conn_idx++;
        }
    }

    /* ---- Neuronios de entrada ---- */
    static const char *in_lbl[MLP_IN]  = { "T", "L", "F" };
    static const char *in_name[MLP_IN] = { "Temperatura", "Carga CPU", "Fan" };
    for (int i = 0; i < MLP_IN; i++) {
        float act = mlp->a_in[i];
        Color nc = nn_node_color(act, 0);
        /* Brilho (halo) */
        DrawCircle(cx_in, yin[i], nr + 7, (Color){ nc.r, nc.g, nc.b, 35 });
        DrawCircle(cx_in, yin[i], nr + 4, (Color){ nc.r, nc.g, nc.b, 70 });
        DrawCircle(cx_in, yin[i], nr,     nc);
        DrawCircleLines(cx_in, yin[i], nr, (Color){ 200, 210, 230, 180 });
        /* Label dentro */
        int lw = MeasureText(in_lbl[i], 12);
        DrawText(in_lbl[i], cx_in - lw/2, yin[i] - 6, 12, WHITE);
        /* Nome fora (esquerda) */
        int nw = MeasureText(in_name[i], 10);
        DrawText(in_name[i], cx_in - nr - nw - 4, yin[i] - 5, 10, HC_DIM);
    }

    /* ---- Neuronios ocultos ---- */
    static const char *hid_lbl[MLP_H1] = { "h0", "h1", "h2", "h3" };
    for (int j = 0; j < MLP_H1; j++) {
        float act = mlp->a_h1[j];
        Color nc = nn_node_color(act, 1);
        DrawCircle(cx_hid, yhid[j], nr + 7, (Color){ nc.r, nc.g, nc.b, 35 });
        DrawCircle(cx_hid, yhid[j], nr + 4, (Color){ nc.r, nc.g, nc.b, 70 });
        DrawCircle(cx_hid, yhid[j], nr,     nc);
        DrawCircleLines(cx_hid, yhid[j], nr, (Color){ 200, 210, 230, 160 });
        int lw = MeasureText(hid_lbl[j], 10);
        DrawText(hid_lbl[j], cx_hid - lw/2, yhid[j] - 5, 10, WHITE);
    }

    /* ---- Neuronios de saida ---- */
    static const char *out_lbl[MLP_OUT]  = { "T+5", "L+5" };
    static const char *out_name[MLP_OUT] = { "Temp prevista", "Carga prevista" };
    for (int k = 0; k < MLP_OUT; k++) {
        float act = mlp->a_out[k];
        Color nc = nn_node_color(act, 2);
        DrawCircle(cx_out, yout[k], nr + 7, (Color){ nc.r, nc.g, nc.b, 35 });
        DrawCircle(cx_out, yout[k], nr + 4, (Color){ nc.r, nc.g, nc.b, 70 });
        DrawCircle(cx_out, yout[k], nr,     nc);
        DrawCircleLines(cx_out, yout[k], nr, (Color){ 200, 210, 230, 160 });
        int lw = MeasureText(out_lbl[k], 10);
        DrawText(out_lbl[k], cx_out - lw/2, yout[k] - 5, 10, WHITE);
        /* Nome à direita */
        DrawText(out_name[k], cx_out + nr + 4, yout[k] - 5, 10, HC_DIM);
    }

    /* ---- Legenda de cores das conexoes ---- */
    int ley = panel_y + ph - 42;
    DrawRectangle(panel_x + 16, ley - 4, 180, 36,
                  (Color){ 15, 17, 26, 220 });
    DrawRectangle(panel_x + 20, ley + 2,  28, 3,
                  (Color){ 55, 130, 255, 220 });
    DrawText("peso positivo", panel_x + 52, ley - 1, 10, HC_DIM);
    DrawRectangle(panel_x + 20, ley + 18, 28, 3,
                  (Color){ 255, 130, 40, 220 });
    DrawText("peso negativo", panel_x + 52, ley + 15, 10, HC_DIM);

    /* ---- Secao de informacao (direita) ---- */
    int iy = panel_y + 58;
    int ix = info_x;

    DrawText("ENTRADAS ATUAIS", ix, iy, 12, HC_GREEN);
    iy += 18;

    static const char *in_units[MLP_IN] = { "C", "%", "%" };
    float in_vals[MLP_IN] = {
        15.0f + mlp->a_in[0] * 40.0f,
        mlp->a_in[1] * 100.0f,
        mlp->a_in[2] * 100.0f
    };
    for (int i = 0; i < MLP_IN; i++) {
        DrawText(in_name[i], ix, iy, 12, HC_DIM);
        char vb[20];
        snprintf(vb, sizeof(vb), "%.1f %s", in_vals[i], in_units[i]);
        int vw = MeasureText(vb, 13);
        Color vc = nn_node_color(mlp->a_in[i], 0);
        DrawText(vb, ix + info_w - vw, iy, 13, vc);
        iy += 16;
    }

    iy += 10;
    DrawRectangle(ix, iy, info_w, 1, (Color){ 40, 44, 56, 255 });
    iy += 10;

    DrawText("PREVISAO (+5 s)", ix, iy, 12, HC_GREEN);
    iy += 18;

    /* Temperatura prevista */
    DrawText("Temperatura:", ix, iy, 12, HC_DIM);
    char pb[24];
    snprintf(pb, sizeof(pb), "%.1f C", mlp->pred_temp);
    int pw2 = MeasureText(pb, 15);
    Color tc = nn_node_color(mlp->a_out[0], 2);
    DrawText(pb, ix + info_w - pw2, iy - 1, 15, tc);
    iy += 20;

    /* Carga prevista */
    DrawText("Carga CPU:", ix, iy, 12, HC_DIM);
    snprintf(pb, sizeof(pb), "%.1f %%", mlp->pred_load);
    pw2 = MeasureText(pb, 15);
    Color lc2 = nn_node_color(mlp->a_out[1], 2);
    DrawText(pb, ix + info_w - pw2, iy - 1, 15, lc2);
    iy += 24;

    DrawRectangle(ix, iy, info_w, 1, (Color){ 40, 44, 56, 255 });
    iy += 10;

    DrawText("CAMADAS OCULTAS", ix, iy, 12, HC_GREEN);
    iy += 18;
    for (int j = 0; j < MLP_H1; j++) {
        char hb[32];
        snprintf(hb, sizeof(hb), "%s:", hid_lbl[j]);
        DrawText(hb, ix, iy, 11, HC_DIM);
        /* Barra de ativação */
        int bar_x = ix + 28;
        int bar_w = info_w - 28 - 34;
        DrawRectangle(bar_x, iy + 1, bar_w, 10, (Color){ 22, 26, 36, 255 });
        int fill = (int)(bar_w * mlp->a_h1[j]);
        Color hc = nn_node_color(mlp->a_h1[j], 1);
        if (fill > 0)
            DrawRectangle(bar_x, iy + 1, fill, 10, hc);
        DrawRectangleLines(bar_x, iy + 1, bar_w, 10,
                           (Color){ 42, 46, 58, 255 });
        char hpct[8];
        snprintf(hpct, sizeof(hpct), "%.0f%%",
                 mlp->a_h1[j] * 100.0f);
        DrawText(hpct, bar_x + bar_w + 4, iy, 10, HC_DIM);
        iy += 16;
    }

    iy += 6;
    DrawRectangle(ix, iy, info_w, 1, (Color){ 40, 44, 56, 255 });
    iy += 8;

    /* Nota sobre a arquitetura */
    DrawText("Arquitetura: 3-4-2 (MLP)", ix, iy, 10, HC_DIM);
    iy += 13;
    DrawText("Ativacao: tanh (oculta), sigmoid (saida)", ix, iy, 10, HC_DIM);
    iy += 13;
    DrawText("Complx: 3x4+4x2 = 20 pesos", ix, iy, 10, HC_DIM);

    /* Botao fechar */
    int cx2 = panel_x + pw - 30;
    int cy2 = panel_y + 10;
    DrawRectangle(cx2, cy2, 20, 20, (Color){ 50, 20, 20, 255 });
    DrawText("X", cx2 + 5, cy2 + 3, 13, HC_RED);
    Vector2 mp2 = GetMousePosition();
    if (mp2.x >= cx2 && mp2.x <= cx2 + 20 &&
        mp2.y >= cy2 && mp2.y <= cy2 + 20) {
        DrawRectangle(cx2, cy2, 20, 20, (Color){ 90, 30, 30, 100 });
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->nn_open = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) hud->nn_open = 0;
}

/* ================================================================== */
/* OVERLAY ARM vs AVR                                                   */
/* ================================================================== */

static void arm_metric(int *y, int x, int col_w,
                        const char *label,
                        const char *val_avr, const char *val_arm,
                        Color avr_col, Color arm_col)
{
    DrawText(label, x, *y, 12, HC_DIM);
    DrawText(val_avr, x + col_w,       *y, 12, avr_col);
    DrawText(val_arm, x + col_w * 2,   *y, 12, arm_col);
    *y += 17;
}

static void arm_bar_row(int *y, int x, int col_w,
                         const char *label,
                         float avr_us, float arm_us,
                         Color avr_col, Color arm_col)
{
    DrawText(label, x, *y, 12, HC_DIM);

    /* Barras com escala raiz quadrada para visualizacao legivel */
    float sa = sqrtf(avr_us);
    float sb = sqrtf(arm_us);
    float max_s = (sa > sb) ? sa : sb;
    if (max_s < 0.001f) max_s = 1.0f;

    int bar_max = 130;
    int avr_bar = (int)(bar_max * sa / max_s);
    int arm_bar = (int)(bar_max * sb / max_s);
    if (arm_bar < 3) arm_bar = 3;

    int bx_avr = x + col_w;
    int bx_arm = x + col_w * 2;

    DrawRectangle(bx_avr, *y + 1, avr_bar, 11, avr_col);
    DrawRectangle(bx_arm, *y + 1, arm_bar,  11, arm_col);

    char avr_s[20], arm_s[20];
    if (avr_us >= 1000.0f)
        snprintf(avr_s, sizeof(avr_s), "%.2f ms", avr_us / 1000.0f);
    else
        snprintf(avr_s, sizeof(avr_s), "%.0f us", avr_us);
    if (arm_us >= 1000.0f)
        snprintf(arm_s, sizeof(arm_s), "%.2f ms", arm_us / 1000.0f);
    else
        snprintf(arm_s, sizeof(arm_s), "%.0f us", arm_us);

    DrawText(avr_s, bx_avr + avr_bar + 4, *y, 10,
             (Color){ avr_col.r, avr_col.g, avr_col.b, 200 });
    DrawText(arm_s, bx_arm + arm_bar + 4, *y, 10,
             (Color){ arm_col.r, arm_col.g, arm_col.b, 200 });

    *y += 18;
}

static void draw_arm_overlay(HudState *hud, int px, int sh)
{
    DrawRectangle(0, 0, px, sh, (Color){ 5, 7, 12, 200 });

    int pw = (px - 40 < 720) ? px - 40 : 720;
    int ph = 468;
    int panel_x = (px - pw) / 2;
    int panel_y = (sh - ph) / 2;

    DrawRectangle(panel_x, panel_y, pw, ph, (Color){ 10, 12, 20, 252 });
    DrawRectangleLines(panel_x, panel_y, pw, ph,
                       (Color){ 50, 55, 75, 255 });
    DrawRectangle(panel_x, panel_y, pw, 3, HC_BLUE);

    DrawText("AJUDA  —", panel_x + 20, panel_y + 14, 16, HC_DIM);
    DrawText("COMPARATIVO: Arduino AVR  vs  ARM Cortex-M4",
             panel_x + 20 + MeasureText("AJUDA  — ", 16),
             panel_y + 14, 16, HC_TITLE);

    DrawRectangle(panel_x + 16, panel_y + 44,
                  pw - 32, 1, (Color){ 40, 44, 56, 255 });

    int mx   = panel_x + 20;
    int y    = panel_y + 54;
    int cw   = (pw - 40) / 3;   /* largura de cada coluna */

    Color avr_col = (Color){ 255, 165, 50,  255 };
    Color arm_col = (Color){  60, 200, 120, 255 };

    /* Cabecalho das colunas */
    DrawText("", mx, y, 12, HC_DIM);
    DrawText("Arduino ATmega2560", mx + cw,     y, 13, avr_col);
    DrawText("ARM Cortex-M4",      mx + cw * 2, y, 13, arm_col);
    y += 22;

    DrawRectangle(mx, y, pw - 40, 1, (Color){ 35, 38, 50, 255 });
    y += 10;

    /* --- Especificacoes --- */
    DrawRectangle(mx, y, 4, 12, HC_BLUE);
    DrawText("ESPECIFICACOES DE HARDWARE", mx + 8, y, 12, HC_BLUE);
    y += 18;

    arm_metric(&y, mx, cw, "Arquitetura  :",
               "AVR RISC 8-bit",    "ARMv7-M 32-bit",
               avr_col, arm_col);
    arm_metric(&y, mx, cw, "Clock        :",
               "16 MHz",            "168 MHz (STM32F4)",
               avr_col, arm_col);
    arm_metric(&y, mx, cw, "FPU hardware :",
               "Nao (soft float)",  "Sim (SP + DSP)",
               (Color){ 255, 90, 80, 255 }, arm_col);
    arm_metric(&y, mx, cw, "RAM total    :",
               "8 KB",              "192 KB",
               avr_col, arm_col);
    arm_metric(&y, mx, cw, "Flash total  :",
               "256 KB",            "1 024 KB",
               avr_col, arm_col);

    y += 4;
    DrawRectangle(mx, y, pw - 40, 1, (Color){ 35, 38, 50, 255 });
    y += 10;

    /* --- Tempo de execucao por ciclo (500 ms) --- */
    DrawRectangle(mx, y, 4, 12, HC_YEL);
    DrawText("TEMPO DE EXECUCAO POR CICLO DE CONTROLE (500 ms)",
             mx + 8, y, 12, HC_YEL);
    y += 18;

    DrawText("(barras em escala raiz quadrada para legibilidade)",
             mx, y, 10, (Color){ 75, 80, 100, 255 });
    y += 14;

    /* Cabecalho das barras */
    DrawText("Tarefa", mx, y, 11, HC_DIM);
    DrawText("AVR", mx + cw, y, 11, avr_col);
    DrawText("ARM", mx + cw * 2, y, 11, arm_col);
    y += 14;

    arm_bar_row(&y, mx, cw, "Fuzzy Mamdani :",
                1150.0f, 10.0f, avr_col, arm_col);
    arm_bar_row(&y, mx, cw, "MLP forward   :",
                 430.0f, 25.0f, avr_col, arm_col);
    arm_bar_row(&y, mx, cw, "Total IA      :",
                1580.0f, 35.0f, avr_col, arm_col);

    /* Headroom */
    y += 2;
    DrawText("Headroom disponivel:", mx, y, 12, HC_DIM);
    DrawText("~498 ms (99.7%)", mx + cw,     y, 12, avr_col);
    DrawText("~499.96 ms (99.99%)", mx + cw * 2, y, 12, arm_col);
    y += 20;

    /* Razao de velocidade */
    DrawRectangle(mx, y, pw - 40, 1, (Color){ 35, 38, 50, 255 });
    y += 8;
    DrawText("Ganho ARM vs AVR:", mx, y, 12, HC_DIM);
    DrawText("Fuzzy: 115x mais rapido   |   MLP: 17x mais rapido",
             mx + MeasureText("Ganho ARM vs AVR:", 12) + 10,
             y, 12, arm_col);
    y += 20;

    DrawRectangle(mx, y, pw - 40, 1, (Color){ 35, 38, 50, 255 });
    y += 10;

    /* --- O que ARM habilita --- */
    DrawRectangle(mx, y, 4, 12, HC_GREEN);
    DrawText("O QUE O ARM CORTEX-M4 HABILITA ALEM DO AVR",
             mx + 8, y, 12, HC_GREEN);
    y += 18;

    static const char *extras[] = {
        "  Comunicacao Ethernet / MQTT em tempo real sem impacto no controle",
        "  Registro em SD card de historico completo (temperatura, fan, PDU)",
        "  Modelos de IA maiores: ANFIS, redes recorrentes (LSTM), ensemble",
        "  RTOS multitarefa (FreeRTOS): sensor, controle, comms em paralelo",
        "  Atualizacao de firmware OTA (Over-The-Air) via rede",
    };
    for (int i = 0; i < 5; i++) {
        DrawText(extras[i], mx, y, 11, HC_DIM);
        y += 14;
    }

    /* Botao fechar */
    int cx2 = panel_x + pw - 30;
    int cy2 = panel_y + 10;
    DrawRectangle(cx2, cy2, 20, 20, (Color){ 50, 20, 20, 255 });
    DrawText("X", cx2 + 5, cy2 + 3, 13, HC_RED);
    Vector2 mp2 = GetMousePosition();
    if (mp2.x >= cx2 && mp2.x <= cx2 + 20 &&
        mp2.y >= cy2 && mp2.y <= cy2 + 20) {
        DrawRectangle(cx2, cy2, 20, 20, (Color){ 90, 30, 30, 100 });
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->arm_open = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) hud->arm_open = 0;
}

/* ================================================================== */
/* OVERLAY: COMPARATIVO FUZZY vs HISTERESE                              */
/* ================================================================== */

static void draw_compare_overlay(HudState *hud, const CompareState *cmp,
                                  int px, int sh)
{
    DrawRectangle(0, 0, px, sh, (Color){ 5, 7, 12, 200 });

    int pw = (px - 40 < 680) ? px - 40 : 680;
    int ph = 420;
    int panel_x = (px - pw) / 2;
    int panel_y = (sh - ph) / 2;

    DrawRectangle(panel_x, panel_y, pw, ph, (Color){ 10, 12, 20, 252 });
    DrawRectangleLines(panel_x, panel_y, pw, ph, (Color){ 50, 55, 75, 255 });
    DrawRectangle(panel_x, panel_y, pw, 3, HC_YEL);

    DrawText("AJUDA  —", panel_x + 20, panel_y + 14, 16, HC_DIM);
    DrawText("FUZZY vs HISTERESE",
             panel_x + 20 + MeasureText("AJUDA  — ", 16),
             panel_y + 14, 16, HC_TITLE);

    DrawRectangle(panel_x + 16, panel_y + 44,
                  pw - 32, 1, (Color){ 40, 44, 56, 255 });

    int gx = panel_x + 16;
    int gw = pw - 32;
    int gy = panel_y + 52;

    int total = (cmp->idx < CMP_HIST) ? cmp->idx : CMP_HIST;
    int head  = cmp->idx % CMP_HIST;

    Color fuzzy_col = (Color){  40, 210, 180, 220 };   /* teal   */
    Color hyst_col  = (Color){ 240, 140,  40, 220 };   /* laranja*/
    Color fblue     = (Color){  55, 160, 255, 200 };   /* azul   */
    Color hred      = (Color){ 240,  60,  60, 200 };   /* vermelho*/

    /* ---- Gráfico de temperatura ---- */
    DrawText("TEMPERATURA MEDIA (C)", gx, gy, 11, HC_DIM);
    gy += 14;

    Rectangle gr_t = { (float)gx, (float)gy, (float)gw, 110.0f };
    DrawRectangleRec(gr_t, (Color){ 14, 16, 24, 255 });
    DrawRectangleLinesEx(gr_t, 1, (Color){ 38, 42, 54, 255 });

    if (total >= 2) {
        draw_sparkline_n(cmp->fuzzy_temp, CMP_HIST, total, head,
                         gr_t, 18.0f, 55.0f, fuzzy_col, 1.8f);
        draw_sparkline_n(cmp->hyst_temp, CMP_HIST, total, head,
                         gr_t, 18.0f, 55.0f, hyst_col, 1.8f);
    }

    /* Limiares de histerese */
    float y40 = gr_t.y + gr_t.height * (1.0f - (40.0f - 18.0f) / 37.0f);
    float y37 = gr_t.y + gr_t.height * (1.0f - (37.0f - 18.0f) / 37.0f);
    DrawLine(gx, (int)y40, gx + gw, (int)y40, (Color){ 255, 80, 60, 60 });
    DrawLine(gx, (int)y37, gx + gw, (int)y37, (Color){ 255, 200, 50, 40 });
    DrawText("40C", gx + gw - 24, (int)y40 - 10, 9, (Color){ 255, 80, 60, 120 });
    DrawText("37C", gx + gw - 24, (int)y37 + 2,  9, (Color){ 255, 200, 50, 100 });

    gy += 118;

    /* Legenda temperatura */
    DrawRectangle(gx,      gy + 3, 22, 3, fuzzy_col);
    DrawText("Fuzzy",  gx + 26, gy, 11, fuzzy_col);
    DrawRectangle(gx + 80, gy + 3, 22, 3, hyst_col);
    DrawText("Histerese", gx + 106, gy, 11, hyst_col);

    /* Valores atuais */
    int last = (head - 1 + CMP_HIST) % CMP_HIST;
    if (total > 0) {
        char vb[32];
        snprintf(vb, sizeof(vb), "Fuzzy: %.1fC", cmp->fuzzy_temp[last]);
        DrawText(vb, gx + gw - 210, gy, 11, fuzzy_col);
        snprintf(vb, sizeof(vb), "Hist: %.1fC", cmp->hyst_temp[last]);
        DrawText(vb, gx + gw - 100, gy, 11, hyst_col);
    }
    gy += 18;

    DrawRectangle(gx, gy, gw, 1, (Color){ 35, 38, 50, 255 });
    gy += 10;

    /* ---- Gráfico de fan speed ---- */
    DrawText("VELOCIDADE DO FAN (%)", gx, gy, 11, HC_DIM);
    gy += 14;

    Rectangle gr_f = { (float)gx, (float)gy, (float)gw, 80.0f };
    DrawRectangleRec(gr_f, (Color){ 14, 16, 24, 255 });
    DrawRectangleLinesEx(gr_f, 1, (Color){ 38, 42, 54, 255 });

    if (total >= 2) {
        draw_sparkline_n(cmp->fuzzy_fan, CMP_HIST, total, head,
                         gr_f, 0.0f, 100.0f, fblue, 1.6f);
        draw_sparkline_n(cmp->hyst_fan,  CMP_HIST, total, head,
                         gr_f, 0.0f, 100.0f, hred,  1.6f);
    }

    gy += 88;

    DrawRectangle(gx,       gy + 3, 22, 3, fblue);
    DrawText("Fuzzy",   gx + 26, gy, 11, fblue);
    DrawRectangle(gx + 80,  gy + 3, 22, 3, hred);
    DrawText("Histerese", gx + 106, gy, 11, hred);
    gy += 16;

    DrawRectangle(gx, gy, gw, 1, (Color){ 35, 38, 50, 255 });
    gy += 8;

    /* ---- Explicação textual ---- */
    hline(&gy, gx, "Fuzzy: o fan sobe gradualmente conforme a temperatura aumenta — sem oscilacoes.", HC_DIM, 11);
    hline(&gy, gx, "Histerese: o fan alterna entre 100% e 15% ao cruzar os limiares 40 C / 37 C.", HC_DIM, 11);
    hline(&gy, gx, "Resultado: o fuzzy usa menos energia media, reduz desgaste mecanico e elimina 'batidas'.", HC_YEL, 11);

    /* Botão fechar */
    int cx2 = panel_x + pw - 30;
    int cy2 = panel_y + 10;
    DrawRectangle(cx2, cy2, 20, 20, (Color){ 50, 20, 20, 255 });
    DrawText("X", cx2 + 5, cy2 + 3, 13, HC_RED);
    Vector2 mp2 = GetMousePosition();
    if (mp2.x >= cx2 && mp2.x <= cx2 + 20 &&
        mp2.y >= cy2 && mp2.y <= cy2 + 20) {
        DrawRectangle(cx2, cy2, 20, 20, (Color){ 90, 30, 30, 100 });
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->compare_open = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) hud->compare_open = 0;
}

/* ================================================================== */
/* OVERLAY: MAPA DE CALOR (VISAO SUPERIOR)                              */
/* ================================================================== */

/* Gradiente de temperatura: frio=azul → quente=vermelho */
static Color heat_color(float t)
{
    typedef struct { float t; unsigned char r, g, b; } Stop;
    static const Stop s[] = {
        { 18.0f,  20,  40, 180 },
        { 26.0f,  20, 180, 220 },
        { 32.0f,  20, 180,  80 },
        { 37.0f, 200, 180,  20 },
        { 42.0f, 240, 120,  20 },
        { 50.0f, 240,  30,  20 },
    };
    int n = 6;
    if (t <= s[0].t)   return (Color){ s[0].r,   s[0].g,   s[0].b,   200 };
    if (t >= s[n-1].t) return (Color){ s[n-1].r, s[n-1].g, s[n-1].b, 200 };
    for (int i = 0; i < n - 1; i++) {
        if (t <= s[i+1].t) {
            float f = (t - s[i].t) / (s[i+1].t - s[i].t);
            return (Color){
                (unsigned char)(s[i].r + f * (s[i+1].r - s[i].r)),
                (unsigned char)(s[i].g + f * (s[i+1].g - s[i].g)),
                (unsigned char)(s[i].b + f * (s[i+1].b - s[i].b)),
                200
            };
        }
    }
    return (Color){ 240, 30, 20, 200 };
}

static void draw_heatmap_overlay(HudState *hud, const SensorState *s,
                                  int px, int sh)
{
    DrawRectangle(0, 0, px, sh, (Color){ 5, 7, 12, 200 });

    int pw = (px - 40 < 540) ? px - 40 : 540;
    int ph = 380;
    int panel_x = (px - pw) / 2;
    int panel_y = (sh - ph) / 2;

    DrawRectangle(panel_x, panel_y, pw, ph, (Color){ 10, 12, 20, 252 });
    DrawRectangleLines(panel_x, panel_y, pw, ph, (Color){ 50, 55, 75, 255 });
    DrawRectangle(panel_x, panel_y, pw, 3, HC_RED);

    DrawText("AJUDA  —", panel_x + 20, panel_y + 14, 16, HC_DIM);
    DrawText("MAPA DE CALOR — VISAO SUPERIOR",
             panel_x + 20 + MeasureText("AJUDA  — ", 16),
             panel_y + 14, 16, HC_TITLE);
    DrawRectangle(panel_x + 16, panel_y + 44,
                  pw - 32, 1, (Color){ 40, 44, 56, 255 });

    /* ---- Grade IDW ----
     * Sala: X ∈ [-12, +12], Z ∈ [-8, +8] → 24×16 unidades.
     * Células: 48×32 a 7 px cada = 336×224 px.                        */
    const int GCOLS  = 48;
    const int GROWS  = 32;
    const int CPXL   = 7;    /* pixels por célula */

    int grid_w = GCOLS * CPXL;  /* 336 */
    int grid_h = GROWS * CPXL;  /* 224 */
    int grid_x = panel_x + (pw - grid_w) / 2;
    int grid_y = panel_y + 54;

    /* Posições 2D dos racks: colunas X e linhas Z */
    static const float RX[4] = { -8.5f, -5.5f, -2.5f, 0.5f };
    static const float RZ[2] = { -2.8f, 2.8f };

    /* IDW e desenho célula a célula */
    for (int gz = 0; gz < GROWS; gz++) {
        float wz = -8.0f + (gz + 0.5f) * 16.0f / GROWS;
        for (int gx2 = 0; gx2 < GCOLS; gx2++) {
            float wx = -12.0f + (gx2 + 0.5f) * 24.0f / GCOLS;

            float sum_w = 0.0f, sum_wt = 0.0f;
            for (int r = 0; r < NUM_RACKS; r++) {
                int   ci = r % 4;
                int   ri = r / 4;
                float dx = wx - RX[ci];
                float dz = wz - RZ[ri];
                float d2 = dx * dx + dz * dz;
                if (d2 < 0.04f) d2 = 0.04f;
                float w  = 1.0f / d2;
                sum_w  += w;
                sum_wt += w * s->racks[r].temperature;
            }
            float t = (sum_w > 0.0f) ? sum_wt / sum_w : 25.0f;
            Color c = heat_color(t);
            DrawRectangle(grid_x + gx2 * CPXL,
                          grid_y + gz  * CPXL, CPXL, CPXL, c);
        }
    }

    /* Borda da grade */
    DrawRectangleLines(grid_x, grid_y, grid_w, grid_h,
                       (Color){ 60, 65, 85, 220 });

    /* ---- Overlay: racks ---- */
    float scale_x = (float)(GCOLS * CPXL) / 24.0f;  /* px/unit */
    float scale_z = (float)(GROWS * CPXL) / 16.0f;

    for (int r = 0; r < NUM_RACKS; r++) {
        int  ci = r % 4;
        int  ri = r / 4;
        float wx = RX[ci];
        float wz = RZ[ri];

        int rx_px = grid_x + (int)((wx + 12.0f) * scale_x);
        int rz_px = grid_y + (int)((wz +  8.0f) * scale_z);

        int rw = (int)(0.9f * scale_x);
        int rh = (int)(0.5f * scale_z);

        DrawRectangleLines(rx_px - rw/2, rz_px - rh/2, rw, rh,
                           (Color){ 200, 210, 230, 200 });

        char rb[4];
        snprintf(rb, sizeof(rb), "R%d", r);
        int   tw = MeasureText(rb, 9);
        Color tc = s->racks[r].alert ? HC_RED : (Color){ 210, 220, 240, 220 };
        DrawText(rb, rx_px - tw/2, rz_px - 5, 9, tc);
    }

    /* ---- Overlay: CRACs ---- */
    static const float CX[2] = { -7.0f, 3.5f };
    static const float CZ_W  = -7.45f;
    for (int c2 = 0; c2 < 2; c2++) {
        int cpx = grid_x + (int)((CX[c2] + 12.0f) * scale_x);
        int cpz = grid_y + (int)((CZ_W   +  8.0f) * scale_z);
        int cw2 = (int)(1.3f * scale_x);
        int ch2 = (int)(0.7f * scale_z);
        Color cc = (hud->crac_state == c2 + 1)
            ? (Color){ 240, 60, 60, 220 } : (Color){ 60, 200, 120, 220 };
        DrawRectangle(cpx - cw2/2, cpz - ch2/2, cw2, ch2, (Color){ cc.r, cc.g, cc.b, 60 });
        DrawRectangleLines(cpx - cw2/2, cpz - ch2/2, cw2, ch2, cc);
        DrawText("CRAC", cpx - MeasureText("CRAC",8)/2, cpz - 4, 8, cc);
    }

    /* ---- Corredor frio ---- */
    float ca_x1 = -9.0f, ca_x2 = 1.0f, ca_z1 = -1.4f, ca_z2 = 1.4f;
    int ca_px = grid_x + (int)((ca_x1 + 12.0f) * scale_x);
    int ca_pz = grid_y + (int)((ca_z1 +  8.0f) * scale_z);
    int ca_pw = (int)((ca_x2 - ca_x1) * scale_x);
    int ca_ph = (int)((ca_z2 - ca_z1) * scale_z);
    DrawRectangleLines(ca_px, ca_pz, ca_pw, ca_ph, (Color){ 60, 120, 240, 160 });
    DrawText("CORREDOR FRIO", ca_px + 4, ca_pz + ca_ph/2 - 4, 8,
             (Color){ 60, 120, 240, 180 });

    /* ---- Rótulos de parede ---- */
    DrawText("N", grid_x + grid_w/2 - 3, grid_y - 14, 11, HC_DIM);
    DrawText("S", grid_x + grid_w/2 - 3, grid_y + grid_h + 2, 11, HC_DIM);

    /* ---- Legenda de cores ---- */
    int ley = grid_y + grid_h + 18;
    static const char *leg_lbl[] = {
        "<26", "26-32", "32-37", "37-42", "42-50", ">50"
    };
    static const float leg_t[] = { 22.0f, 29.0f, 34.5f, 39.5f, 46.0f, 52.0f };
    int lsz = 14;
    int ltotal = 6;
    int leg_start = panel_x + (pw - ltotal*(lsz+28)) / 2;
    for (int i = 0; i < ltotal; i++) {
        int lx = leg_start + i * (lsz + 28);
        DrawRectangle(lx, ley, lsz, lsz, heat_color(leg_t[i]));
        DrawText(leg_lbl[i], lx + lsz + 2, ley + 1, 9, HC_DIM);
    }

    /* Botão fechar */
    int cx2 = panel_x + pw - 30;
    int cy2 = panel_y + 10;
    DrawRectangle(cx2, cy2, 20, 20, (Color){ 50, 20, 20, 255 });
    DrawText("X", cx2 + 5, cy2 + 3, 13, HC_RED);
    Vector2 mp2 = GetMousePosition();
    if (mp2.x >= cx2 && mp2.x <= cx2 + 20 &&
        mp2.y >= cy2 && mp2.y <= cy2 + 20) {
        DrawRectangle(cx2, cy2, 20, 20, (Color){ 90, 30, 30, 100 });
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) hud->heatmap_open = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) hud->heatmap_open = 0;
}

/* ------------------------------------------------------------------ */
/* API publica                                                           */
/* ------------------------------------------------------------------ */

void hud_init(HudState *hud)
{
    memset(hud, 0, sizeof(*hud));
    hud->manual_fan_speed = 50.0f;
    hud->manual_pdu       = 50.0f;

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,  0x14161eff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x1e2030ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xc8cad8ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x34364aff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x282a40ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x32345aff);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 13);
}

void hud_log(HudState *hud, const char *msg)
{
    int idx = (hud->log_head + hud->log_count) % HUD_LOG_LINES;
    strncpy(hud->log_lines[idx], msg, HUD_LOG_LEN - 1);
    hud->log_lines[idx][HUD_LOG_LEN - 1] = '\0';
    if (hud->log_count < HUD_LOG_LINES)
        hud->log_count++;
    else
        hud->log_head = (hud->log_head + 1) % HUD_LOG_LINES;
}

void hud_update(HudState *hud, const SensorState *s, float fan_speed, float pdu)
{
    int idx = hud->history_idx % HUD_HISTORY_LEN;
    hud->temp_history[idx] = sensors_avg_temp(s);
    hud->load_history[idx] = sensors_avg_load(s);
    hud->fan_history [idx] = fan_speed;
    hud->history_idx++;
    (void)pdu;
}

void hud_draw(HudState *hud, const SensorState *s, const FuzzyEngine *eng,
              const MlpNet *mlp, const CompareState *cmp,
              float fan_speed, float pdu_balance)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int px = PX();
    int iw = PANEL_W - 16;   /* inner width */
    int x  = px + 8;
    int y  = 0;

    /* ---- Fundo do painel ---- */
    DrawRectangle(px, 0, PANEL_W, sh, (Color){ 13, 14, 20, 218 });
    DrawRectangle(px, 0, 1, sh,       (Color){ 40, 44, 56, 255 });

    /* ================================================================
     * 1. BARRA DE STATUS (topo)
     * ================================================================ */
    int hot_rack  = sensors_hottest_rack(s);
    float hot_t   = s->racks[hot_rack].temperature;
    int any_alert = 0;
    for (int i = 0; i < NUM_RACKS; i++) if (s->racks[i].alert) any_alert = 1;

    Color  sc;
    const char *stxt;
    if (any_alert) {
        sc = (Color){ 190, 35, 25, 255 };
        stxt = "ALERTA  —  temperatura critica!";
    } else if (hot_t > 38.0f) {
        sc = (Color){ 185, 130, 15, 255 };
        stxt = "ATENCAO  —  temperatura elevada";
    } else {
        sc = (Color){ 25, 145, 55, 255 };
        stxt = "NORMAL  —  sistema estavel";
    }

    DrawRectangle(px, y, PANEL_W, 38, sc);
    /* Indicador circular */
    float pulse = any_alert ? (sinf(GetTime() * 5.0f) > 0 ? 1.0f : 0.5f) : 1.0f;
    DrawCircle(x + 8, y + 19, 6.0f,
               (Color){ (unsigned char)(255*pulse), (unsigned char)(255*pulse),
                        (unsigned char)(255*pulse), 220 });
    DrawText(stxt, x + 20, y + 12, 13, WHITE);

    /* FPS discreto no canto da status bar */
    char fps_buf[12];
    snprintf(fps_buf, sizeof(fps_buf), "%d fps", GetFPS());
    DrawText(fps_buf, px + PANEL_W - MeasureText(fps_buf, 10) - 6, y + 14, 10,
             (Color){ 255, 255, 255, 100 });
    y += 44;

    /* ================================================================
     * 2. TIRA DE RACKS
     * ================================================================ */
    section_hdr(&y, "TEMPERATURA DOS RACKS", (Color){ 75, 125, 255, 255 });

    int gap   = 3;
    int bw    = (iw - gap * 7) / 8;
    int bh    = 42;
    for (int i = 0; i < NUM_RACKS; i++) {
        int bx = x + i * (bw + gap);
        float t = s->racks[i].temperature;
        Color bc = rack_col(t);

        /* Fundo escurecido */
        DrawRectangle(bx, y, bw, bh,
                      (Color){ (unsigned char)(bc.r/5), (unsigned char)(bc.g/5),
                               (unsigned char)(bc.b/5), 255 });
        /* Bloco colorido (3/4 superiores) */
        DrawRectangle(bx, y, bw, bh - 14, bc);

        /* Piscagem de alerta */
        if (s->racks[i].alert) {
            unsigned char a = (sinf(GetTime() * 6.0f) > 0.0f) ? 160 : 0;
            DrawRectangle(bx, y, bw, bh - 14, (Color){ 255, 255, 255, a });
        }

        /* Temperatura centralizada */
        char tmp[6];
        snprintf(tmp, sizeof(tmp), "%.0f", t);
        int tw = MeasureText(tmp, 10);
        DrawText(tmp, bx + (bw - tw) / 2, y + (bh - 14) / 2 - 5, 10, WHITE);

        /* Rótulo do rack */
        char rid[4];
        snprintf(rid, sizeof(rid), "R%d", i);
        int rw = MeasureText(rid, 9);
        DrawText(rid, bx + (bw - rw) / 2, y + bh - 12, 9,
                 (Color){ 140, 145, 160, 255 });
    }
    y += bh + 6;

    /* Rack mais quente em destaque */
    char hot_buf[48];
    snprintf(hot_buf, sizeof(hot_buf), "Mais quente: Rack #%d  (%.1f C)", hot_rack, hot_t);
    DrawText(hot_buf, x, y, 12, rack_col(hot_t));
    y += 17;

    sep(&y, 14);

    /* ================================================================
     * 3. CONTROLADOR FUZZY — NARRATIVA
     * ================================================================ */
    section_hdr(&y, "CONTROLADOR FUZZY", (Color){ 55, 195, 115, 255 });

    int tdom = dominant(eng->mu_in[FUZZY_IN_TEMP]);
    int ldom = dominant(eng->mu_in[FUZZY_IN_LOAD]);
    float avg_t = sensors_avg_temp(s);
    float avg_l = sensors_avg_load(s);

    /* Linha de temperatura */
    DrawText("Temperatura:", x, y, 13, (Color){ 130, 135, 150, 255 });
    DrawText(PT_UPPER[tdom], x + 102, y, 13, label_col(tdom));
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "  %.1fC", avg_t);
    DrawText(vbuf, x + 102 + MeasureText(PT_UPPER[tdom], 13), y, 11,
             (Color){ 110, 115, 130, 255 });
    y += 17;

    /* Linha de carga */
    DrawText("Carga CPU:", x, y, 13, (Color){ 130, 135, 150, 255 });
    DrawText(PT_UPPER[ldom], x + 102, y, 13, label_col(ldom));
    snprintf(vbuf, sizeof(vbuf), "  %.1f%%", avg_l);
    DrawText(vbuf, x + 102 + MeasureText(PT_UPPER[ldom], 13), y, 11,
             (Color){ 110, 115, 130, 255 });
    y += 18;

    /* Caixa de narrativa */
    const char *nl1, *nl2;
    narrative(tdom, ldom, fan_speed, hot_rack, hot_t, &nl1, &nl2);

    int narr_h = 46;
    DrawRectangle(x, y, iw, narr_h, (Color){ 20, 23, 32, 255 });
    DrawRectangle(x, y, 2,  narr_h, label_col(tdom));
    DrawText(nl1, x + 8, y + 7,  12, WHITE);
    DrawText(nl2, x + 8, y + 25, 12, (Color){ 160, 165, 180, 255 });
    y += narr_h + 8;

    /* Minigráfico: temperatura (vermelho) vs fan (azul) — causa e efeito */
    DrawText("Historico: temp (verm) / fan (azul)", x, y, 10,
             (Color){ 100, 105, 120, 255 });
    y += 12;
    int cnt = hud->history_idx < HUD_HISTORY_LEN ? hud->history_idx : HUD_HISTORY_LEN;
    int head = hud->history_idx % HUD_HISTORY_LEN;
    Rectangle gr = { (float)x, (float)y, (float)iw, 44.0f };
    DrawRectangleRec(gr, (Color){ 16, 18, 26, 255 });
    DrawRectangleLinesEx(gr, 1, (Color){ 40, 43, 54, 255 });
    draw_sparkline(hud->temp_history, cnt, head, gr, 20.0f, 55.0f,
                   (Color){ 255, 85, 55, 210 }, 1.5f);
    draw_sparkline(hud->fan_history,  cnt, head, gr, 0.0f,  100.0f,
                   (Color){ 55, 160, 255, 200 }, 1.5f);
    /* Rótulos de escala */
    DrawText("55C", x + iw - 26, y + 1,  8, (Color){ 255, 85, 55, 130 });
    DrawText("20C", x + iw - 26, y + 35, 8, (Color){ 255, 85, 55, 130 });
    y += 50;

    sep(&y, 12);

    /* ================================================================
     * 4. ATUADORES
     * ================================================================ */
    section_hdr(&y, "ATUADORES", (Color){ 95, 155, 255, 255 });

    /* Fan */
    DrawText("Ventiladores (Fan)", x, y, 13, (Color){ 160, 165, 180, 255 });
    char pct[14];
    snprintf(pct, sizeof(pct), "%.0f%%", fan_speed);
    DrawText(pct, x + iw - MeasureText(pct, 16), y - 1, 16, fan_col(fan_speed));
    y += 18;
    pbar(x, y, iw, 14, fan_speed, fan_col(fan_speed));
    y += 22;

    /* PDU */
    DrawText("Balanceamento PDU", x, y, 13, (Color){ 160, 165, 180, 255 });
    snprintf(pct, sizeof(pct), "%.0f%%", pdu_balance);
    DrawText(pct, x + iw - MeasureText(pct, 16), y - 1, 16,
             (Color){ 200, 158, 75, 255 });
    y += 18;
    pbar(x, y, iw, 14, pdu_balance, (Color){ 200, 158, 75, 255 });
    y += 22;

    sep(&y, 12);

    /* ================================================================
     * 5. MODO DE OPERACAO
     * ================================================================ */
    section_hdr(&y, "MODO DE OPERACAO", (Color){ 155, 95, 255, 255 });

    int tw2 = (iw - 6) / 2;
    Color auto_bg = hud->manual_override
        ? (Color){ 30, 33, 42, 255 } : (Color){ 22, 100, 45, 255 };
    Color man_bg  = hud->manual_override
        ? (Color){ 140, 65, 15, 255 } : (Color){ 30, 33, 42, 255 };

    DrawRectangle(x,          y, tw2, 26, auto_bg);
    DrawRectangle(x + tw2 + 6, y, tw2, 26, man_bg);
    DrawRectangleLines(x,          y, tw2, 26, (Color){ 52, 55, 66, 255 });
    DrawRectangleLines(x + tw2 + 6, y, tw2, 26, (Color){ 52, 55, 66, 255 });

    const char *al = "AUTOMATICO", *ml = "MANUAL";
    DrawText(al, x + (tw2 - MeasureText(al, 11)) / 2, y + 7, 11,
             hud->manual_override ? (Color){ 80, 85, 98, 255 } : WHITE);
    DrawText(ml, x + tw2 + 6 + (tw2 - MeasureText(ml, 11)) / 2, y + 7, 11,
             hud->manual_override ? WHITE : (Color){ 80, 85, 98, 255 });

    Vector2 mp = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (mp.x >= x && mp.x < x + tw2 && mp.y >= y && mp.y < y + 26)
            hud->manual_override = 0;
        if (mp.x >= x + tw2 + 6 && mp.x < x + tw2 + 6 + tw2 &&
            mp.y >= y && mp.y < y + 26)
            hud->manual_override = 1;
    }
    y += 32;

    if (hud->manual_override) {
        DrawText("Fan:", x, y + 3, 12, (Color){ 130, 135, 150, 255 });
        GuiSlider((Rectangle){ (float)(x + 38), (float)y,
                               (float)(iw - 72), 18.0f },
                  NULL, NULL, &hud->manual_fan_speed, 0.0f, 100.0f);
        snprintf(pct, sizeof(pct), "%.0f%%", hud->manual_fan_speed);
        DrawText(pct, x + iw - MeasureText(pct, 12), y + 2, 12, WHITE);
        y += 26;

        DrawText("PDU:", x, y + 3, 12, (Color){ 130, 135, 150, 255 });
        GuiSlider((Rectangle){ (float)(x + 38), (float)y,
                               (float)(iw - 72), 18.0f },
                  NULL, NULL, &hud->manual_pdu, 0.0f, 100.0f);
        snprintf(pct, sizeof(pct), "%.0f%%", hud->manual_pdu);
        DrawText(pct, x + iw - MeasureText(pct, 12), y + 2, 12, WHITE);
        y += 26;
    }

    sep(&y, 12);

    /* ================================================================
     * 6. LOG DE EVENTOS (ultimas 4 linhas)
     * ================================================================ */
    section_hdr(&y, "LOG DE EVENTOS", (Color){ 110, 115, 135, 255 });

    int show = hud->log_count < 4 ? hud->log_count : 4;
    for (int i = 0; i < show; i++) {
        int idx = ((hud->log_head + hud->log_count - 1 - i)
                   % HUD_LOG_LINES + HUD_LOG_LINES) % HUD_LOG_LINES;
        Color lc = (i == 0) ? (Color){ 195, 200, 215, 255 }
                             : (Color){ 100, 105, 120, 255 };
        DrawText(hud->log_lines[idx], x, y, 11, lc);
        y += 14;
    }

    y += 8;
    sep(&y, 2);

    /* ================================================================
     * 7. DICA DE CAMARA (rodapé)
     * ================================================================ */
    const char *hint = "Btn Dir: orbitar  |  Scroll: zoom  |  W: wire";
    DrawText(hint, x, y + 4, 10, (Color){ 70, 75, 90, 255 });

    /* ================================================================
     * 8. BARRA DE STATUS NA BASE DA AREA 3D
     * ================================================================ */
    char status[80];
    snprintf(status, sizeof(status),
             "Rack mais quente: R%d (%.1fC)   Avg: %.1fC   Umid: %.0f%%",
             hot_rack, hot_t,
             sensors_avg_temp(s), s->ambient_humid);
    DrawRectangle(0, sh - 20, px, 20, (Color){ 13, 14, 20, 200 });
    DrawText(status, 8, sh - 16, 11, (Color){ 130, 135, 150, 255 });

    /* Dica de inspecao */
    const char *tip = "Passe o mouse sobre os objetos para inspecionar";
    DrawText(tip, px - MeasureText(tip, 10) - 8, sh - 16, 10,
             (Color){ 80, 85, 100, 200 });

    /* ================================================================
     * 8.5 DIAGRAMA COMPACTO DA REDE NEURAL
     *     Visivel apenas quando nenhum painel de overlay esta aberto.
     * ================================================================ */
    if (!hud->help_open && !hud->nn_open && !hud->arm_open &&
        !hud->compare_open && !hud->heatmap_open)
        draw_nn_compact(mlp);

    /* ================================================================
     * 9. BOTOES DE ACESSO RAPIDO (canto inferior esquerdo)
     *    Layout: labels acima dos botoes para nao sobrepor nada
     *    [?]      [N]          [A]
     *    AJUDA    REDE NEURAL  ARM vs AVR
     * ================================================================ */
    /* Dimensoes fixas */
    int btn_sz  = 28;                /* lado do botao                  */
    int btn_y   = sh - 54;           /* topo do botao                  */
    int lbl_y   = btn_y - 13;        /* topo do label (acima do botao) */
    int b1x = 8;                     /* botao 1 — Ajuda                */
    int b2x = 50;                    /* botao 2 — Rede Neural          */
    int b3x = 92;                    /* botao 3 — ARM vs AVR           */

    /* ---- Botao 1: AJUDA ---- */
    {
        Color bg = hud->help_open
            ? (Color){ 18, 55, 100, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = hud->help_open
            ? HC_BLUE : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b1x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b1x, btn_y, btn_sz, btn_sz, bd);
        DrawText("?", b1x + 8, btn_y + 5, 16,
                 hud->help_open ? HC_BLUE : HC_DIM);
        DrawText("AJUDA", b1x + 1, lbl_y, 9, HC_DIM);
        Vector2 mp = GetMousePosition();
        if (mp.x >= b1x && mp.x < b1x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->help_open = !hud->help_open;
            if (hud->help_open) { hud->help_page = 0; }
        }
    }

    /* ---- Botao 2: REDE NEURAL ---- */
    {
        Color bg = hud->nn_open
            ? (Color){ 12, 70, 45, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = hud->nn_open
            ? HC_GREEN : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b2x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b2x, btn_y, btn_sz, btn_sz, bd);
        DrawText("N", b2x + 8, btn_y + 5, 16,
                 hud->nn_open ? HC_GREEN : HC_DIM);
        DrawText("IA", b2x + 7, lbl_y, 9, HC_DIM);
        Vector2 mp = GetMousePosition();
        if (mp.x >= b2x && mp.x < b2x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->nn_open  = !hud->nn_open;
            if (hud->nn_open) hud->arm_open = 0;
        }
    }

    /* ---- Botao 3: ARM vs AVR ---- */
    {
        Color bg = hud->arm_open
            ? (Color){ 55, 35, 10, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = hud->arm_open
            ? HC_YEL : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b3x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b3x, btn_y, btn_sz, btn_sz, bd);
        DrawText("A", b3x + 8, btn_y + 5, 16,
                 hud->arm_open ? HC_YEL : HC_DIM);
        DrawText("ARM", b3x + 3, lbl_y, 9, HC_DIM);
        Vector2 mp = GetMousePosition();
        if (mp.x >= b3x && mp.x < b3x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->arm_open = !hud->arm_open;
            if (hud->arm_open) hud->nn_open = 0;
        }
    }

    int b4x = 134;   /* VS   — comparativo */
    int b5x = 176;   /* MAPA — heatmap     */
    int b6x = 218;   /* CRAC — falha       */

    /* ---- Botao 4: COMPARATIVO Fuzzy vs Histerese ---- */
    {
        Color bg = hud->compare_open
            ? (Color){ 10, 60, 60, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = hud->compare_open
            ? (Color){ 40, 210, 180, 255 } : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b4x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b4x, btn_y, btn_sz, btn_sz, bd);
        DrawText("C", b4x + 8, btn_y + 5, 16,
                 hud->compare_open ? (Color){ 40, 210, 180, 255 } : HC_DIM);
        DrawText("VS", b4x + 5, lbl_y, 9, HC_DIM);
        Vector2 mp = GetMousePosition();
        if (mp.x >= b4x && mp.x < b4x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->compare_open = !hud->compare_open;
            if (hud->compare_open) {
                hud->heatmap_open = 0;
                hud->help_open    = 0;
                hud->nn_open      = 0;
                hud->arm_open     = 0;
            }
        }
    }

    /* ---- Botao 5: MAPA DE CALOR ---- */
    {
        Color bg = hud->heatmap_open
            ? (Color){ 55, 12, 12, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = hud->heatmap_open
            ? HC_RED : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b5x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b5x, btn_y, btn_sz, btn_sz, bd);
        DrawText("H", b5x + 8, btn_y + 5, 16,
                 hud->heatmap_open ? HC_RED : HC_DIM);
        DrawText("MAPA", b5x - 1, lbl_y, 9, HC_DIM);
        Vector2 mp = GetMousePosition();
        if (mp.x >= b5x && mp.x < b5x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->heatmap_open = !hud->heatmap_open;
            if (hud->heatmap_open) {
                hud->compare_open = 0;
                hud->help_open    = 0;
                hud->nn_open      = 0;
                hud->arm_open     = 0;
            }
        }
    }

    /* ---- Botao 6: FALHA DE CRAC (cicla 0→1→2→0) ---- */
    {
        int   fail = (hud->crac_state > 0);
        Color bg = fail ? (Color){ 55, 12, 12, 255 } : (Color){ 22, 26, 40, 220 };
        Color bd = fail ? HC_RED : (Color){ 52, 58, 80, 255 };
        DrawRectangle(b6x, btn_y, btn_sz, btn_sz, bg);
        DrawRectangleLines(b6x, btn_y, btn_sz, btn_sz, bd);
        DrawText("F", b6x + 8, btn_y + 5, 16, fail ? HC_RED : HC_DIM);
        DrawText("CRAC", b6x - 1, lbl_y, 9, fail ? HC_RED : HC_DIM);
        /* Indicador de qual CRAC falhou */
        if (hud->crac_state > 0) {
            static const char *crac_lbl[2] = { "#0", "#1" };
            const char *fb = crac_lbl[(hud->crac_state - 1) & 1];
            DrawText(fb, b6x + 2, btn_y + btn_sz - 12, 9, HC_RED);
        }
        Vector2 mp = GetMousePosition();
        if (mp.x >= b6x && mp.x < b6x + btn_sz &&
            mp.y >= btn_y && mp.y < btn_y + btn_sz &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hud->crac_state = (hud->crac_state + 1) % 3;
        }
    }

    /* ================================================================
     * 10. OVERLAYS (desenhados por cima de tudo, mutuamente exclusivos)
     * ================================================================ */
    int any_overlay = hud->help_open || hud->nn_open || hud->arm_open
                    || hud->compare_open || hud->heatmap_open;
    if (any_overlay) {
        /* Escurece o painel lateral direito para nao competir com o overlay */
        DrawRectangle(px, 0, PANEL_W, sh, (Color){ 0, 0, 0, 175 });

        if (hud->help_open)
            draw_help_overlay(hud, px, sh);
        else if (hud->nn_open)
            draw_nn_overlay(hud, mlp, px, sh);
        else if (hud->arm_open)
            draw_arm_overlay(hud, px, sh);
        else if (hud->compare_open)
            draw_compare_overlay(hud, cmp, px, sh);
        else
            draw_heatmap_overlay(hud, s, px, sh);
    }

    (void)sw;
}
