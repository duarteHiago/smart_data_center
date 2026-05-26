/*
 * Module:      inspector
 * File:        inspector.c
 * Description: Ray casting com GetMouseRay + GetRayCollisionBox para detectar
 *              qual objeto 3D está sob o cursor. Tooltip 2D renderizado com
 *              dados ao vivo da simulação.
 */

#include "inspector.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---- Geometria dos objetos (deve bater com scene.c / rack.c) ---- */
#define RACK_W    0.65f
#define RACK_H_M  2.10f   /* altura do rack em metros */
#define RACK_D    1.05f

/* CRAC: crac_xs={-7,3.5}, cz = -SCENE_DEPTH*0.5 + 0.55 = -7.45
 * Chassis DrawCube center=(cx,1.15,cz), size=(1.3,2.3,0.7) */
#define CRAC_CZ       -7.45f
#define CRAC_HW        0.65f   /* metade da largura: 1.3/2 */
#define CRAC_HH        1.15f   /* metade da altura:  2.3/2 */
#define CRAC_HD        0.35f   /* metade da profund: 0.7/2 */
static const float CRAC_CX[2] = { -7.0f, 3.5f };

/* Corredor frio: contenção de z=-1.42 a z=+1.42, x de -11.2 a 3.2, y 0 a 4 */
#define AISLE_X_MIN  -11.2f
#define AISLE_X_MAX    3.2f
#define AISLE_Z_MIN   -1.42f
#define AISLE_Z_MAX    1.42f
#define AISLE_Y_MAX    4.0f

/* ------------------------------------------------------------------ */
void inspector_init(InspectorState *ins,
                    const RackModel racks[NUM_RACKS],
                    Vector3 arduino_pos)
{
    memset(ins, 0, sizeof(*ins));
    ins->hovered_idx = -1;
    int n = 0;

    /* 8 racks */
    for (int i = 0; i < NUM_RACKS; i++) {
        Vector3 p = racks[i].position;
        ins->targets[n++] = (InspectTarget){
            .type = INSPECT_RACK,
            .id   = i,
            .box  = {
                .min = { p.x - RACK_W * 0.5f, 0.0f,      p.z - RACK_D * 0.5f },
                .max = { p.x + RACK_W * 0.5f, RACK_H_M,  p.z + RACK_D * 0.5f }
            }
        };
    }

    /* 2 CRACs */
    for (int c = 0; c < 2; c++) {
        float cx = CRAC_CX[c];
        ins->targets[n++] = (InspectTarget){
            .type = INSPECT_CRAC,
            .id   = c,
            .box  = {
                .min = { cx - CRAC_HW, 0.0f,              CRAC_CZ - CRAC_HD },
                .max = { cx + CRAC_HW, CRAC_HH * 2.0f,    CRAC_CZ + CRAC_HD }
            }
        };
    }

    /* Arduino — bounding box generosa para facilitar picking */
    ins->targets[n++] = (InspectTarget){
        .type = INSPECT_ARDUINO,
        .id   = 0,
        .box  = {
            .min = { arduino_pos.x - 0.22f, arduino_pos.y - 0.08f, arduino_pos.z - 0.14f },
            .max = { arduino_pos.x + 0.22f, arduino_pos.y + 0.08f, arduino_pos.z + 0.14f }
        }
    };

    /* Corredor frio — menor prioridade (verificado por último) */
    ins->targets[n++] = (InspectTarget){
        .type = INSPECT_COLD_AISLE,
        .id   = 0,
        .box  = {
            .min = { AISLE_X_MIN, 0.0f,        AISLE_Z_MIN },
            .max = { AISLE_X_MAX, AISLE_Y_MAX, AISLE_Z_MAX }
        }
    };

    ins->num_targets = n;
}

/* ------------------------------------------------------------------ */
void inspector_update(InspectorState *ins, Camera3D camera)
{
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    float closest_dist = 1e9f;
    ins->hovered_idx = -1;

    for (int i = 0; i < ins->num_targets; i++) {
        RayCollision rc = GetRayCollisionBox(ray, ins->targets[i].box);
        if (rc.hit && rc.distance > 0.0f && rc.distance < closest_dist) {
            closest_dist = rc.distance;
            ins->hovered_idx = i;
        }
    }
}

/* ------------------------------------------------------------------ */
void inspector_draw_3d(const InspectorState *ins)
{
    if (ins->hovered_idx < 0) return;
    const InspectTarget *t = &ins->targets[ins->hovered_idx];

    /* Cor do contorno por tipo */
    Color outline;
    switch (t->type) {
        case INSPECT_RACK:       outline = (Color){ 255, 220,  60, 200 }; break;
        case INSPECT_CRAC:       outline = (Color){  40, 200, 240, 200 }; break;
        case INSPECT_ARDUINO:    outline = (Color){  60, 220,  80, 200 }; break;
        case INSPECT_COLD_AISLE: outline = (Color){  80, 120, 255, 160 }; break;
        default:                 outline = WHITE;
    }
    DrawBoundingBox(t->box, outline);
}

/* ------------------------------------------------------------------ */
/* Helpers de desenho do tooltip                                        */
/* ------------------------------------------------------------------ */

#define TT_W       238     /* largura do tooltip em pixels  */
#define TT_PAD     10      /* padding interno               */
#define TT_FS_T    16      /* font size do título           */
#define TT_FS_B    13      /* font size do corpo            */
#define TT_LH      18      /* line height                   */
#define TT_MARGIN  14      /* distância do cursor           */

static Color temp_color(float t)
{
    if (t < 30.0f) return (Color){ 80, 160, 255, 255 };
    if (t < 38.0f) return (Color){ 80, 210,  80, 255 };
    if (t < 42.0f) return (Color){ 240, 190,  30, 255 };
    return                (Color){ 255,  70,  50, 255 };
}

static Color load_color(float l)
{
    if (l < 40.0f) return (Color){ 80, 210, 80, 255 };
    if (l < 75.0f) return (Color){ 240, 190, 30, 255 };
    return                (Color){ 255, 70, 50, 255 };
}

/* Desenha uma linha de texto no tooltip; avança *y */
static void tt_line(int x, int *y, const char *label, const char *value, Color vc)
{
    DrawText(label, x, *y, TT_FS_B, (Color){ 160, 165, 175, 255 });
    DrawText(value, x + 100, *y, TT_FS_B, vc);
    *y += TT_LH;
}

/* Separador horizontal */
static void tt_sep(int x, int *y)
{
    DrawRectangle(x, *y + 3, TT_W - TT_PAD * 2, 1, (Color){ 60, 65, 75, 200 });
    *y += 9;
}

/* ---- Tooltip: RACK ---- */
static void draw_tooltip_rack(int rx, int ry, int rack_id,
                               const RackModel *rm, const RackSensor *rs)
{
    /* Contagem de dispositivos */
    int servers = 0, switches = 0, ups = 0, patches = 0, blanks = 0;
    for (int i = 0; i < SERVERS_PER_RACK; i++) {
        switch (rm->device_layout[i]) {
            case DEV_SERVER_1U:
            case DEV_SERVER_2U: servers++;  break;
            case DEV_SWITCH_1U: switches++; break;
            case DEV_UPS_1U:    ups++;      break;
            case DEV_PATCH_1U:  patches++;  break;
            case DEV_BLANK_1U:  blanks++;   break;
        }
    }

    /* Número de linhas: título + sep + temp + carga + (devices) + status */
    int dev_lines = (servers > 0) + (switches > 0) + (ups > 0)
                  + (patches > 0) + (blanks > 0);
    int total_h = TT_PAD + TT_FS_T + 6 + 9 + TT_LH * 2 + 9
                + TT_LH * dev_lines + 9 + TT_LH + TT_PAD;

    DrawRectangleRounded((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                         (Color){ 14, 16, 22, 235 });
    DrawRectangleRoundedLines((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                              (Color){ 55, 58, 68, 180 });

    /* Barra de acento lateral (cor por temperatura) */
    Color accent = rs->alert ? (Color){ 255, 60, 40, 255 } : temp_color(rs->temperature);
    DrawRectangle(rx, ry + 4, 3, total_h - 8, accent);

    int tx = rx + TT_PAD + 4;
    int ty = ry + TT_PAD;

    char title[32];
    snprintf(title, sizeof(title), "RACK  #%d", rack_id);
    DrawText(title, tx, ty, TT_FS_T, WHITE);
    ty += TT_FS_T + 6;

    tt_sep(tx, &ty);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f C", rs->temperature);
    tt_line(tx, &ty, "Temperatura:", buf, temp_color(rs->temperature));

    snprintf(buf, sizeof(buf), "%.1f%%", rs->load);
    tt_line(tx, &ty, "Carga CPU:", buf, load_color(rs->load));

    tt_sep(tx, &ty);

    if (servers  > 0) { snprintf(buf, sizeof(buf), "%dx Servidor",    servers);  tt_line(tx, &ty, "Servidores:", buf,    (Color){ 180, 185, 200, 255 }); }
    if (switches > 0) { snprintf(buf, sizeof(buf), "%dx Switch",     switches);  tt_line(tx, &ty, "Rede:",       buf,    (Color){ 180, 185, 200, 255 }); }
    if (ups      > 0) { snprintf(buf, sizeof(buf), "%dx UPS",        ups);       tt_line(tx, &ty, "No-break:",   buf,    (Color){ 180, 185, 200, 255 }); }
    if (patches  > 0) { snprintf(buf, sizeof(buf), "%dx Patch Panel", patches);  tt_line(tx, &ty, "Patch:",      buf,    (Color){ 180, 185, 200, 255 }); }
    if (blanks   > 0) { snprintf(buf, sizeof(buf), "%dx Vazio",       blanks);   tt_line(tx, &ty, "Blank:",      buf,    (Color){ 100, 105, 115, 255 }); }

    tt_sep(tx, &ty);

    if (rs->alert) {
        DrawText("! ALERTA: acima de 42 C", tx, ty, TT_FS_B, (Color){ 255, 80, 60, 255 });
    } else {
        DrawText("OK  Operando normalmente", tx, ty, TT_FS_B, (Color){ 60, 210, 80, 255 });
    }
}

/* ---- Tooltip: CRAC ---- */
static void draw_tooltip_crac(int rx, int ry, int crac_id, float fan_speed)
{
    int total_h = TT_PAD + TT_FS_T + 6 + 9 + TT_LH * 4 + TT_PAD;

    DrawRectangleRounded((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                         (Color){ 14, 16, 22, 235 });
    DrawRectangleRoundedLines((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                              (Color){ 55, 58, 68, 180 });
    DrawRectangle(rx, ry + 4, 3, total_h - 8, (Color){ 40, 200, 240, 255 });

    int tx = rx + TT_PAD + 4;
    int ty = ry + TT_PAD;

    char title[32];
    snprintf(title, sizeof(title), "CRAC UNIT  #%d", crac_id + 1);
    DrawText(title, tx, ty, TT_FS_T, WHITE);
    ty += TT_FS_T + 6;

    tt_sep(tx, &ty);

    /* Vazão estimada: tipicamente 1000–3500 m³/h proporcional ao fan */
    float flow = 1000.0f + fan_speed * 25.0f;
    char buf[32];

    tt_line(tx, &ty, "Status:",    "Operacional",               (Color){ 60, 210, 80, 255 });
    snprintf(buf, sizeof(buf), "~%.0f m3/h", flow);
    tt_line(tx, &ty, "Vazao ar:", buf,                           (Color){ 40, 200, 240, 255 });
    tt_line(tx, &ty, "Tipo:",     "CRAC – Computer Room A/C",   (Color){ 160, 165, 175, 255 });
    tt_line(tx, &ty, "Local:",    (crac_id == 0) ? "Canto NW" : "Canto NE",
                                                                  (Color){ 160, 165, 175, 255 });
}

/* ---- Tooltip: Arduino ---- */
static void draw_tooltip_arduino(int rx, int ry,
                                  const SensorState *sensors,
                                  float fan_speed, float pdu_balance,
                                  int manual_override)
{
    int total_h = TT_PAD + TT_FS_T + 6 + 9 + TT_LH * 6 + 9 + TT_LH + TT_PAD;

    DrawRectangleRounded((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                         (Color){ 14, 16, 22, 235 });
    DrawRectangleRoundedLines((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                              (Color){ 55, 58, 68, 180 });
    DrawRectangle(rx, ry + 4, 3, total_h - 8, (Color){ 60, 220, 80, 255 });

    int tx = rx + TT_PAD + 4;
    int ty = ry + TT_PAD;

    DrawText("ARDUINO MEGA 2560", tx, ty, TT_FS_T, WHITE);
    ty += TT_FS_T + 6;

    tt_sep(tx, &ty);

    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f C",  sensors_avg_temp((SensorState *)sensors));
    tt_line(tx, &ty, "Temp entrada:", buf, temp_color(sensors_avg_temp((SensorState *)sensors)));

    snprintf(buf, sizeof(buf), "%.1f%%", sensors_avg_load((SensorState *)sensors));
    tt_line(tx, &ty, "Carga media:", buf, load_color(sensors_avg_load((SensorState *)sensors)));

    snprintf(buf, sizeof(buf), "%.1f%%", sensors->ambient_humid);
    tt_line(tx, &ty, "Umidade:", buf,    (Color){ 100, 180, 255, 255 });

    tt_line(tx, &ty, "Interface:", "I2C + One-Wire + ADC", (Color){ 160, 165, 175, 255 });

    snprintf(buf, sizeof(buf), "%.0f%%  (TIM3 PWM)", fan_speed);
    tt_line(tx, &ty, "Fan PWM out:", buf, (Color){ 80, 200, 255, 255 });

    snprintf(buf, sizeof(buf), "%.0f%%  (PDU relay)", pdu_balance);
    tt_line(tx, &ty, "PDU balance:", buf, (Color){ 200, 160, 80, 255 });

    tt_sep(tx, &ty);

    DrawText(manual_override ? "! Modo: MANUAL (override)" : "Modo: Automatico (fuzzy)",
             tx, ty, TT_FS_B,
             manual_override ? (Color){ 255, 180, 40, 255 } : (Color){ 60, 210, 80, 255 });
}

/* ---- Tooltip: Corredor Frio ---- */
static void draw_tooltip_aisle(int rx, int ry, const SensorState *sensors)
{
    int total_h = TT_PAD + TT_FS_T + 6 + 9 + TT_LH * 4 + TT_PAD;

    DrawRectangleRounded((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                         (Color){ 14, 16, 22, 235 });
    DrawRectangleRoundedLines((Rectangle){ rx, ry, TT_W, total_h }, 0.12f, 6,
                              (Color){ 55, 58, 68, 180 });
    DrawRectangle(rx, ry + 4, 3, total_h - 8, (Color){ 60, 100, 220, 255 });

    int tx = rx + TT_PAD + 4;
    int ty = ry + TT_PAD;

    DrawText("CORREDOR FRIO", tx, ty, TT_FS_T, WHITE);
    ty += TT_FS_T + 6;

    tt_sep(tx, &ty);

    char buf[32];
    float avg = sensors_avg_temp((SensorState *)sensors);

    tt_line(tx, &ty, "Contencao:",  "Ativa (paineis)",          (Color){ 60, 100, 220, 255 });
    snprintf(buf, sizeof(buf), "%.1f C", avg - 4.0f);          /* estimativa de entrada */
    tt_line(tx, &ty, "Temp. ar fr:", buf,                       temp_color(avg - 4.0f));
    tt_line(tx, &ty, "Racks:",      "8 unidades (2 fileiras)",  (Color){ 160, 165, 175, 255 });
    tt_line(tx, &ty, "Layout:",     "Cold/Hot Aisle",            (Color){ 160, 165, 175, 255 });
}

/* ------------------------------------------------------------------ */
void inspector_draw_2d(const InspectorState *ins,
                       const RackModel       racks[NUM_RACKS],
                       const SensorState    *sensors,
                       const FuzzyEngine    *fuzzy,
                       float                 fan_speed,
                       float                 pdu_balance,
                       int                   manual_override)
{
    (void)fuzzy;
    if (ins->hovered_idx < 0) return;

    const InspectTarget *t = &ins->targets[ins->hovered_idx];

    /* Posição do tooltip: à direita do cursor, clampeado na tela */
    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    /* Altura estimada do tooltip por tipo (valor conservador) */
    int est_h;
    switch (t->type) {
        case INSPECT_RACK:       est_h = 190; break;
        case INSPECT_CRAC:       est_h = 130; break;
        case INSPECT_ARDUINO:    est_h = 180; break;
        case INSPECT_COLD_AISLE: est_h = 130; break;
        default:                 est_h = 120;
    }

    int rx = (int)mouse.x + TT_MARGIN;
    int ry = (int)mouse.y - est_h / 2;

    /* Clamp horizontal */
    if (rx + TT_W > sw - 4) rx = (int)mouse.x - TT_W - TT_MARGIN;
    if (rx < 4)              rx = 4;

    /* Clamp vertical */
    if (ry < 4)              ry = 4;
    if (ry + est_h > sh - 4) ry = sh - est_h - 4;

    switch (t->type) {
        case INSPECT_RACK:
            draw_tooltip_rack(rx, ry, t->id, &racks[t->id], &sensors->racks[t->id]);
            break;
        case INSPECT_CRAC:
            draw_tooltip_crac(rx, ry, t->id, fan_speed);
            break;
        case INSPECT_ARDUINO:
            draw_tooltip_arduino(rx, ry, sensors, fan_speed, pdu_balance, manual_override);
            break;
        case INSPECT_COLD_AISLE:
            draw_tooltip_aisle(rx, ry, sensors);
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Labels flutuantes 3D projetados em 2D                                */
/* ------------------------------------------------------------------ */

/* Verifica se o ponto está na frente da câmera */
static int in_front(Vector3 world, Camera3D cam)
{
    Vector3 dir  = Vector3Subtract(cam.target, cam.position);
    Vector3 toP  = Vector3Subtract(world, cam.position);
    return Vector3DotProduct(dir, toP) > 0.0f;
}

/* Desenha um label flutuante projetado da posição 3D world */
static void floating_label(Vector3 world, Camera3D cam,
                            const char *text, int fs, Color tc, Color bg)
{
    if (!in_front(world, cam)) return;

    Vector2 sp = GetWorldToScreen(world, cam);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (sp.x < -100 || sp.x > sw + 100 || sp.y < -100 || sp.y > sh + 100) return;

    int tw = MeasureText(text, fs);
    int lx = (int)sp.x - tw / 2 - 5;
    int ly = (int)sp.y - fs / 2 - 3;

    DrawRectangle(lx, ly, tw + 10, fs + 6, bg);
    DrawRectangleLines(lx, ly, tw + 10, fs + 6, (Color){ 60, 64, 76, 140 });
    DrawText(text, lx + 5, ly + 3, fs, tc);
}

static Color rack_temp_label_col(float t)
{
    if (t < 30.0f) return (Color){  55, 130, 255, 255 };
    if (t < 36.0f) return (Color){  55, 210,  80, 255 };
    if (t < 42.0f) return (Color){ 240, 175,  25, 255 };
    return                (Color){ 255,  55,  45, 255 };
}

void inspector_draw_labels(Camera3D camera, const RackModel racks[NUM_RACKS])
{
    /* ---- Racks ---- */
    for (int i = 0; i < NUM_RACKS; i++) {
        Vector3 p = racks[i].position;

        /* Label com número do rack, acima do rack */
        Vector3 top = { p.x, 2.45f, p.z };
        char lbl[10];
        snprintf(lbl, sizeof(lbl), "RACK %d", i);
        floating_label(top, camera, lbl, 11,
                       (Color){ 200, 205, 220, 220 },
                       (Color){ 10, 12, 18, 175 });

        /* Temperatura logo abaixo do label */
        Vector3 temp_pos = { p.x, 2.22f, p.z };
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%.0fC", racks[i].temperature);
        floating_label(temp_pos, camera, tbuf, 10,
                       rack_temp_label_col(racks[i].temperature),
                       (Color){ 10, 12, 18, 140 });
    }

    /* ---- CRACs ---- */
    const char *crac_names[] = { "CRAC 1", "CRAC 2" };
    Vector3 crac_tops[] = {
        { -7.0f, 2.55f, CRAC_CZ },
        {  3.5f, 2.55f, CRAC_CZ }
    };
    for (int c = 0; c < 2; c++) {
        floating_label(crac_tops[c], camera, crac_names[c], 11,
                       (Color){ 40, 200, 240, 220 },
                       (Color){ 8, 22, 30, 175 });
    }

    /* ---- Arduino (controlador) ---- */
    Vector3 ard_top = { 9.5f, 1.25f, -6.5f };
    floating_label(ard_top, camera, "CONTROLADOR", 11,
                   (Color){ 60, 220, 80, 220 },
                   (Color){ 8, 22, 10, 175 });
    Vector3 ard_sub = { 9.5f, 1.06f, -6.5f };
    floating_label(ard_sub, camera, "Arduino Mega", 10,
                   (Color){ 100, 170, 110, 200 },
                   (Color){ 8, 18, 10, 140 });

    /* ---- Corredor frio (etiqueta centralizada no corredor) ---- */
    Vector3 aisle_lbl = { -4.0f, 0.25f, 0.0f };
    floating_label(aisle_lbl, camera, "CORREDOR FRIO", 11,
                   (Color){ 100, 140, 255, 200 },
                   (Color){ 8, 12, 30, 155 });
}
