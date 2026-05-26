/*
 * Module:      arduino
 * File:        arduino.c
 * Description: Renderização 3D da PCB Arduino Mega 2560 com animação de LEDs.
 * Responsibility: Visual do hardware embarcado na cena 3D.
 *
 * Arduino Mega 2560 physical layout (desenhado aproximado):
 *   Comprimento: 10.16 cm  Largura: 5.33 cm  (escala ~0.1 no mundo 3D)
 *
 * ARM Cortex-M4 note:
 *   LED on:  GPIOB->BSRR  = (1u << LED_PIN)
 *   LED off: GPIOB->BRR   = (1u << LED_PIN)
 *   PWM duty via TIM3->CCR2 conforme frequência do timer
 */

#include "arduino.h"
#include <math.h>
#include <string.h>

/* Posições relativas dos LEDs na placa (offset em unidades de mundo) */
static const Vector3 LED_OFFSETS[ARDUINO_NUM_LEDS] = {
    { 0.02f,  0.05f,  0.01f }, /* LED power (verde) */
    { 0.04f,  0.05f,  0.01f }, /* LED TX serial     */
    { 0.06f,  0.05f,  0.01f }, /* LED RX serial     */
    {-0.02f,  0.05f, -0.01f }, /* LED status fan 1  */
    { 0.0f,   0.05f, -0.01f }, /* LED status fan 2  */
    { 0.02f,  0.05f, -0.01f }, /* LED alerta        */
};

static const Color LED_BASE_COLORS[ARDUINO_NUM_LEDS] = {
    { 50, 220,  50, 255 }, /* verde  */
    {220, 160,  50, 255 }, /* âmbar  */
    { 50, 160, 220, 255 }, /* azul   */
    { 50, 220, 220, 255 }, /* ciano  */
    { 50, 220, 220, 255 }, /* ciano  */
    {220,  50,  50, 255 }, /* vermelho */
};

static const float LED_BLINK_RATES[ARDUINO_NUM_LEDS] = {
    0.0f,  /* power: sempre ligado      */
    4.0f,  /* TX: pisca rápido          */
    3.0f,  /* RX: pisca rápido          */
    1.5f,  /* fan1: pisca com velocidade*/
    1.5f,  /* fan2                      */
    0.5f,  /* alerta: pisca lento       */
};

void arduino_init(ArduinoModel *am, Vector3 position)
{
    memset(am, 0, sizeof(*am));
    am->position   = position;
    am->rotation_y = 15.0f;
    am->scale      = 0.8f;
    for (int i = 0; i < ARDUINO_NUM_LEDS; i++) {
        am->led_color[i]      = LED_BASE_COLORS[i];
        am->led_brightness[i] = 1.0f;
    }
}

void arduino_update(ArduinoModel *am, float dt, float fan_speed, float pdu_balance)
{
    /* Velocidade de blink do LED de fan proporcional à velocidade do fan */
    float fan_rate = 0.5f + fan_speed * 0.04f; /* 0.5–4.5 Hz */

    for (int i = 0; i < ARDUINO_NUM_LEDS; i++) {
        float rate = (i == 3 || i == 4) ? fan_rate : LED_BLINK_RATES[i];

        if (rate < 0.01f) {
            am->led_brightness[i] = 1.0f;
            continue;
        }
        am->led_timer[i] += dt * rate * 6.2832f;
        /* Brilho: onda senoidal retificada → pulso suave */
        float b = sinf(am->led_timer[i]);
        am->led_brightness[i] = b > 0.0f ? b : 0.0f;
    }

    /* LED de alerta: vermelho intenso se fan > 80 ou pdu > 80 */
    if (fan_speed > 80.0f || pdu_balance > 80.0f) {
        am->led_color[5] = (Color){ 255, 30, 30, 255 };
    } else {
        am->led_color[5] = (Color){ 220, 50, 50, 255 };
    }

    /* Armazena PWM para exibição */
    am->fan_pwm[0] = fan_speed;
    am->fan_pwm[1] = fan_speed;
    am->fan_pwm[2] = pdu_balance;
    am->fan_pwm[3] = pdu_balance;
}

static void draw_pin_row(Vector3 base, int count, float spacing, Color col)
{
    for (int i = 0; i < count; i++) {
        Vector3 p = { base.x + i * spacing, base.y, base.z };
        DrawCylinder(p, 0.002f, 0.002f, 0.012f, 4, col);
    }
}

void arduino_draw(const ArduinoModel *am)
{
    float s = am->scale;
    Vector3 pos = am->position;

    /* Corpo da PCB — placa verde */
    DrawCube(pos,
             0.22f * s,   /* largura  */
             0.01f * s,   /* espessura*/
             0.12f * s,   /* comprimento*/
             (Color){ 20, 100, 40, 255 });

    DrawCubeWires(pos,
                  0.22f * s, 0.01f * s, 0.12f * s,
                  (Color){ 10, 60, 20, 255 });

    /* Chip ATmega2560 (retângulo preto maior) */
    Vector3 chip_pos = { pos.x, pos.y + 0.006f * s, pos.z };
    DrawCube(chip_pos,
             0.06f * s, 0.004f * s, 0.06f * s,
             (Color){ 20, 20, 20, 255 });

    /* Cristal oscilador */
    Vector3 xtal_pos = { pos.x + 0.06f * s, pos.y + 0.007f * s, pos.z - 0.02f * s };
    DrawCylinder(xtal_pos, 0.006f * s, 0.006f * s, 0.02f * s, 8,
                 (Color){ 180, 180, 50, 255 });

    /* Porta USB */
    Vector3 usb_pos = { pos.x - 0.1f * s, pos.y + 0.008f * s, pos.z };
    DrawCube(usb_pos, 0.018f * s, 0.012f * s, 0.014f * s,
             (Color){ 160, 160, 160, 255 });

    /* Conector de alimentação */
    Vector3 pwr_pos = { pos.x - 0.09f * s, pos.y + 0.008f * s, pos.z + 0.04f * s };
    DrawCube(pwr_pos, 0.012f * s, 0.014f * s, 0.012f * s,
             (Color){ 30, 30, 30, 255 });

    /* Fileiras de pinos digitais */
    Vector3 pin_row1 = { pos.x - 0.08f * s, pos.y + 0.014f * s, pos.z - 0.05f * s };
    draw_pin_row(pin_row1, 18, 0.008f * s, (Color){ 200, 200, 50, 255 });

    Vector3 pin_row2 = { pos.x - 0.08f * s, pos.y + 0.014f * s, pos.z + 0.05f * s };
    draw_pin_row(pin_row2, 18, 0.008f * s, (Color){ 200, 200, 50, 255 });

    /* Pinos analógicos */
    Vector3 apin_row = { pos.x + 0.02f * s, pos.y + 0.014f * s, pos.z - 0.05f * s };
    draw_pin_row(apin_row, 8, 0.008f * s, (Color){ 50, 200, 200, 255 });

    /* LEDs */
    for (int i = 0; i < ARDUINO_NUM_LEDS; i++) {
        Vector3 lp = {
            pos.x + LED_OFFSETS[i].x * s,
            pos.y + LED_OFFSETS[i].y * s,
            pos.z + LED_OFFSETS[i].z * s
        };
        float b = am->led_brightness[i];
        Color c = am->led_color[i];
        Color lit = {
            (unsigned char)(c.r * b),
            (unsigned char)(c.g * b),
            (unsigned char)(c.b * b),
            255
        };
        DrawSphere(lp, 0.004f * s, lit);
        /* Halo de brilho */
        if (b > 0.3f) {
            Color halo = { c.r, c.g, c.b, (unsigned char)(60 * b) };
            DrawSphere(lp, 0.008f * s, halo);
        }
    }
}
