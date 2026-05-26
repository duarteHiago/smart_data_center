/*
 * Module:      fuzzy
 * File:        fuzzy.c
 * Description: Implementação do motor Mamdani com defuzzificação centroid.
 * Responsibility: Toda a computação fuzzy — sem Raylib, portável para firmware.
 *
 * Lógica Fuzzy — variáveis:
 *   ENTRADAS:
 *     0. temperatura  (°C)   : VERY_LOW<20  LOW<28  MEDIUM<34  HIGH<40  VERY_HIGH>=40
 *     1. carga        (%)    : VERY_LOW<15  LOW<35  MEDIUM<55  HIGH<75  VERY_HIGH>=75
 *     2. umidade      (%)    : VERY_LOW<25  LOW<40  MEDIUM<55  HIGH<70  VERY_HIGH>=70
 *   SAÍDAS:
 *     0. fan_speed    (%)    : LOW<25  MEDIUM<50  HIGH<75  VERY_HIGH>=75
 *     1. pdu_balance  (%)    : LOW<25  MEDIUM<50  HIGH<75  VERY_HIGH>=75
 *
 * ARM Cortex-M4 note:
 *   fuzzy_compute() equivale à ISR do TIM2 no firmware:
 *     void TIM2_IRQHandler(void) {
 *         TIM2->SR &= ~TIM_SR_UIF;
 *         fuzzy_compute(&g_engine, g_inputs, g_outputs);
 *         TIM3->CCR2 = (uint16_t)(g_outputs[0] * TIM3->ARR / 100.0f);
 *     }
 */

#include "fuzzy.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Grau de pertinência trapezoidal                                      */
/* ------------------------------------------------------------------ */
float fuzzy_membership(const FuzzySet *s, float x)
{
    if (x <= s->a || x >= s->d) return 0.0f;
    if (x >= s->b && x <= s->c) return 1.0f;
    if (x < s->b)  return (x - s->a) / (s->b - s->a);
    /* x > c */    return (s->d - x) / (s->d - s->c);
}

/* ------------------------------------------------------------------ */
/* Nome textual dos rótulos                                             */
/* ------------------------------------------------------------------ */
const char *fuzzy_label_name(FuzzyLabel l)
{
    switch (l) {
        case FS_VERY_LOW:  return "MUITO_BAIXO";
        case FS_LOW:       return "BAIXO";
        case FS_MEDIUM:    return "MEDIO";
        case FS_HIGH:      return "ALTO";
        case FS_VERY_HIGH: return "MUITO_ALTO";
        default:           return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Helpers internos                                                     */
/* ------------------------------------------------------------------ */
static float fmin2(float a, float b) { return a < b ? a : b; }
static float fmax2(float a, float b) { return a > b ? a : b; }
static float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------------ */
/* Inicialização das variáveis linguísticas                             */
/* ------------------------------------------------------------------ */
static void init_input_temp(FuzzyVar *v)
{
    v->name    = "temperatura";
    v->min_val = 15.0f;
    v->max_val = 55.0f;
    v->num_sets = 5;
    /* VERY_LOW: 15–20–20–24 */
    v->sets[0] = (FuzzySet){ 15.0f, 15.0f, 20.0f, 24.0f, FS_VERY_LOW  };
    /* LOW: 20–24–28–32 */
    v->sets[1] = (FuzzySet){ 20.0f, 24.0f, 28.0f, 32.0f, FS_LOW       };
    /* MEDIUM: 28–32–35–39 */
    v->sets[2] = (FuzzySet){ 28.0f, 32.0f, 35.0f, 39.0f, FS_MEDIUM    };
    /* HIGH: 35–39–42–46 */
    v->sets[3] = (FuzzySet){ 35.0f, 39.0f, 42.0f, 46.0f, FS_HIGH      };
    /* VERY_HIGH: 42–46–55–55 */
    v->sets[4] = (FuzzySet){ 42.0f, 46.0f, 55.0f, 55.0f, FS_VERY_HIGH };
}

static void init_input_load(FuzzyVar *v)
{
    v->name    = "carga";
    v->min_val = 0.0f;
    v->max_val = 100.0f;
    v->num_sets = 5;
    v->sets[0] = (FuzzySet){  0.0f,  0.0f, 15.0f, 25.0f, FS_VERY_LOW  };
    v->sets[1] = (FuzzySet){ 15.0f, 25.0f, 35.0f, 45.0f, FS_LOW       };
    v->sets[2] = (FuzzySet){ 35.0f, 45.0f, 55.0f, 65.0f, FS_MEDIUM    };
    v->sets[3] = (FuzzySet){ 55.0f, 65.0f, 75.0f, 85.0f, FS_HIGH      };
    v->sets[4] = (FuzzySet){ 75.0f, 85.0f,100.0f,100.0f, FS_VERY_HIGH };
}

static void init_input_humid(FuzzyVar *v)
{
    v->name    = "umidade";
    v->min_val = 0.0f;
    v->max_val = 100.0f;
    v->num_sets = 5;
    v->sets[0] = (FuzzySet){  0.0f,  0.0f, 25.0f, 35.0f, FS_VERY_LOW  };
    v->sets[1] = (FuzzySet){ 25.0f, 35.0f, 42.0f, 50.0f, FS_LOW       };
    v->sets[2] = (FuzzySet){ 42.0f, 50.0f, 57.0f, 65.0f, FS_MEDIUM    };
    v->sets[3] = (FuzzySet){ 57.0f, 65.0f, 72.0f, 80.0f, FS_HIGH      };
    v->sets[4] = (FuzzySet){ 72.0f, 80.0f,100.0f,100.0f, FS_VERY_HIGH };
}

static void init_output_fan(FuzzyVar *v)
{
    v->name    = "fan_speed";
    v->min_val = 0.0f;
    v->max_val = 100.0f;
    v->num_sets = 5;
    v->sets[0] = (FuzzySet){  0.0f,  0.0f, 10.0f, 25.0f, FS_VERY_LOW  };
    v->sets[1] = (FuzzySet){ 10.0f, 25.0f, 35.0f, 50.0f, FS_LOW       };
    v->sets[2] = (FuzzySet){ 35.0f, 50.0f, 60.0f, 70.0f, FS_MEDIUM    };
    v->sets[3] = (FuzzySet){ 60.0f, 70.0f, 80.0f, 90.0f, FS_HIGH      };
    v->sets[4] = (FuzzySet){ 80.0f, 90.0f,100.0f,100.0f, FS_VERY_HIGH };
}

static void init_output_pdu(FuzzyVar *v)
{
    v->name    = "pdu_balance";
    v->min_val = 0.0f;
    v->max_val = 100.0f;
    v->num_sets = 5;
    v->sets[0] = (FuzzySet){  0.0f,  0.0f, 10.0f, 25.0f, FS_VERY_LOW  };
    v->sets[1] = (FuzzySet){ 10.0f, 25.0f, 35.0f, 50.0f, FS_LOW       };
    v->sets[2] = (FuzzySet){ 35.0f, 50.0f, 60.0f, 70.0f, FS_MEDIUM    };
    v->sets[3] = (FuzzySet){ 60.0f, 70.0f, 80.0f, 90.0f, FS_HIGH      };
    v->sets[4] = (FuzzySet){ 80.0f, 90.0f,100.0f,100.0f, FS_VERY_HIGH };
}

/* ------------------------------------------------------------------ */
/* Base de regras Mamdani                                               */
/* antecedent[i] = -1  →  "qualquer valor" (don't care)               */
/*                                                                      */
/* R01: IF temp=VH AND load=VH            THEN fan=VH  pdu=VH          */
/* R02: IF temp=VH AND load=H             THEN fan=VH  pdu=H           */
/* R03: IF temp=H  AND load=VH            THEN fan=H   pdu=VH          */
/* R04: IF temp=H  AND load=H             THEN fan=H   pdu=H           */
/* R05: IF temp=H  AND load=M             THEN fan=H   pdu=M           */
/* R06: IF temp=M  AND load=VH            THEN fan=M   pdu=VH          */
/* R07: IF temp=M  AND load=H             THEN fan=M   pdu=H           */
/* R08: IF temp=M  AND load=M             THEN fan=M   pdu=M           */
/* R09: IF temp=L  AND load=M             THEN fan=L   pdu=M           */
/* R10: IF temp=L  AND load=L             THEN fan=L   pdu=L           */
/* R11: IF temp=VL AND load=*             THEN fan=VL  pdu=VL          */
/* R12: IF humid=VH AND temp>=M           THEN fan=VH  pdu=M           */
/* R13: IF humid=H  AND temp=H            THEN fan=H   pdu=L           */
/* R14: IF humid=L  AND temp=VH           THEN fan=VH  pdu=VH          */
/* R15: IF temp=VH AND load=L             THEN fan=H   pdu=L           */
/* R16: IF temp=VH AND load=VL            THEN fan=M   pdu=VL          */
/* R17: IF temp=H  AND load=L             THEN fan=M   pdu=L           */
/* R18: IF temp=M  AND load=L             THEN fan=L   pdu=L           */
/* R19: IF temp=M  AND load=VL            THEN fan=VL  pdu=VL          */
/* R20: IF temp=L  AND load=VH            THEN fan=M   pdu=VH          */
/* R21: IF temp=L  AND load=H             THEN fan=L   pdu=H           */
/* R22: IF temp=VL AND load=VH            THEN fan=L   pdu=VH          */
/* R23: IF humid=VH AND temp=VH           THEN fan=VH  pdu=VH          */
/* R24: IF humid=VH AND temp=L            THEN fan=M   pdu=VL          */
/* R25: IF temp=H  AND load=VL            THEN fan=L   pdu=VL          */
/* ------------------------------------------------------------------ */
static void init_rules(FuzzyEngine *eng)
{
    int r = 0;

#define RULE(t,l,h, f,p, w) \
    eng->rules[r].antecedent[0]=t; \
    eng->rules[r].antecedent[1]=l; \
    eng->rules[r].antecedent[2]=h; \
    eng->rules[r].consequent[0]=f; \
    eng->rules[r].consequent[1]=p; \
    eng->rules[r].weight=w; r++

    /* temp, load, humid → fan, pdu */
    RULE(FS_VERY_HIGH, FS_VERY_HIGH, -1,           FS_VERY_HIGH, FS_VERY_HIGH, 1.0f);
    RULE(FS_VERY_HIGH, FS_HIGH,      -1,           FS_VERY_HIGH, FS_HIGH,      1.0f);
    RULE(FS_HIGH,      FS_VERY_HIGH, -1,           FS_HIGH,      FS_VERY_HIGH, 1.0f);
    RULE(FS_HIGH,      FS_HIGH,      -1,           FS_HIGH,      FS_HIGH,      1.0f);
    RULE(FS_HIGH,      FS_MEDIUM,    -1,           FS_HIGH,      FS_MEDIUM,    1.0f);
    RULE(FS_MEDIUM,    FS_VERY_HIGH, -1,           FS_MEDIUM,    FS_VERY_HIGH, 1.0f);
    RULE(FS_MEDIUM,    FS_HIGH,      -1,           FS_MEDIUM,    FS_HIGH,      1.0f);
    RULE(FS_MEDIUM,    FS_MEDIUM,    -1,           FS_MEDIUM,    FS_MEDIUM,    1.0f);
    RULE(FS_LOW,       FS_MEDIUM,    -1,           FS_LOW,       FS_MEDIUM,    1.0f);
    RULE(FS_LOW,       FS_LOW,       -1,           FS_LOW,       FS_LOW,       1.0f);
    RULE(FS_VERY_LOW,  -1,           -1,           FS_VERY_LOW,  FS_VERY_LOW,  1.0f);
    RULE(FS_MEDIUM,    -1,           FS_VERY_HIGH, FS_VERY_HIGH, FS_MEDIUM,    0.9f);
    RULE(FS_HIGH,      -1,           FS_HIGH,      FS_HIGH,      FS_LOW,       0.8f);
    RULE(FS_VERY_HIGH, -1,           FS_LOW,       FS_VERY_HIGH, FS_VERY_HIGH, 0.9f);
    RULE(FS_VERY_HIGH, FS_LOW,       -1,           FS_HIGH,      FS_LOW,       1.0f);
    RULE(FS_VERY_HIGH, FS_VERY_LOW,  -1,           FS_MEDIUM,    FS_VERY_LOW,  1.0f);
    RULE(FS_HIGH,      FS_LOW,       -1,           FS_MEDIUM,    FS_LOW,       1.0f);
    RULE(FS_MEDIUM,    FS_LOW,       -1,           FS_LOW,       FS_LOW,       1.0f);
    RULE(FS_MEDIUM,    FS_VERY_LOW,  -1,           FS_VERY_LOW,  FS_VERY_LOW,  1.0f);
    RULE(FS_LOW,       FS_VERY_HIGH, -1,           FS_MEDIUM,    FS_VERY_HIGH, 1.0f);
    RULE(FS_LOW,       FS_HIGH,      -1,           FS_LOW,       FS_HIGH,      1.0f);
    RULE(FS_VERY_LOW,  FS_VERY_HIGH, -1,           FS_LOW,       FS_VERY_HIGH, 1.0f);
    RULE(FS_VERY_HIGH, -1,           FS_VERY_HIGH, FS_VERY_HIGH, FS_VERY_HIGH, 1.0f);
    RULE(FS_LOW,       -1,           FS_VERY_HIGH, FS_MEDIUM,    FS_VERY_LOW,  0.7f);
    RULE(FS_HIGH,      FS_VERY_LOW,  -1,           FS_LOW,       FS_VERY_LOW,  1.0f);

#undef RULE
    eng->num_rules = r;
}

/* ------------------------------------------------------------------ */
/* fuzzy_init                                                           */
/* ------------------------------------------------------------------ */
void fuzzy_init(FuzzyEngine *eng)
{
    memset(eng, 0, sizeof(*eng));
    init_input_temp (&eng->inputs[FUZZY_IN_TEMP]);
    init_input_load (&eng->inputs[FUZZY_IN_LOAD]);
    init_input_humid(&eng->inputs[FUZZY_IN_HUMID]);
    init_output_fan (&eng->outputs[FUZZY_OUT_FAN]);
    init_output_pdu (&eng->outputs[FUZZY_OUT_PDU]);
    init_rules(eng);
}

/* ------------------------------------------------------------------ */
/* fuzzy_compute — ciclo completo                                       */
/* ------------------------------------------------------------------ */
void fuzzy_compute(FuzzyEngine *eng,
                   const float inputs[FUZZY_NUM_INPUTS],
                   float outputs[FUZZY_NUM_OUTPUTS])
{
    int i, j, k;

    /* ---- 1. Fuzzificação ---- */
    for (i = 0; i < FUZZY_NUM_INPUTS; i++) {
        float clamped = fclamp(inputs[i],
                               eng->inputs[i].min_val,
                               eng->inputs[i].max_val);
        for (j = 0; j < eng->inputs[i].num_sets; j++) {
            eng->mu_in[i][j] = fuzzy_membership(&eng->inputs[i].sets[j], clamped);
        }
    }

    /* ---- 2. Avaliação das regras (AND = mínimo) ---- */
    for (j = 0; j < FUZZY_NUM_OUTPUTS; j++)
        for (k = 0; k < FUZZY_MAX_SETS; k++)
            eng->mu_out[j][k] = 0.0f;

    for (i = 0; i < eng->num_rules; i++) {
        float activation = 1.0f;
        for (j = 0; j < FUZZY_NUM_INPUTS; j++) {
            int set_idx = eng->rules[i].antecedent[j];
            if (set_idx < 0) continue; /* don't care */
            activation = fmin2(activation, eng->mu_in[j][set_idx]);
        }
        activation *= eng->rules[i].weight;
        eng->rule_activation[i] = activation;

        /* Aggregação por máximo (OR) nos conjuntos de saída */
        for (j = 0; j < FUZZY_NUM_OUTPUTS; j++) {
            int out_set = eng->rules[i].consequent[j];
            eng->mu_out[j][out_set] = fmax2(eng->mu_out[j][out_set], activation);
        }
    }

    /* ---- 3. Defuzzificação — Centroid (centro de gravidade) ---- */
    for (j = 0; j < FUZZY_NUM_OUTPUTS; j++) {
        float num = 0.0f, den = 0.0f;
        float lo  = eng->outputs[j].min_val;
        float hi  = eng->outputs[j].max_val;
        float step = (hi - lo) / (float)FUZZY_RESOLUTION;

        for (k = 0; k <= FUZZY_RESOLUTION; k++) {
            float x = lo + k * step;
            /* Grau agregado = máximo de todos os conjuntos de saída */
            float mu = 0.0f;
            for (int s = 0; s < eng->outputs[j].num_sets; s++) {
                float m = fuzzy_membership(&eng->outputs[j].sets[s], x);
                /* Clamp pelo grau de ativação do conjunto */
                m = fmin2(m, eng->mu_out[j][s]);
                mu = fmax2(mu, m);
            }
            num += x * mu;
            den += mu;
        }

        eng->crisp_out[j] = (den > 1e-6f) ? (num / den) : 0.0f;
        outputs[j] = fclamp(eng->crisp_out[j], 0.0f, 100.0f);
    }
}
