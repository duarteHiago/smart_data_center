/*
 * Module:      scene
 * File:        scene.c
 * Description: Ambiente 3D aberto: sem teto sólido, sem paredes sólidas.
 *              Pilares, piso técnico, luminárias suspensas e CRAC units.
 *              Visual "open-plan" comum em simulações de data center.
 */

#include "scene.h"
#include "rlgl.h"
#include <math.h>
#include <string.h>

void scene_init(SceneEnv *env)
{
    memset(env, 0, sizeof(*env));
    env->floor_color    = (Color){  50, 52, 57, 255 };
    env->wall_color     = (Color){  66, 68, 74, 255 };
    env->ceiling_color  = (Color){  42, 44, 49, 255 };
    env->ambient_intensity = 0.6f;
}

/* ---- Piso técnico elevado ---- */
static void draw_floor(void)
{
    float W = SCENE_WIDTH, D = SCENE_DEPTH;
    float hW = W * 0.5f, hD = D * 0.5f;

    for (int tx = 0; tx < (int)W; tx++) {
        for (int tz = 0; tz < (int)D; tz++) {
            float x = -hW + tx + 0.5f;
            float z = -hD + tz + 0.5f;

            Color tile = ((tx + tz) % 2 == 0)
                ? (Color){ 52, 54, 60, 255 }
                : (Color){ 44, 46, 52, 255 };

            /* Painéis de acesso ao cabeamento (subfloor) */
            if ((tx % 3 == 1) && (tz % 3 == 1))
                tile = (Color){ 40, 42, 50, 255 };

            DrawCube((Vector3){ x, -0.06f, z }, 0.97f, 0.12f, 0.97f, tile);
            DrawCubeWires((Vector3){ x, -0.06f, z }, 0.97f, 0.12f, 0.97f,
                          (Color){ 28, 30, 35, 55 });
        }
    }

    /* Faixa de marcação do corredor frio (azul, nível do piso) */
    DrawCube((Vector3){ -4.0f, 0.001f, 0.0f }, 11.0f, 0.004f, 1.8f,
             (Color){ 30, 50, 140, 70 });

    /* Linhas de segurança nos limites do corredor */
    float hD2 = D * 0.5f;
    for (int s = 0; s < (int)W; s++) {
        float sx = -W * 0.5f + s + 0.5f;
        DrawCube((Vector3){ sx, 0.001f, -0.88f }, 0.9f, 0.004f, 0.05f,
                 (Color){ 200, 175, 15, 110 });
        DrawCube((Vector3){ sx, 0.001f,  0.88f }, 0.9f, 0.004f, 0.05f,
                 (Color){ 200, 175, 15, 110 });
        (void)hD2;
    }
}

/*
 * Pilares e rodapé — sem paredes sólidas para não bloquear a câmera.
 * Referência espacial sem obstruir a visão dos racks.
 */
static void draw_pillars_and_base(void)
{
    float W = SCENE_WIDTH, D = SCENE_DEPTH, H = SCENE_HEIGHT;
    float hW = W * 0.5f, hD = D * 0.5f;
    Color col_pillar = (Color){ 70, 72, 78, 255 };
    Color col_accent = (Color){ 88, 90, 96, 255 };

    /* 4 pilares nos cantos */
    float corners[4][2] = {
        { -hW + 0.2f, -hD + 0.2f },
        {  hW - 0.2f, -hD + 0.2f },
        { -hW + 0.2f,  hD - 0.2f },
        {  hW - 0.2f,  hD - 0.2f }
    };
    for (int c = 0; c < 4; c++) {
        DrawCube((Vector3){ corners[c][0], H * 0.5f, corners[c][1] },
                 0.30f, H, 0.30f, col_pillar);
        /* Faixa metálica no meio do pilar */
        DrawCube((Vector3){ corners[c][0], H * 0.5f, corners[c][1] },
                 0.32f, 0.06f, 0.32f, col_accent);
    }

    /* Pilares intermediários nas paredes longas (referência espacial) */
    float inter_z[3] = { -hD * 0.55f, 0.0f, hD * 0.55f };
    for (int p = 0; p < 3; p++) {
        DrawCube((Vector3){ -hW + 0.15f, H * 0.5f, inter_z[p] },
                 0.20f, H, 0.20f, col_pillar);
        DrawCube((Vector3){  hW - 0.15f, H * 0.5f, inter_z[p] },
                 0.20f, H, 0.20f, col_pillar);
    }

    /* Rodapé metálico (baixo, no piso) — apenas as quatro bordas */
    Color rodape = (Color){ 80, 82, 88, 255 };
    DrawCube((Vector3){ 0.0f,    0.10f, -hD + 0.10f }, W - 0.4f, 0.20f, 0.10f, rodape);
    DrawCube((Vector3){ 0.0f,    0.10f,  hD - 0.10f }, W - 0.4f, 0.20f, 0.10f, rodape);
    DrawCube((Vector3){ -hW + 0.10f, 0.10f, 0.0f }, 0.10f, 0.20f, D - 0.4f, rodape);
    DrawCube((Vector3){  hW - 0.10f, 0.10f, 0.0f }, 0.10f, 0.20f, D - 0.4f, rodape);

    /* Viga de teto ligando os pilares (estrutura visível no alto) */
    Color viga = (Color){ 60, 62, 68, 255 };
    DrawCube((Vector3){ 0.0f,    H - 0.08f, -hD + 0.15f }, W - 0.4f, 0.16f, 0.16f, viga);
    DrawCube((Vector3){ 0.0f,    H - 0.08f,  hD - 0.15f }, W - 0.4f, 0.16f, 0.16f, viga);
    DrawCube((Vector3){ -hW + 0.15f, H - 0.08f, 0.0f }, 0.16f, 0.16f, D - 0.3f, viga);
    DrawCube((Vector3){  hW - 0.15f, H - 0.08f, 0.0f }, 0.16f, 0.16f, D - 0.3f, viga);
}

/*
 * Calhas de cabos e luminárias LED suspensas no nível do teto.
 * Sem slab sólido — visíveis de baixo e de cima.
 */
static void draw_ceiling_fixtures(float time)
{
    float W = SCENE_WIDTH, D = SCENE_DEPTH, H = SCENE_HEIGHT;
    float hD = D * 0.5f;

    /* Calhas principais de cabo (longitudinais, ao longo dos racks) */
    Color tray = (Color){ 80, 82, 88, 255 };
    float tray_xs[4] = { -8.2f, -5.2f, -2.2f, 0.8f };
    for (int t = 0; t < 4; t++) {
        /* Perfil em U da calha */
        DrawCube((Vector3){ tray_xs[t], H - 0.10f, 0.0f },
                 0.16f, 0.08f, D - 0.4f, tray);
        /* Tampa com furos simulados (plano fino) */
        DrawCube((Vector3){ tray_xs[t], H - 0.062f, 0.0f },
                 0.14f, 0.004f, D - 0.44f,
                 (Color){ 65, 67, 73, 255 });
        /* Suportes a cada ~1.5 m */
        for (int s = 0; s < 9; s++) {
            float sz = -hD + 0.8f + s * 1.7f;
            if (sz > hD - 0.8f) break;
            DrawCube((Vector3){ tray_xs[t], H - 0.055f, sz },
                     0.20f, 0.012f, 0.05f,
                     (Color){ 95, 97, 103, 255 });
        }
    }

    /* Calha transversal (liga as longitudinais) */
    DrawCube((Vector3){ -3.7f, H - 0.10f,  hD - 0.7f },
             W * 0.65f, 0.08f, 0.16f, tray);
    DrawCube((Vector3){ -3.7f, H - 0.10f, -hD + 0.7f },
             W * 0.65f, 0.08f, 0.16f, tray);

    /* ---- Luminárias LED sobre o corredor frio ---- */
    float lp = 0.90f + 0.10f * sinf(time * 0.25f);  /* leve flicker */
    Color led_glow = (Color){
        (unsigned char)(235 * lp),
        (unsigned char)(242 * lp),
        (unsigned char)(255 * lp),
        255
    };

    /* 4 barras LED ao longo do corredor (uma por par de racks) */
    float led_xs[4] = { -8.2f, -5.2f, -2.2f, 0.8f };
    for (int l = 0; l < 4; l++) {
        /* Suporte do luminário */
        DrawCube((Vector3){ led_xs[l], H - 0.04f, 0.0f },
                 0.07f, 0.04f, D * 0.58f,
                 (Color){ 55, 57, 63, 255 });
        /* Difusor branco brilhante */
        DrawCube((Vector3){ led_xs[l], H - 0.060f, 0.0f },
                 0.055f, 0.007f, D * 0.55f,
                 led_glow);
        /* Reflexo suave no piso abaixo (plano quase invisível) */
        DrawCube((Vector3){ led_xs[l], 0.005f, 0.0f },
                 0.5f, 0.001f, D * 0.5f,
                 (Color){ led_glow.r, led_glow.g, led_glow.b, 18 });
    }

    /* Luz de emergência nos pilares (vermelho pulsante) */
    float em = (sinf(time * 1.4f) > 0.3f) ? 1.0f : 0.15f;
    Color em_red = { (unsigned char)(210 * em), 15, 15, 220 };
    DrawSphere((Vector3){ -11.8f, H - 0.3f, -7.8f }, 0.06f, em_red);
    DrawSphere((Vector3){  11.8f, H - 0.3f,  7.8f }, 0.06f, em_red);
    DrawSphere((Vector3){ -11.8f, H - 0.3f,  7.8f }, 0.06f, em_red);
    DrawSphere((Vector3){  11.8f, H - 0.3f, -7.8f }, 0.06f, em_red);
}

/* CRAC units nas paredes.
 * crac_failed[c] = 1 escurece o chassi e exibe LED de falha piscante. */
static void draw_crac_units(const int *crac_failed)
{
    float hD = SCENE_DEPTH * 0.5f;

    float crac_xs[2] = { -7.0f, 3.5f };
    for (int c = 0; c < 2; c++) {
        float cx = crac_xs[c];
        float cz = -hD + 0.55f;
        int   fail = crac_failed[c];

        /* Chassi principal — escuro quando em falha */
        Color ch_col = fail ? (Color){ 25, 22, 22, 255 }
                            : (Color){ 48, 50, 58, 255 };
        Color wf_col = fail ? (Color){ 40, 35, 35, 100 }
                            : (Color){ 68, 70, 78, 180 };

        DrawCube((Vector3){ cx, 1.15f, cz }, 1.3f, 2.3f, 0.7f, ch_col);
        DrawCubeWires((Vector3){ cx, 1.15f, cz }, 1.3f, 2.3f, 0.7f, wf_col);

        /* Grelha de admissão */
        Color gr_col = fail ? (Color){ 20, 20, 22, 255 }
                            : (Color){ 28, 30, 36, 255 };
        for (int g = 0; g < 8; g++) {
            float gy = 0.12f + g * 0.17f;
            DrawCube((Vector3){ cx, gy, cz - 0.30f },
                     1.10f, 0.025f, 0.05f, gr_col);
        }

        /* Painel de controle */
        DrawCube((Vector3){ cx, 2.05f, cz - 0.31f }, 0.55f, 0.32f, 0.018f,
                 fail ? (Color){ 18, 8, 8, 255 } : (Color){ 8, 22, 8, 255 });

        if (fail) {
            /* LED de falha: pisca vermelho */
            float blink = (sinf((float)GetTime() * 4.0f) > 0.0f) ? 1.0f : 0.2f;
            DrawSphere((Vector3){ cx, 2.08f, cz - 0.32f }, 0.022f,
                       (Color){ (unsigned char)(220 * blink), 15, 15, 240 });
        } else {
            DrawSphere((Vector3){ cx - 0.20f, 2.08f, cz - 0.32f }, 0.013f,
                       (Color){ 40, 220, 40, 255 });
            DrawSphere((Vector3){ cx - 0.20f, 1.88f, cz - 0.32f }, 0.013f,
                       (Color){ 40, 100, 220, 255 });
        }

        /* Duto de saída */
        DrawCube((Vector3){ cx, 2.34f, cz - 0.12f }, 1.05f, 0.09f, 0.38f,
                 fail ? (Color){ 28, 26, 26, 255 } : (Color){ 42, 44, 52, 255 });

        /* Pés */
        for (int f = 0; f < 4; f++) {
            float fx = cx - 0.45f + (f % 2) * 0.9f;
            float fz = cz + (f < 2 ? -0.2f : 0.2f);
            DrawCylinder((Vector3){ fx, 0.0f, fz },
                         0.04f, 0.04f, 0.08f, 6,
                         (Color){ 60, 62, 68, 255 });
        }
    }
}

/* Painéis de contenção do corredor frio (semi-transparentes) */
static void draw_aisle_containment(void)
{
    float W = SCENE_WIDTH, H = SCENE_HEIGHT;

    Color cold = (Color){ 40, 80, 180, 28 };

    /* Parede norte da contenção (sobre a fileira norte de racks) */
    DrawCube((Vector3){ -4.0f, H * 0.5f, -1.42f },
             W * 0.6f, H, 0.04f, cold);
    /* Parede sul */
    DrawCube((Vector3){ -4.0f, H * 0.5f,  1.42f },
             W * 0.6f, H, 0.04f, cold);
    /* Tampa superior da contenção */
    DrawCube((Vector3){ -4.0f, H - 0.01f, 0.0f },
             W * 0.6f, 0.04f, 2.84f, cold);

    /* Porta de vidro da contenção */
    DrawCube((Vector3){ -4.0f, H * 0.42f, -1.43f }, 0.9f, H * 0.84f, 0.022f,
             (Color){ 100, 140, 220, 42 });
    DrawCubeWires((Vector3){ -4.0f, H * 0.42f, -1.43f }, 0.9f, H * 0.84f, 0.022f,
                  (Color){ 90, 130, 210, 140 });
    /* Puxador */
    DrawCylinder((Vector3){ -4.38f, H * 0.46f, -1.44f },
                 0.014f, 0.014f, 0.22f, 6,
                 (Color){ 155, 160, 170, 255 });
}

/* ------------------------------------------------------------------ */
void scene_draw(const SceneEnv *env)
{
    static float s_time = 0.0f;
    s_time += 0.016f;

    draw_floor();
    draw_pillars_and_base();
    draw_ceiling_fixtures(s_time);
    draw_crac_units(env->crac_failed);
}

/* Desenha apenas os elementos transparentes (contenção do corredor frio).
 * Deve ser chamada DEPOIS de todos os objetos opacos para que a
 * mesclagem alpha seja correta e o depth buffer não bloqueie os racks. */
void scene_draw_transparent(const SceneEnv *env)
{
    rlDisableDepthMask();   /* nao escreve no depth buffer */

    draw_aisle_containment();

    rlEnableDepthMask();
    (void)env;
}

void scene_update_lighting(SceneEnv *env, float avg_temp)
{
    float t = (avg_temp - 25.0f) / 20.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    env->ambient_intensity = 0.55f + t * 0.15f;
}
