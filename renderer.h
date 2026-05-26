/*
 * Module:      renderer
 * File:        renderer.h
 * Description: Câmera orbital, loop de renderização e efeitos visuais (bloom, fog).
 * Responsibility: Orquestrar o frame 3D completo; não contém lógica de domínio.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT  720
#define TARGET_FPS      60

typedef struct {
    Camera3D camera;
    float    cam_angle_h;   /* ângulo horizontal (graus)          */
    float    cam_angle_v;   /* ângulo vertical (graus)            */
    float    cam_distance;  /* distância ao alvo                  */
    Vector3  cam_target;    /* ponto de foco orbital              */
    int      wireframe;     /* 0 = sólido, 1 = wireframe          */
    float    fog_density;   /* densidade de névoa (0.0–0.05)      */
} RendererState;

/* Inicializa janela Raylib e câmera orbital */
void renderer_init(RendererState *r);

/* Atualiza câmera orbital com input de mouse/teclado; dt em segundos */
void renderer_update_camera(RendererState *r, float dt);

/* Fecha janela e libera recursos Raylib */
void renderer_shutdown(void);

#endif /* RENDERER_H */
