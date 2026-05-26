/*
 * Module:      compare
 * File:        compare.h
 * Description: Simulação paralela com controlador de histerese para comparação
 *              live contra o controlador fuzzy.
 * Responsibility: Manter histórico de ambas as estratégias de controle.
 */

#ifndef COMPARE_H
#define COMPARE_H

#define CMP_HIST 180   /* ~3 minutos a 1 amostra/s */

typedef struct {
    float fuzzy_temp[CMP_HIST];  /* temperatura real (fuzzy ativo)     */
    float fuzzy_fan [CMP_HIST];  /* saída do fan fuzzy                  */
    float hyst_temp [CMP_HIST];  /* temperatura simulada com histerese  */
    float hyst_fan  [CMP_HIST];  /* saída do fan histerese              */
    int   idx;                   /* contador raw (cresce indefinidamente)*/
    float hyst_temp_sim;         /* estado térmico do modelo paralelo   */
    float hyst_prev_fan;         /* último fan da histerese (deadband)  */
} CompareState;

/* Inicializa estrutura com valores plausíveis */
void compare_init(CompareState *cmp);

/* Avança um passo temporal:
 *   fuzzy_fan      — saída atual do controlador fuzzy (0-100)
 *   real_avg_temp  — média de temperatura dos sensores reais
 *   avg_load       — média de carga dos racks
 *   dt             — delta de tempo em segundos                        */
void compare_update(CompareState *cmp,
                    float fuzzy_fan, float real_avg_temp,
                    float avg_load,  float dt);

#endif /* COMPARE_H */
