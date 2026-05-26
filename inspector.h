/*
 * Module:      inspector
 * File:        inspector.h
 * Description: Inspeção interativa de objetos 3D via hover do mouse.
 *              Ray casting contra bounding boxes; tooltip 2D com dados ao vivo.
 * Responsibility: Picking e overlay informativo; sem lógica de simulação.
 */

#ifndef INSPECTOR_H
#define INSPECTOR_H

#include "raylib.h"
#include "sensors.h"
#include "fuzzy.h"
#include "rack.h"

typedef enum {
    INSPECT_NONE       = 0,
    INSPECT_RACK       = 1,
    INSPECT_CRAC       = 2,
    INSPECT_ARDUINO    = 3,
    INSPECT_COLD_AISLE = 4,
} InspectType;

typedef struct {
    InspectType type;
    int         id;        /* índice do rack (0-7), crac (0-1), etc. */
    BoundingBox box;
} InspectTarget;

#define INSPECT_MAX_TARGETS 14  /* 8 racks + 2 CRACs + 1 Arduino + 1 corredor */

typedef struct {
    InspectTarget targets[INSPECT_MAX_TARGETS];
    int           num_targets;
    int           hovered_idx;   /* -1 = nenhum objeto sob o cursor */
} InspectorState;

/* Registra todos os objetos inspecionáveis com suas bounding boxes */
void inspector_init(InspectorState *ins,
                    const RackModel racks[NUM_RACKS],
                    Vector3 arduino_pos);

/* Atualiza hovered_idx via ray cast (chamar a cada frame, antes de BeginMode3D) */
void inspector_update(InspectorState *ins, Camera3D camera);

/* Destaque 3D do objeto sob o cursor — chamar DENTRO de BeginMode3D */
void inspector_draw_3d(const InspectorState *ins);

/* Tooltip 2D com dados ao vivo — chamar FORA de BeginMode3D */
void inspector_draw_2d(const InspectorState *ins,
                       const RackModel       racks[NUM_RACKS],
                       const SensorState    *sensors,
                       const FuzzyEngine    *fuzzy,
                       float                 fan_speed,
                       float                 pdu_balance,
                       int                   manual_override);

/* Labels flutuantes sobre cada objeto — chamar FORA de BeginMode3D */
void inspector_draw_labels(Camera3D camera, const RackModel racks[NUM_RACKS]);

#endif /* INSPECTOR_H */
