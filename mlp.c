/*
 * Module:      mlp
 * File:        mlp.c
 * Description: Implementação da rede neural feedforward para previsão térmica.
 *              Sem alocação dinâmica — compatível com AVR (Arduino) e ARM.
 */

#include "mlp.h"
#include <math.h>

/* -----------------------------------------------------------------------
 * Pesos calibrados para a física do data center:
 *   h0 "quente e sobrecarregado"  → sinal de temperatura subindo
 *   h1 "refrigeração eficiente"   → sinal de temperatura caindo
 *   h2 "carga estável"            → correlação com carga persistente
 *   h3 "risco de sobrecarga"      → carga muito alta independente do fan
 * ----------------------------------------------------------------------- */

static const float W1[MLP_H1][MLP_IN] = {
    /*            T_norm   L_norm  Fan_norm */
    /* h0 */  {  2.10f,   1.80f, -1.60f },
    /* h1 */  { -1.40f,  -0.80f,  2.20f },
    /* h2 */  {  0.40f,   1.90f, -0.40f },
    /* h3 */  {  1.10f,   2.40f, -0.90f },
};
static const float B1[MLP_H1] = { -1.60f, -0.40f, -0.90f, -2.10f };

static const float W2[MLP_OUT][MLP_H1] = {
    /*              h0      h1      h2      h3  */
    /* T_pred */ {  1.30f, -0.90f,  0.60f,  0.70f },
    /* L_pred */ {  0.20f, -0.30f,  1.40f,  0.40f },
};
static const float B2[MLP_OUT] = { 0.15f, 0.25f };

/* ----------------------------------------------------------------------- */

static float clamp01(float x)
{
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

/* tanh remapeado para [0,1] — mesma equação executável em AVR via libm */
static float tanh_norm(float x)
{
    return (tanhf(x) + 1.0f) * 0.5f;
}

static float sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

void mlp_init(MlpNet *net)
{
    for (int i = 0; i < MLP_H1; i++) {
        for (int j = 0; j < MLP_IN; j++)
            net->w1[i][j] = W1[i][j];
        net->b1[i] = B1[i];
    }
    for (int i = 0; i < MLP_OUT; i++) {
        for (int j = 0; j < MLP_H1; j++)
            net->w2[i][j] = W2[i][j];
        net->b2[i] = B2[i];
    }
    net->pred_temp = 25.0f;
    net->pred_load = 50.0f;
}

void mlp_forward(MlpNet *net, float temp, float load, float fan)
{
    /* ---- Normalização [0,1] ---- */
    float in[MLP_IN];
    in[0] = clamp01((temp - 15.0f) / 40.0f);  /* [15,55] → [0,1] */
    in[1] = clamp01(load / 100.0f);
    in[2] = clamp01(fan  / 100.0f);
    for (int i = 0; i < MLP_IN; i++)
        net->a_in[i] = in[i];

    /* ---- Camada oculta (tanh normalizado) ---- */
    float h[MLP_H1];
    for (int j = 0; j < MLP_H1; j++) {
        float z = net->b1[j];
        for (int i = 0; i < MLP_IN; i++)
            z += net->w1[j][i] * in[i];
        h[j] = tanh_norm(z);
        net->a_h1[j] = h[j];
    }

    /* ---- Camada de saída (sigmoid) ---- */
    for (int k = 0; k < MLP_OUT; k++) {
        float z = net->b2[k];
        for (int j = 0; j < MLP_H1; j++)
            z += net->w2[k][j] * h[j];
        net->a_out[k] = sigmoid(z);
    }

    /* ---- Desnormalização ---- */
    net->pred_temp = 15.0f + net->a_out[0] * 40.0f;
    net->pred_load =          net->a_out[1] * 100.0f;
}
