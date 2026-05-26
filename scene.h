/*
 * Module:      scene
 * File:        scene.h
 * Description: Ambiente 3D da sala de data center: piso, paredes, teto,
 *              iluminação ambiente e fontes de luz dinâmicas.
 * Responsibility: Contexto geométrico e de iluminação da simulação.
 */

#ifndef SCENE_H
#define SCENE_H

#include "raylib.h"

/* Dimensões da sala (unidades de mundo = metros) */
#define SCENE_WIDTH  24.0f
#define SCENE_DEPTH  16.0f
#define SCENE_HEIGHT  4.0f

typedef struct {
    Color  floor_color;
    Color  wall_color;
    Color  ceiling_color;
    float  ambient_intensity; /* 0.0–1.0 */
    int    crac_failed[2];    /* 1 = unidade CRAC [0] ou [1] em falha  */
} SceneEnv;

/* Inicializa ambiente com cores e luzes padrão */
void scene_init(SceneEnv *env);

/* Desenha geometria opaca (piso, pilares, CRAC, luminárias) */
void scene_draw(const SceneEnv *env);

/* Desenha geometria transparente (contenção do corredor frio).
 * Chamar DEPOIS de todos os objetos opacos da cena. */
void scene_draw_transparent(const SceneEnv *env);

/* Atualiza intensidade de luz baseado em temperatura média */
void scene_update_lighting(SceneEnv *env, float avg_temp);

#endif /* SCENE_H */
