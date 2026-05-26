/*
 * Module:      mlp
 * File:        mlp.h
 * Description: Rede neural feedforward (MLP) para previsão de temperatura e
 *              carga CPU ~5 s à frente. Pesos pré-definidos que refletem a
 *              física do sistema térmico; inferência portável para AVR/ARM.
 *
 * Arquitetura: 3 entradas → 4 ocultos (tanh) → 2 saídas (sigmoid)
 * Entradas:    temperatura média (°C), carga CPU (%), velocidade fan (%)
 * Saídas:      temperatura prevista (+5 s), carga prevista (+5 s)
 */

#ifndef MLP_H
#define MLP_H

#define MLP_IN  3   /* T, L, Fan */
#define MLP_H1  4   /* neurônios ocultos */
#define MLP_OUT 2   /* T_pred, L_pred */

typedef struct {
    float w1[MLP_H1][MLP_IN];   /* pesos camada de entrada→oculta */
    float b1[MLP_H1];            /* bias camada oculta             */
    float w2[MLP_OUT][MLP_H1];  /* pesos camada oculta→saída       */
    float b2[MLP_OUT];           /* bias camada de saída            */

    /* Ativações normalizadas [0..1] — lidas pela visualização animada */
    float a_in [MLP_IN];
    float a_h1 [MLP_H1];
    float a_out[MLP_OUT];

    /* Predições em unidades físicas */
    float pred_temp;   /* °C previsto para +5 s */
    float pred_load;   /* % CPU previsto para +5 s */
} MlpNet;

void mlp_init   (MlpNet *net);
void mlp_forward(MlpNet *net, float temp, float load, float fan);

#endif /* MLP_H */
