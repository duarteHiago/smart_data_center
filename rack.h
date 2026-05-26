/*
 * Module:      rack
 * File:        rack.h
 * Description: Renderização 3D detalhada dos racks de servidores com
 *              equipamentos variados: servidores 1U/2U, switches, patch panels,
 *              UPS, painéis em branco. LEDs de atividade animados por carga.
 * Responsibility: Visual realista dos racks; layout cold/hot aisle correto.
 */

#ifndef RACK_MODEL_H
#define RACK_MODEL_H

#include "raylib.h"
#include "sensors.h"

#define SERVERS_PER_RACK  8   /* slots por rack                          */
#define MAX_PORT_LEDS    24   /* LEDs de porta nos switches              */

/* Tipos de equipamento em cada slot do rack */
typedef enum {
    DEV_SERVER_1U = 0,  /* servidor genérico 1U — bezel com drive bays  */
    DEV_SERVER_2U = 1,  /* servidor 2U — mais alto, mais drives          */
    DEV_SWITCH_1U = 2,  /* switch de rede 1U — 24 LEDs de porta          */
    DEV_PATCH_1U  = 3,  /* patch panel RJ-45 — 24 portas                 */
    DEV_BLANK_1U  = 4,  /* painel em branco (slot vazio)                 */
    DEV_UPS_1U    = 5,  /* no-break / UPS — display e indicadores        */
} DevType;

typedef struct {
    Vector3 position;      /* centro geométrico do rack no mundo         */
    Color   body_color;    /* cor do chassi (varia com temperatura)       */

    /* Por slot */
    Color   led_colors    [SERVERS_PER_RACK];
    float   led_intensity [SERVERS_PER_RACK];  /* 0–1, proporcional à carga */
    float   blink_t       [SERVERS_PER_RACK];  /* timer de animação do LED   */
    float   port_blink_t  [MAX_PORT_LEDS];     /* para LEDs de switch        */
    DevType device_layout [SERVERS_PER_RACK];  /* tipo de equip. por slot    */

    float   temperature;
    float   load;
    int     rack_id;
    float   sign_z;        /* +1 = frente voltada para +z (row norte)    */
                           /* -1 = frente voltada para -z (row sul)      */
} RackModel;

/* Inicializa 8 racks com layout de equipamentos variados */
void rack_init_all(RackModel racks[NUM_RACKS]);

/* Atualiza cores, LEDs e animações; dt em segundos */
void rack_update(RackModel *rm, const RackSensor *sensor, float dt);

/* Desenha um rack com todos os equipamentos */
void rack_draw(const RackModel *rm);

/* Desenha todos os racks */
void rack_draw_all(const RackModel racks[NUM_RACKS]);

/* Retorna cor baseada em temperatura (frio=azul, quente=vermelho) */
Color rack_temp_color(float temp);

/* Anel pulsante de alerta no rack mais quente */
void rack_draw_hottest_highlight(const RackModel *rm, float time);

/* Retorna posição 3D do rack */
Vector3 rack_get_position(const RackModel *rm);

#endif /* RACK_MODEL_H */
