/*
 * Module:      compare
 * File:        compare.c
 * Description: Modelo térmico paralelo com controlador de histerese.
 *              A temperatura simulada diverge da fuzzy conforme a estratégia
 *              de controle provoca oscilações (liga/desliga) vs suavidade.
 */

#include "compare.h"
#include <string.h>
#include <math.h>

void compare_init(CompareState *cmp)
{
    memset(cmp, 0, sizeof(*cmp));
    cmp->hyst_temp_sim = 32.0f;
    cmp->hyst_prev_fan = 15.0f;
}

void compare_update(CompareState *cmp,
                    float fuzzy_fan, float real_avg_temp,
                    float avg_load,  float dt)
{
    /* ---- Controlador de histerese ----
     * Liga fan a 100 % quando temperatura cruza 40 °C;
     * desliga para 15 % quando cai abaixo de 37 °C;
     * mantém o estado anterior dentro do deadband [37, 40].            */
    float hf;
    if      (cmp->hyst_temp_sim > 40.0f) hf = 100.0f;
    else if (cmp->hyst_temp_sim < 37.0f) hf =  15.0f;
    else                                  hf = cmp->hyst_prev_fan;
    cmp->hyst_prev_fan = hf;

    /* ---- Modelo térmico simplificado ----
     * target = carga aquece + fan esfria; constante de tempo 25 s.     */
    float target = 22.0f + avg_load * 0.18f - hf * 0.09f;
    float alpha  = 1.0f - expf(-dt / 25.0f);
    cmp->hyst_temp_sim += alpha * (target - cmp->hyst_temp_sim);
    if (cmp->hyst_temp_sim < 18.0f) cmp->hyst_temp_sim = 18.0f;
    if (cmp->hyst_temp_sim > 55.0f) cmp->hyst_temp_sim = 55.0f;

    /* ---- Grava no buffer circular ---- */
    int wi = cmp->idx % CMP_HIST;
    cmp->fuzzy_temp[wi] = real_avg_temp;
    cmp->fuzzy_fan [wi] = fuzzy_fan;
    cmp->hyst_temp [wi] = cmp->hyst_temp_sim;
    cmp->hyst_fan  [wi] = hf;
    cmp->idx++;
}
