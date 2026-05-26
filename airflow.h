/*
 * Module:      airflow
 * File:        airflow.h
 * Description: Sistema de partículas para visualizar fluxo de ar frio e quente
 *              nos corredores do data center (cold aisle / hot aisle).
 * Responsibility: Animação de partículas; sem dependência de lógica de negócio.
 */

#ifndef AIRFLOW_H
#define AIRFLOW_H

#include "raylib.h"

#define AIRFLOW_MAX_PARTICLES 400

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Color   color;
    float   lifetime;    /* segundos restantes de vida              */
    float   max_life;
    float   size;
    int     active;      /* 1 = viva, 0 = disponível para reutilizar*/
    int     hot;         /* 1 = ar quente, 0 = ar frio              */
} Particle;

typedef struct {
    Particle particles[AIRFLOW_MAX_PARTICLES];
    int      count_active;
    float    spawn_timer;   /* temporizador para emissão              */
    float    fan_speed;     /* 0–100 % → controla velocidade/taxa     */
} AirflowSystem;

/* Inicializa sistema de partículas */
void airflow_init(AirflowSystem *af);

/* Atualiza física e lifetime das partículas; dt em segundos */
void airflow_update(AirflowSystem *af, float dt, float fan_speed, float avg_temp);

/* Desenha todas as partículas ativas como esferas semi-transparentes */
void airflow_draw(const AirflowSystem *af);

#endif /* AIRFLOW_H */
