/*
 * Module:      signals
 * File:        signals.h
 * Description: Linhas de sinal animadas em 3D mostrando a comunicação entre
 *              o Arduino e os racks: leitura de sensores (rack→Arduino) e
 *              comandos PWM (Arduino→rack/fan).
 * Responsibility: Visualização didática do fluxo de controle embarcado.
 *
 * Dois tipos de sinal:
 *   SENSOR  — dado de temperatura/corrente saindo do rack em direção ao Arduino
 *              Cor azul→vermelho conforme temperatura. Representam ADC + One-Wire.
 *   PWM_CMD — comando de velocidade saindo do Arduino em direção ao fan do rack
 *              Cor verde→laranja conforme intensidade. Representam TIM3->CCR*.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

#include "raylib.h"
#include "sensors.h"

/* Pulsos simultâneos por caminho (spacing visual) */
#define SIG_PULSES   3
/* 8 caminhos sensor + 8 caminhos PWM */
#define SIG_NUM_PATHS (NUM_RACKS * 2)

typedef enum {
    SIG_SENSOR = 0,   /* rack → Arduino (leitura)   */
    SIG_PWM    = 1    /* Arduino → rack  (comando)  */
} SignalType;

typedef struct {
    Vector3    from;
    Vector3    to;
    float      pulse_t[SIG_PULSES]; /* posição 0.0–1.0 de cada pulso no caminho */
    float      speed;               /* avanço por segundo (0.0–1.0)             */
    Color      line_color;          /* cor da linha base (fina, semi-transparente)*/
    Color      pulse_color;         /* cor do pulso (esfera brilhante)           */
    float      intensity;           /* 0.0–1.0 → controla tamanho do pulso      */
    char       label[24];           /* "38.2°C" ou "PWM 74%"                    */
    SignalType type;
    int        rack_id;
    int        active;
} SignalPath;

typedef struct {
    SignalPath paths[SIG_NUM_PATHS];
    float      time;
    /* Índice do rack mais quente (para destaque especial) */
    int        hottest_rack;
} SignalSystem;

/* Inicializa caminhos com posições do Arduino e dos racks */
void signals_init(SignalSystem *ss,
                  Vector3 arduino_pos,
                  const Vector3 rack_positions[NUM_RACKS]);

/* Atualiza pulsos e rótulos com valores atuais de sensores e fan_speed */
void signals_update(SignalSystem *ss, float dt,
                    float fan_speed,
                    const RackSensor sensors[NUM_RACKS],
                    int hottest_rack);

/* Desenha linhas base, pulsos animados e rótulos flutuantes */
void signals_draw(const SignalSystem *ss);

#endif /* SIGNALS_H */
