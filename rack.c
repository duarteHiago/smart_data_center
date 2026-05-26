/*
 * Module:      rack
 * File:        rack.c
 * Description: Renderização 3D detalhada de racks de data center com:
 *              — Frame metálico com 4 colunas e trilhos
 *              — Equipamentos variados por slot (servidor, switch, patch, UPS)
 *              — Frentes detalhadas: drive bays, port LEDs, displays, botões
 *              — Bundle de cabos saindo do topo de cada rack
 *              — Orientação correta: frente sempre voltada para o corredor frio
 */

#include "rack.h"
#include <math.h>
#include <string.h>

/* ---- Geometria base ---- */
#define RACK_W   0.65f   /* largura                                */
#define RACK_H   2.10f   /* altura (42U em escala ~1:21)           */
#define RACK_D   1.05f   /* profundidade                           */
#define SLOT_H   (RACK_H / (float)SERVERS_PER_RACK)  /* por slot  */

/* Posições X e Z dos 8 racks no mundo */
static const float RACK_POS_X[NUM_RACKS] = {
    -8.5f, -5.5f, -2.5f,  0.5f,   /* row norte (z negativo) */
    -8.5f, -5.5f, -2.5f,  0.5f    /* row sul  (z positivo)  */
};
static const float RACK_POS_Z[NUM_RACKS] = {
    -2.8f, -2.8f, -2.8f, -2.8f,
     2.8f,  2.8f,  2.8f,  2.8f
};

/*
 * Layout de equipamentos: cada rack tem uma combinação diferente de devices.
 * Slot 0 = base do rack, slot 7 = topo.
 */
static const DevType DEVICE_LAYOUTS[NUM_RACKS][SERVERS_PER_RACK] = {
    /* R0 */ { DEV_UPS_1U,    DEV_PATCH_1U, DEV_SWITCH_1U, DEV_SERVER_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_2U },
    /* R1 */ { DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U,
               DEV_SWITCH_1U, DEV_SERVER_1U, DEV_SERVER_2U, DEV_SERVER_1U },
    /* R2 */ { DEV_PATCH_1U,  DEV_PATCH_1U,  DEV_SWITCH_1U, DEV_SWITCH_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_BLANK_1U  },
    /* R3 */ { DEV_SERVER_2U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U },
    /* R4 */ { DEV_UPS_1U,    DEV_BLANK_1U,  DEV_PATCH_1U,  DEV_SWITCH_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_2U, DEV_SERVER_1U },
    /* R5 */ { DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U,
               DEV_SWITCH_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_2U },
    /* R6 */ { DEV_PATCH_1U,  DEV_SWITCH_1U, DEV_SERVER_1U, DEV_SERVER_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_BLANK_1U  },
    /* R7 */ { DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_2U, DEV_SERVER_1U,
               DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U, DEV_SERVER_1U },
};

/* ------------------------------------------------------------------ */
/* Utilitários                                                          */
/* ------------------------------------------------------------------ */
Color rack_temp_color(float temp)
{
    float t = (temp - 20.0f) / 30.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    unsigned char r, g, b;
    if (t < 0.5f) {
        float u = t * 2.0f;
        r = (unsigned char)(u * 50);
        g = (unsigned char)(100 + u * 150);
        b = (unsigned char)(200 - u * 200);
    } else {
        float u = (t - 0.5f) * 2.0f;
        r = (unsigned char)(50 + u * 200);
        g = (unsigned char)(200 - u * 200);
        b = 0;
    }
    return (Color){ r, g, b, 255 };
}

static float fclamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* ------------------------------------------------------------------ */
/* Inicialização                                                        */
/* ------------------------------------------------------------------ */
void rack_init_all(RackModel racks[NUM_RACKS])
{
    for (int i = 0; i < NUM_RACKS; i++) {
        memset(&racks[i], 0, sizeof(RackModel));
        racks[i].rack_id  = i;
        racks[i].sign_z   = (i < 4) ? 1.0f : -1.0f;
        racks[i].position = (Vector3){
            RACK_POS_X[i],
            RACK_H * 0.5f,
            RACK_POS_Z[i]
        };
        racks[i].body_color  = (Color){ 38, 38, 43, 255 };
        racks[i].temperature = 28.0f;
        racks[i].load        = 50.0f;

        for (int j = 0; j < SERVERS_PER_RACK; j++) {
            racks[i].device_layout[j]  = DEVICE_LAYOUTS[i][j];
            racks[i].led_colors[j]     = (Color){ 40, 200, 40, 255 };
            racks[i].led_intensity[j]  = 0.8f;
            racks[i].blink_t[j]        = (float)j * 0.37f; /* fase inicial variada */
        }
        for (int p = 0; p < MAX_PORT_LEDS; p++)
            racks[i].port_blink_t[p] = (float)p * 0.13f;
    }
}

/* ------------------------------------------------------------------ */
/* Atualização                                                          */
/* ------------------------------------------------------------------ */
void rack_update(RackModel *rm, const RackSensor *sensor, float dt)
{
    rm->temperature = sensor->temperature;
    rm->load        = sensor->load;

    for (int j = 0; j < SERVERS_PER_RACK; j++) {
        float slot_bias = (float)j / (float)SERVERS_PER_RACK * 2.5f;
        rm->led_colors[j]    = rack_temp_color(sensor->temperature + slot_bias);
        rm->led_intensity[j] = fclamp01(0.35f + sensor->load * 0.0065f);

        /* Blink de atividade varia com carga */
        float rate = 0.5f + sensor->load * 0.05f;
        rm->blink_t[j] += dt * rate;
    }

    float port_rate = 1.0f + sensor->load * 0.04f;
    for (int p = 0; p < MAX_PORT_LEDS; p++)
        rm->port_blink_t[p] += dt * (port_rate + (float)(p % 5) * 0.2f);

    /* Cor do chassi: sutil variação com temperatura */
    float t = fclamp01((sensor->temperature - 25.0f) / 20.0f);
    rm->body_color = (Color){
        (unsigned char)(38 + t * 20),
        (unsigned char)(38),
        (unsigned char)(43),
        255
    };
}

/* ------------------------------------------------------------------ */
/* Funções de desenho internas                                          */
/* ------------------------------------------------------------------ */

/*
 * Estrutura do rack — chassi, colunas, trilhos e painéis laterais.
 * fz  = z da face frontal do rack
 * sz  = sinal da direção frontal (+1 ou -1)
 */
static void draw_rack_frame(Vector3 p, float fz, float sz)
{
    /* Chassi base (volume principal, cor escura) */
    DrawCube(p, RACK_W, RACK_H, RACK_D, (Color){ 30, 30, 34, 255 });

    /* Painel traseiro — mais escuro, com fendas de ventilação */
    float rz = p.z - sz * (RACK_D * 0.5f - 0.01f);
    DrawCube((Vector3){ p.x, p.y, rz },
             RACK_W - 0.04f, RACK_H - 0.04f, 0.015f,
             (Color){ 24, 24, 28, 255 });
    /* Fendas de ventilação traseira (barras horizontais) */
    for (int v = 0; v < 12; v++) {
        float vy = p.y - RACK_H * 0.4f + v * (RACK_H * 0.8f / 12.0f);
        DrawCube((Vector3){ p.x, vy, rz - sz * 0.008f },
                 RACK_W * 0.75f, 0.008f, 0.008f,
                 (Color){ 18, 18, 22, 255 });
    }

    /* ---- 4 colunas de montagem (uprights) ---- */
    float cx[2] = { p.x - RACK_W * 0.5f + 0.025f, p.x + RACK_W * 0.5f - 0.025f };
    float cz[2] = { fz - sz * 0.03f, p.z - sz * (RACK_D * 0.5f - 0.06f) };
    Color upright_col = (Color){ 58, 58, 64, 255 };
    for (int c = 0; c < 2; c++) {
        /* Coluna frontal */
        DrawCube((Vector3){ cx[c], p.y, cz[0] }, 0.04f, RACK_H, 0.04f, upright_col);
        /* Coluna traseira */
        DrawCube((Vector3){ cx[c], p.y, cz[1] }, 0.04f, RACK_H, 0.04f, upright_col);
        /* Travessas horizontais de topo e base */
        DrawCube((Vector3){ cx[c], p.y + RACK_H * 0.5f - 0.02f,
                             (cz[0] + cz[1]) * 0.5f },
                 0.04f, 0.04f, RACK_D - 0.1f, upright_col);
        DrawCube((Vector3){ cx[c], p.y - RACK_H * 0.5f + 0.02f,
                             (cz[0] + cz[1]) * 0.5f },
                 0.04f, 0.04f, RACK_D - 0.1f, upright_col);
    }

    /* Trilhos marcadores de slot (linhas horizontais nas colunas) */
    for (int s = 0; s <= SERVERS_PER_RACK; s++) {
        float sy = p.y - RACK_H * 0.5f + s * SLOT_H;
        for (int c = 0; c < 2; c++) {
            DrawCube((Vector3){ cx[c], sy, cz[0] },
                     0.042f, 0.005f, 0.012f,
                     (Color){ 80, 80, 90, 255 });
        }
    }

    /* Painéis laterais — planos finos com brilho metálico */
    Color side_col = (Color){ 42, 42, 48, 255 };
    DrawCube((Vector3){ p.x - RACK_W * 0.5f + 0.008f, p.y, p.z },
             0.012f, RACK_H - 0.06f, RACK_D - 0.06f, side_col);
    DrawCube((Vector3){ p.x + RACK_W * 0.5f - 0.008f, p.y, p.z },
             0.012f, RACK_H - 0.06f, RACK_D - 0.06f, side_col);

    /* PDU strip lateral (faixa de tomadas no lado direito) */
    DrawCube((Vector3){ p.x + RACK_W * 0.5f + 0.015f, p.y, p.z + sz * 0.1f },
             0.025f, RACK_H * 0.7f, 0.06f,
             (Color){ 28, 28, 32, 255 });
    /* Tomadas no PDU */
    for (int t = 0; t < 6; t++) {
        float ty = p.y - RACK_H * 0.28f + t * (RACK_H * 0.56f / 6.0f);
        DrawCube((Vector3){ p.x + RACK_W * 0.5f + 0.022f, ty, p.z + sz * 0.1f },
                 0.01f, 0.025f, 0.018f,
                 (Color){ 15, 15, 18, 255 });
    }

    /* Bundle de cabos saindo do topo (roteando para a calha no teto) */
    float cable_x = p.x + RACK_W * 0.2f;
    float cable_top_y = p.y + RACK_H * 0.5f + 0.6f;
    Color cable_colors[4] = {
        { 220, 60,  60,  200 },  /* vermelho */
        { 60,  160, 220, 200 },  /* azul     */
        { 220, 220, 60,  200 },  /* amarelo  */
        { 60,  200, 60,  200 },  /* verde    */
    };
    for (int c = 0; c < 4; c++) {
        float offset_x = (float)(c - 1) * 0.025f;
        DrawLine3D(
            (Vector3){ cable_x + offset_x, p.y + RACK_H * 0.5f, p.z },
            (Vector3){ cable_x + offset_x, cable_top_y, p.z },
            cable_colors[c]
        );
    }

    /* Placa de identificação no topo */
    DrawCube((Vector3){ p.x, p.y + RACK_H * 0.5f + 0.04f, fz - sz * 0.1f },
             RACK_W * 0.6f, 0.06f, 0.04f,
             (Color){ 20, 60, 140, 230 });
}

/* Servidor 1U — bezel com drive bays, botão, LEDs, portas */
static void draw_server_1u(Vector3 sv, float fz, float sz,
                            Color led_col, float intensity, float blink_t,
                            int slot_idx)
{
    float bh = SLOT_H - 0.015f;  /* altura do bezel */
    float bw = RACK_W - 0.055f;
    float bd = RACK_D - 0.06f;

    /* Corpo do servidor (chassi metálico) */
    DrawCube((Vector3){ sv.x, sv.y, sv.z }, bw, bh, bd,
             (Color){ 48, 48, 54, 255 });

    /* Bezel frontal (plástico mais escuro) */
    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
             bw, bh, 0.014f,
             (Color){ 30, 30, 36, 255 });

    /* Faixa de drive bays (4 bays em linha) */
    float bay_w = bw * 0.12f;
    float bay_start_x = sv.x - bw * 0.3f;
    for (int b = 0; b < 4; b++) {
        float bx = bay_start_x + b * (bay_w + 0.006f);
        /* Recessso do bay */
        DrawCube((Vector3){ bx, sv.y, fz - sz * 0.012f },
                 bay_w, bh * 0.55f, 0.01f,
                 (Color){ 20, 20, 24, 255 });
        /* Indicador de atividade do drive */
        float da = 0.4f + 0.6f * fabsf(sinf(blink_t * 3.1f + b * 1.7f));
        Color drive_led = { 0, (unsigned char)(120 * da * intensity),
                            (unsigned char)(255 * da * intensity), 255 };
        DrawSphere((Vector3){ bx, sv.y + bh * 0.28f, fz - sz * 0.018f },
                   0.006f, drive_led);
    }

    /* Botão power (direita, círculo verde) */
    float pw_x = sv.x + bw * 0.42f;
    Color pw_col = (intensity > 0.3f)
        ? (Color){ 40, 220, 40, 255 }
        : (Color){ 120, 120, 120, 255 };
    DrawSphere((Vector3){ pw_x, sv.y, fz - sz * 0.018f }, 0.012f, pw_col);

    /* LED de status de temperatura */
    float si = fabsf(sinf(blink_t * 1.5f));
    Color status = {
        (unsigned char)(led_col.r * si * intensity),
        (unsigned char)(led_col.g * si * intensity),
        (unsigned char)(led_col.b * si * intensity),
        255
    };
    DrawSphere((Vector3){ pw_x - 0.028f, sv.y, fz - sz * 0.018f },
               0.009f, status);

    /* LED de rede (azul piscando) */
    float ni = fabsf(sinf(blink_t * 7.3f + slot_idx));
    DrawSphere((Vector3){ pw_x - 0.052f, sv.y, fz - sz * 0.018f },
               0.007f, (Color){ 40, 80, (unsigned char)(200 * ni), 255 });

    /* Porta USB (retângulo pequeno) */
    DrawCube((Vector3){ sv.x - bw * 0.46f, sv.y, fz - sz * 0.016f },
             0.014f, 0.008f, 0.008f,
             (Color){ 18, 18, 22, 255 });

    /* Ventilação frontal (fendas no lado) */
    for (int v = 0; v < 3; v++) {
        float vx = sv.x + bw * 0.1f + v * 0.022f;
        DrawCube((Vector3){ vx, sv.y, fz - sz * 0.012f },
                 0.008f, bh * 0.6f, 0.006f,
                 (Color){ 22, 22, 26, 255 });
    }

    /* Borda cromada do bezel */
    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
                  bw, bh, 0.014f,
                  (Color){ 70, 70, 80, 120 });
}

/* Servidor 2U — mais robusto, mais drive bays */
static void draw_server_2u(Vector3 sv, float fz, float sz,
                            Color led_col, float intensity, float blink_t)
{
    float bh = SLOT_H * 1.85f;
    float bw = RACK_W - 0.055f;
    float bd = RACK_D - 0.06f;

    DrawCube((Vector3){ sv.x, sv.y, sv.z }, bw, bh, bd,
             (Color){ 44, 44, 50, 255 });
    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.009f },
             bw, bh, 0.016f, (Color){ 26, 26, 32, 255 });

    /* 8 drive bays em grade 4×2 */
    float bay_w = bw * 0.10f;
    float bay_h = bh * 0.28f;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 4; col++) {
            float bx = sv.x - bw * 0.28f + col * (bay_w + 0.005f);
            float by = sv.y + (row == 0 ? bh * 0.18f : -bh * 0.18f);
            DrawCube((Vector3){ bx, by, fz - sz * 0.014f },
                     bay_w, bay_h, 0.01f,
                     (Color){ 18, 18, 22, 255 });
            float da = 0.3f + 0.7f * fabsf(sinf(blink_t * 2.5f + col * 0.9f + row));
            DrawSphere((Vector3){ bx + bay_w * 0.35f, by + bay_h * 0.35f,
                                   fz - sz * 0.019f },
                       0.005f,
                       (Color){ 0, (unsigned char)(80 * da * intensity),
                                (unsigned char)(220 * da * intensity), 255 });
        }
    }

    /* Indicadores e botões */
    float px = sv.x + bw * 0.42f;
    DrawSphere((Vector3){ px, sv.y + bh * 0.2f, fz - sz * 0.019f },
               0.013f, (Color){ 40, 220, 40, 255 });
    float si = fabsf(sinf(blink_t * 1.2f));
    Color status = {
        (unsigned char)(led_col.r * si * intensity),
        (unsigned char)(led_col.g * si * intensity),
        (unsigned char)(led_col.b * si * intensity), 255
    };
    DrawSphere((Vector3){ px - 0.032f, sv.y + bh * 0.2f, fz - sz * 0.019f },
               0.01f, status);

    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.009f },
                  bw, bh, 0.016f, (Color){ 65, 65, 75, 100 });
}

/* Switch 1U — 24 LEDs de porta em linha */
static void draw_switch_1u(Vector3 sv, float fz, float sz,
                            float intensity, float *port_blink_t)
{
    float bh = SLOT_H - 0.015f;
    float bw = RACK_W - 0.055f;

    /* Bezel preto / azul escuro */
    DrawCube((Vector3){ sv.x, sv.y, sv.z },
             bw, bh, RACK_D - 0.06f,
             (Color){ 20, 22, 35, 255 });
    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
             bw, bh, 0.014f,
             (Color){ 15, 18, 30, 255 });

    /* 24 LEDs de porta em linha */
    float port_area_w = bw * 0.78f;
    float port_spacing = port_area_w / 24.0f;
    float port_start_x = sv.x - port_area_w * 0.5f + port_spacing * 0.5f;
    for (int p = 0; p < 24; p++) {
        float px = port_start_x + p * port_spacing;
        float blink = fabsf(sinf(port_blink_t[p] * 4.0f));
        /* Verde = link ativo, apagado = sem link */
        int active = (p % 5 != 3); /* alguns ports sem link */
        Color pc = active
            ? (Color){ 0, (unsigned char)(160 * blink * intensity),
                       0, 255 }
            : (Color){ 20, 20, 20, 255 };
        DrawSphere((Vector3){ px, sv.y + bh * 0.1f, fz - sz * 0.018f },
                   0.006f, pc);

        /* Porta física (pequeno retângulo) */
        DrawCube((Vector3){ px, sv.y - bh * 0.06f, fz - sz * 0.013f },
                 port_spacing * 0.7f, bh * 0.35f, 0.008f,
                 (Color){ 10, 12, 20, 255 });
    }

    /* 2 portas SFP+ de uplink (direita) */
    for (int u = 0; u < 2; u++) {
        float ux = sv.x + bw * 0.42f - u * 0.028f;
        DrawCube((Vector3){ ux, sv.y, fz - sz * 0.014f },
                 0.022f, bh * 0.55f, 0.01f,
                 (Color){ 8, 10, 18, 255 });
        float amber = fabsf(sinf(port_blink_t[22 + u] * 3.0f)) * intensity;
        DrawSphere((Vector3){ ux, sv.y + bh * 0.35f, fz - sz * 0.019f },
                   0.007f,
                   (Color){ (unsigned char)(200 * amber), (unsigned char)(120 * amber), 0, 255 });
    }

    /* Label "SWITCH" */
    DrawCube((Vector3){ sv.x - bw * 0.3f, sv.y + bh * 0.3f, fz - sz * 0.009f },
             0.06f, 0.005f, 0.006f,
             (Color){ 40, 80, 160, 200 });

    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
                  bw, bh, 0.014f, (Color){ 40, 50, 80, 100 });
}

/* Patch panel — 24 portas RJ-45 */
static void draw_patch_panel(Vector3 sv, float fz, float sz)
{
    float bh = SLOT_H - 0.015f;
    float bw = RACK_W - 0.055f;

    /* Chassi metálico claro */
    DrawCube((Vector3){ sv.x, sv.y, sv.z },
             bw, bh, RACK_D * 0.3f,
             (Color){ 68, 68, 75, 255 });
    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.006f },
             bw, bh, 0.01f,
             (Color){ 72, 72, 80, 255 });

    /* 12 portas RJ-45 em linha (2 fileiras) */
    float port_w  = bw * 0.055f;
    float port_h  = bh * 0.35f;
    float start_x = sv.x - bw * 0.43f;
    for (int row = 0; row < 2; row++) {
        float ry = sv.y + (row == 0 ? bh * 0.16f : -bh * 0.16f);
        for (int c = 0; c < 12; c++) {
            float px = start_x + c * (port_w + 0.003f);
            /* Recessso da porta */
            DrawCube((Vector3){ px, ry, fz - sz * 0.009f },
                     port_w, port_h, 0.007f,
                     (Color){ 30, 30, 35, 255 });
            /* Travinha do clip (tab plástico) */
            DrawCube((Vector3){ px, ry - port_h * 0.45f, fz - sz * 0.014f },
                     port_w * 0.5f, 0.004f, 0.005f,
                     (Color){ 20, 20, 25, 255 });
            /* Numeração (pequena linha decorativa) */
            DrawCube((Vector3){ px, ry + port_h * 0.58f, fz - sz * 0.006f },
                     port_w * 0.7f, 0.003f, 0.003f,
                     (Color){ 120, 120, 130, 180 });
        }
    }

    /* Etiqueta "PATCH" */
    DrawCube((Vector3){ sv.x + bw * 0.38f, sv.y, fz - sz * 0.007f },
             0.04f, 0.004f, 0.004f,
             (Color){ 140, 140, 150, 200 });

    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.006f },
                  bw, bh, 0.01f, (Color){ 90, 90, 100, 100 });
}

/* Painel em branco — placa metálica lisa */
static void draw_blank_panel(Vector3 sv, float fz, float sz)
{
    float bh = SLOT_H - 0.015f;
    float bw = RACK_W - 0.055f;

    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.005f },
             bw, bh, 0.008f,
             (Color){ 55, 55, 62, 255 });
    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.005f },
                  bw, bh, 0.008f,
                  (Color){ 72, 72, 80, 100 });
    /* Rebites decorativos */
    DrawSphere((Vector3){ sv.x - bw * 0.44f, sv.y, fz - sz * 0.01f },
               0.007f, (Color){ 75, 75, 85, 255 });
    DrawSphere((Vector3){ sv.x + bw * 0.44f, sv.y, fz - sz * 0.01f },
               0.007f, (Color){ 75, 75, 85, 255 });
}

/* UPS — no-break com display e indicadores de bateria */
static void draw_ups(Vector3 sv, float fz, float sz, float blink_t)
{
    float bh = SLOT_H - 0.015f;
    float bw = RACK_W - 0.055f;

    /* Chassi robusto (mais fundo) */
    DrawCube((Vector3){ sv.x, sv.y, sv.z + sz * 0.05f },
             bw, bh, RACK_D * 0.85f,
             (Color){ 35, 35, 40, 255 });
    DrawCube((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
             bw, bh, 0.014f,
             (Color){ 28, 28, 34, 255 });

    /* Display LCD (retângulo verde escuro com "pixels") */
    float dw = bw * 0.28f;
    float dh = bh * 0.55f;
    DrawCube((Vector3){ sv.x - bw * 0.15f, sv.y, fz - sz * 0.012f },
             dw, dh, 0.008f,
             (Color){ 10, 30, 10, 255 });
    /* "Dígitos" no display */
    float digit_b = 0.5f + 0.5f * sinf(blink_t * 0.4f);
    for (int d = 0; d < 3; d++) {
        DrawCube((Vector3){ sv.x - bw * 0.22f + d * (dw * 0.28f),
                             sv.y, fz - sz * 0.016f },
                 dw * 0.1f, dh * 0.55f, 0.004f,
                 (Color){ 0, (unsigned char)(180 * digit_b), 0, 255 });
    }

    /* Indicadores de bateria (barras verticais) */
    float bat_start = sv.x + bw * 0.1f;
    float bat_level = 0.7f + 0.2f * sinf(blink_t * 0.15f);
    for (int b = 0; b < 5; b++) {
        float bx = bat_start + b * 0.018f;
        int filled = (b < (int)(bat_level * 5));
        Color bc = filled
            ? (Color){ 40, 200, 40, 255 }
            : (Color){ 28, 28, 32, 255 };
        DrawCube((Vector3){ bx, sv.y, fz - sz * 0.014f },
                 0.012f, bh * 0.45f, 0.008f, bc);
    }

    /* Botão On/Off */
    DrawSphere((Vector3){ sv.x + bw * 0.42f, sv.y, fz - sz * 0.019f },
               0.014f, (Color){ 50, 180, 50, 255 });

    /* Saídas de força (2 retângulos) */
    for (int o = 0; o < 2; o++) {
        DrawCube((Vector3){ sv.x + bw * 0.28f + o * 0.04f, sv.y, fz - sz * 0.014f },
                 0.025f, bh * 0.4f, 0.01f,
                 (Color){ 18, 18, 22, 255 });
    }

    DrawCubeWires((Vector3){ sv.x, sv.y, fz - sz * 0.008f },
                  bw, bh, 0.014f, (Color){ 55, 55, 65, 100 });
}

/* ------------------------------------------------------------------ */
/* Desenho público de um rack                                           */
/* ------------------------------------------------------------------ */
void rack_draw(const RackModel *rm)
{
    Vector3 p  = rm->position;
    float   sz = rm->sign_z;
    float   fz = p.z + sz * (RACK_D * 0.5f);  /* z da face frontal */

    draw_rack_frame(p, fz, sz);

    for (int j = 0; j < SERVERS_PER_RACK; j++) {
        float y_off = -RACK_H * 0.5f + (j + 0.5f) * SLOT_H;
        Vector3 sv  = { p.x, p.y + y_off, p.z };

        switch (rm->device_layout[j]) {
            case DEV_SERVER_1U:
                draw_server_1u(sv, fz, sz,
                               rm->led_colors[j], rm->led_intensity[j],
                               rm->blink_t[j], j);
                break;
            case DEV_SERVER_2U:
                draw_server_2u(sv, fz, sz,
                               rm->led_colors[j], rm->led_intensity[j],
                               rm->blink_t[j]);
                break;
            case DEV_SWITCH_1U:
                draw_switch_1u(sv, fz, sz,
                               rm->led_intensity[j],
                               (float *)rm->port_blink_t);
                break;
            case DEV_PATCH_1U:
                draw_patch_panel(sv, fz, sz);
                break;
            case DEV_BLANK_1U:
                draw_blank_panel(sv, fz, sz);
                break;
            case DEV_UPS_1U:
                draw_ups(sv, fz, sz, rm->blink_t[j]);
                break;
        }
    }
}

void rack_draw_all(const RackModel racks[NUM_RACKS])
{
    for (int i = 0; i < NUM_RACKS; i++)
        rack_draw(&racks[i]);
}

/* ------------------------------------------------------------------ */
/* Destaque do rack mais quente                                         */
/* ------------------------------------------------------------------ */
void rack_draw_hottest_highlight(const RackModel *rm, float time)
{
    Vector3 p = rm->position;
    float pulse  = 0.5f + 0.5f * sinf(time * 3.5f);
    float pulse2 = 0.5f + 0.5f * sinf(time * 3.5f + 1.5f);

    DrawCylinderWires((Vector3){ p.x, 0.05f, p.z },
                      0.55f + pulse * 0.25f, 0.55f + pulse * 0.25f, 0.02f, 16,
                      (Color){ 255, 60, 40, (unsigned char)(200 * (1.0f - pulse * 0.5f)) });

    DrawCylinderWires((Vector3){ p.x, 0.08f, p.z },
                      0.45f + pulse2 * 0.15f, 0.45f + pulse2 * 0.15f, 0.02f, 16,
                      (Color){ 255, 140, 40, (unsigned char)(150 * (1.0f - pulse2)) });

    float bar_h  = 0.3f + pulse * 0.2f;
    Vector3 btop = { p.x, p.y + RACK_H * 0.5f + 0.15f + bar_h, p.z };
    Vector3 bbot = { p.x, p.y + RACK_H * 0.5f + 0.15f,         p.z };
    DrawLine3D(bbot, btop, (Color){ 255, 80, 40, (unsigned char)(180 + pulse * 75) });
    DrawSphere(btop, 0.08f + pulse * 0.04f,
               (Color){ 255, 60, 40, (unsigned char)(200 + pulse * 55) });

    DrawCylinderWires((Vector3){ p.x, p.y, p.z },
                      RACK_W * 0.55f, RACK_W * 0.55f, RACK_H * 0.95f, 8,
                      (Color){ 255, 80, 40, (unsigned char)(30 + pulse * 40) });
}

Vector3 rack_get_position(const RackModel *rm)
{
    return rm->position;
}
