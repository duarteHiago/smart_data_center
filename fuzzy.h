/*
 * Module:      fuzzy
 * File:        fuzzy.h
 * Description: Motor de lógica fuzzy completo — fuzzificação, base de regras Mamdani,
 *              defuzzificação por centro de gravidade (centroid).
 * Responsibility: Toda a computação fuzzy. Sem dependência de Raylib.
 *              Equivale ao firmware que rodaria no ARM Cortex-M4.
 *
 * Arduino Mega 2560 / ARM Cortex-M4 context:
 *   - Executa na ISR de timer (TIM2) a cada 500 ms
 *   - Registrador NVIC_ISER0 habilita interrupção do TIM2 (bit 28)
 *   - Stack alocada estaticamente — sem heap no firmware real
 */

#ifndef FUZZY_H
#define FUZZY_H

/* ---- Dimensões estáticas ---- */
#define FUZZY_MAX_SETS     5   /* conjuntos por variável                  */
#define FUZZY_MAX_POINTS   4   /* pontos de corte por conjunto trapezoidal*/
#define FUZZY_MAX_RULES   25   /* regras na base                          */
#define FUZZY_RESOLUTION 100   /* amostras para defuzzificação            */

/* ---- Variáveis de entrada (índices) ---- */
#define FUZZY_IN_TEMP     0   /* temperatura média dos racks (°C)        */
#define FUZZY_IN_LOAD     1   /* carga elétrica média (%)                */
#define FUZZY_IN_HUMID    2   /* umidade relativa (%)                    */
#define FUZZY_NUM_INPUTS  3

/* ---- Variáveis de saída (índices) ---- */
#define FUZZY_OUT_FAN     0   /* velocidade dos fans (0–100 %)           */
#define FUZZY_OUT_PDU     1   /* redistribuição de carga PDU (0–100 %)   */
#define FUZZY_NUM_OUTPUTS 2

/* ---- Rótulos dos conjuntos fuzzy ---- */
typedef enum {
    FS_VERY_LOW  = 0,
    FS_LOW       = 1,
    FS_MEDIUM    = 2,
    FS_HIGH      = 3,
    FS_VERY_HIGH = 4
} FuzzyLabel;

/* Conjunto fuzzy trapezoidal/triangular (a=b → triângulo) */
typedef struct {
    float a, b, c, d;   /* trapézio: /‾\  com a≤b≤c≤d               */
    FuzzyLabel label;
} FuzzySet;

/* Variável linguística */
typedef struct {
    float       min_val;
    float       max_val;
    int         num_sets;
    FuzzySet    sets[FUZZY_MAX_SETS];
    const char *name;
} FuzzyVar;

/* Regra: IF in[0]=A AND in[1]=B AND in[2]=C THEN out[0]=X AND out[1]=Y */
typedef struct {
    int antecedent[FUZZY_NUM_INPUTS];   /* índice do conjunto (-1 = "any") */
    int consequent[FUZZY_NUM_OUTPUTS];  /* índice do conjunto de saída      */
    float weight;                        /* peso da regra (0.0–1.0)          */
} FuzzyRule;

/* Estado completo do motor fuzzy — sem ponteiros, sem heap */
typedef struct {
    FuzzyVar   inputs [FUZZY_NUM_INPUTS];
    FuzzyVar   outputs[FUZZY_NUM_OUTPUTS];
    FuzzyRule  rules  [FUZZY_MAX_RULES];
    int        num_rules;

    /* Graus de pertinência fuzzificados */
    float mu_in [FUZZY_NUM_INPUTS] [FUZZY_MAX_SETS];
    /* Grau de ativação de cada regra */
    float rule_activation[FUZZY_MAX_RULES];
    /* Conjuntos de saída agregados (grau máximo atingido por conjunto) */
    float mu_out[FUZZY_NUM_OUTPUTS][FUZZY_MAX_SETS];
    /* Saídas defuzzificadas */
    float crisp_out[FUZZY_NUM_OUTPUTS];
} FuzzyEngine;

/* ---- API pública ---- */

/* Inicializa o motor com variáveis e regras padrão para o data center */
void fuzzy_init(FuzzyEngine *eng);

/* Executa um ciclo completo: fuzzifica -> avalia regras -> defuzzifica
 * inputs[3]: {temperatura, carga, umidade}
 * outputs[2]: {fan_speed, pdu_balance} — valores 0.0–100.0
 */
void fuzzy_compute(FuzzyEngine *eng,
                   const float inputs[FUZZY_NUM_INPUTS],
                   float outputs[FUZZY_NUM_OUTPUTS]);

/* Grau de pertinência de x no conjunto trapezoidal s */
float fuzzy_membership(const FuzzySet *s, float x);

/* Retorna o nome textual de um label fuzzy */
const char *fuzzy_label_name(FuzzyLabel l);

#endif /* FUZZY_H */
