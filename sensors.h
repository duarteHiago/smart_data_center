/*
 * Module:      sensors
 * File:        sensors.h
 * Description: Sensores simulados com variação temporal realista.
 *              Temperatura, umidade e carga elétrica por rack.
 * Responsibility: Gerar leituras verossímeis sem hardware real.
 *
 * Arduino Mega 2560 pin mapping:
 *   - Temp sensors (DS18B20):  One-Wire → pino 22 (PL1) por rack
 *   - Humidity (DHT22):        pino 24 (PL5)
 *   - Current clamp (ACS712): pino A0–A7 (analog, ADC0–ADC7)
 *
 * ARM Cortex-M4 note:
 *   ADC leitura: registrador ADC1->DR após habilitar ADC_CR2_ADON
 *   One-Wire timing via TIM3 em modo input capture (CCR1)
 */

#ifndef SENSORS_H
#define SENSORS_H

#define NUM_RACKS 8

/* Leitura instantânea de um rack */
typedef struct {
    float temperature;  /* °C   — faixa normal 20–45 °C            */
    float humidity;     /* %    — faixa normal 30–60 %             */
    float load;         /* %    — carga elétrica 0–100 %           */
    float power_w;      /* W    — potência instantânea do rack      */
    int   alert;        /* 1 se temperatura > limiar crítico        */
} RackSensor;

/* Estado global de sensores */
typedef struct {
    RackSensor racks[NUM_RACKS];
    float      ambient_temp;    /* temperatura ambiente da sala     */
    float      ambient_humid;   /* umidade ambiente                 */
    float      total_power_kw;  /* soma dos racks em kW             */
    float      time_elapsed;    /* tempo de simulação em segundos   */
} SensorState;

/* Inicializa sensores com valores basais plausíveis */
void sensors_init(SensorState *s);

/* Atualiza todos os sensores; dt em segundos.
 * crac_factor : 1.0 = todos OK, 0.55 = um CRAC falhou.
 * fan_speed   : 0–100 %; high fan → racks mais frios.                  */
void sensors_update(SensorState *s, float dt,
                    float crac_factor, float fan_speed);

/* Média de temperatura entre todos os racks */
float sensors_avg_temp(const SensorState *s);

/* Média de carga entre todos os racks */
float sensors_avg_load(const SensorState *s);

/* Rack mais quente: retorna índice 0–7 */
int sensors_hottest_rack(const SensorState *s);

#endif /* SENSORS_H */
