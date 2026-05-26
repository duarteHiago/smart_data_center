/*
 * Module:      signals
 * File:        signals.c
 * Description: Sistema de linhas de sinal animadas entre Arduino e racks.
 * Responsibility: Tornar visível o fluxo de dados do sistema embarcado.
 *
 * Funcionamento visual:
 *   - Linha fina semi-transparente = conexão permanente (o "fio")
 *   - Esferas brilhantes movendo-se ao longo da linha = pulso de dado
 *   - Sensor (azul→vermelho): saem dos racks em direção ao Arduino
 *   - PWM (verde→laranja):    saem do Arduino em direção aos racks
 *   - Intensidade do pulso proporcional ao valor (temp ou fan_speed)
 */

#include "signals.h"
#include "rack.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Offset na PCB do Arduino para cada sinal ter origem em pino diferente */
static const Vector3 ARDUINO_PIN_OFFSETS[NUM_RACKS] = {
    {-0.05f, 0.0f,  0.03f},
    {-0.03f, 0.0f,  0.03f},
    {-0.01f, 0.0f,  0.03f},
    { 0.01f, 0.0f,  0.03f},
    {-0.05f, 0.0f, -0.03f},
    {-0.03f, 0.0f, -0.03f},
    {-0.01f, 0.0f, -0.03f},
    { 0.01f, 0.0f, -0.03f},
};

void signals_init(SignalSystem *ss,
                  Vector3 arduino_pos,
                  const Vector3 rack_positions[NUM_RACKS])
{
    memset(ss, 0, sizeof(*ss));

    for (int i = 0; i < NUM_RACKS; i++) {
        Vector3 pin = {
            arduino_pos.x + ARDUINO_PIN_OFFSETS[i].x,
            arduino_pos.y + ARDUINO_PIN_OFFSETS[i].y,
            arduino_pos.z + ARDUINO_PIN_OFFSETS[i].z
        };
        /* Destino no rack: meio da altura, frente do rack */
        Vector3 rack_pt = {
            rack_positions[i].x,
            rack_positions[i].y,   /* centro vertical do rack */
            rack_positions[i].z
        };

        /* --- Caminho SENSOR: rack → Arduino --- */
        SignalPath *sp = &ss->paths[i];
        sp->from        = rack_pt;
        sp->to          = pin;
        sp->speed       = 0.22f;      /* ciclo ~4.5 s */
        sp->type        = SIG_SENSOR;
        sp->rack_id     = i;
        sp->active      = 1;
        sp->intensity   = 0.5f;
        sp->line_color  = (Color){ 60, 120, 200, 50 };
        sp->pulse_color = (Color){ 80, 160, 255, 220 };
        /* Pulsos espaçados em 1/3 do caminho */
        for (int p = 0; p < SIG_PULSES; p++)
            sp->pulse_t[p] = (float)p / (float)SIG_PULSES;

        /* --- Caminho PWM: Arduino → rack --- */
        SignalPath *pp = &ss->paths[NUM_RACKS + i];
        pp->from        = pin;
        pp->to          = rack_pt;
        pp->speed       = 0.35f;      /* ciclo ~2.9 s */
        pp->type        = SIG_PWM;
        pp->rack_id     = i;
        pp->active      = 1;
        pp->intensity   = 0.5f;
        pp->line_color  = (Color){ 80, 200, 80, 45 };
        pp->pulse_color = (Color){ 120, 255, 80, 220 };
        for (int p = 0; p < SIG_PULSES; p++)
            pp->pulse_t[p] = (float)p / (float)SIG_PULSES;
    }
}

void signals_update(SignalSystem *ss, float dt,
                    float fan_speed,
                    const RackSensor sensors[NUM_RACKS],
                    int hottest_rack)
{
    ss->time        += dt;
    ss->hottest_rack = hottest_rack;

    for (int i = 0; i < NUM_RACKS; i++) {
        /* ---- Sinal SENSOR ---- */
        SignalPath *sp = &ss->paths[i];
        float temp = sensors[i].temperature;

        /* Intensidade: temperatura normalizada 20-50°C */
        sp->intensity = (temp - 20.0f) / 30.0f;
        if (sp->intensity < 0.15f) sp->intensity = 0.15f;
        if (sp->intensity > 1.0f)  sp->intensity = 1.0f;

        /* Velocidade maior quando rack está quente (urgência) */
        float sensor_speed = 0.18f + sp->intensity * 0.18f;

        for (int p = 0; p < SIG_PULSES; p++) {
            sp->pulse_t[p] += dt * sensor_speed;
            if (sp->pulse_t[p] > 1.0f) sp->pulse_t[p] -= 1.0f;
        }

        /* Cor do pulso: azul frio → vermelho quente */
        sp->pulse_color = rack_temp_color(temp);
        sp->pulse_color.a = 230;

        /* Linha mais brilhante se rack em alerta */
        sp->line_color.a = sensors[i].alert ? 90 : 45;

        snprintf(sp->label, sizeof(sp->label), "%.1f C", temp);

        /* ---- Sinal PWM ---- */
        SignalPath *pp = &ss->paths[NUM_RACKS + i];
        float duty = fan_speed / 100.0f;
        pp->intensity = duty;
        if (pp->intensity < 0.1f) pp->intensity = 0.1f;

        /* Velocidade proporcional ao PWM (fan mais rápido = sinal mais rápido) */
        float pwm_speed = 0.2f + duty * 0.4f;

        for (int p = 0; p < SIG_PULSES; p++) {
            pp->pulse_t[p] += dt * pwm_speed;
            if (pp->pulse_t[p] > 1.0f) pp->pulse_t[p] -= 1.0f;
        }

        /* Cor: verde (fan lento) → laranja (fan rápido) */
        unsigned char r = (unsigned char)(50  + duty * 200);
        unsigned char g = (unsigned char)(220 - duty * 140);
        pp->pulse_color = (Color){ r, g, 40, 220 };
        pp->line_color  = (Color){ 60, 160, 60, (unsigned char)(30 + duty * 50) };

        snprintf(pp->label, sizeof(pp->label), "PWM %.0f%%", fan_speed);
    }
}

/* Interpolação linear entre dois Vector3 */
static Vector3 vec3_lerp(Vector3 a, Vector3 b, float t)
{
    return (Vector3){
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

/* Desenha um rótulo 3D usando DrawLine3D + posição de texto */
static void draw_label_3d(Vector3 pos, const char *text, Color col)
{
    /* Linha vertical pontilhada de referência */
    DrawLine3D(pos, (Vector3){ pos.x, pos.y + 0.3f, pos.z },
               (Color){ col.r, col.g, col.b, 80 });
    /* Esfera de ancoragem */
    DrawSphere((Vector3){ pos.x, pos.y + 0.32f, pos.z }, 0.06f, col);
}

void signals_draw(const SignalSystem *ss)
{
    for (int i = 0; i < SIG_NUM_PATHS; i++) {
        const SignalPath *sp = &ss->paths[i];
        if (!sp->active) continue;

        /* Linha base fina (o "fio") */
        DrawLine3D(sp->from, sp->to, sp->line_color);

        /* Pulsos animados ao longo do fio */
        for (int p = 0; p < SIG_PULSES; p++) {
            float t = sp->pulse_t[p];
            Vector3 pulse_pos = vec3_lerp(sp->from, sp->to, t);

            float size = 0.04f + sp->intensity * 0.06f;

            /* Núcleo brilhante */
            DrawSphere(pulse_pos, size, sp->pulse_color);

            /* Halo exterior semi-transparente */
            Color halo = sp->pulse_color;
            halo.a = (unsigned char)(80 * sp->intensity);
            DrawSphere(pulse_pos, size * 2.0f, halo);
        }

        /* Rótulo animado na metade do caminho para sinal sensor */
        if (sp->type == SIG_SENSOR) {
            float mid_t = 0.5f;
            /* Pulso mais próximo do meio */
            float best = 99.0f;
            for (int p = 0; p < SIG_PULSES; p++) {
                float d = sp->pulse_t[p] - mid_t;
                if (d < 0.0f) d = -d;
                if (d < best) { best = d; }
            }
            /* Só mostra rótulo quando um pulso está perto da metade */
            if (best < 0.08f) {
                Vector3 mid = vec3_lerp(sp->from, sp->to, mid_t);
                mid.y += 0.15f;
                draw_label_3d(mid, sp->label, sp->pulse_color);
            }
        }

        /* Rótulo PWM próximo ao rack de destino */
        if (sp->type == SIG_PWM) {
            /* Mostra quando qualquer pulso chega perto do rack (t > 0.85) */
            int show = 0;
            for (int p = 0; p < SIG_PULSES; p++)
                if (sp->pulse_t[p] > 0.85f) { show = 1; break; }
            if (show) {
                Vector3 near_rack = vec3_lerp(sp->from, sp->to, 0.9f);
                near_rack.y += 0.2f;
                draw_label_3d(near_rack, sp->label, sp->pulse_color);
            }
        }
    }
}
