/*
 * Module:      arduino
 * File:        arduino.h
 * Description: Modelo 3D simplificado da PCB Arduino Mega 2560.
 *              Renderiza placa, pinos e LEDs piscando com Raylib.
 * Responsibility: Representação visual do hardware embarcado na cena.
 *
 * Arduino Mega 2560 pin mapping (comentários de firmware):
 *   D22 (PL1)  — One-Wire temp sensor bus
 *   D24 (PL5)  — DHT22 humidity
 *   D2  (PE4)  — Fan PWM rack 0-1  (Timer3 OC3B)
 *   D3  (PE5)  — Fan PWM rack 2-3  (Timer3 OC3C)
 *   D4  (PG5)  — Fan PWM rack 4-5  (Timer0 OC0B)
 *   D5  (PE3)  — Fan PWM rack 6-7  (Timer3 OC3A)
 *   A0–A7      — ACS712 current clamps (ADC0–ADC7)
 *   SDA/SCL    — I2C para LCD 20x4 (TWI, pinos 20/21)
 *
 * ARM Cortex-M4 register notes:
 *   PWM fans: TIM3->CCR2 = (uint16_t)(duty * TIM3->ARR / 100)
 *   GPIO LED: GPIOB->BSRR = (1 << pin)  /  GPIOB->BRR = (1 << pin)
 */

#ifndef ARDUINO_H
#define ARDUINO_H

#include "raylib.h"

/* Número de LEDs visíveis na placa */
#define ARDUINO_NUM_LEDS 6

typedef struct {
    Vector3 position;           /* posição na cena 3D               */
    float   rotation_y;         /* rotação em graus                 */
    float   scale;              /* escala do modelo                 */

    /* Estado dos LEDs da placa (animados) */
    float   led_brightness[ARDUINO_NUM_LEDS]; /* 0.0–1.0           */
    float   led_timer[ARDUINO_NUM_LEDS];      /* temporizador de blink */
    Color   led_color[ARDUINO_NUM_LEDS];

    /* Valores de PWM dos fans (0–100 %) para exibir na placa */
    float   fan_pwm[4];
} ArduinoModel;

/* Inicializa modelo e posição */
void arduino_init(ArduinoModel *am, Vector3 position);

/* Atualiza animação de LEDs; dt em segundos */
void arduino_update(ArduinoModel *am, float dt, float fan_speed, float pdu_balance);

/* Renderiza a placa 3D na cena */
void arduino_draw(const ArduinoModel *am);

#endif /* ARDUINO_H */
