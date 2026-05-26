/*
 * Module:      main
 * File:        main.c
 * Description: Ponto de entrada. Integra todos os módulos e executa o loop
 *              principal de simulação e renderização.
 * Responsibility: Orquestração; sem lógica de domínio própria.
 *
 * Fluxo por frame:
 *   1. Sensores atualizam leituras (sensors_update)
 *   2. Motor fuzzy computa saídas (fuzzy_compute)
 *   3. Partículas, racks e sinais atualizam estado visual
 *   4. Frame 3D renderizado (cena + sinais animados + destaque de alerta)
 *   5. HUD 2D sobreposto (sensores, fuzzy, regras ativas, gráfico causa/efeito)
 *
 * Arduino Mega 2560 / ARM Cortex-M4 equivalent main loop:
 *   int main(void) {
 *       SystemClock_Config();   // PLL 168 MHz
 *       HAL_Init();
 *       sensor_hw_init();       // ADC, I2C, One-Wire
 *       fuzzy_init(&g_eng);
 *       TIM2_Start_IT();        // ISR a cada 500 ms
 *       while (1) {
 *           HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
 *       }
 *   }
 *   // ISR faz sensors_update + fuzzy_compute + atualiza TIM3->CCR* (PWM fans)
 */

#include "raylib.h"
#include "fuzzy.h"
#include "sensors.h"
#include "arduino.h"
#include "scene.h"
#include "rack.h"
#include "airflow.h"
#include "signals.h"
#include "renderer.h"
#include "hud.h"
#include "inspector.h"
#include "mlp.h"
#include "compare.h"

#include <stdio.h>
#include <math.h>

/* ---- Estado global estático (sem malloc) ---- */
static FuzzyEngine    g_fuzzy;
static SensorState    g_sensors;
static ArduinoModel   g_arduino;
static SceneEnv       g_scene;
static RackModel      g_racks[NUM_RACKS];
static AirflowSystem  g_airflow;
static SignalSystem   g_signals;
static RendererState  g_renderer;
static HudState       g_hud;
static InspectorState g_inspector;
static MlpNet         g_mlp;
static CompareState   g_compare;

#define LOG_INTERVAL 5.0f

int main(void)
{
    /* ---- Inicialização ---- */
    fuzzy_init   (&g_fuzzy);
    sensors_init (&g_sensors);
    scene_init   (&g_scene);
    rack_init_all(g_racks);
    airflow_init (&g_airflow);
    renderer_init(&g_renderer);
    hud_init     (&g_hud);
    mlp_init     (&g_mlp);
    compare_init (&g_compare);

    Vector3 arduino_pos = { 9.5f, 0.8f, -6.5f };
    arduino_init(&g_arduino, arduino_pos);

    /* Coleta posições dos racks para o sistema de sinais */
    Vector3 rack_positions[NUM_RACKS];
    for (int i = 0; i < NUM_RACKS; i++)
        rack_positions[i] = rack_get_position(&g_racks[i]);

    signals_init(&g_signals, arduino_pos, rack_positions);
    inspector_init(&g_inspector, g_racks, arduino_pos);

    float fan_speed   = 50.0f;
    float pdu_balance = 50.0f;
    float fuzzy_inputs [FUZZY_NUM_INPUTS];
    float fuzzy_outputs[FUZZY_NUM_OUTPUTS];
    float log_timer   = 0.0f;
    float sim_time    = 0.0f;

    hud_log(&g_hud, "Sistema iniciado.");
    hud_log(&g_hud, "Controlador fuzzy ativo.");
    hud_log(&g_hud, "Monitorando 8 racks.");
    hud_log(&g_hud, "Sinais 3D: azul=sensor, verde=PWM");

    /* ---- Loop principal ---- */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        sim_time += dt;

        /* 1. Sensores */
        float crac_factor = (g_hud.crac_state == 0) ? 1.0f : 0.55f;
        g_scene.crac_failed[0] = (g_hud.crac_state == 1) ? 1 : 0;
        g_scene.crac_failed[1] = (g_hud.crac_state == 2) ? 1 : 0;

        sensors_update(&g_sensors, dt, crac_factor, fan_speed);
        int hot_rack = sensors_hottest_rack(&g_sensors);

        /* 2. Lógica fuzzy (sempre computa para atualizar graus no HUD) */
        fuzzy_inputs[FUZZY_IN_TEMP]  = sensors_avg_temp(&g_sensors);
        fuzzy_inputs[FUZZY_IN_LOAD]  = sensors_avg_load(&g_sensors);
        fuzzy_inputs[FUZZY_IN_HUMID] = g_sensors.ambient_humid;
        fuzzy_compute(&g_fuzzy, fuzzy_inputs, fuzzy_outputs);

        if (!g_hud.manual_override) {
            fan_speed   = fuzzy_outputs[FUZZY_OUT_FAN];
            pdu_balance = fuzzy_outputs[FUZZY_OUT_PDU];
        } else {
            fan_speed   = g_hud.manual_fan_speed;
            pdu_balance = g_hud.manual_pdu;
        }

        /* 3. Atualizar visuais */
        for (int i = 0; i < NUM_RACKS; i++)
            rack_update(&g_racks[i], &g_sensors.racks[i], dt);

        airflow_update(&g_airflow, dt, fan_speed, sensors_avg_temp(&g_sensors));
        arduino_update(&g_arduino, dt, fan_speed, pdu_balance);
        scene_update_lighting(&g_scene, sensors_avg_temp(&g_sensors));
        signals_update(&g_signals, dt, fan_speed, g_sensors.racks, hot_rack);
        renderer_update_camera(&g_renderer, dt);
        inspector_update(&g_inspector, g_renderer.camera);
        hud_update(&g_hud, &g_sensors, fan_speed, pdu_balance);
        mlp_forward(&g_mlp,
                    sensors_avg_temp(&g_sensors),
                    sensors_avg_load(&g_sensors),
                    fan_speed);
        compare_update(&g_compare, fan_speed,
                       sensors_avg_temp(&g_sensors),
                       sensors_avg_load(&g_sensors), dt);

        /* Log periódico */
        log_timer += dt;
        if (log_timer >= LOG_INTERVAL) {
            log_timer = 0.0f;
            char buf[HUD_LOG_LEN];
            snprintf(buf, sizeof(buf),
                     "[%.0fs] R%d: %.1fC  Fan:%.0f%%  PDU:%.0f%%",
                     sim_time, hot_rack,
                     g_sensors.racks[hot_rack].temperature,
                     fan_speed, pdu_balance);
            hud_log(&g_hud, buf);
            for (int i = 0; i < NUM_RACKS; i++) {
                if (g_sensors.racks[i].alert) {
                    snprintf(buf, sizeof(buf), "ALERTA: Rack %d acima de 42C!", i);
                    hud_log(&g_hud, buf);
                }
            }
        }

        /* ---- Renderização ---- */
        BeginDrawing();
        ClearBackground((Color){ 8, 8, 18, 255 });

        BeginMode3D(g_renderer.camera);
            /* Opacos primeiro */
            scene_draw(&g_scene);
            rack_draw_all(g_racks);
            rack_draw_hottest_highlight(&g_racks[hot_rack], sim_time);
            airflow_draw(&g_airflow);
            arduino_draw(&g_arduino);
            signals_draw(&g_signals);
            inspector_draw_3d(&g_inspector);
            DrawGrid(24, 1.0f);

            /* Transparentes por último: corredor frio sem depth write */
            scene_draw_transparent(&g_scene);
        EndMode3D();

        hud_draw(&g_hud, &g_sensors, &g_fuzzy, &g_mlp, &g_compare,
                 fan_speed, pdu_balance);

        /* Nao desenhar inspector sobre paineis de overlay */
        if (!g_hud.help_open && !g_hud.nn_open && !g_hud.arm_open &&
            !g_hud.compare_open && !g_hud.heatmap_open) {
            inspector_draw_2d(&g_inspector, g_racks, &g_sensors, &g_fuzzy,
                              fan_speed, pdu_balance, g_hud.manual_override);
            inspector_draw_labels(g_renderer.camera, g_racks);
        }

        EndDrawing();
    }

    renderer_shutdown();
    return 0;
}
