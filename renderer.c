/*
 * Module:      renderer
 * File:        renderer.c
 * Description: Inicialização da janela Raylib, câmera orbital e controles.
 * Responsibility: Gerenciar o frame 3D; sem lógica de domínio.
 */

#include "renderer.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void renderer_init(RendererState *r)
{
    memset(r, 0, sizeof(*r));

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Smart Data Center — Fuzzy Control System");
    SetTargetFPS(TARGET_FPS);

    r->cam_angle_h  = 35.0f;
    r->cam_angle_v  = 22.0f;
    r->cam_distance = 14.0f;
    r->cam_target   = (Vector3){ -3.5f, 0.8f, 0.0f };
    r->wireframe    = 0;
    r->fog_density  = 0.005f;

    /* Câmera perspectiva */
    r->camera.fovy       = 60.0f;
    r->camera.projection = CAMERA_PERSPECTIVE;
    r->camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };

    /* Posição inicial da câmera */
    float ah = r->cam_angle_h * (float)M_PI / 180.0f;
    float av = r->cam_angle_v * (float)M_PI / 180.0f;
    r->camera.position = (Vector3){
        r->cam_target.x + r->cam_distance * cosf(av) * sinf(ah),
        r->cam_target.y + r->cam_distance * sinf(av),
        r->cam_target.z + r->cam_distance * cosf(av) * cosf(ah)
    };
    r->camera.target = r->cam_target;
}

void renderer_update_camera(RendererState *r, float dt)
{
    (void)dt;

    /* Botão direito do mouse: orbitar */
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        r->cam_angle_h -= delta.x * 0.4f;
        r->cam_angle_v -= delta.y * 0.4f;
        if (r->cam_angle_v >  89.0f) r->cam_angle_v =  89.0f;
        if (r->cam_angle_v <   5.0f) r->cam_angle_v =   5.0f;
    }

    /* Scroll: zoom */
    float wheel = GetMouseWheelMove();
    r->cam_distance -= wheel * 1.2f;
    if (r->cam_distance <  4.0f) r->cam_distance =  4.0f;
    if (r->cam_distance > 40.0f) r->cam_distance = 40.0f;

    /* Botão do meio: pan */
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        float ah = r->cam_angle_h * (float)M_PI / 180.0f;
        r->cam_target.x -= (cosf(ah) * delta.x - sinf(ah) * delta.y) * 0.02f;
        r->cam_target.z += (sinf(ah) * delta.x + cosf(ah) * delta.y) * 0.02f;
    }

    /* Tecla W/F: wireframe toggle */
    if (IsKeyPressed(KEY_W)) r->wireframe = !r->wireframe;

    /* Recalcula posição da câmera */
    float ah = r->cam_angle_h * (float)M_PI / 180.0f;
    float av = r->cam_angle_v * (float)M_PI / 180.0f;
    r->camera.position = (Vector3){
        r->cam_target.x + r->cam_distance * cosf(av) * sinf(ah),
        r->cam_target.y + r->cam_distance * sinf(av),
        r->cam_target.z + r->cam_distance * cosf(av) * cosf(ah)
    };
    r->camera.target = r->cam_target;
}

void renderer_shutdown(void)
{
    CloseWindow();
}
