/*
 * Module:      hud
 * File:        hud.h
 * Description: Interface 2D sobreposta: dashboard de sensores, gráficos das
 *              funções fuzzy em tempo real, controles manuais e log de eventos.
 * Responsibility: Toda apresentação 2D com raygui; sem lógica de simulação.
 */

#ifndef HUD_H
#define HUD_H

#include "raylib.h"
#include "sensors.h"
#include "fuzzy.h"
#include "mlp.h"
#include "compare.h"

#define HUD_LOG_LINES   12
#define HUD_LOG_LEN     80
#define HUD_HISTORY_LEN 120  /* amostras de histórico para mini-gráficos */

typedef struct {
    /* Log de eventos */
    char  log_lines[HUD_LOG_LINES][HUD_LOG_LEN];
    int   log_head;   /* índice circular                          */
    int   log_count;

    /* Histórico para sparklines */
    float temp_history[HUD_HISTORY_LEN];
    float load_history[HUD_HISTORY_LEN];
    float fan_history [HUD_HISTORY_LEN];
    int   history_idx;

    /* Controles manuais (raygui sliders) */
    float manual_fan_speed;  /* 0–100 */
    float manual_pdu;        /* 0–100 */
    int   manual_override;   /* 0 = automático fuzzy, 1 = manual  */

    /* Painel ativo */
    int   active_tab;        /* 0=sensors 1=fuzzy 2=controls 3=log */

    /* Painel de ajuda */
    int   help_open;         /* 0 = fechado, 1 = aberto */
    int   help_page;         /* pagina atual: 0, 1 ou 2  */

    /* Painel da rede neural */
    int   nn_open;           /* 0 = fechado, 1 = aberto */

    /* Painel ARM vs AVR */
    int   arm_open;          /* 0 = fechado, 1 = aberto */

    /* Painel comparativo Fuzzy vs Histerese */
    int   compare_open;      /* 0 = fechado, 1 = aberto */

    /* Painel mapa de calor */
    int   heatmap_open;      /* 0 = fechado, 1 = aberto */

    /* Estado da falha de CRAC (ciclado pelo botao [F]) */
    int   crac_state;        /* 0 = OK, 1 = CRAC-0 falhou, 2 = CRAC-1 falhou */
} HudState;

/* Inicializa estado do HUD */
void hud_init(HudState *hud);

/* Adiciona linha ao log circular */
void hud_log(HudState *hud, const char *msg);

/* Atualiza histórico de sparklines */
void hud_update(HudState *hud, const SensorState *s, float fan_speed, float pdu);

/* Desenha todo o overlay 2D */
void hud_draw(HudState *hud, const SensorState *s, const FuzzyEngine *eng,
              const MlpNet *mlp, const CompareState *cmp,
              float fan_speed, float pdu_balance);

#endif /* HUD_H */
