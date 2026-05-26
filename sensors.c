/*
 * Module:      sensors
 * File:        sensors.c
 * Description: Simulação de sensores com variação temporal realista usando
 *              ruído de Perlin simplificado + tendências por carga de trabalho.
 * Responsibility: Produzir leituras plausíveis sem hardware.
 *
 * ARM Cortex-M4 note:
 *   Em firmware real, sensors_update() seria chamada após leitura de ADC:
 *     uint16_t raw = ADC1->DR;
 *     float volts = raw * 3.3f / 4095.0f;
 *     float temp  = (volts - 0.5f) / 0.01f;  // LM35: 10 mV/°C
 */

#include "sensors.h"
#include <math.h>
#include <string.h>

/* Gerador de ruído pseudo-aleatório determinístico (sem rand()) */
static float noise(float x)
{
    /* Hash de ponto flutuante — padrão para firmware sem stdlib completa */
    int ix = (int)(x * 127.1f);
    float v = sinf((float)ix * 43758.5453f);
    return v - floorf(v);
}

static float smooth_noise(float x)
{
    float i = floorf(x);
    float f = x - i;
    float u = f * f * (3.0f - 2.0f * f); /* suavização cúbica */
    return noise(i) * (1.0f - u) + noise(i + 1.0f) * u;
}

/* Padrões de carga por rack (horários simulados) — valores base em % */
static const float BASE_LOAD[NUM_RACKS] = {
    60.0f, 45.0f, 75.0f, 30.0f, 80.0f, 55.0f, 40.0f, 65.0f
};
/* Temperatura base de cada rack (correlaciona com carga) */
static const float BASE_TEMP[NUM_RACKS] = {
    32.0f, 28.0f, 36.0f, 26.0f, 38.0f, 31.0f, 27.0f, 34.0f
};

void sensors_init(SensorState *s)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < NUM_RACKS; i++) {
        s->racks[i].temperature = BASE_TEMP[i];
        s->racks[i].humidity    = 45.0f;
        s->racks[i].load        = BASE_LOAD[i];
        s->racks[i].power_w     = BASE_LOAD[i] * 5.0f; /* 5 W por % de carga */
        s->racks[i].alert       = 0;
    }
    s->ambient_temp  = 22.0f;
    s->ambient_humid = 42.0f;
    s->time_elapsed  = 0.0f;
}

void sensors_update(SensorState *s, float dt,
                    float crac_factor, float fan_speed)
{
    s->time_elapsed += dt;
    float t = s->time_elapsed;

    /* Ciclo diário comprimido: 90 s = 1 "dia" para demo responsivo */
    float day_phase = t / 90.0f;

    /* Carga de pico no horário comercial (meados do ciclo) */
    float peak = 0.5f + 0.4f * sinf(day_phase * 6.2832f - 1.5708f);

    float total_power = 0.0f;
    for (int i = 0; i < NUM_RACKS; i++) {
        /* Ruído individual por rack com seed diferente */
        float seed_l = t * 0.07f + (float)i * 17.3f;
        float seed_t = t * 0.05f + (float)i * 11.7f;
        float seed_h = t * 0.03f + (float)i * 23.1f;

        float noise_l = (smooth_noise(seed_l) - 0.5f) * 10.0f;
        float noise_t = (smooth_noise(seed_t) - 0.5f) *  3.0f;
        float noise_h = (smooth_noise(seed_h) - 0.5f) *  4.0f;

        float load = BASE_LOAD[i] * peak + noise_l;
        if (load < 5.0f)   load = 5.0f;
        if (load > 100.0f) load = 100.0f;

        /* Temperatura segue carga com inércia térmica.
         * fan_speed resfria em até ±5 °C em relação ao ponto de 50 %.
         * crac_factor < 1 aquece em até +12 °C quando CRAC falha.      */
        float fan_cooling  = (fan_speed - 50.0f) / 50.0f * 5.0f;
        float crac_penalty = (1.0f - crac_factor) * 12.0f;
        float target_temp  = BASE_TEMP[i] + (load - BASE_LOAD[i]) * 0.12f
                           + noise_t + crac_penalty - fan_cooling;
        float alpha = 1.0f - expf(-dt / 8.0f);  /* constante de tempo 8 s */
        s->racks[i].temperature += alpha * (target_temp - s->racks[i].temperature);

        /* Umidade levemente anticorrelacionada com temperatura */
        float target_humid = 45.0f - (s->racks[i].temperature - 28.0f) * 0.5f + noise_h;
        if (target_humid < 20.0f) target_humid = 20.0f;
        if (target_humid > 75.0f) target_humid = 75.0f;
        s->racks[i].humidity = s->racks[i].humidity * 0.98f + target_humid * 0.02f;

        s->racks[i].load    = load;
        s->racks[i].power_w = load * 5.5f; /* ~550 W max por rack */
        s->racks[i].alert   = (s->racks[i].temperature > 42.0f) ? 1 : 0;

        total_power += s->racks[i].power_w;
    }

    s->total_power_kw = total_power / 1000.0f;
    s->ambient_temp   = 20.0f + 2.0f * sinf(t * 0.01f);
    s->ambient_humid  = 42.0f + 5.0f * smooth_noise(t * 0.02f + 99.9f);
}

float sensors_avg_temp(const SensorState *s)
{
    float sum = 0.0f;
    for (int i = 0; i < NUM_RACKS; i++) sum += s->racks[i].temperature;
    return sum / (float)NUM_RACKS;
}

float sensors_avg_load(const SensorState *s)
{
    float sum = 0.0f;
    for (int i = 0; i < NUM_RACKS; i++) sum += s->racks[i].load;
    return sum / (float)NUM_RACKS;
}

int sensors_hottest_rack(const SensorState *s)
{
    int   idx  = 0;
    float max  = s->racks[0].temperature;
    for (int i = 1; i < NUM_RACKS; i++) {
        if (s->racks[i].temperature > max) {
            max = s->racks[i].temperature;
            idx = i;
        }
    }
    return idx;
}
