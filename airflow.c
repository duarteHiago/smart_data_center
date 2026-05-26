/*
 * Module:      airflow
 * File:        airflow.c
 * Description: Sistema de partículas para visualizar fluxo de ar frio
 *              (corredor frio → racks) e ar quente (racks → corredor quente).
 * Responsibility: Animação de partículas; física simples sem colisão.
 */

#include "airflow.h"
#include <math.h>
#include <string.h>

/* Gerador pseudo-aleatório sem rand() (LCG simples) */
static unsigned int g_seed = 12345u;
static float rng_float(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return (float)(g_seed & 0xFFFFu) / 65535.0f;
}

static float rng_range(float lo, float hi)
{
    return lo + rng_float() * (hi - lo);
}

/* Emite uma partícula de ar frio no corredor frio central */
static void spawn_cold(Particle *p, float fan_speed)
{
    p->active    = 1;
    p->hot       = 0;
    p->max_life  = rng_range(3.0f, 6.0f);
    p->lifetime  = p->max_life;
    p->size      = rng_range(0.04f, 0.10f);

    /* Origem: corredor frio (Z ~ 0, entre os racks) */
    p->position.x = rng_range(-9.0f, 2.0f);
    p->position.y = rng_range(0.1f, 1.8f);
    p->position.z = rng_range(-0.8f, 0.8f);

    /* Velocidade: diagonal em direção aos racks (X fixo, drift Z) */
    float speed = 0.3f + fan_speed * 0.015f;
    int   dir   = (p->position.z < 0.0f) ? -1 : 1;
    p->velocity.x = rng_range(-0.05f, 0.05f);
    p->velocity.y = rng_range(-0.02f, 0.08f);
    p->velocity.z = dir * speed + rng_range(-0.05f, 0.05f);

    /* Cor azul-ciano semi-transparente */
    unsigned char alpha = (unsigned char)rng_range(60.0f, 140.0f);
    p->color = (Color){ 40, 120, 220, alpha };
}

/* Emite uma partícula de ar quente saindo pelos racks */
static void spawn_hot(Particle *p, float avg_temp)
{
    p->active    = 1;
    p->hot       = 1;
    p->max_life  = rng_range(2.0f, 5.0f);
    p->lifetime  = p->max_life;
    p->size      = rng_range(0.06f, 0.14f);

    /* Origem: parte traseira dos racks (Z ~ ±2.5, saída de ar quente) */
    int   side = (rng_float() > 0.5f) ? 1 : -1;
    float z0   = side * 2.5f + side * rng_range(0.5f, 1.5f);
    p->position.x = rng_range(-9.0f, 2.0f);
    p->position.y = rng_range(0.5f, 2.0f);
    p->position.z = z0;

    /* Ar quente sobe e se afasta */
    float intensity = (avg_temp - 20.0f) / 25.0f;
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    p->velocity.x = rng_range(-0.05f, 0.05f);
    p->velocity.y = 0.2f + intensity * 0.4f + rng_range(0.0f, 0.1f);
    p->velocity.z = (float)side * rng_range(0.05f, 0.2f);

    /* Cor laranja-vermelho semi-transparente */
    unsigned char r = (unsigned char)(180 + intensity * 75.0f);
    unsigned char g = (unsigned char)(60  - intensity * 30.0f);
    unsigned char alpha = (unsigned char)rng_range(40.0f, 100.0f);
    p->color = (Color){ r, g, 20, alpha };
}

void airflow_init(AirflowSystem *af)
{
    memset(af, 0, sizeof(*af));
    af->fan_speed = 50.0f;
    /* Pré-popula metade das partículas */
    for (int i = 0; i < AIRFLOW_MAX_PARTICLES / 2; i++) {
        if (i % 2 == 0)
            spawn_cold(&af->particles[i], af->fan_speed);
        else
            spawn_hot(&af->particles[i], 30.0f);
        /* Distribui lifetimes para evitar spawn simultâneo */
        af->particles[i].lifetime *= rng_float();
    }
    af->count_active = AIRFLOW_MAX_PARTICLES / 2;
}

void airflow_update(AirflowSystem *af, float dt, float fan_speed, float avg_temp)
{
    af->fan_speed = fan_speed;

    /* Taxa de emissão: proporcional à velocidade dos fans */
    float emit_rate = 5.0f + fan_speed * 0.5f; /* partículas/segundo */
    af->spawn_timer += dt;
    float spawn_interval = 1.0f / emit_rate;

    /* Atualiza partículas existentes */
    af->count_active = 0;
    for (int i = 0; i < AIRFLOW_MAX_PARTICLES; i++) {
        Particle *p = &af->particles[i];
        if (!p->active) continue;

        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) {
            p->active = 0;
            continue;
        }

        /* Física simples: posição += velocidade * dt */
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->position.z += p->velocity.z * dt;

        /* Ar quente sobe com turbulência */
        if (p->hot) {
            p->velocity.y += 0.1f * dt;
            p->velocity.x += (rng_float() - 0.5f) * 0.05f * dt;
        }

        /* Transparência faz fade no fim da vida */
        float life_ratio = p->lifetime / p->max_life;
        p->color.a = (unsigned char)(p->color.a * (0.3f + life_ratio * 0.7f));

        /* Descarta partícula fora dos limites da sala */
        if (p->position.y > 4.2f || p->position.y < -0.1f ||
            fabsf(p->position.x) > 13.0f ||
            fabsf(p->position.z) >  9.0f) {
            p->active = 0;
            continue;
        }

        af->count_active++;
    }

    /* Emite novas partículas */
    while (af->spawn_timer >= spawn_interval) {
        af->spawn_timer -= spawn_interval;

        /* Encontra slot livre */
        for (int i = 0; i < AIRFLOW_MAX_PARTICLES; i++) {
            if (!af->particles[i].active) {
                if (rng_float() < 0.6f)
                    spawn_cold(&af->particles[i], fan_speed);
                else
                    spawn_hot(&af->particles[i], avg_temp);
                af->count_active++;
                break;
            }
        }
    }
}

void airflow_draw(const AirflowSystem *af)
{
    for (int i = 0; i < AIRFLOW_MAX_PARTICLES; i++) {
        const Particle *p = &af->particles[i];
        if (!p->active) continue;

        float life_ratio = p->lifetime / p->max_life;
        float size = p->size * (0.5f + life_ratio * 0.5f);

        DrawSphere(p->position, size, p->color);
    }
}
